#include"Cserver.h"
#include"const.h"
#include"global.h"
#include"HttpConnection.h"
#include"LogicSystem.h"
#include<boost/asio.hpp>
#include<iostream>
#include<json/json.h>
#include<json/value.h>
#include<json/reader.h>
#include"ConfigMgr.h"
#include"RedisMgr.h"
#include<hiredis/hiredis.h>

// 简单测试 RedisMgr 封装的常用 Redis 操作。
// 这里使用 assert 做 demo 级别校验：某一步失败时程序会直接中断。
void TestRedisMgr() {
	// 连接 Redis 服务并完成密码认证。
	auto& gCfgMgr = ConfigMgr::Instance();
	std::string redis_host = gCfgMgr["Redis"]["Host"];
	std::string redis_port_str = gCfgMgr["Redis"]["Port"];
	std::string redis_passwd = gCfgMgr["Redis"]["Passwd"];
	if (redis_host.empty()) {
		redis_host = "127.0.0.1";
	}
	if (redis_port_str.empty()) {
		redis_port_str = "6379";
	}
	int redis_port = atoi(redis_port_str.c_str());
	assert(RedisMgr::GetInstance()->Connect(redis_host, redis_port));
	if (!redis_passwd.empty()) {
		assert(RedisMgr::GetInstance()->Auth(redis_passwd));
	}

	// 测试字符串类型的 Set/Get 操作。
	assert(RedisMgr::GetInstance()->Set("blogwebsite", "llfc.club"));
	std::string value = "";
	assert(RedisMgr::GetInstance()->Get("blogwebsite", value));

	// 读取不存在的 key 时应返回 false。
	assert(RedisMgr::GetInstance()->Get("nonekey", value) == false);

	// 测试 Hash 类型的写入、读取和 key 是否存在。
	assert(RedisMgr::GetInstance()->HSet("bloginfo", "blogwebsite", "llfc.club"));
	assert(RedisMgr::GetInstance()->HGet("bloginfo", "blogwebsite") != "");
	assert(RedisMgr::GetInstance()->ExistsKey("bloginfo"));

	// 删除 key，并确认删除后不再存在。
	assert(RedisMgr::GetInstance()->Del("bloginfo"));
	assert(RedisMgr::GetInstance()->Del("bloginfo"));
	assert(RedisMgr::GetInstance()->ExistsKey("bloginfo") == false);

	// 测试 List 类型的左侧入队、右侧出队和左侧出队。
	assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue1"));
	assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue2"));
	assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue3"));
	assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
	assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
	assert(RedisMgr::GetInstance()->LPop("lpushkey1", value));

	// 对不存在的 list 做 LPop，期望返回 false。
	assert(RedisMgr::GetInstance()->LPop("lpushkey2", value) == false);

	// 测试结束后关闭 Redis 连接。
	RedisMgr::GetInstance()->Close();
}

int main()
{
	try
	{
		// 启动网关服务前，先执行 Redis 封装的连通性和基础命令测试。
		//TestRedisMgr();

		// 读取配置文件中的 GateServer 端口号。
		auto& gCfgMgr = ConfigMgr::Instance();
		std::string gate_port_str = gCfgMgr["GateServer"]["Port"];

		// 将字符串端口转换为无符号短整型，供服务器监听使用。
		unsigned short gate_port = atoi(gate_port_str.c_str());

		//错误日志6.8.1：GateServer 启动时读取 Redis 配置，用于注册接口校验邮箱验证码。
		std::string redis_host = gCfgMgr["Redis"]["Host"];
		std::string redis_port_str = gCfgMgr["Redis"]["Port"];
		std::string redis_passwd = gCfgMgr["Redis"]["Passwd"];
		if (redis_host.empty()) {
			redis_host = "127.0.0.1";
		}
		if (redis_port_str.empty()) {
			redis_port_str = "6379";
		}

		int redis_port = atoi(redis_port_str.c_str());
		//错误日志6.8.2：启动阶段先连接并认证 Redis，避免 /user_register 读取验证码时返回 1003。
		if (!RedisMgr::GetInstance()->Connect(redis_host, redis_port)) {
			std::cerr << "Redis connect failed: " << redis_host << ":" << redis_port << std::endl;
			return EXIT_FAILURE;
		}
		if (!redis_passwd.empty() && !RedisMgr::GetInstance()->Auth(redis_passwd)) {
			std::cerr << "Redis auth failed" << std::endl;
			return EXIT_FAILURE;
		}

		// 创建 Asio 事件循环。参数 1 表示该 io_context 使用一个线程运行。
		net::io_context ioc{ 1 };

		// 监听 Ctrl+C(SIGINT) 和终止信号(SIGTERM)，用于优雅退出。
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);

		// 异步等待系统信号；收到信号后停止 io_context。
		signals.async_wait([&ioc](const boost::system::error_code& error, int signal_number)
		{
			if(error)
			{
				return;
			}
			ioc.stop();
		});

		// 创建网关服务器对象并开始监听端口。
		std::make_shared<Cserver>(ioc, gate_port)->Start();

		// run 会阻塞当前线程，并持续处理异步网络事件。
		std::cout << "Server is running to listen to the port" << std::endl;
		ioc.run();
	}
	catch (std::exception const &e)
	{
		// 捕获启动或运行过程中抛出的异常，打印错误信息后退出。
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
