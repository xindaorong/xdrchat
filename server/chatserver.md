### 1所有server的主体逻辑大概套路

chatserver.cpp的主体逻辑如下，其他server大概也是这么写

```
读取配置
  ↓
启动 Asio I/O 线程池
  ↓
创建主 io_context
  ↓
注册退出信号
  ↓
创建 CServer、监听端口
  ↓
io_context.run() 进入事件循环
  ↓
收到 Ctrl+C
  ↓
停止主 io_context 和工作线程池
  ↓
main 退出、对象析构
```

#### 1.1sectionInfo.h的作用

**成员变量**:_section_datas` 保存 INI 文件某个配置分组中的全部键值对。例如 `[SelfServer]` 下的 `Host`、`Port`。

**方法**

- 默认构造、析构和复制：负责正常创建、复制 `SectionInfo`。
- 赋值运算符：复制另一组配置数据。
- `operator[]`：通过键名读取配置值，不存在时返回空字符串。

#### 1.2ConfigMgr.h

**整体的作用**

`ConfigMgr.h` 定义了一个单例配置管理类。程序启动后读取一次配置，其他模块统一通过它获取参数：

```
auto& cfg = ConfigMgr::Instance();
auto port = cfg["SelfServer"]["Port"];
```



**成员变量：**

私有构造函数configMgr(); 

ConfigMgr cfg; // 编译失败

**方法**

`ConfigMgr()`：私有构造，负责读取配置文件。

`Instance()`：返回唯一的 `ConfigMgr` 单例。

`operator[]`：根据分组名读取 `SectionInfo`。

禁止复制和赋值，避免产生多个配置管理器。
#### 1.3Singleton.h

**整体的作用**

`Singleton<T>` 通过模板保存每种类型唯一的 `shared_ptr`，使用 `call_once` 保证多线程环境下只创建一次，并通过继承和友元访问派生类的私有构造函数
**私有域**：

有参无参构造默认，赋值删除

**保存类型T的唯一实例**

公共方法：

```
进入 GetInstance()
    ↓
call_once 检查 s_flag
    ↓
第一次调用，执行 Lambda
    ↓
new T 创建对象
    ↓
shared_ptr 保存到 _instance
    ↓
返回 _instance
```

**第二次调用：**
auto pool2 = AsioIOServicePool::GetInstance();
call_once 发现初始化已经执行过，不再执行 new T，直接返回原对象。
因此：
pool.get() == pool2.get(); // true
std::once_flag 和 std::call_once 保证多个线程同时调用时，**也只有一个线程执行：**
**_instance = std::shared_ptr<T>(new T);**
其他线程会等待初始化完成。
这里的 [&] 没有使用任何外部局部变量，可以简单写成：
std::call_once(s_flag, []() {
    _instance = std::shared_ptr<T>(new T);
});
为什么使用 new T
以 AsioIOServicePool 为例，它的构造函数是私有的：
private:
    AsioIOServicePool(...);
同时声明：
friend Singleton<AsioIOServicePool>;
这表示允许 Singleton<AsioIOServicePool> 调用其私有构造函数，所以模板内部可以执行：
new T;
外部仍然不能直接构造：
AsioIOServicePool pool; // 编译失败

static shared_ptr<T>这个static表示共享同一个实例指针，并且只在程序中为它分配一份存储空间

#### 1.4AsioIOServerpool.h

##### **整体作用**

这个类是 Asio 网络 I/O 线程池。它的职责是创建多个 `io_context` 和线程，把不同客户端连接轮流分配给它们，从而并发处理 TCP 收发。

##### **成员变量**

**io_context** → 任务箱
**Work**       → 保证任务箱一直开放
**thread**     → 处理任务的工人
**_nextIOService** → 决定下一个任务交给哪个工人



  **_ioServices**

```
std::vector<IOService> _ioServices;
保存多个 io_context。Session 的异步读写操作会注册到其中一个 io_context。
假设创建了四个：
_ioServices[0]
_ioServices[1]
_ioServices[2]
_ioServices[3]
```

**_works**

```
std::vector<WorkPtr> _works;
```

每个 `io_context` 对应一个 `Work`：

```
_ioServices[0] ← _works[0]
_ioServices[1] ← _works[1]
_ioServices[2] ← _works[2]
```

没有 `Work` 时，如果暂时没有异步任务：

```
_ioServices[i].run();
```



**_threads**

```
std::vector<std::thread> _threads;
```

保存工作线程。通常一个线程负责运行一个 `io_context`：

```
_threads[0] → _ioServices[0].run();
_threads[1] → _ioServices[1].run();
_threads[2] → _ioServices[2].run();
```

`io_context` 本身不会创建线程，必须由某个线程调用 `run()`，异步回调才会被执行。



**_nextIOService**

```
std::size_t _nextIOService;
```

记录下次返回哪个 `io_context`，用于轮询分配：

```
连接1 → io_context 0
连接2 → io_context 1
连接3 → io_context 2
连接4 → io_context 0
```

#### 1.4对应的AsioIOServerpool.cpp

##### 构造函数

```
for (std::size_t i = 0; i < size; i++)
{
    _works[i] =
        std::unique_ptr<Work>(new Work(_ioServices[i]));
}
```

1每个 `Work` 都和一个 `io_context` 绑定：

```
_works[0] → 保活 _ioServices[0]
_works[1] → 保活 _ioServices[1]
_works[2] → 保活 _ioServices[2]
_works[3] → 保活 _ioServices[3]
```

**如果没有 `Work`，在线程刚启动、还没有异步任务时：**

```
_ioServices[i].run();
```

可能立即返回，线程直接结束。

有了 `Work` 后，即使暂时没有连接或读写事件，`run()` 也会继续等待。

`unique_ptr` 表示 `_works[i]` 独占这个 `Work`，后续调用：

```
_works[i].reset();
```

就能自动销毁它。

这句可以简化成：

```
_works[i] = std::make_unique<Work>(_ioServices[i]);
```

**2创建工作线程**

```
for (std::size_t i = 0; i < _ioServices.size(); ++i)
{
    _threads.emplace_back([this, i]() {
        _ioServices[i].run();
    });
}
```

每循环一次，就在 `_threads` 中创建一个线程。

线程和 `io_context` 的关系是：

```
_threads[0] → _ioServices[0].run()
_threads[1] → _ioServices[1].run()
_threads[2] → _ioServices[2].run()
_threads[3] → _ioServices[3].run()
```

`[this, i]`

Lambda 捕获了：

- `this`：用于访问当前对象的 `_ioServices`。
- `i`：保存当前线程对应的下标。

`i` 是按值捕获的，所以每个线程都保存自己的下标。否则循环继续执行后，下标可能发生变化。

`run()`

```
_ioServices[i].run();
```

让当前线程进入 Asio 事件循环：

```
等待网络事件
    ↓
Socket 读取完成
    ↓
执行读取回调
    ↓
Socket 写入完成
    ↓
执行写入回调
    ↓
继续等待
```

只要 `io_context` 没有被停止，并且 `Work` 仍然存在，线程就会一直运行。

**为什么必须先创建 Work**，因为并且 `Work` 仍然存在，线程就会一直运行。

当前顺序是正确的：

```
先创建 Work
    ↓
再创建线程
    ↓
线程调用 run()
```

如果反过来：

```
先创建线程
    ↓
run() 发现没有异步任务
    ↓
立即返回
    ↓
线程退出
    ↓
此时再创建 Work 已经晚了
```

##### Stop方法

三个一起使用，是为了完成“通知停止 → 解除保活 → 等待退出”的完整流程：

1. `stop()`

   通知 `io_context`停止事件循环，使 `run()`尽快返回。否则即使释放 `Work`，未完成的异步读写仍可能让 `run()`继续等待。

2. `reset()`

   销毁 `Work`，解除对 `io_context`的人工保活并释放资源。

3. `join()`

   等待运行 `io_context::run()`的工作线程真正结束，避免线程池析构后线程还在访问资源。

总结：

```
stop()  → 让事件循环停下来
reset() → 撤销保活并释放 Work
join()  → 等待线程彻底退出
```

三者共同确保线程池安全、完整地关闭。

#### 1.5CServer.h

**整体作用**

`CServer` 是 TCP 服务器的“连接管理器”。它负责监听端口、接收新连接、保存所有 Session、清理断线 Session 和检查心跳；具体收发数据由 `CSession` 负责。

**成员变量**

boost::asio::io_context& _io_context;

保存主事件循环的引用。

它主要驱动：

- `_acceptor` 的连接接收事件。

- `_timer` 的心跳检查事件

  

_acceptor绑定ip和端口

2监听端口

3异步接收客户端连接

`_sessions`

```
std::map<std::string, std::shared_ptr<CSession>> _sessions;
```

保存当前服务器的所有连接：

```
session_id → CSession
```

例如：

```
"uuid-001" → 客户端A的Session
"uuid-002" → 客户端B的Session
"uuid-003" → 客户端C的Session
```

`shared_ptr`既方便其他模块获得 Session，也负责维持 Session 的生命周期。

只要 `_sessions`中还保存着它，Session 就不会被析构。

`_mutex`

```
std::mutex _mutex;
```

保护 `_sessions`。

因为多个线程可能同时访问连接表：

- 主线程接收新连接。
- I/O 线程发现连接断开。
- 逻辑线程查找 Session。
- 定时器线程清理过期连接。

所以添加、查找、删除 Session 时都需要加锁。

`_timer`

```
boost::asio::steady_timer _timer;
```

周期性检查客户端心跳：

```
定时器到期
    ↓
遍历所有 Session
    ↓
判断最后心跳时间
    ↓
关闭并清理超时连接
    ↓
重新设置下一次定时器
```

使用 `steady_timer` 是合适的，因为它基于稳定时钟，不受系统时间被手动修改的影响。

**方法与类的特性**

让成员函数可以通过：

```
shared_from_this()
```

获得指向当前对象的 `shared_ptr`。

主要用于异步定时器：

```
auto self = shared_from_this();

_timer.async_wait([self](auto ec) {
    self->on_timer(ec);
});
```

即使外部临时释放了 Server，回调持有的 `self`仍能保证 Server 在回调执行前不会析构。

因此，`CServer`必须使用 `shared_ptr`创建：

```
auto server = std::make_shared<CServer>(io_context, port);
```

##### 构造函数

```
CServer(boost::asio::io_context& io_context, short port);
```

负责：

1. 保存 `io_context`和端口。
2. 创建并绑定 `_acceptor`。
3. 创建心跳定时器。
4. 调用 `StartAccept()`开始等待连接。

对应执行过程：

```
构造 CServer
    ↓
绑定监听端口
    ↓
注册第一次 async_accept
    ↓
等待客户端连接
```

##### StartAccept()

```
void StartAccept();
```

开始一次异步连接接收。

实现逻辑是：

```
auto& io_context =
    AsioIOServicePool::GetInstance()->GetIOService();

auto new_session =
    std::make_shared<CSession>(io_context, this);

_acceptor.async_accept(
    new_session->GetSocket(),
    ...);
```

它完成三件事：

1. 从 I/O 线程池轮询获取一个工作 `io_context`。
2. 使用该 `io_context`创建新 Session。
3. 把 Session 的 Socket 交给 `_acceptor`等待客户端连接。

所以：

```
主 io_context
└── 负责 accept 新连接

I/O线程池
├── 负责 Session A 的收发
├── 负责 Session B 的收发
└── 负责 Session C 的收发
```

```
void HandleAccept(
    std::shared_ptr<CSession> new_session,
    const boost::system::error_code& error);
```

衔接上一个

#####  HandleAccept

当 `async_accept()`完成时调用。

连接成功后：

```
new_session->Start();
_sessions.insert({
    new_session->GetSessionId(),
    new_session
});
```

其中：

- `Start()`开始异步读取客户端数据。
- `_sessions`保存新连接。

最后再次调用：

```
StartAccept();
```

继续等待下一个客户端。因此形成持续的接收循环：

```
StartAccept
    ↓
客户端连接
    ↓
HandleAccept
    ↓
保存 Session
    ↓
再次 StartAccept
```

这里建议先把 Session 插入 `_sessions`，再调用 `Start()`：

```
{
    std::lock_guard<std::mutex> lock(_mutex);
    _sessions.emplace(
        new_session->GetSessionId(),
        new_session);
}

new_session->Start();
```

##### ClearSession()

```
void ClearSession(std::string session_id);
```

根据 Session ID 删除连接。

通常流程是：

```
连接异常或心跳超时
    ↓
根据 session_id 找到 Session
    ↓
移除 UserMgr 中的用户与Session关系
    ↓
从 _sessions 中删除
    ↓
shared_ptr引用归零后析构Session
```

#####  StartTimer()

```
void StartTimer();
```

启动心跳检查定时器：

```c++
auto self = shared_from_this();

_timer.async_wait([self](auto ec) {
    self->on_timer(ec);
});
```

捕获 `self`是为了保证异步等待期间 `CServer`仍然存在。

更详细的解释

```
对象方法被调用
    │
    ▼
auto self = shared_from_this();    // 引用计数 +1（暂时）
    │
    ▼
_timer.async_wait([self](auto ec) {
    self->on_timer(ec);            // 定时器到期后执行
});                                // lambda 持有 self，引用计数持续 +1
    │
    ▼
方法返回，局部变量 self 析构      // 引用计数 -1（但 lambda 中还持有一份）
    │
    │  ... 其他代码继续运行，对象可能在其他地方被释放 ...
    │  ... 但 lambda 中的 self 保证了对象不会被完全销毁 ...
    │
    ▼
定时器到期，io_context 调用 lambda
    │
    ▼
self->on_timer(ec);               // 安全调用成员函数
    │
    ▼
lambda 执行完毕并析构             // 引用计数 -1，如为 0 则释放对象

```

##### `StopTimer()`

```
void StopTimer();
```

内部调用：

```
_timer.cancel();
```

取消异步定时器。

取消后，等待回调一般会收到一个错误码：

```
boost::asio::error::operation_aborted
```

所以 `on_timer()`开头需要检查 `ec`。

##### on_timer()

// 定时器到期后执行的回调函数。
// 主要负责检查Session心跳、清理超时连接、更新在线人数，
// 并重新注册下一次定时检查。
void CServer::on_timer(const boost::system::error_code& ec)
{
    // 如果定时器被取消，或者异步等待发生错误，则停止本轮处理。
    // 调用 _timer.cancel() 时，ec通常是operation_aborted。
    if (ec) {
        std::cout << "timer error: "
                  << ec.message()
                  << std::endl;
        return;
    }

```c++
// 保存本轮检查中发现的心跳超时Session。
// 这里只暂时收集，遍历结束后再统一清理。
std::vector<std::shared_ptr<CSession>> _expired_sessions;

// 记录当前仍然有效的Session数量。
int session_count = 0;

// 创建服务器连接表的快照。
// 后续遍历副本，避免长时间持有CServer::_mutex。
std::map<std::string, std::shared_ptr<CSession>>
    sessions_copy;

{
    // _sessions可能被多个线程同时访问，因此复制时需要加锁。
    std::lock_guard<std::mutex> lock(_mutex);

    // shared_ptr复制只会增加引用计数，
    // 不会复制真正的CSession对象。
    sessions_copy = _sessions;
}
// 离开作用域后，lock自动析构并释放_mutex。

// 获取当前时间。
// 本轮所有Session都使用同一个时间点检查心跳。
std::time_t now = std::time(nullptr);

// 遍历连接表快照，逐个检查Session是否心跳超时。
for (auto iter = sessions_copy.begin();
     iter != sessions_copy.end();
     ++iter) {

    // iter->first是session_id。
    // iter->second是对应的shared_ptr<CSession>。
    bool b_expired =
        iter->second->IsHeartbeatExpired(now);

    if (b_expired) {
        // 关闭超时Session的Socket。
        // 如果Socket上存在未完成的async_read，
        // 关闭后通常会触发异步读取的错误回调。
        iter->second->Close();

        // 暂存过期Session，稍后统一清理。
        _expired_sessions.push_back(iter->second);

        // 过期连接不计入有效连接数。
        continue;
    }

    // 心跳没有超时，计入当前有效Session数量。
    ++session_count;
}

// 获取当前ChatServer的配置名称。
auto& cfg = ConfigMgr::Inst();
auto self_name = cfg["SelfServer"]["Name"];

// Redis接口接收字符串，因此把连接数转换成字符串。
auto count_str = std::to_string(session_count);

// 将当前服务器的有效连接数写入Redis。
// 例如：LOGIN_COUNT["chatserver1"] = "10"。
RedisMgr::GetInstance()->HSet(
    LOGIN_COUNT,
    self_name,
    count_str);

// 统一处理过期Session。
//
// 这里执行时已经释放了CServer::_mutex。
// DealExceptionSession()最终可能调用ClearSession()，
// 而ClearSession()也会获取_mutex。
// 如果在持锁状态下调用，就可能发生重复加锁和死锁。
for (auto& session : _expired_sessions) {
    session->DealExceptionSession();
}

// 将定时器的下一次到期时间设置为60秒后。
_timer.expires_after(std::chrono::seconds(60));

// 注册下一次异步等待。
// 60秒后，Asio会再次调用on_timer()。
_timer.async_wait(
    [this](const boost::system::error_code& ec) {
        on_timer(ec);
    });
```
#### 1.6CSession.h与CServer.cpp

**整体作用**

这段头文件定义了两个类：

- `CSession`：代表一个客户端与服务器之间的 TCP 连接。
- `LogicNode`：把“连接对象 + 收到的完整消息”包装成业务任务，投递给 `LogicSystem`。

核心关系：

```
客户端
  ↓ TCP连接
CSession
  ↓ 收到完整消息
LogicNode
  ↓ 放入消息队列
LogicSystem
  ↓ 执行业务逻辑
```

##### 构造函数

`CSession`构造函数为新客户端准备 Socket、服务器关联、唯一 Session ID、消息头缓冲区和初始心跳状态，但真正的网络读取要等连接成功后调用 `Start()`才开始

##### Start()

开始读取客户端数据

##### AsyncReadHead与异步编程

异步编程

#### 1.7`MsgNode` 

 是一个**原始字符缓冲区包装类**，很简单：

- **`_data`** — 堆上分配的 `char[]`，存放实际字节数据
- **`_total_len`** — 缓冲区总容量
- **`_cur_len`** — 当前已读写到的位置
- **`Clear()`** — 重置缓冲区（清零 + 游标归零）

两个子类继承它，加了 `_msg_id`：

| 子类       | 用途                                                         |
| ---------- | ------------------------------------------------------------ |
| `RecvNode` | 存放**接收**到的消息体（`_data` = 消息体内容，`_msg_id` = 消息类型） |
| `SendNode` | 存放**待发送**的完整包（`_data` = 头部+消息体拼好的整包，`_msg_id` = 消息类型） |

在 `CSession` 里，`_recv_head_node` 也是 `MsgNode`（用于暂存 4 字节的头部），收到完整头部后才会 new 一个 `RecvNode` 来接收消息体。

```
_data 布局:
┌──────────────┬──────────────┬─────────────────────┐
│  msg_id (2B) │  body_len(2B)│  msg body (max_len) │
│  网络字节序    │  网络字节序    │  原始内容             │
└──────────────┴──────────────┴─────────────────────┘
```

```
SendNode::SendNode(const char* msg, short max_len, short msg_id) :MsgNode(max_len + HEAD_TOTAL_LEN)
, _msg_id(msg_id) {
	//先发送id, 转为网络字节序
	short msg_id_host = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
	//_data消息头两字节
	memcpy(_data, &msg_id_host, HEAD_ID_LEN);
	//转为网络字节序
	short max_len_host = boost::asio::detail::socket_ops::host_to_network_short(max_len);
	//消息体长度两字节
	memcpy(_data + HEAD_ID_LEN, &max_len_host, HEAD_DATA_LEN);
	//消息内容长度
	memcpy(_data + HEAD_ID_LEN + HEAD_DATA_LEN, msg, max_len);
}

```

#### 1.8RedisMgr.h与RedisMgr.cpp

1. 连接池（`RedisConPool`）— 为什么不用一个连接？

Redis 用的是 **TCP 长连接**。每次 `redisConnect` 要三次握手，用完断开又要四次挥手，开销大。所以预先创建一堆连接放在 `queue` 里，**用完还回去**（`returnConnection`），下次继续用——这就是"池化"思想。

2. 心跳检测线程（`check_thread_`）— 连接断了怎么办？

TCP 连接如果长时间没数据来往，中间的路由器/NAT 可能会把它干掉，但双方都不知道。所以后台线程每隔一段时间对所有连接发 `PING`，**主动探测**连接是否还活着。断了就 `fail_count_++`，再调用 `reconnect()` 补回来。

3. 条件变量阻塞等待（`cond_.wait`）— 连接用完了怎么办？

`getConnection()` 里用了 `cond_.wait`：如果池子里的连接全被借走了，**调用者就原地睡觉**，不占 CPU。等有人 `returnConnection` 还回来时，`cond_.notify_one()` 唤醒一个等待者拿走连接。比忙等的 `while` 循环高效得多。同时还提供了 `getConNonBlock()` 给心跳线程用——没连接就直接返回 `nullptr`，不能等（等就死锁了）。

------

**一句话总结**：用连接池省去反复建连断连的开销，用心跳线程保证池子里都是活连接，用条件变量让业务线程优雅地排队等连接。

##### 1. 初始化连接池（构造函数）

从配置文件读取 Redis 的 host、port、密码，创建 10 个连接的池子。

##### 2. 封装 Redis 命令（其余所有方法）

把 hiredis 的 C 风格 API 包成 C++ 接口，每条方法都是固定套路：



```
借连接 → 发命令 → 检查结果 → freeReplyObject → 还连接 → 返回结果
```

具体命令包括：

| 分类     | 方法                                                         | 对应 Redis 命令                |
| -------- | ------------------------------------------------------------ | ------------------------------ |
| 基础     | `Get` / `Set` / `Del` / `ExistsKey`                          | GET / SET / DEL / EXISTS       |
| 链表     | `LPush` / `LPop` / `RPush` / `RPop`                          | 列表的左右 push/pop            |
| 哈希     | `HSet` / `HGet` / `HDel`                                     | Hash 操作                      |
| 分布式锁 | `acquireLock` / `releaseLock`                                | 基于 Redis 实现锁              |
| 计数     | `IncreaseCount` / `DecreaseCount` / `InitCount` / `DelCount` | 在线人数统计（带分布式锁保护） |

还有一个细节：`HSet` 提供了两个重载——一个传 `std::string`，一个传 `const char*` + 长度，后者用 `redisCommandArgv` 以支持二进制数据（`hvalue` 里可能含 `\0`）。

**本质就是一个"翻译层"**：把上层业务调用翻译成 Redis 网络协议命令发给 Redis 服务器。

#### 1.9 DistLock类

`DistLock` 是利用 Redis 实现的 **分布式锁**。两个核心方法：

`acquireLock` — 加锁

用 Redis 命令 `SET lockKey UUID NX EX lockTimeout` 去抢锁：

- **NX**：只有当这个 key **不存在**时才设置成功 → 谁先 SET 成功谁拿到锁
- **EX**：设置过期时间，防止持锁的进程崩溃后锁永远不释放
- **UUID**：每个加锁者生成唯一标识，保证解锁时"自己只能解自己的锁"

如果抢不到，就每隔 1ms 重试，直到超时（`acquireTimeout`）放弃。

`releaseLock` — 解锁

用 **Lua 脚本** 原子性地执行：



```
if (GET key == 我的UUID) → DEL key
else → 什么都不做
```

必须**先比较再删除**做成一步原子操作。如果拆成两步（先 GET 再 DEL），中间可能发生：

1. 你的 GET 返回"是我的锁"
2. 锁刚好过期，别人抢到了
3. 你的 DEL 把别人的锁删了 ❌

Lua 脚本在 Redis 里是原子执行的，避免了这个问题。

------

**一句话**：多个进程/服务器抢同一把锁时，靠 Redis 的 `SET NX` 保证只有一个能抢到，靠 Lua 脚本保证不会误删别人的锁。这个项目中用它来保护**在线人数计数**的并发安全。

#### 1.10 Asiopool线程池

`io_context`（也叫 io_service）就相当于一个**银行柜台**，网络上的读写请求就是**来办业务的人**。一个柜台忙不过来，就开多个柜台。

`AsioIOServicePool` 就是**一排柜台**：

- 构造函数：开 N 个柜台（默认 = CPU 核心数），每个柜台配一个线程在那等着接活
- `work`：相当于告诉柜台"别下班，一直开着"——没有 work 的话柜台没活就关门了
- `GetIOService()`：**轮询**选一个柜台给你（柜台1 → 柜台2 → ... → 柜台N → 转回来柜台1），保证负载均匀分配
- `Stop()`：关掉所有柜台，线程退出

为什么这样设计

网络请求随时可能来（有人发消息、有人登录），不能让主线程一直等着——所以开**多个线程**，每来一个请求就丢给某个 io_context 处理，其他请求不受影响。多个 io_context 轮询分配，避免所有请求挤在一个线程上。

**一句话**：就是一组线程池，用来并发处理网络 I/O。
