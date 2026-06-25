# getConnection() 详解

## 源代码

```cpp
std::unique_ptr<SqlConnection> getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] {
        if (b_stop_) {
            return true;
        }
        return !pool_.empty();
    });
    if (b_stop_) {
        return nullptr;
    }
    std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
    pool_.pop();
    return con;
}
```

---

## 前置知识

### 整体架构

```
LogicSystem（业务层）
  └─ MysqlMgr（单例门面）
       └─ MysqlDao（数据访问对象）
            └─ MysqlPool（连接池） ← 本文讨论的核心
```

### 为什么需要连接池？

**问题**：如果不使用连接池，每次操作数据库都要：

```
建立 TCP 连接（三次握手）→ MySQL 认证 → 执行 SQL → 断开连接（四次挥手）
```

每次开销数百毫秒，高并发下性能极差。

**方案**：启动时预创建几个连接放在池子里，业务线程用完就归还，重复使用。

### `SqlConnection` 结构体（池子里每个连接的包装）

```cpp
class SqlConnection {
public:
    SqlConnection(sql::Connection* con, int64_t lasttime)
        : _con(con), _last_oper_time(lasttime) {}

    std::unique_ptr<sql::Connection> _con;   // MySQL Connector/C++ 的真实连接
    int64_t _last_oper_time;                  // 上次使用/保活的时间戳（秒）
};
```

- `_con`：底层 MySQL 连接，用 `unique_ptr` 管理，保证只有一个所有者。
- `_last_oper_time`：配合 `checkConnection()` 判断是否需要执行 `SELECT 1` 保活。

### `MysqlPool` 成员变量一览

```cpp
std::string url_;       // 连接地址，格式 host:port，如 "127.0.0.1:3306"
std::string user_;      // MySQL 用户名
std::string pass_;      // MySQL 密码
std::string schema_;    // 目标数据库名（连接后执行 USE schema_）
int poolSize_;          // 连接池大小（当前硬编码为 5）
std::queue<std::unique_ptr<SqlConnection>> pool_;   // 连接队列（池子本体）
std::mutex mutex_;      // 保护 pool_ 的互斥锁
std::condition_variable cond_;                      // 条件变量，用于阻塞/唤醒
std::atomic<bool> b_stop_;                          // 关闭标志
std::thread _check_thread;                          // 保活后台线程
```

### 连接池的两个核心操作

| 操作 | 方法 | 角色 |
|------|------|------|
| 取连接 | `getConnection()` | 消费者——池空则阻塞等待 |
| 还连接 | `returnConnection()` | 生产者——归还并唤醒等待者 |

这就是典型的**有界阻塞队列**（生产者-消费者模式）：最多 5 个人同时用，第 6 个排队等。

### `unique_lock` vs `lock_guard`

| | `std::lock_guard` | `std::unique_lock` |
|---|---|---|
| 自动加锁 | ✅ | ✅ |
| 自动解锁 | ✅ | ✅ |
| 可以主动解锁 | ❌ | ✅ |
| 可以配合条件变量 | ❌ | ✅ |

`cond_.wait()` 内部需要**暂时释放锁**，所以必须用 `unique_lock`。

### `cond_.wait(lock, predicate)` 内部等价于

```cpp
while (!predicate()) {       // 条件不满足
    cond_.wait(lock);         // 释放锁 + 睡觉 + 被叫醒 + 重新拿锁
}
// 条件满足，持锁继续
```

其中无参 `cond_.wait(lock)` 这一步做了**三件事**，而且是原子操作：

```
原子操作开始
  ① 释放锁（让出 mutex_）
  ② 将自己挂到条件变量的等待队列里
原子操作结束
  ③ 阻塞/睡觉（操作系统不再调度此线程）
  ... 被 notify 后 ...
  ④ 重新获取锁
  ⑤ 从④返回，继续 while 循环判断条件
```

### Lambda 表达式语法

```cpp
[this]       // 捕获列表：把外部 this 指针抓进来
{            // 函数体
    ...
    return true;   // 返回 bool
}
```

等价于当场写了一个返回 `bool` 的小函数，传给 `wait` 反复调用。

---

## 逐步拆解

### 第一步：加锁

```cpp
std::unique_lock<std::mutex> lock(mutex_);
```

线程 A 拿到 `mutex_`，保护 `pool_` 队列。

### 第二步：等待（核心）

```cpp
cond_.wait(lock, [this] {
    if (b_stop_) {
        return true;
    }
    return !pool_.empty();
});
```

行为等价于：

```
while (条件不满足) {
    暂时释放 lock（让别的线程能操作 pool_）
    自己睡觉等待通知
    被唤醒后重新获取 lock
}
条件满足 → 持锁，继续往下走
```

条件判断的三种情况：

| 情况 | `b_stop_` | `pool_` | lambda 返回 | 含义 |
|------|-----------|---------|-------------|------|
| 有连接了 | `false` | 非空 | `true` | 正常——有人归还连接了，去取吧 |
| 要关闭了 | `true` | 不管 | `true` | 关闭——别等了，拿不到连接的 |
| 继续等 | `false` | 空 | `false` | 没连接也没关闭，继续睡 |

**为什么 `b_stop_` 要放在 `pool_.empty()` 前面检查？**

如果先判断 `pool_.empty()`：池子空 → 返回 `false`，继续等；但此时根本没人会往池子里放连接了（因为 `b_stop_` 已经是 `true`），线程就会**永远卡在 wait 里**。

所以逻辑是：**先看是不是要关门了，关门就直接走人，不用管池子空不空。**

### 第三步：再验一次

```cpp
if (b_stop_) {
    return nullptr;
}
```

被唤醒且条件满足后，再次确认是不是因为"关闭"而被唤醒的。如果是，返回 `nullptr`。

调用者看到空指针就知道"关门了，拿不到了"（见 `MysqlDao.cpp:22`：`if (con == nullptr) return false;`）。

### 第四步：取走连接

```cpp
std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
pool_.pop();
return con;
```

此时 `pool_` 非空且没关闭。

1. `std::move(pool_.front())` — 把队头连接的所有权**转移**到局部变量 `con`
2. `pool_.pop()` — 删除队列里的那个**空壳**（`front()` 指向的 `unique_ptr` 已经被 move 掏空了）
3. `return con` — 把所有权交给调用者

必须 `std::move`，因为 `unique_ptr` 不可拷贝——同一个连接不能有两个主人。

---

## 完整流程图

```
getConnection() 被调用
        │
        ▼
   加锁 mutex_
        │
        ▼
   cond_.wait()
   ┌─ 循环 ───────────────────────┐
   │ pool_ 空 且 b_stop_ 为 false │ → 解锁，睡觉
   └──────────────────────────────┘
        │ (被 notify 唤醒或超时)
        │ 条件满足，持锁返回
        ▼
   b_stop_ == true ?
      YES → return nullptr  (调用者：关门了，撤)
      NO  → 继续往下
        ▼
   move(front()) + pop()
   return con  (调用者：拿到连接了，用吧)
```

---

## 完整时序：线程 A 取连接，线程 B 归还连接

假设池子里有 **0 个连接**。

### 第 1 步：线程 A 调用 `getConnection()`

```cpp
std::unique_lock<std::mutex> lock(mutex_);  // A 拿到 mutex_
```

状态：

```
mutex_  : 被 A 持有
pool_   : 空
b_stop_ : false
```

### 第 2 步：线程 A 进入 `cond_.wait(lock, predicate)`

predicate 返回 `false`（池子空且没关门），进入内部 `cond_.wait(lock)`：

```
原子操作：
  ① A 释放 mutex_          ← A 放锁
  ② A 把自己挂到 cond_ 的等待队列里
原子操作结束
  ③ A 进入睡眠状态
```

状态：

```
mutex_  : 空着，没人持有
cond_等待队列 : [线程A]
pool_   : 空
线程A  : 睡眠中
```

### 第 3 步：线程 B（用完了连接）调用 `returnConnection()`

```cpp
void returnConnection(std::unique_ptr<SqlConnection> con) {
    std::unique_lock<std::mutex> lock(mutex_);  // B 拿到 mutex_
```

A 已经放了锁，所以 B 拿得到。

状态：

```
mutex_  : 被 B 持有
pool_   : 空
线程A  : 还在睡
```

### 第 4 步：线程 B 归还连接

```cpp
    pool_.push(std::move(con));    // 连接入队
```

状态：

```
mutex_  : 被 B 持有
pool_   : [1个连接]
线程A  : 还在睡
```

### 第 5 步：线程 B 叫醒 A

```cpp
    cond_.notify_one();  // "有空位了，等的人起来吧"
}
// B 离开作用域 → lock 析构 → B 释放 mutex_
```

`notify_one()` 做了什么？

```
操作系统找到 cond_ 等待队列里的第一个线程（线程A）
把线程A从睡眠队列移到就绪队列
线程A变成"就绪"状态，等 CPU 调度
```

状态：

```
mutex_  : 刚刚被 B 释放
pool_   : [1个连接]
线程A  : 就绪态，等 CPU 分时间片
```

### 第 6 步：线程 A 被 CPU 调度，继续执行

线程 A 从 `cond_.wait(lock)` 的第④步开始：

```
  ④ A 尝试重新获取 mutex_
     此时 mutex_ 空着（B 已经释放了），A 直接拿到
  ⑤ 从 wait 返回
```

回到外层的 `while` 循环，再次执行 predicate：

```cpp
[this] {
    if (b_stop_) return true;      // 还是 false
    return !pool_.empty();          // !false = true ✅
}
// predicate 返回 true，退出循环
```

状态：

```
mutex_  : 被 A 持有
pool_   : [1个连接]
线程A  : 正在运行，持锁
```

### 第 7 步：线程 A 取走连接

```cpp
std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
pool_.pop();
return con;
```

---

## 现实类比

| 代码 | 现实 |
|------|------|
| `mutex_` | 柜台窗口的锁——一次只能一个人站在窗口前 |
| `pool_` | 柜台里的号码牌（几条连接就是几个牌） |
| `cond_` | 大厅的叫号系统 |
| `cond_.wait()` | 去窗口发现没牌了 → 离开窗口（放锁）+ 大厅坐下等（睡觉）+ 听到叫号回到窗口（重新拿锁） |
| `notify_one()` | 有人还牌 → 按叫号器 → "请001号到窗口" |
| predicate | 回到窗口后再看一眼："牌确实在吗？"（防止虚假唤醒） |

---

## 关键细节：为什么"放锁"和"入队"必须是原子操作？

假设不是原子操作，先放锁再入队：

```
A 放锁（mutex_ 空了）
    ↓   ← 这中间有个缝隙！
B 拿到锁，归还连接，notify_one()  ← 但 A 还没入队，通知发给了空气
B 通知完了
    ↓
A 现在才入队 → 通知已丢失 → 永远睡下去
```

这就是**丢失唤醒（Lost Wakeup）**问题。

内核把"放锁 + 入队"做成原子操作，就杜绝了这个缝隙：

- 通知发生在入队之前 → 锁还没放，B 拿不到锁，无法 notify
- 通知发生在入队之后 → B 才能拿到锁，此时等待队列里已经有 A 了

---

## 一句总结

> `cond_.wait` 帮你做了"没连接就自己放锁然后自己睡觉，被叫醒后自己重新拿锁"这三件事。
> 放和睡的都是自己，叫醒靠别人 `notify_one()`。
cond_.wait(lock, predicate);true是唤醒线程，false是让线程等着睡觉
         ↑        ↑
      你睡觉的   检查真正条件（pool_空不空，关门没）
      叫号机     叫号机不管这个，你自己看