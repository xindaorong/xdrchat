# Server 项目说明

这个目录包含一个聊天/账号体系中的服务端雏形，主要由 C++ HTTP 网关 `GateServer`、Node.js gRPC 邮箱验证码服务 `VerifyServer`，以及一个 Redis 测试工程 `test-redis` 组成。

## 项目结构

```text
server/
├── gateserver/
│   └── GateServer/        # C++ HTTP 网关服务，使用 Boost.Asio/Beast、gRPC、hiredis、jsoncpp
├── VerifyServer/          # Node.js gRPC 验证码服务，负责生成验证码并发送邮件
└── test-redis/            # hiredis 连接测试工程，目前基本为空
```

## 整体功能

当前实现的核心链路是“客户端通过 HTTP 请求验证码，GateServer 转发到 VerifyServer，VerifyServer 发送邮箱验证码”。

```text
客户端
  │
  │ HTTP GET/POST
  ▼
GateServer(C++)
  │
  │ gRPC VerifyService.GetVerifyCode
  ▼
VerifyServer(Node.js)
  │
  │ SMTP
  ▼
用户邮箱
```

## GateServer 功能

`GateServer` 是一个 C++ HTTP 网关服务，入口在 `gateserver/GateServer/GateServer.cpp`。

已实现能力：

- 使用 `Boost.Asio` + `Boost.Beast` 监听 HTTP 请求。
- 端口从 `gateserver/GateServer/config.ini` 的 `[GateServer] Port` 读取，当前默认是 `8080`。
- 支持异步 accept/read/write。
- 使用 `AsioIOServicePool` 创建多个 `io_context`，新连接按轮询方式分配到不同 IO 线程。
- 每个 HTTP 连接设置 60 秒超时，到期后关闭 socket。
- 支持 `SIGINT`、`SIGTERM` 信号停止主 `io_context`。
- `HttpConnection` 负责解析请求、写响应、解析 GET query 参数，并提供 URL encode/decode 工具。
- `LogicSystem` 负责注册和分发 GET/POST 路由。
- 未匹配路由返回 `404`，不支持的方法返回 `405`。
- 通过 `VerifyGrpcClient` 调用 Node.js 验证码 gRPC 服务。
- 提供 `RedisMgr`，封装了 hiredis 的常用命令：`Connect`、`Auth`、`Get`、`Set`、`LPush`、`LPop`、`RPush`、`RPop`、`HSet`、`HGet`、`Del`、`ExistsKey`、`Close`。

### HTTP 接口

| 方法 | 路径 | 功能 |
| --- | --- | --- |
| `GET` | `/get_test` | 测试接口，返回 `receive get_test request` |
| `POST` | `/get_verifycode` | 接收邮箱地址，调用 gRPC 验证码服务发送邮件 |

### `POST /get_verifycode`

请求体：

```json
{
  "email": "user@example.com"
}
```

成功响应示例：

```json
{
  "error": 0,
  "email": "user@example.com"
}
```

JSON 解析失败响应示例：

```json
{
  "error": 1001
}
```

GateServer 中定义的错误码：

| 错误码 | 含义 |
| --- | --- |
| `0` | 成功 |
| `1001` | JSON 解析失败 |
| `1002` | gRPC 调用失败 |

## VerifyServer 功能

`VerifyServer` 是 Node.js gRPC 服务，主要文件在 `VerifyServer/`。

已实现能力：

- 使用 `@grpc/grpc-js` 和 `@grpc/proto-loader` 加载 `message.proto`。
- 暴露 `VerifyService.GetVerifyCode` gRPC 方法。
- 接收邮箱地址，生成验证码。
- 使用 `nodemailer` 通过 SMTP 发送验证码邮件。
- 返回邮箱和错误码。

`package.json` 当前主入口是 `server.js`，启动脚本是：

```powershell
npm run serve
```

目录里还有一个 `index.js`，它也是一个可运行的 gRPC 服务版本，支持通过环境变量配置 SMTP，并且会把生成的 `code` 放进响应里。当前 `npm run serve` 使用的是 `server.js`。

### gRPC 协议

`GateServer` 和 `VerifyServer` 使用相同的 `message.proto`：

```proto
syntax = "proto3";

package message;

service VerifyService {
  rpc GetVerifyCode (GetVerifyReq) returns (GetVerifyRsp) {}
}

message GetVerifyReq {
  string email = 1;
}

message GetVerifyRsp {
  int32 error = 1;
  string email = 2;
  string code = 3;
}
```

VerifyServer 中定义的错误码：

| 错误码 | 含义 |
| --- | --- |
| `0` | 成功 |
| `1` | Redis 错误，目前主要是预留 |
| `2` | 异常 |

## 配置说明

### GateServer 配置

配置文件：`gateserver/GateServer/config.ini`

当前内容：

```ini
[GateServer]
Port = 8080

[VarifyServer]
Port = 50051
```

`VerifyGrpcClient` 当前会读取 `[VarifyServer] Host` 和 `[VarifyServer] Port`，所以建议补充：

```ini
[VarifyServer]
Host = 127.0.0.1
Port = 50051
```

注意：代码里的 section 名称是 `VarifyServer`，拼写和 `VerifyServer` 不一致，配置文件里需要保持和代码一致。

### VerifyServer 配置

配置文件：`VerifyServer/config.json`

包含邮箱、MySQL、Redis 配置。当前 `server.js` 主要使用邮箱账号和授权码发送邮件；MySQL、Redis 配置目前只是被读取或预留。

建议不要把真实邮箱授权码提交到仓库，可以改成环境变量或本地私有配置文件。

## 运行方式

### 1. 启动 VerifyServer

```powershell
cd D:\qtstudy\server\VerifyServer
npm install
npm run serve
```

`server.js` 当前监听：

```text
127.0.0.1:50051
```

### 2. 启动 GateServer

使用 Visual Studio 2019 或 Developer PowerShell 构建：

```powershell
cd D:\qtstudy\server
msbuild .\gateserver\GateServer\GateServer.sln /p:Configuration=Debug /p:Platform=x64
```

也可以直接用 Visual Studio 打开：

```text
gateserver/GateServer/GateServer.sln
```

选择 `Debug|x64` 后生成并运行。

运行时要保证当前工作目录能找到 `config.ini`。如果直接运行 exe，需要把工作目录切到 `gateserver/GateServer`，或把 `config.ini` 放到 exe 的当前工作目录。

### 3. 测试接口

测试 GET：

```powershell
curl.exe http://127.0.0.1:8080/get_test
```

测试验证码接口：

```powershell
curl.exe -X POST http://127.0.0.1:8080/get_verifycode -H "Content-Type: application/json" -d '{"email":"user@example.com"}'
```

## 主要依赖

### GateServer

- Visual Studio 2019 / MSVC v142
- Boost 1.81
- Boost.Asio
- Boost.Beast
- Boost.Filesystem
- Boost.PropertyTree
- gRPC C++
- Protobuf
- jsoncpp
- hiredis
- vcpkg NuGet 集成

工程配置中存在一些本机硬编码路径，例如：

```text
D:\boost_1_81_0
D:\cppsoft\grpc
D:\cppsoft\libjson
D:\cppsoft\vcpkg\vcpkg-master
```

如果换机器构建，需要同步调整 `GateServer.vcxproj` 和 `GrpcDebug.props`。

### VerifyServer

- Node.js
- `@grpc/grpc-js`
- `@grpc/proto-loader`
- `nodemailer`
- `redis`
- `uuid`

## test-redis

`test-redis` 是一个独立的 Visual Studio C++ 工程，目前 `main.cpp` 只引入了 hiredis 头文件，没有实现实际逻辑。它更像是 hiredis 环境验证/实验工程。

## 当前待完善点

- `GateServer.cpp` 启动时会先调用 `TestRedisMgr()`，该函数使用硬编码的远程 Redis 地址、端口和密码；如果 Redis 不可用，程序会在真正启动 HTTP 服务前失败。
- `VerifyGrpcClient` 构造函数创建了 `RPConPool`，但 `GetVarifyCode()` 当前调用的是 `stub_`，而 `stub_` 没有初始化，运行时需要修复为从连接池取 stub，或直接初始化 `stub_`。
- `config.ini` 当前缺少 `[VarifyServer] Host`，但代码会读取它。
- `RedisConPool::getConnection()` 目前返回 `nullptr`，连接池尚未完成。
- `RedisMgr::Set()` 的成功判断逻辑需要复核，当前对 `OK` 状态的处理看起来可能写反了。
- `VerifyServer/server.js` 使用 UUID 作为验证码并发送邮件，但响应没有填充 `code` 字段；`index.js` 版本会返回 `code`。
- `VerifyServer/config.json` 中不建议保存真实邮箱授权码，最好改成环境变量。
- 部分中文注释出现编码异常，建议统一源码编码为 UTF-8。

