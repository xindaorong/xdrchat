# VerifyServer — gRPC 邮箱验证码服务

## 项目结构

```
VerifyServer/
├── server.js      # gRPC 服务端，处理验证码请求
├── redis.js       # Redis 操作封装（get/set/expire/exists）
├── email.js       # 邮件发送模块
├── config.js      # 配置读取模块
├── config.json    # 配置文件（邮箱、MySQL、Redis 连接信息）
├── const.js       # 常量定义（错误码、前缀等）
├── proto.js       # gRPC protobuf 协议定义
└── README.md
```

## 核心知识点

---

### 1. `require` — 模块导入

```js
const Redis = require('ioredis');          // npm 包导入（从 node_modules 查找）
const config_module = require('./config');  // 相对路径导入（同目录下的 config.js）
const { v4: uuidv4 } = require('uuid');    // 解构导入（只取 uuid 包的 v4 方法，起别名 uuidv4）
```

| 写法 | 说明 |
|------|------|
| `require('xxx')` | 从 node_modules 查 npm 包 |
| `require('./xxx')` | 从当前目录查本地文件 |
| `require('@scope/xxx')` | 作用域包（如 @grpc/grpc-js） |
| `{ v4: uuidv4 }` | 解构赋值，取出 v4 并重命名为 uuidv4 |

---

### 2. `module.exports` — 模块导出

```js
module.exports = { GetRedis, QueryRedis, SetRedisExpire, Quit };
```

- **`module.exports`** 是 Node.js 每个文件对外暴露的出口
- ES6 属性简写：`{ GetRedis }` 等价于 `{ GetRedis: GetRedis }`
- 只有导出的内容，其他文件 `require` 后才能使用
- 未导出的变量/函数对外不可见（封装）

---

### 3. `new Redis({...})` — 创建 ioredis 客户端实例

```js
const RedisCli = new Redis({
    host: config_module.redis_host,     // Redis 服务器地址
    port: config_module.redis_port,     // Redis 端口（默认 6379）
    password: config_module.redis_passwd, // Redis 密码（可选）
});
```

- `Redis` 是 ioredis 库导出的构造函数，`new` 创建实例
- 传入配置对象，连接信息来自 config 模块
- 创建后即可通过 `RedisCli` 操作 Redis

---

### 4. `.on()` — EventEmitter 事件监听

```js
RedisCli.on('error', function(err) {
    console.log('Redis Client connect Error', err);
    RedisCli.quit();
});
```

- **`.on('事件名', 回调函数)`** 是 Node.js EventEmitter 的核心方法
- ioredis 客户端继承 EventEmitter，发射 `error`/`connect`/`ready`/`close` 等事件
- 本质是**发布-订阅模式**：先订阅（.on），事件发生时自动回调
- 常见 ioredis 事件：
  | 事件 | 触发时机 |
  |------|----------|
  | `connect` | 连接建立 |
  | `ready` | 连接就绪，可执行命令 |
  | `error` | 连接出错 |
  | `close` | 连接关闭 |
  | `reconnecting` | 正在重连 |

---

### 5. `async/await` — 异步编程

```js
async function GetRedis(key) {
    const result = await RedisCli.get(key);
    return result;
}
```

- **`async`** — 声明异步函数，调用后返回 Promise
- **`await`** — 等待 Promise 完成，取出内部结果
- 代码写成同步风格，实际异步执行，不阻塞主线程
- 不用 `await` 时得到的是 `Promise { <pending> }`，不是实际值

---

### 6. `try/catch` — 异常处理

```js
try {
    const result = await RedisCli.get(key);
    // 正常逻辑...
} catch (error) {
    console.log('error is', error);
    return null;
}
```

- `try` 块包裹可能出错的代码（网络请求、文件操作等）
- `catch` 块捕获异常，防止程序崩溃
- 这里的策略是**异常吞没**：出错返回 `null`，让调用方无需额外 try-catch

---

### 7. `callback` — gRPC 回调

```js
callback(null, {
    email: call.request.email,
    error: const_module.Errors.Success
});
```

- gRPC 服务函数的固定参数：`call`（请求）、`callback`（响应）
- **第一个参数**：系统错误（`null` = 无错误，Node.js 惯例）
- **第二个参数**：响应数据对象
- `await` 和 `callback` 不冲突：`await` 是函数内部的等待机制，`callback` 是向外部返回结果的机制

---

### 8. `null` vs `undefined` vs `''` vs `0`

| 值 | 含义 | 类型 | 在 Redis 中的意义 |
|----|------|------|-------------------|
| `null` | **故意为空** | `object` | key 不存在 |
| `undefined` | 未定义 | `undefined` | 变量未赋值 |
| `''`（空字符串） | 有值但无内容 | `string` | — |
| `0` | 数字零 | `number` | — |

- 判断时用 `=== null`（严格等于），不要用 `==`
- `null == undefined` 为 `true`（宽松比较），`null === undefined` 为 `false`（严格比较）

---

### 9. 读操作 vs 写操作的返回值约定

| 操作类型 | 封装习惯 | 成功返回 | 失败返回 |
|----------|----------|----------|----------|
| 读（`GetRedis`） | 返回数据本身 | value | `null` |
| 查询（`QueryRedis`） | 返回数据本身 | 1/0 | `null` |
| 写（`SetRedisExpire`） | 返回布尔值 | `true` | `false` |

> 读操作返回数据本身（null 表示没数据），写操作返回布尔值（表示成功与否），语义清晰。

---

### 10. 验证码服务完整流程

```
客户端 gRPC 请求
      │
      ▼
GetVerifyCode(call, callback)
      │
      ▼
await GetRedis(code_prefix + email)  ← 查 Redis 是否有旧验证码
      │
      ├── 有旧验证码（query_res != null）
      │     └── 直接用旧验证码发邮件
      │
      └── 无旧验证码（query_res == null）
            │
            ├── uuidv4() → 截取前4位 → 生成验证码
            │
            ├── await SetRedisExpire(key, code, 600) ← 存 Redis，600秒过期
            │     ├── 成功 → await SendMail → callback Success
            │     └── 失败 → callback RedisError → return（不发邮件）
            │
            └── catch 异常 → callback Exception
```

---

### 11. 常见踩坑

| 问题 | 说明 |
|------|------|
| `async` 拼成 `sync` | await 报错，async 函数必须以 `a` 开头 |
| 对 `null` 调用 `.length` | `null` 没有 .length 属性，直接 TypeError |
| `callback` 拼成 `clallback` | 变量未定义，运行时 ReferenceError |
| `console.log` 拼成 `cosnsole.log` | 多打了一个 `s`，ReferenceError |
| 忘记 `await` | 得到 Promise 对象而非实际值 |
| 用 `==` 判断 null | 会把 `undefined` 也混进来，用 `=== null` |
