#pragma once
#include"const.h"
#include<hiredis/hiredis.h>
#include<atomic>
class RedisConPool
{
public:
    RedisConPool(size_t poolSize, const char* host, int port, const char* pwd);
      
    ~RedisConPool();
     redisContext* getConnection();
private:   
std::atomic<bool> b_stop_;                         // 停止标志，通知所有等待线程释放
size_t poolSize_;                             // 期望的连接池大小
const char* host_;                           // Redis 服务器地址
int port_;                                   // Redis 端口
std::queue<redisContext*> connections_;      // 存储空闲连接的队列
std::mutex mutex_;                           // 互斥锁，保护队列和停止标志
std::condition_variable cond_;               // 条件变量，用于等待可用连接
};

