#include "Cserver.h"
#include "HttpConnection.h"
#include <iostream>
#include<memory>
#include <utility>
#include"AsioIOServicePool.h"
Cserver::Cserver(boost::asio::io_context&ioc,unsigned short&port):_ioc(ioc),
_acceptor(ioc,tcp::endpoint(boost::asio::ip::tcp::v4(),port)),_socket(ioc){
    cout << "Csever constructed!" << endl;
}
void Cserver::Start()
{
    cout << "Csever constructed! 11111" << endl;
    auto self=shared_from_this();//返回当前对象的shared_ptr
    std::cout << "[Start] refcount after self: " << self.use_count() << std::endl;
    auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
    //捕获当前的shared_ptr
    std::shared_ptr<HttpConnection>new_con=std::make_shared<HttpConnection>(io_context);
    _acceptor.async_accept( new_con->GetSocket(),[self,new_con](beast::error_code ec) { 
        std::cout << "[callback enter] refcount: " << self.use_count() << std::endl;
        try
        {
          //出错则放弃这个连接，继续监听新链接
          if(ec)
          {
            self->Start();
            return;
          }
          //处理新链接，创建hpptConnection类管理新连接
          new_con->Start();
          
          //继续监听新链接
          self->Start();
          std::cout << "[callback exit]  refcount: " << self.use_count() << std::endl;
        }
        catch(std::exception&exp)
        {
            std::cerr<<"Exception:"<<exp.what()<<std::endl;
            self->Start();
          //异常处理  
        }
        std::cout << "[Start] refcount before return: " << self.use_count() << std::endl;
    });
          

}
