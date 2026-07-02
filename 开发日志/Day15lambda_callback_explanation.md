# lambda 回调机制详解

这份笔记解释项目中这类代码为什么叫“回调”，以及这个 lambda 是什么时候保存、什么时候执行的。

示例代码：

```cpp
RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
    auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

    // 业务处理逻辑：
    // 1. 解析 JSON
    // 2. 校验验证码
    // 3. 查询 MySQL
    // 4. 更新密码
    // 5. 写入 HTTP 响应
});
```

## 1. 什么是回调

回调的核心思想是：

> 你先把一段函数逻辑交给系统保存起来，自己不主动调用它。等某个事件发生时，系统再反过来调用你提供的函数。

在你的项目中，“某个事件”就是：

```text
客户端发送 POST /reset_pwd 请求
```

lambda 就是你提前交给系统的处理函数。

## 2. lambda 本质是什么

这段代码：

```cpp
[](std::shared_ptr<HttpConnection> connection) {
    // ...
}
```

是一个 lambda 表达式，也可以理解成一个“匿名函数对象”。

它的参数是：

```cpp
std::shared_ptr<HttpConnection> connection
```

也就是说，将来这个函数被调用时，需要传入一个当前 HTTP 连接对象。

这个 `connection` 里面保存了本次请求和响应：

```cpp
connection->_request   // 客户端发来的 HTTP 请求
connection->_response  // 服务端要返回给客户端的 HTTP 响应
```

所以 lambda 内部才能读取请求体、设置响应头、写响应 body。

## 3. 项目里如何保存这个 lambda

在 `LogicSystem.h` 中，路由处理函数的类型是：

```cpp
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;
```

这表示：

```text
HttpHandler 是一种函数类型
它接收 shared_ptr<HttpConnection>
它没有返回值 void
```

所以你的 lambda 可以被保存到 `HttpHandler` 里面。

`RegPost` 的定义大概是：

```cpp
void LogicSystem::RegPost(std::string url, HttpHandler handler) {
    _post_handlers.insert(make_pair(url, handler));
}
```

当你写：

```cpp
RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
    // ...
});
```

实际含义是：

```text
把 URL "/reset_pwd" 和对应的 lambda 处理函数绑定起来
然后存进 _post_handlers 这张路由表
```

可以简单理解为：

```cpp
_post_handlers["/reset_pwd"] = lambda;
```

注意：这个时候 lambda 只是被保存了，还没有执行。

## 4. 什么时候真正执行 lambda

真正执行发生在客户端请求到来的时候。

流程如下：

```text
客户端发送 POST /reset_pwd
        |
        v
HttpConnection::Start()
        |
        v
http::async_read 异步读取请求
        |
        v
HttpConnection::HandleRequest()
        |
        v
判断请求方法是 POST
        |
        v
LogicSystem::HandlePost(_request.target(), shared_from_this())
        |
        v
在 _post_handlers 里查找 "/reset_pwd"
        |
        v
执行 _post_handlers[path](con)
        |
        v
调用你注册的 lambda
```

关键代码在 `HandlePost`：

```cpp
bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con) {
    if (_post_handlers.find(path) == _post_handlers.end()) {
        return false;
    }

    _post_handlers[path](con);
    return true;
}
```

最关键的是这一句：

```cpp
_post_handlers[path](con);
```

这句代码的意思是：

```text
从 map 中取出 path 对应的函数
然后把 con 作为参数传进去执行
```

如果 `path` 是 `"/reset_pwd"`，那它就等价于：

```cpp
resetPwdLambda(con);
```

也就是执行你当初注册进去的这段代码：

```cpp
[](std::shared_ptr<HttpConnection> connection) {
    // reset_pwd 的业务逻辑
}
```

## 5. 为什么这叫“回调”

因为你的业务代码不是你自己直接调用的。

你没有写：

```cpp
resetPwdHandler(connection);
```

而是先注册：

```cpp
RegPost("/reset_pwd", handler);
```

然后等请求到来时，由框架代码调用：

```cpp
_post_handlers[path](con);
```

所以调用方向是这样的：

```text
你把函数交给 LogicSystem
LogicSystem 保存函数
HttpConnection 收到请求
LogicSystem 再回过头调用你的函数
```

这就是“回调”的含义。

## 6. 注册阶段和执行阶段要分清

注册阶段：

```cpp
RegPost("/reset_pwd", lambda);
```

这个阶段只是建立映射关系：

```text
"/reset_pwd" -> lambda
```

执行阶段：

```cpp
_post_handlers[path](con);
```

这个阶段才是真正调用 lambda。

二者不要混在一起理解。

## 7. 你的 lambda 为什么能拿到当前请求

因为执行时传入了当前连接对象：

```cpp
_post_handlers[path](con);
```

这里的 `con` 来自：

```cpp
shared_from_this()
```

它表示当前正在处理请求的 `HttpConnection` 对象。

因此 lambda 的参数：

```cpp
std::shared_ptr<HttpConnection> connection
```

收到的就是当前连接。

所以你可以在 lambda 里这样访问当前请求：

```cpp
connection->_request.body()
```

也可以这样写当前响应：

```cpp
beast::ostream(connection->_response.body()) << jsonstr;
```

## 8. 和普通函数写法对比

如果不用 lambda，也可以先写一个普通函数：

```cpp
void ResetPwdHandler(std::shared_ptr<HttpConnection> connection) {
    // reset_pwd 的业务逻辑
}
```

然后注册：

```cpp
RegPost("/reset_pwd", ResetPwdHandler);
```

lambda 的好处是可以直接把处理逻辑写在注册路由的位置：

```cpp
RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
    // reset_pwd 的业务逻辑
});
```

所以 lambda 不是特殊魔法，它只是更方便地写了一个函数对象。

## 9. 关于 return true 的小细节

你的 `HttpHandler` 类型是：

```cpp
std::function<void(std::shared_ptr<HttpConnection>)>
```

它的返回值是 `void`。

因此 lambda 里更准确的写法应该是：

```cpp
return;
```

而不是：

```cpp
return true;
```

如果你希望业务处理函数真的返回 `bool`，那就应该把 `HttpHandler` 改成：

```cpp
typedef std::function<bool(std::shared_ptr<HttpConnection>)> HttpHandler;
```

否则 `true` 本身没有业务意义，只是用来提前结束 lambda。

## 10. 一句话总结

你的 lambda 回调机制可以总结成：

```text
RegPost 负责注册 lambda
_post_handlers 负责保存 lambda
HandlePost 负责查找 lambda
_post_handlers[path](con) 负责执行 lambda
connection 负责把当前请求和响应传给 lambda
```

所以这套代码本质是：

```text
URL 路由表 + std::function + lambda + HttpConnection
```

共同实现了一个简单的 HTTP 回调分发系统。
