1. redisCommand 执行 SET 命令
2. Redis 返回一个 redisReply 对象
3. 判断这个对象是不是 NULL
4. 判断返回类型是不是 STATUS，内容是不是 OK
5. 如果失败：释放 redisReply，返回 false
6. 如果成功：释放 redisReply，返回 true
7. redisCommand(...)每执行一次 Redis 命令，hiredis 都会在堆上创建一个 redisReply 对象，用来保存 Redis 返回的结果。所以无论如何都要进行释放操作

具体代码模块

```
bool RedisMgr::Set(const std::string &key, const std::string &value)
{   //执行redis命令行命令，返回一个redisReply对象
	//把redisCommand之后的东西转换为redisReply指针对象并且赋值给redisMgr类下的_reply
	this->_reply = (redisReply*)redisCommand(this->_connect, "SET %s %s", key.c_str(), value.c_str());
	//如果为空
	if (this->_reply == NULL)
	{
		std::cout << "[SET" << key << "]failed" << std::endl;
		//freeReplyObject(this->_reply);
		return false;
	}
	//如果执行失败则释放连接对象，返回false
	//strcmp比较的是两个字符串是否相等 
	if (!(this->_reply->type== REDIS_REPLY_STATUS&&(strcmp(this->_reply->str, "OK")== 0||strcmp(this->_reply->str,
		 "ok") == 0)))
	{
		
		std::cout << "Execut commmand[SET" << key << "]failed!" << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}
	//执行成功 释放redisCommand执行返回的redisReply对象所占用的内存，返回true
	freeReplyObject(this->_reply);
	std::cout << "Succeed to execute command [ SET " << key << "  ]" << std::endl;
	return true;
}
```

