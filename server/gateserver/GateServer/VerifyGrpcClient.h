#include <grpcpp/grpcpp.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include "message.grpc.pb.h"
#include "const.h"
#include "Singleton.h"
using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;


class RPConPool{
public:
    RPConPool(size_t pool_size, std::string host, std::string port):pool_size_(pool_size), host_(host), port_(port), b_stop_(false)
    {
      for (size_t i = 0; i < pool_size_; ++i)
     {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        connections.push(VerifyService::NewStub(channel));
     }
      
    }
    ~RPConPool()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Close();
        while(!connections.empty())
        {
            connections.pop();
        }
    }
    std::unique_ptr<VerifyService::Stub> GetCon()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]
        {
            if(b_stop_)
            {
                return true;
            }
            return!connections.empty();
        });
        if(b_stop_)
        {
            return nullptr;
        }
        auto context = std::move(connections.front());
        connections.pop();  
        return context;
    }
    void returnCon(std::unique_ptr<VerifyService::Stub>context)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_)
        {
            return;
        }
        connections.push(std::move(context));
        cond_.notify_one();
    }
    void Close()
    {
        b_stop_ = true;
        cond_.notify_all();
    }
private:
    std::atomic<bool>b_stop_;
    size_t pool_size_;
    std::string host_;
    std::string port_;
    std::queue<std::unique_ptr<VerifyService::Stub>>connections;
    std::mutex mutex_;
    std::condition_variable cond_;

};
class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;
public:

        GetVerifyRsp GetVarifyCode(std::string email) {
        ClientContext context;
        GetVerifyRsp reply;
        GetVerifyReq request;
        request.set_email(email);
        //错误日志6.8.6：给 gRPC 请求设置 30 秒超时，给 VerifyServer 发邮件留出时间，同时避免 GateServer 一直等待。
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(30));

        auto stub = pool_->GetCon();
        //错误日志6.8.7：连接池取不到 stub 时直接返回 RPCFailed，避免空连接调用导致异常。
        if (stub == nullptr) {
            reply.set_error(ErrorCodes::RPCFailed);
            return reply;
        }

        Status status = stub->GetVerifyCode(&context, request, &reply);
       
        if (status.ok()) {
            pool_->returnCon(std::move(stub));
            return reply;
        }
        else {
            pool_->returnCon(std::move(stub));
            reply.set_error(ErrorCodes::RPCFailed);
            return reply;
        }
    }

private:
    VerifyGrpcClient();

    std::unique_ptr<RPConPool> pool_;
};
