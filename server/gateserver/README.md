# GateServer 验证码链路修复记录

记录时间：2026-06-07

## 问题现象

客户端注册页点击“获取”验证码后，没有出现“验证码已发送到邮箱，请注意查收”的成功提示。

客户端成功提示的触发条件在 `D:\qtstudy\helloworld\client\registerdialog.cpp`：

1. `HttpMgr` 收到 HTTP 成功响应。
2. 响应内容能解析成 JSON。
3. JSON 中的 `error == 0`。

所以如果 GateServer 没有返回 HTTP 响应，或者返回的 JSON 中 `error != 0`，前端就不会显示成功提示。

## 请求链路

本次验证码请求的真实调用链路是：

```text
RegisterDialog::on_get_code_clicked()
    -> HttpMgr::PostHttpReq("http://localhost:8080/get_verifycode")
    -> GateServer: LogicSystem::RegPost("/get_verifycode")
    -> VerifyGrpcClient::GetVarifyCode(email)
    -> VerifyServer: GetVerifyCode(call, callback)
    -> Redis 查询/保存验证码
    -> nodemailer 发送邮件
    -> 返回 {"email": "...", "error": 0}
```

## 本次改动

### 1. 修复 gRPC 客户端没有使用连接池的问题

文件：

```text
D:\qtstudy\server\gateserver\GateServer\VerifyGrpcClient.h
```

原问题：

`VerifyGrpcClient` 构造函数里创建的是 `pool_`：

```cpp
pool_.reset(new RPConPool(5, host, port));
```

但 `GetVarifyCode()` 里实际调用的是没有初始化的 `stub_`：

```cpp
Status status = stub_->GetVerifyCode(&context, request, &reply);
```

这会导致 GateServer 调用 VerifyServer 时失败，严重时会让 GateServer 请求处理异常，客户端就等不到成功回包。

修复后：

```cpp
auto stub = pool_->GetCon();
if (stub == nullptr) {
    reply.set_error(ErrorCodes::RPCFailed);
    return reply;
}

Status status = stub->GetVerifyCode(&context, request, &reply);
pool_->returnCon(std::move(stub));
```

同时移除了未使用的 `stub_` 成员，并补充了头文件依赖：

```cpp
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
```

`b_stop_` 也改成了更明确的：

```cpp
std::atomic<bool> b_stop_;
```

### 2. 补充 VerifyServer 的 Host 配置

文件：

```text
D:\qtstudy\server\gateserver\GateServer\config.ini
```

原配置只有端口：

```ini
[VarifyServer]
Port = 50051
```

但 `VerifyGrpcClient.cpp` 读取的是：

```cpp
std::string host = gCfgMgr["VarifyServer"]["Host"];
std::string port = gCfgMgr["VarifyServer"]["Port"];
```

所以补充为：

```ini
[GateServer]
Port = 8080
[VarifyServer]
Host = 127.0.0.1
Port = 50051
```

这样 GateServer 会连接到：

```text
127.0.0.1:50051
```

也就是当前 VerifyServer 的 gRPC 监听地址。

## 验证结果

由于原来的：

```text
D:\qtstudy\server\gateserver\GateServer\x64\Debug\GateServer.exe
```

当时被一个旧进程占用，直接构建标准输出目录会出现：

```text
LINK : fatal error LNK1168: 无法打开 ...\GateServer.exe 进行写入
```

所以先构建到了临时输出目录：

```text
D:\qtstudy\server\gateserver\GateServer\x64\Debug_codex\GateServer.exe
```

临时构建结果：

```text
0 个错误
```

启动临时 GateServer 后，接口验证结果如下。

### GET /get_test

请求：

```powershell
curl.exe http://127.0.0.1:8080/get_test
```

结果：

```text
HTTP 200
receive get_test request
```

### POST /get_verifycode

请求体：

```json
{"email":"user@example.com"}
```

返回：

```json
{
   "email" : "user@example.com",
   "error" : 0
}
```

这个 `error: 0` 正好满足客户端成功提示条件，所以前端点击“获取”后可以显示：

```text
验证码已发送到邮箱，请注意查收
```

## 运行注意事项

### 1. VerifyServer 必须先启动

在：

```text
D:\qtstudy\server\VerifyServer
```

启动：

```powershell
npm run serve
```

它会监听：

```text
127.0.0.1:50051
```

### 2. GateServer 读取的是当前工作目录下的 config.ini

`ConfigMgr` 使用的是：

```cpp
boost::filesystem::current_path() / "config.ini"
```

所以运行 GateServer 时，工作目录要是：

```text
D:\qtstudy\server\gateserver\GateServer
```

否则它可能读不到正确的 `config.ini`。

### 3. 客户端读取的是客户端 exe 目录下的 config.ini

客户端在 `main.cpp` 里读取的是：

```cpp
QCoreApplication::applicationDirPath() + "/config.ini"
```

当前客户端配置应指向：

```ini
[GateServer]
host=localhost
port=8080
```

### 4. 如果标准构建报 LNK1168

说明旧的 `GateServer.exe` 还在占用输出文件。先检查并停止旧进程：

```powershell
Get-Process GateServer -ErrorAction SilentlyContinue
Stop-Process -Id <pid> -Force
```

然后再用 VS2019 或 MSBuild 重新生成 `Debug|x64`。

## 当前结论

本次前端不显示“验证码已发送”的根因在后端验证码链路：

1. GateServer gRPC 客户端调用了未初始化的 `stub_`。
2. GateServer 配置里缺少 VerifyServer 的 `Host`。

修复后，GateServer 能正常把 `/get_verifycode` 转发到 VerifyServer，并返回 `error: 0` 给客户端。
