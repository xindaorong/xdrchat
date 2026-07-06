# Day16--HttpMgr 单例与登录请求分发流程

## 1. 核心结论

`HttpMgr::GetInstance()` 的作用是获取 `HttpMgr` 的全局唯一实例对象。

登录界面不会直接创建或管理 HTTP 请求对象，而是通过：

```cpp
HttpMgr::GetInstance()->PostHttpReq(
    QUrl(gate_url_prefix + "/user_login"),
    json_obj,
    ReqId::ID_LOGIN_USER,
    Modules::LOGINMOD
);
```

把登录请求交给全局唯一的 `HttpMgr` 统一发送。

可以拆成两步理解：

```cpp
auto http_mgr = HttpMgr::GetInstance();
http_mgr->PostHttpReq(...);
```

也就是说，真正调用 `PostHttpReq()` 的对象，是 `Singleton<HttpMgr>::_instance` 这个智能指针管理的唯一 `HttpMgr` 对象。

## 2. Singleton 的作用

模板单例基类的主要作用：

1. 保证派生类全局只有一个实例。
2. 第一次使用时才创建对象，也就是懒加载。
3. 使用 `std::call_once` 保证多线程环境下也只初始化一次。

关键代码：

```cpp
template<typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>& st) = delete;

    static std::shared_ptr<T> _instance;

public:
    static std::shared_ptr<T> GetInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() {
            _instance = std::shared_ptr<T>(new T);
        });

        return _instance;
    }
};
```

第一次调用 `GetInstance()` 时：

```text
_instance == nullptr
        |
        v
std::call_once 执行初始化
        |
        v
new HttpMgr
        |
        v
std::shared_ptr<HttpMgr> 保存对象
        |
        v
返回 _instance
```

后续再次调用 `GetInstance()` 时，不会重新创建对象，而是直接返回之前创建好的 `_instance`。

## 3. 登录请求的 JSON 组装

登录时先把用户输入组装成 JSON：

```cpp
QJsonObject json_obj;
json_obj["user"] = user;
json_obj["passwd"] = xorString(pwd);
```

其中：

- `user` 是用户输入的账号。
- `pwd` 是用户输入的密码。
- `xorString(pwd)` 是对密码做简单异或处理后的结果。

最终要发送的数据大致是：

```json
{
  "user": "用户账号",
  "passwd": "处理后的密码"
}
```

## 4. 请求分发流程图

完整流程可以这样理解：

```text
用户点击登录按钮
        |
        v
LoginDialog 获取 user 和 pwd
        |
        v
组装 QJsonObject
        |
        v
HttpMgr::GetInstance()
        |
        |  第一次调用：创建唯一 HttpMgr 对象
        |  后续调用：返回已经存在的 HttpMgr 对象
        v
HttpMgr::_instance
        |
        v
调用 PostHttpReq(...)
        |
        v
QJsonDocument(json).toJson()
        |
        v
构造 QNetworkRequest
        |
        v
设置请求头 Content-Type: application/json
        |
        v
_manager.post(request, data)
        |
        v
发送 POST 请求到 GateServer /user_login
        |
        v
服务器处理登录逻辑
        |
        v
返回 HTTP 响应
        |
        v
QNetworkReply::finished 信号触发
        |
        v
lambda 回调读取响应结果
        |
        v
emit sig_http_finish(...)
        |
        v
LOGINMOD 模块接收登录结果
```

## 5. PostHttpReq 内部做了什么

`PostHttpReq()` 的职责是把 JSON 请求真正发出去，并在请求结束后发出统一信号。

关键代码：

```cpp
void HttpMgr::PostHttpReq(const QUrl& url,
                          const QJsonObject& json,
                          ReqId req_id,
                          Modules mod)
{
    QByteArray data = QJsonDocument(json).toJson();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader,
                      QByteArray::number(data.length()));

    auto self = shared_from_this();
    QNetworkReply* reply = _manager.post(request, data);

    QObject::connect(reply, &QNetworkReply::finished,
                     [reply, self, req_id, mod]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "error:" << reply->errorString();
            emit self->sig_http_finish(req_id, "", ErrorCode::ERR_NETWORK, mod);
        } else {
            QString res = reply->readAll();
            emit self->sig_http_finish(req_id, res, ErrorCode::Success, mod);
        }

        reply->deleteLater();
    });
}
```

它主要分为 5 步：

1. 把 `QJsonObject` 转成 HTTP 可以发送的 JSON 字节数据。
2. 根据 URL 创建 `QNetworkRequest`。
3. 设置请求头，告诉服务器请求体是 JSON。
4. 使用 `_manager.post(request, data)` 异步发送请求。
5. 等 `QNetworkReply::finished` 触发后，通过 `sig_http_finish` 把结果通知出去。

## 6. 为什么要用 shared_from_this

这一句很重要：

```cpp
auto self = shared_from_this();
```

HTTP 请求是异步的。调用 `_manager.post()` 后，函数不会一直等服务器返回，而是先结束，等网络响应回来后再执行 lambda。

如果 lambda 里直接使用裸指针对象，有可能出现对象已经释放、但回调还在执行的风险。

捕获 `self` 后：

```cpp
[reply, self, req_id, mod]()
```

可以让 `HttpMgr` 的智能指针引用计数增加。只要这个 lambda 还没执行结束，`HttpMgr` 对象就不会被释放。

这就是代码里注释说的：

```text
构造伪闭包并增加智能指针引用计数
```

## 7. 信号分发结果

请求结束后，不管成功还是失败，都会发出统一信号：

```cpp
emit self->sig_http_finish(req_id, res, ErrorCode::Success, mod);
```

或者：

```cpp
emit self->sig_http_finish(req_id, "", ErrorCode::ERR_NETWORK, mod);
```

其中：

- `req_id` 表示这次请求的类型，例如 `ID_LOGIN_USER`。
- `res` 表示服务器返回的数据。
- `ErrorCode` 表示请求成功还是失败。
- `mod` 表示结果要分发给哪个模块，例如 `LOGINMOD`。

这样设计的好处是：`HttpMgr` 只负责网络请求，不直接处理登录业务。登录业务由对应模块根据 `req_id` 和 `mod` 再继续处理。

## 8. 总结

这条链路可以总结为一句话：

`LoginDialog` 组装登录 JSON，然后通过 `HttpMgr::GetInstance()` 获取全局唯一的 HTTP 管理器，再由 `HttpMgr` 使用 `QNetworkAccessManager` 异步发送请求；服务器返回后，`HttpMgr` 通过 `sig_http_finish` 信号把结果分发给登录模块。

最终结构是：

```text
界面层 LoginDialog
        |
        v
网络管理层 HttpMgr 单例
        |
        v
Qt 网络层 QNetworkAccessManager
        |
        v
GateServer /user_login
        |
        v
sig_http_finish 分发响应
        |
        v
登录模块处理结果
```

这种写法把界面逻辑、网络发送逻辑、业务响应逻辑分开了，结构更清晰，也方便后续增加注册、重置密码、验证码等其他请求。

## 9. 注意点

如果 `HttpMgr` 的构造函数是 `private`，并且由 `Singleton<HttpMgr>` 内部 `new HttpMgr` 创建对象，那么 `HttpMgr` 需要把对应的单例模板声明为友元：

```cpp
friend class Singleton<HttpMgr>;
```

另外，如果使用了：

```cpp
shared_from_this();
```

那么 `HttpMgr` 通常需要继承：

```cpp
std::enable_shared_from_this<HttpMgr>
```

否则在运行时可能无法正确通过当前对象获取 `shared_ptr`。
