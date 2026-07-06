# Day19--VerifyServer gRPC 服务注册名不一致问题

## 1. 问题现象

重置密码页面点击“获取验证码”后，客户端提示：

```text
参数错误
```

但数据库中实际存在该用户，例如：

```text
name = xdr
email = 2055773715@qq.com
```

## 2. 实际错误

通过 PowerShell 直接测试 GateServer 接口：

```powershell
$body = @{ email = '2055773715@qq.com' } | ConvertTo-Json -Compress
Invoke-RestMethod -Uri 'http://127.0.0.1:8080/get_verifycode' -Method Post -ContentType 'application/json' -Body $body
```

返回：

```json
{"email":"2055773715@qq.com","error":1002}
```

在 GateServer 的 `const.h` 中：

```cpp
RPCFailed = 1002
```

所以真正问题不是 MySQL 用户不存在，而是：

```text
GateServer 调 VerifyServer 的 gRPC 请求失败
```

## 3. 根因

GateServer 使用的 proto 是：

```proto
service VarifyService {
  rpc GetVarifyCode (GetVarifyReq) returns (GetVarifyRsp) {}
}
```

但 Node VerifyServer 原来注册的是：

```js
server.addService(message_proto.VerifyService.service, {
    GetVarifyCode: GetVerifyCode
});
```

问题在于服务名不一致：

```text
proto 中是 VarifyService
Node 中写成 VerifyService
```

因此 Node 端没有正确注册 C++ GateServer 要调用的 gRPC 服务。

## 4. 正确写法

`server.js` 中应改为：

```js
function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VarifyService.service, {
        GetVarifyCode: GetVerifyCode
    })
    server.bindAsync('127.0.0.1:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')
    })
}
```

含义：

```text
注册 VarifyService 服务
如果收到 GetVarifyCode 请求
就执行 JS 里的 GetVerifyCode 函数
```

其中：

```text
左边 GetVarifyCode：proto 里的 RPC 方法名
右边 GetVerifyCode：Node 里的 JS 处理函数名
```

## 5. 请求链路

```text
ResetDialog 点击“获取验证码”
        |
        v
POST /get_verifycode
        |
        v
GateServer
        |
        v
stub->GetVarifyCode(...)
        |
        v
Node VerifyServer
        |
        v
GetVerifyCode(call, callback)
        |
        v
生成验证码、写入 Redis、发送邮件
```

## 6. 注意点

浏览器地址栏直接打开：

```text
http://127.0.0.1:8080/get_verifycode
```

这是 GET 请求。

但 GateServer 注册的是：

```cpp
RegPost("/get_verifycode", ...)
```

所以测试该接口必须使用 POST，例如 PowerShell、curl、Postman、Apifox，或者 Qt 客户端。

## 7. 总结

这次“参数错误”不是数据库用户不存在，而是客户端把所有非 0 错误都显示成了“参数错误”。

真实错误码是：

```text
1002 = RPCFailed
```

根本原因是：

```text
Node VerifyServer 注册的 gRPC 服务名和 GateServer proto 中的服务名不一致。
```

