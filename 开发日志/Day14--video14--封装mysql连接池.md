# Day14--video14--封装 MySQL 连接池

本节主要是在 GateServer 中加入 MySQL 访问层，让注册接口不再只依赖 Redis 判断用户是否存在，而是把用户信息真正写入 MySQL。

整体调用链：

```text
客户端注册请求
  -> LogicSystem 注册回调
  -> MysqlMgr::GetInstance()->RegUser(name, email, pwd)
  -> MysqlDao::RegUser(...)
  -> MysqlPool::getConnection()
  -> CALL reg_user(?,?,?,@result)
  -> SELECT @result AS result
  -> MysqlPool::returnConnection(...)
```

## 1. 相关文件职责

| 文件 | 作用 |
|------|------|
| `MysqlMgr.h/.cpp` | 对外提供 MySQL 单例管理入口，业务层通过它调用数据库 |
| `MysqlDao.h/.cpp` | 真正封装 MySQL 连接池和 SQL 操作 |
| `config.ini` | 保存 MySQL 的 Host、Port、User、Passwd、Schema |
| `fix_reg_user.sql` | 定义注册用户的存储过程 `reg_user` |

## 2. `MysqlMgr`：业务层入口

`MysqlMgr` 的作用是把数据库操作再包一层，让 `LogicSystem` 不直接依赖 `MysqlDao`。

```cpp
#pragma once
#include "const.h"
#include "MysqlDao.h"

// MysqlMgr 继承 Singleton<MysqlMgr>，表示整个 GateServer 进程里只创建一个 MysqlMgr。
class MysqlMgr : public Singleton<MysqlMgr>
{
    // Singleton 需要能访问 MysqlMgr 的私有构造函数，所以声明为友元。
    friend class Singleton<MysqlMgr>;

public:
    ~MysqlMgr();

    // 注册用户接口：业务层只关心 name/email/pwd，不关心底层 SQL 怎么执行。
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);

private:
    // 构造函数私有化，防止外部直接 new MysqlMgr。
    MysqlMgr();

    // MysqlMgr 内部持有一个 MysqlDao，由 Dao 负责真正访问数据库。
    MysqlDao _dao;
};
```

实现部分：

```cpp
#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr()
{
}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
    // 这里本身不写 SQL，只是把请求转交给 MysqlDao。
    // 好处：以后业务层不用知道连接池、PreparedStatement、存储过程这些细节。
    return _dao.RegUser(name, email, pwd);
}

MysqlMgr::MysqlMgr()
{
}
```

在 `LogicSystem.cpp` 中的调用位置：

```cpp
auto name = src_root["user"].asString();
auto email = src_root["email"].asString();
auto pwd = src_root["passwd"].asString();

// 调用 MySQL 注册用户，返回值由存储过程决定。
int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd);

if (uid == 0) {
    // uid == 0：用户名或邮箱已经存在。
    root["error"] = ErrorCodes::UserExist;
}
else if (uid == -1) {
    // uid == -1：数据库执行异常。
    // 当前代码这里也返回 UserExist，后面可以考虑单独加一个 MysqlError 错误码。
    root["error"] = ErrorCodes::UserExist;
}
else {
    // uid > 0：注册成功，uid 是数据库生成的新用户 id。
    root["uid"] = uid;
}
```

## 3. `SqlConnection`：连接对象包装

```cpp
// SqlConnection 不是连接池，它只是池子里每一个连接的包装对象。
class SqlConnection {
public:
    // con：MySQL Connector/C++ 创建出来的真实数据库连接。
    // lasttime：上一次使用或保活的时间戳，用于判断是否需要执行 SELECT 1 保活。
    SqlConnection(sql::Connection* con, int64_t lasttime)
        : _con(con), _last_oper_time(lasttime) {}

    // unique_ptr 表示这个连接对象只有一个所有者。
    // 当 SqlConnection 被销毁时，_con 也会自动释放。
    std::unique_ptr<sql::Connection> _con;

    // 记录连接最后一次操作时间，单位是秒。
    int64_t _last_oper_time;
};
```

## 4. `MysqlPool`：连接池核心

连接池的核心思想：程序启动时提前创建几个数据库连接，业务请求来了就从队列里取一个，用完再放回队列，避免每次注册都重新建立 TCP/MySQL 连接。

```cpp
class MysqlPool {
public:
    MysqlPool(const std::string& url,
              const std::string& user,
              const std::string& pass,
              const std::string& schema,
              int poolSize)
        : url_(url),
          user_(user),
          pass_(pass),
          schema_(schema),
          poolSize_(poolSize),
          b_stop_(false)
    {
        try {
            for (int i = 0; i < poolSize_; ++i) {
                // 获取 MySQL Connector/C++ 的驱动对象。
                sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

                // 创建数据库连接，例如 tcp://127.0.0.1:3308 或 127.0.0.1:3308。
                auto* con = driver->connect(url_, user_, pass_);

                // 选择使用哪个数据库，对应 config.ini 里的 Schema=xdr。
                con->setSchema(schema_);

                // 记录当前时间，作为连接的最后操作时间。
                auto currentTime = std::chrono::system_clock::now().time_since_epoch();
                long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

                // 把连接包装成 SqlConnection，再放入连接池队列。
                pool_.push(std::make_unique<SqlConnection>(con, timestamp));
            }

            // 启动后台线程，定时检查连接是否还可用。
            _check_thread = std::thread([this]() {
                while (!b_stop_) {
                    checkConnection();
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                }
            });

            // detach 后线程独立运行，不需要主线程 join。
            // 注意：detach 线程生命周期不好控制，后面项目更稳定的写法是析构前 Close + join。
            _check_thread.detach();
        }
        catch (sql::SQLException& e) {
            std::cout << "mysql pool init failed, error is " << e.what() << std::endl;
        }
    }
```

### 4.1 定时保活 `checkConnection`

```cpp
void checkConnection() {
    // 多线程访问 pool_ 队列，必须加锁。
    std::lock_guard<std::mutex> guard(mutex_);

    int poolsize = pool_.size();

    auto currentTime = std::chrono::system_clock::now().time_since_epoch();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

    for (int i = 0; i < poolsize; i++) {
        // 从队头取出一个连接，检查完之后再放回队尾。
        auto con = std::move(pool_.front());
        pool_.pop();

        // Defer 是 RAII 小工具：当前循环结束时自动执行 lambda。
        // 这样不管中间 continue 还是正常走完，都能把连接放回 pool_。
        Defer defer([this, &con]() {
            pool_.push(std::move(con));
        });

        // 如果连接刚刚使用过，就不需要保活。
        if (timestamp - con->_last_oper_time < 5) {
            continue;
        }

        try {
            // SELECT 1 是常见的数据库连接保活语句，只检查连接是否可用。
            std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
            stmt->executeQuery("SELECT 1");

            // 保活成功，更新最后操作时间。
            con->_last_oper_time = timestamp;
        }
        catch (sql::SQLException& e) {
            std::cout << "Error keeping connection alive: " << e.what() << std::endl;

            // 如果连接已经断开，就重新创建一个连接替换旧连接。
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            auto* newcon = driver->connect(url_, user_, pass_);
            newcon->setSchema(schema_);

            con->_con.reset(newcon);
            con->_last_oper_time = timestamp;
        }
    }
}
```

### 4.2 取连接 `getConnection`

```cpp
std::unique_ptr<SqlConnection> getConnection() {
    // unique_lock 可以配合 condition_variable 使用。
    std::unique_lock<std::mutex> lock(mutex_);

    // 如果连接池为空，就阻塞等待；直到有连接归还，或者连接池准备关闭。
    cond_.wait(lock, [this] {
        if (b_stop_) {
            return true;
        }
        return !pool_.empty();
    });

    // 如果连接池已经关闭，就返回 nullptr，调用方需要判断。
    if (b_stop_) {
        return nullptr;
    }

    // 从队头拿走一个连接，所有权从 pool_ 转移给调用方。
    std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
    pool_.pop();
    return con;
}
```

### 4.3 还连接 `returnConnection`

```cpp
void returnConnection(std::unique_ptr<SqlConnection> con) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (b_stop_) {
        return;
    }

    // 用完的连接重新放回队列。
    pool_.push(std::move(con));

    // 唤醒一个正在 getConnection() 里等待的线程。
    cond_.notify_one();
}
```

### 4.4 关闭连接池

```cpp
void Close() {
    // 标记连接池停止工作。
    b_stop_ = true;

    // 唤醒所有等待连接的线程，避免程序退出时线程一直卡住。
    cond_.notify_all();
}

~MysqlPool() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 清空队列，unique_ptr 会自动释放每个 SqlConnection。
    while (!pool_.empty()) {
        pool_.pop();
    }
}
```

### 4.5 私有成员变量

这些私有变量是连接池的核心状态，必须优先看懂。前面的函数其实都是围绕这些变量在做“创建连接、借出连接、归还连接、保活连接、关闭连接池”。

```cpp
private:
    // MySQL 服务器地址。
    // 例如 config.ini 中 Host=127.0.0.1、Port=3308，
    // 这里最终会拼成 127.0.0.1:3308。
    std::string url_;

    // MySQL 登录用户名，例如 root。
    std::string user_;

    // MySQL 登录密码。
    std::string pass_;

    // 当前连接要使用的数据库名。
    // 代码中 con->setSchema(schema_) 会切换到这个数据库，例如 xdr。
    std::string schema_;

    // 连接池大小，也就是初始化时提前创建多少个 MySQL 连接。
    // 当前 MysqlDao 里传的是 5。
    int poolSize_;

    // 空闲连接队列。
    // 队列里存的是 unique_ptr<SqlConnection>，表示每个 SqlConnection 只能有一个所有者。
    //
    // 取连接时：
    //     pool_ -> 调用方
    //     auto con = std::move(pool_.front());
    //
    // 还连接时：
    //     调用方 -> pool_
    //     pool_.push(std::move(con));
    //
    // unique_ptr 离开作用域时会自动释放 SqlConnection，
    // SqlConnection 内部的 sql::Connection 也会自动释放，减少内存泄漏风险。
    std::queue<std::unique_ptr<SqlConnection>> pool_;

    // 互斥锁，保护 pool_ 队列以及连接池共享状态。
    // 多个线程可能同时 getConnection() 或 returnConnection()，
    // 所以凡是 push/pop/read pool_ 的地方都要先持有这把锁。
    std::mutex mutex_;

    // 条件变量，用来实现“连接不够时等待，有连接归还时唤醒”。
    //
    // 消费者：
    //     getConnection() 发现 pool_ 为空时，
    //     调用 cond_.wait(lock, predicate) 阻塞等待。
    //
    // 生产者：
    //     returnConnection() 把连接放回 pool_ 后，
    //     调用 cond_.notify_one() 唤醒一个等待中的线程。
    //
    // 关闭：
    //     Close() 调用 cond_.notify_all() 唤醒所有等待线程，
    //     让它们检测到 b_stop_ == true 后返回 nullptr 并退出。
    std::condition_variable cond_;

    // 停止标志。
    // atomic<bool> 保证多个线程读写这个标志时是线程安全的。
    // getConnection()、Close()、健康检查线程都会关注它。
    std::atomic<bool> b_stop_;

    // 后台健康检查线程。
    // 构造函数里启动它，定期调用 checkConnection()，
    // 通过 SELECT 1 检查连接是否可用，断线时重新连接。
    std::thread _check_thread;
```

## 5. `MysqlDao`：数据库访问对象

`MysqlDao` 的职责是读取配置、创建连接池，并封装具体 SQL 操作。

```cpp
MysqlDao::MysqlDao()
{
    // ConfigMgr 是配置管理单例，启动时读取当前工作目录下的 config.ini。
    auto& cfg = ConfigMgr::Instance();

    // 读取 [Mysql] 小节中的配置。
    const auto& host = cfg["Mysql"]["Host"];
    const auto& port = cfg["Mysql"]["Port"];
    const auto& pwd = cfg["Mysql"]["Passwd"];
    const auto& schema = cfg["Mysql"]["Schema"];
    const auto& user = cfg["Mysql"]["User"];

    // 创建连接池，池子大小为 5。
    // host + ":" + port 对应 127.0.0.1:3308。
    pool_.reset(new MysqlPool(host + ":" + port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao()
{
    // Dao 析构时通知连接池停止工作。
    pool_->Close();
}
```

## 6. 注册用户 `MysqlDao::RegUser`

```cpp
int MysqlDao::RegUser(const std::string& name,
                      const std::string& email,
                      const std::string& pwd)
{
    // 从连接池取一个连接。
    auto con = pool_->getConnection();

    try {
        if (con == nullptr) {
            // 当前函数返回 int，false 会转换成 0。
            // 这里语义上更建议返回 -1，否则会和“用户已存在”的 0 混在一起。
            return false;
        }

        // 调用 MySQL 存储过程 reg_user。
        // 前 3 个 ? 是输入参数，@result 是 MySQL 会话变量，用来接收 OUT result。
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->_con->prepareStatement("CALL reg_user(?,?,?,@result)")
        );

        // 设置输入参数：
        // 1 -> new_name
        // 2 -> new_email
        // 3 -> new_pwd
        stmt->setString(1, name);
        stmt->setString(2, email);
        stmt->setString(3, pwd);

        // 执行存储过程。
        stmt->execute();

        // Connector/C++ 的 PreparedStatement 这里没有直接注册 OUT 参数，
        // 所以再执行一次 SELECT，把会话变量 @result 查出来。
        std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
        std::unique_ptr<sql::ResultSet> res(
            stmtResult->executeQuery("SELECT @result AS result")
        );

        if (res->next()) {
            int result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;

            // 用完连接必须归还连接池。
            pool_->returnConnection(std::move(con));
            return result;
        }

        // 没查到 result，归还连接，返回 -1 表示异常。
        pool_->returnConnection(std::move(con));
        return -1;
    }
    catch (sql::SQLException& e) {
        // SQL 抛异常时也要归还连接，否则连接池会越来越少。
        pool_->returnConnection(std::move(con));

        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return -1;
    }
}
```

## 7. 存储过程返回值

`fix_reg_user.sql` 里的 `reg_user` 存储过程负责判断用户名和邮箱是否重复，并插入新用户。

```sql
IF EXISTS (SELECT 1 FROM `user` WHERE `name` = new_name) THEN
    SET result = 0;
ELSEIF EXISTS (SELECT 1 FROM `user` WHERE `email` = new_email) THEN
    SET result = 0;
ELSE
    UPDATE `user_id` SET `id` = `id` + 1;
    SELECT `id` INTO new_id FROM `user_id` LIMIT 1;

    INSERT INTO `user` (`uid`, `name`, `email`, `pwd`)
    VALUES (new_id, new_name, new_email, new_pwd);

    SET result = new_id;
END IF;
```

返回值含义：

| 返回值 | 含义 |
|--------|------|
| `> 0` | 注册成功，新用户 uid |
| `0` | 用户名或邮箱已存在 |
| `-1` | SQL 异常或注册失败 |

## 8. 本节容易混淆的点

1. `MysqlMgr` 不是连接池，它只是业务层访问 MySQL 的统一入口。
2. `MysqlDao` 负责具体数据库操作，内部持有 `MysqlPool`。
3. `MysqlPool` 里保存的是多个 `SqlConnection`，不是多个 `MysqlDao`。
4. `std::move` 表示转移 `unique_ptr` 所有权，取连接和还连接都依赖它。
5. `condition_variable` 用来在连接池为空时等待，避免一直 while 循环空转。
6. `Defer` 用 RAII 保证 `checkConnection()` 中的连接会自动放回队列。
7. `CALL reg_user(?,?,?,@result)` 执行存储过程，`SELECT @result AS result` 再取输出结果。
8. 当前 `getConnection()` 返回空时 `return false` 会变成 `0`，容易和“用户已存在”混淆，建议后续改成 `return -1;`。

## 9. 当前注册流程总结

```text
1. 前端提交 user/email/passwd/confirm/varifycode
2. GateServer 解析 JSON
3. 先校验验证码
4. 再从 Redis 做一次用户存在性判断
5. 调用 MysqlMgr::RegUser 写入 MySQL
6. MysqlDao 从连接池取连接
7. 调用 reg_user 存储过程
8. 根据返回 uid/0/-1 生成响应 JSON
```
