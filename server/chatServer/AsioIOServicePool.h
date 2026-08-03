#pragma once
#include"Singleton.h"
#include <vector>
#include <boost/asio.hpp>
class AsioIOServicePool:public Singleton<AsioIOServicePool>
{
	friend Singleton<AsioIOServicePool>;
public:
	//分别给变量起别名
	using IOService = boost::asio::io_context;
	using Work = boost::asio::io_context::work;
	using WorkPtr = std::unique_ptr<Work>;

	~AsioIOServicePool();
	AsioIOServicePool(const AsioIOServicePool&) = delete;
	AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

	boost::asio::io_context& GetIOService();
	void stop();


private:
	AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());
	std::vector<IOService>_ioServices;
	std::vector<WorkPtr>_works;
	std::vector<std::thread>_threads;
	std::size_t _nextIOService;
};

