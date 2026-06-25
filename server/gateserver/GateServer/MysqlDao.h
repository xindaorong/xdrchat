#pragma once
#include "const.h"
#include <thread>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <queue>
#include <mutex>
#include <string>
//定义一个sql连接类
//只要多个线程会同时读 / 写同一个变量 / 容器，一律加锁
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
//定义一个mysql的连接线程池
//连接池的核心思想：程序启动时提前创建几个数据库连接，业务请求来了就从队列里取一个，
//用完再放回队列，避免每次注册都重新建立 TCP / MySQL 连接
class MysqlPool {
public:
	MysqlPool(const std::string& url, const std::string& user, const std::string& pass, const std::string& schema, int poolSize)
		: url_(url), user_(user), pass_(pass), schema_(schema), poolSize_(poolSize), b_stop_(false) {
		try {
			for (int i = 0; i < poolSize_; ++i) {
				sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
				auto* con = driver->connect(url_, user_, pass_);
				con->setSchema(schema_);
				// 获取当前时间戳
				auto currentTime = std::chrono::system_clock::now().time_since_epoch();
				// 将时间戳转换为秒
				long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
				pool_.push(std::make_unique<SqlConnection>(con, timestamp));
			}

			_check_thread = std::thread([this]() {
				while (!b_stop_) {
					checkConnection();
					std::this_thread::sleep_for(std::chrono::seconds(60));
				}
				});

			_check_thread.detach();
		}
		catch (sql::SQLException& e) {
			// 处理异常
			std::cout << "mysql pool init failed, error is " << e.what() << std::endl;
		}
	}
	void checkConnection() {
		std::lock_guard<std::mutex> guard(mutex_);
		int poolsize = pool_.size();
		// 获取当前时间戳
		auto currentTime = std::chrono::system_clock::now().time_since_epoch();
		// 将时间戳转换为秒
		long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
		for (int i = 0; i < poolsize; i++) {
			auto con = std::move(pool_.front());
			pool_.pop();
			Defer defer([this, &con]() {
				pool_.push(std::move(con));
				});

			if (timestamp - con->_last_oper_time < 5) {
				continue;
			}

			try {
				std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
				stmt->executeQuery("SELECT 1");
				con->_last_oper_time = timestamp;
				//std::cout << "execute timer alive query , cur is " << timestamp << std::endl;
			}
			catch (sql::SQLException& e) {
				std::cout << "Error keeping connection alive: " << e.what() << std::endl;
				// 重新创建连接并替换旧的连接
				sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
				auto* newcon = driver->connect(url_, user_, pass_);
				newcon->setSchema(schema_);
				con->_con.reset(newcon);
				con->_last_oper_time = timestamp;
			}
		}
	}
	//就像排队取号——没空位的时候你自己去旁边坐着（放锁、睡觉），有空位了服务员叫你（notify），
	//你站起来走回柜台（重新拿锁），办业务。全程"放"和"睡"的都是你自己，不是别人。
	//线程取连接操作
	std::unique_ptr<SqlConnection> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);
		//调用wait
		cond_.wait(lock, [this]
			{
			if (b_stop_) {
				return true;
			}
			return !pool_.empty(); });
		if (b_stop_) {
			return nullptr;
		}
		//下面才是真正的取到了连接
		std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
		pool_.pop();
		return con;
	}

	void returnConnection(std::unique_ptr<SqlConnection> con) {
		std::unique_lock<std::mutex> lock(mutex_);//加锁
		if (b_stop_) {
			return;
		}
		pool_.push(std::move(con));//通过移动构造交换主动权
		cond_.notify_one();//唤醒下一个等待的线程
	}
	void Close() {
		//1标记为停止
		b_stop_ = true;
		//2唤醒所有等待的线程，避免程序退出时线程卡住
		cond_.notify_all();
	}

	~MysqlPool() {
		std::unique_lock<std::mutex> lock(mutex_);
		while (!pool_.empty()) {
			pool_.pop();
		}
	}

private:
    std::string url_;//user_reg等
    std::string user_;//使用者
    std::string pass_;//密码
    std::string schema_;//数据库
    int poolSize_;//
    std::queue<std::unique_ptr<SqlConnection>> pool_;//连接队列，unique_ptr保证sql只有一个所有者，
	//离开作用域自动释放底层Mysql连接，杜绝内存泄漏
    std::mutex mutex_;//保护队列的线程安全，对pool_的操作都需要持有此锁
	//一个条件变量
	/*消费者（getConnection）：当 pool_ 为空时，调用 cond_.wait(lock, predicate) 阻塞当前线程，
	直到有连接可用或 b_stop_ 为 true。
		生产者（returnConnection）：归还连接后调用 cond_.notify_one() 唤醒一个等待中的消费者。
		关闭（Close）：调用 cond_.notify_all() 唤醒所有等待线程，让它们检测到 b*/
	//_stop_ == true 后返回 nullptr 并退出。
    std::condition_variable cond_;
    std::atomic<bool> b_stop_;//停止标志
	std::thread _check_thread;//std::thread _check_thread — 健康检查线程
};
class MysqlDao
{
public:
	MysqlDao();
	~MysqlDao();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	/*bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);
	bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
	bool AddFriendApply(const int& from, const int& to);
	bool AuthFriendApply(const int& from, const int& to);
	bool AddFriend(const int& from, const int& to, std::string back_name);
	std::shared_ptr<UserInfo> GetUser(int uid);
	std::shared_ptr<UserInfo> GetUser(std::string name);
	bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int offset, int limit);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo> >& user_info);*/
private:
	std::unique_ptr<MysqlPool> pool_;
};
