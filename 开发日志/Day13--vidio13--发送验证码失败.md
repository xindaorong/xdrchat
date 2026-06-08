# 错误日志

## 2026-04-29 GateServer 链接错误

### 1. LNK2005：json_vc71_libmtd.lib 与 MSVC 运行库冲突

报错现象：

```text
错误 LNK2005
"protected: char * __cdecl std::basic_streambuf<char,struct std::char_traits<char> >::eback(void)const "
已经在 json_vc71_libmtd.lib(json_writer.obj) 中定义
GateServer
msvcprtd.lib(MSVCP140D.dll)
```

同类错误大量出现，主要集中在 `json_vc71_libmtd.lib` 和 `msvcprtd.lib` 的重复符号冲突。

原因：

`json_vc71_libmtd.lib` 是 jsoncpp 静态库，编译参数为 `/MTd`；而 GateServer 的 Debug x64 配置使用 `/MDd`。两个库使用的 C/C++ 运行库不一致，导致 STL/运行库符号重复定义。

处理：

- 从 `GrpcDebug.props` 的附加依赖项里移除 `json_vc71_libmtd.lib`
- 在 `GateServer.vcxproj` 中加入 `JSON_NO_AUTOLINK`，避免 jsoncpp 头文件自动链接旧库
- 将 jsoncpp 源文件直接加入 GateServer 工程编译：
  - `json_reader.cpp`
  - `json_value.cpp`
  - `json_writer.cpp`

结果：

该批 LNK2005 错误消失。

### 2. LNK1104：找不到 Boost filesystem 库

报错现象：

```text
LINK : fatal error LNK1104:
无法打开文件“libboost_filesystem-vc142-mt-gd-x64-1_81.lib”
```

原因：

工程里的 Boost 库目录配置为：

```text
D:\boost_1_81_0\libs
```

但实际 `.lib` 文件在：

```text
D:\boost_1_81_0\lib
```

处理：

- 将 `GateServer.vcxproj` 中的 Boost 库目录改为 `D:\boost_1_81_0\lib`
- 将 `GrpcDebug.props` 中的 Boost 库目录和链接器附加库目录改为 `D:\boost_1_81_0\lib`

最终验证：

使用 VS2019 MSBuild 编译 `Debug|x64`：

```text
已成功生成。
0 个警告
0 个错误
```

输出文件：

```text
D:\qtstudy\server\gateserver\GateServer\x64\Debug\GateServer.exe
```

### 3没有跑redis服务之前

就是assert为false的时候，那么你运行gateserver.cpp的时候就会出现

## 2026-06-08 验证码邮件链路排查（导致发送验证码失败）

### 1. GateServer 调 VerifyServer 的 gRPC 连接池使用错误

现象：

客户端点击“获取”验证码后，前端可能显示失败，或者后端没有稳定返回正确结果。

原因：

GateServer 中 `VerifyGrpcClient` 使用 gRPC 客户端连接池时，需要从连接池取出一个 stub，调用完成后再归还连接。否则容易出现以下问题：

- 调用未初始化的 stub
- 连接取出后没有归还
- 后续请求拿不到可用连接
- gRPC 调用失败后没有正确设置错误码

正确处理逻辑：

```cpp
auto stub = pool_->getConnection();
Status status = stub->GetVarifyCode(&context, request, &reply);

if (status.ok()) {
    pool_->returnConnection(std::move(stub));
    return reply;
}
else {
    pool_->returnConnection(std::move(stub));
    reply.set_error(ErrorCodes::RPCFailed);
    return reply;
}
```

关键点：

- `getConnection()` 后必须配套 `returnConnection()`
- 成功和失败分支都要归还连接
- gRPC 调用失败时要明确设置 `RPCFailed`

### 2. VerifyServer 错误码字段名不一致，导致“假成功”

现象：

Redis 明明报错，客户端却可能仍然拿到 `error = 0`，从而显示“验证码已发送到邮箱，请注意查收”。

原因：

`const.js` 中定义的错误码名称和 `server.js` 中使用的名称不一致。

例如 `const.js` 中是：

```js
RedisErr: 1
```

但 `server.js` 中使用的是：

```js
const_module.Errors.RedisError
```

如果字段名写错，`const_module.Errors.RedisError` 的值就是 `undefined`。而 gRPC/protobuf 的 `int32 error` 字段在收到异常值时，可能按默认值 `0` 处理，导致 GateServer 或客户端误以为请求成功。

处理：

把错误码名称统一，例如统一成：

```js
RedisError: 1
```

并保证 `server.js` 和 `const.js` 使用同一个名字。

### 3. Redis 服务器未启动，导致验证码没有写入，也不会发邮件

报错日志：

```text
email is  user@example.com
GetRedis error is Error: Connection is closed.
query_res is  null
SetRedisExpire error is Error: Connection is closed.
```

原因：

VerifyServer 会先查 Redis 里有没有旧验证码：

```js
let query_res = await redis_module.GetRedis(code_prefix + email);
```

如果没有旧验证码，就生成新验证码并写入 Redis：

```js
let bres = await redis_module.SetRedisExpire(code_prefix + email, uniqueId, 600);
```

当 Redis 没有启动，或者连接已经关闭时：

- `GetRedis()` 会失败
- `SetRedisExpire()` 也会失败
- `bres` 为 `false`
- `server.js` 会直接 `callback` 返回错误并 `return`
- 后面的 `SendMail()` 不会执行

所以 Redis 未启动时，验证码邮件不会真正发送。

检查 Redis 是否启动：

```powershell
redis-cli -h 127.0.0.1 -p 6379 ping
```

正常结果：

```text
PONG
```

### 4. Redis 启动后，邮件发送成功的日志

修复 Redis 连接和错误码后，日志出现：

```text
Result: <db98> Get key success!...
query_res is  db98
uniqueId is  db98
邮件已成功发送：250 Mail OK queued as ...
send_result is  250 Mail OK queued as ...
```

说明：

- VerifyServer 已经成功从 Redis 读取验证码
- `uniqueId` 使用 Redis 中已有验证码
- `nodemailer` 已经把邮件交给 163 SMTP
- `250 Mail OK queued as ...` 表示 SMTP 服务器已经接受邮件并进入发送队列

注意：

`250 Mail OK queued` 代表发送方 SMTP 已接收邮件，不等于收件方一定立刻进入收件箱。QQ 邮箱仍可能有延迟、垃圾箱拦截或风控过滤。

### 5. 邮箱地址拼写也要检查

日志中出现过：

```text
email is  user@example.invalid
```

这里是 `qqc.om`，不是 `qq.com`。如果输入框里真实填写的是这个地址，邮件会发往错误域名，自然不会出现在 QQ 邮箱里。

正确 QQ 邮箱格式应类似：

```text
user@example.com
```

### 最终结论

这次验证码收不到不是单一问题，而是三层问题叠在一起：

1. GateServer 的 gRPC 客户端连接池调用需要正确取连接、归还连接。
2. VerifyServer 的 Redis 错误码名称不一致，会造成 `error = 0` 的假成功。
3. Redis 服务器没启动时，验证码写入失败，邮件发送逻辑不会执行。

修复后，看到：

```text
邮件已成功发送：250 Mail OK queued as ...
```

才说明 VerifyServer 已经真正走到了 `SendMail()`。
