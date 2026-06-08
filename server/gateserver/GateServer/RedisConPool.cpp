#include "RedisConPool.h"
RedisConPool::RedisConPool(size_t poolSize, const char* host, int port, const char* pwd)
    : poolSize_(poolSize), host_(host), port_(port), b_stop_(false) {

    for (size_t i = 0; i < poolSize_; ++i) {
        //1 建立TCP连接
        auto* context = redisConnect(host, port);
        //检查是否连接成功
        if (context == nullptr || context->err != 0) {
            //连接失败：若上下文对象已成功分配则释放，避免内存泄露
            if (context != nullptr) {
                redisFree(context);
            }
            continue;
        }
        //2发送AUTH命令进行密码认证
        auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd);
        //3检查认证结果
        if (reply->type == REDIS_REPLY_ERROR) {
            //认证失败:输出错误信息，释放reply对象
            std::cout << "认证失败" << std::endl;
            //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
            freeReplyObject(reply);
            redisFree(context);
            continue;
        }

        //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
        freeReplyObject(reply);
        std::cout << "认证成功" << std::endl;

        //6将有效连接存入队列，供getConnection()取出使用
        connections_.push(context);
    }

}

RedisConPool::~RedisConPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) {
        connections_.pop();
    }
}
//RedisConPool::redisContext*getConnection()
//{
//    std::unique_lock<std::mutex>lock(mutex_);
//}
redisContext* RedisConPool::getConnection()
{
    std::unique_lock<std::mutex> lock(mutex_);

    // 后面写你的获取连接的代码
    return nullptr; // 示例
}