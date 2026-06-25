#pragma once
#include"Singleton.h"
#include<functional>
#include<map>
#include"const.h"

// 前置声明 HttpConnection，头文件这里只需要用 shared_ptr 保存连接对象，
// 不需要知道 HttpConnection 的完整成员，减少头文件之间的互相包含。
class HttpConnection;

// 每个路由处理函数的统一类型。
// 参数是当前 HTTP 连接对象，处理函数内部负责读取请求并写入响应。
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;

// 业务逻辑分发系统。
// 通过单例保存所有 GET/POST 路由，把不同 URL 分发到对应的处理函数。
class LogicSystem:public Singleton<LogicSystem>
{
    // 允许 Singleton<LogicSystem> 调用私有构造函数创建唯一实例。
    friend class Singleton<LogicSystem>;
public:
  // 析构函数目前没有额外资源需要释放，定义放在 cpp 中。
  ~LogicSystem();

  // 根据请求 path 查找 GET 路由，找到后执行对应处理函数。
  // 返回 true 表示已处理，false 表示没有匹配到路由。
  bool HandleGet(std::string,std::shared_ptr<HttpConnection>);

  // 注册一个 GET 路由，例如 /get_test。
  void RegGet(std::string url,HttpHandler handler);

  // 注册一个 POST 路由，例如 /get_verifycode、/user_register。
  void RegPost(std::string url, HttpHandler handler);

  // 根据请求 path 查找 POST 路由，找到后执行对应处理函数。
  // 返回 true 表示已处理，false 表示没有匹配到路由。
  bool HandlePost(std::string path, std::shared_ptr<HttpConnection> con);
private:
   // 构造函数私有化，保证只能通过 Singleton 获取 LogicSystem 实例。
   // 构造函数里会集中注册内置路由。
   LogicSystem();

   // GET 请求路由表：key 是 URL path，value 是对应业务处理函数。
   std::map<std::string,HttpHandler>_get_handlers;

   // POST 请求路由表：key 是 URL path，value 是对应业务处理函数。
   std::map<std::string, HttpHandler>_post_handlers;
};
