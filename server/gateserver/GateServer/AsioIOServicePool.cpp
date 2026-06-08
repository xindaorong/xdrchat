#include "AsioIOServicePool.h"
#include <iostream>
using namespace std;    
AsioIOServicePool::AsioIOServicePool(std::size_t size) :_ioServices(size) ,_works(size), _nextIoService(0){
    for (std::size_t i=0; i <size; ++i) {
        _works[i] = std::unique_ptr<Work>(new Work(_ioServices[i]));
    }
    //遍历多个ioservice对象，创建线程池,每个线程内部启动ioservice
    for (std::size_t i=0; i <_ioServices.size(); ++i) {
        _threads.emplace_back([this, i](){
            _ioServices[i].run();
        });
    }
}

AsioIOServicePool::~AsioIOServicePool() {
    Stop();
    std::cout << "AsioIOServicePool is destroyed" << std::endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
    auto& service = _ioServices[_nextIoService++];
    if(_nextIoService == _ioServices.size()) {
        _nextIoService = 0;
    }
    return service;
}
void AsioIOServicePool::Stop() {
    //因为仅仅执行work.reset()并不能让iocontext从run的状态中退出
    for (auto& work : _works) {
        work.reset();
    }
    for (auto& thread : _threads) {
        thread.join();
    }
}