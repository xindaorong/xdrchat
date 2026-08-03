# GateServer 请求→回包流程详解

## 问题

在 `GateServer.cpp` 的 `main()` 中只看到了服务启动和事件循环，没有找到"把 HTTP 响应发回客户端"的逻辑代码。回包到底是在哪里完成的？

---

## 一句话答案

> Lambda 只负责**往内存缓冲区填数据**，真正的 TCP 发包在 `HttpConnection::WriteResponse()` 里，由 `HandleRequest()` 在 Lambda 返回后自动调用。

---

## 涉及的关键文件

| 文件 | 角色 |
|------|------|
| [GateServer.cpp](GateServer.cpp) | 启动入口：创建 Server、启动 `ioc.run()` 事件循环 |
| [Cserver.cpp](Cserver.cpp) | `async_accept` 接收新 TCP 连接，为每个连接创建 `HttpConnection` |
| [HttpConnection.cpp](HttpConnection.cpp) | 读取 HTTP 请求 → 路由到业务 Lambda → **调用 `async_write` 回包** |
| [LogicSystem.cpp](LogicSystem.cpp) | 构造函数注册所有路由 Lambda，`HandlePost`/`HandleGet` 查找并执行 |

---

## 完整请求→回包链路

```
客户端 TCP 连接到达
        │
        ▼
① CServer::Start()                          [Cserver.cpp:17]
   _acceptor.async_accept(...)
   接收到连接 → 创建 std::make_shared<HttpConnection>(io_context)
   调用 new_con->Start()
        │
        ▼
② HttpConnection::Start()                   [HttpConnection.cpp:131]
   http::async_read(_socket, _buffer, _request, callback)
   异步读取 HTTP 原始字节到 _request
   读完后回调执行 ③④⑤
        │
        ▼
③ HttpConnection::HandleRequest()           [HttpConnection.cpp:171]
   判断 _request.method()：
   ├── GET  → LogicSystem::HandleGet(path, shared_from_this())
   └── POST → LogicSystem::HandlePost(path, shared_from_this())
                                           ↓
④ LogicSystem 构造函数注册的 Lambda         [LogicSystem.cpp:22-255]
   以 /user_register 为例：
   ├── 解析 JSON body
   ├── 从 Redis 取验证码校验
   ├── 检查用户是否已存在
   ├── 写入 MySQL 数据库
   └── beast::ostream(connection->_response.body()) << jsonstr;
       ↑ 注意：这行只是把 JSON 字符串写入 _response 的内存缓冲区！
         此时还没有任何数据从网线发出。
        │
        ▼  ← Lambda 返回，控制权回到 HandleRequest()
        │
⑤ HttpConnection::WriteResponse()           [HttpConnection.cpp:219]
   void WriteResponse() {
       _response.content_length(_response.body().size());
       http::async_write(_socket, _response, [self](...) {  ← ★ 真正发包
           self->_socket.shutdown(tcp::socket::shutdown_send, ec);
           self->deadline_.cancel();
       });
   }
```

---

## 关键代码逐段解析

### 第三步：HandleRequest 中的路由与回包

```cpp
// [HttpConnection.cpp:195-209]
if (_request.method() == http::verb::post) {
    // 1. 在 _post_handlers map 中查找对应的 Lambda
    bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
    //  ↑ Lambda 已执行完毕，_response.body() 已被填好 JSON 字符串

    if (!success) {
        _response.result(http::status::not_found);     // 设置 HTTP 404
        _response.set(http::field::content_type, "text/plain");
        beast::ostream(_response.body()) << "url not found\r\n";
        WriteResponse();   // ← 回包！
        return;
    }

    _response.result(http::status::ok);                // 设置 HTTP 200
    _response.set(http::field::server, "GateServer");
    WriteResponse();       // ← 回包！
    return;
}
```

**重点**：`WriteResponse()` 在 `HandlePost` 返回**之后**被调用。Lambda 只是生产者（往 `_response` 里写数据），`WriteResponse()` 才是消费者（把 `_response` 发到 socket）。

### 第四步：Lambda 做了什么（以注册为例）

```cpp
// [LogicSystem.cpp:55-136] 简化版
RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
    auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
    connection->_response.set(http::field::content_type, "text/json");

    Json::Value root;
    // ... 解析 JSON、校验验证码、写库 ...

    root["error"] = 0;
    root["uid"] = uid;
    std::string jsonstr = root.toStyledString();

    // ★ 关键：只写内存缓冲区，不发网络数据
    beast::ostream(connection->_response.body()) << jsonstr;
    return true;
});
```

Lambda 的返回值 `true/false` 告诉 `HandleRequest`：
- `true` → 路径匹配，设置 200 OK，调用 `WriteResponse()`
- `false` → 路径不匹配，设置 404 Not Found，调用 `WriteResponse()`

### 第五步：WriteResponse — 真正发包的地方

```cpp
// [HttpConnection.cpp:219-228]
void HttpConnection::WriteResponse()
{
    auto self = shared_from_this();            // 延长自身生命周期
    _response.content_length(_response.body().size());  // 设置 Content-Length 头
    http::async_write(_socket, _response,      // ★ 异步写入 TCP socket
        [self](beast::error_code ec, std::size_t) {
            self->_socket.shutdown(tcp::socket::shutdown_send, ec);  // 关闭写端
            self->deadline_.cancel();           // 取消 60 秒超时定时器
        });
}
```

`http::async_write` 是 Boost.Beast 的异步写函数，内部会把 `_response` 序列化成 HTTP 协议格式的字节流，通过 `_socket` 发回客户端。

---

## 架构分工图

```
                     GateServer.cpp (main)
                     ┌──────────────────┐
                     │ 创建 io_context  │
                     │ 创建 CServer     │
                     │ ioc.run()        │  ← 事件循环驱动一切
                     └──────┬───────────┘
                            │
              ┌─────────────┼─────────────┐
              │             │             │
              ▼             ▼             ▼
        CServer       HttpConnection  LogicSystem
        ┌──────┐      ┌───────────┐   ┌───────────┐
        │接收   │      │读请求     │   │路由Lambda │
        │TCP连接│ ──→  │路由       │ ──→│执行业务   │
        │      │      │写响应  ←──│   │填_response│
        └──────┘      └───────────┘   └───────────┘
                           │
                           │ async_write(_socket, _response, ...)
                           ▼
                      ┌───────────┐
                      │ 客户端收到  │
                      │ HTTP 响应  │
                      └───────────┘
```

---

## 总结

| 你在找的 | 实际位置 |
|----------|----------|
| 启动监听 | `Cserver::Start()` — `async_accept` |
| 读请求 | `HttpConnection::Start()` — `async_read` |
| 路由分发 | `HttpConnection::HandleRequest()` |
| 业务处理 | `LogicSystem` 构造函数的 Lambda |
| **回包发送** | `HttpConnection::WriteResponse()` — `async_write` |
| 事件循环调度 | `GateServer.cpp:main()` — `ioc.run()` |

**`GateServer.cpp` 只负责启动和事件循环——它完全不需要知道回包细节。回包是 `HttpConnection` 这个类自动完成的：Lambda 写入 `_response.body()`，`HandleRequest()` 调用 `WriteResponse()`，`async_write` 发出数据。**
