#include "RedisMgr.h"

RedisMgr::RedisMgr()
	: _connect(nullptr), _reply(nullptr)
{
}

bool RedisMgr::Connect(const std::string& host, int port)
{
	if (this->_connect != nullptr)
	{
		redisFree(this->_connect);
		this->_connect = nullptr;
	}
	this->_connect = redisConnect(host.c_str(), port);
	if (this->_connect == nullptr)
	{
		std::cout << "connect error: redisConnect returned null" << std::endl;
		return false;
	}
	if (this->_connect->err)
	{
		std::cout << "connect error" << this->_connect->errstr << std::endl;
		redisFree(this->_connect);
		this->_connect = nullptr;
		return false;
	}
	return true;
}
bool RedisMgr::Get(const std::string &key, std::string& value)
{
	if (this->_connect == nullptr)
	{
		std::cout << "[GET " << key << "] failed: redis not connected" << std::endl;
		return false;
	}
	//redisCommand 函数返回的是 void* 类型（因为 Redis 的 hiredis 库中该函数返回通用指针，实际指向 redisReply 结构体）。

	//而 this->_reply 的类型是 redisReply* ，因此需要将 void* 显式转换为 redisReply* 才能赋值。
	//错误日志6.8.3：这里必须真正执行 Redis GET，之前 GET 被注释吞掉会导致 GateServer 读不到验证码。
	redisReply* reply = (redisReply*)redisCommand(this->_connect, "GET %s", key.c_str());
	this->_reply = reply;
	if (this->_reply == NULL)
	{
		std::cout << "[GET" <<key<< "]failed"<<std::endl;
		return false;
	}
	if (this->_reply->type!= REDIS_REPLY_STRING)
	{
		std::cout << "[GET" << key << "]failed" << std::endl;
		freeReplyObject(this->_reply);
		//错误日志6.8.4：释放失败 reply 后置空，避免后续误用已释放指针。
		this->_reply = nullptr;
		return false;
	}
	value = this->_reply->str;
	freeReplyObject(this->_reply);
	//错误日志6.8.5：GET 成功后释放 reply 并置空，避免悬空指针影响下一次 Redis 操作。
	this->_reply = nullptr;
	std::cout << "Succeed to execute command [ GET " << key << "  ]" << std::endl;
	return true;
}
bool RedisMgr::Set(const std::string &key, const std::string &value)
{   //执行redis命令行命令，返回一个redisReply对象
	if (this->_connect == nullptr)
	{
		std::cout << "[SET " << key << "] failed: redis not connected" << std::endl;
		return false;
	}
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
bool RedisMgr::Auth(const std::string& password)
{
	if (this->_connect == nullptr)
	{
		std::cout << "认证失败: redis not connected" << std::endl;
		return false;
	}
	this->_reply = (redisReply*)redisCommand(this->_connect, "AUTH %s", password.c_str());
	if (this->_reply == nullptr)
	{
		std::cout << "认证失败: empty redis reply" << std::endl;
		return false;
	}
	if (this->_reply->type == REDIS_REPLY_ERROR) {
		std::cout << "认证失败" << std::endl;
		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(this->_reply);
		return false;
	}
	else {
		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(this->_reply);
		std::cout << "认证成功" << std::endl;
		return true;
	}
}
//左侧push
bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
	//1向Redis发送SET 命令
	// 例如 key = "blogwebsite", value = "llfc.club"
	// 实际发送给 Redis 的命令就是：SET blogwebsite llfc.club
	// 
	//RedisCommand返回的是void*，实际指向redisReply对象
	//所以这里需要强制转换为redisReply*
	this->_reply = (redisReply*)redisCommand(this->_connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == this->_reply)
	{
		std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		//freeReplyObject(this->_reply);
		return false;
	}

	if (this->_reply->type != REDIS_REPLY_INTEGER || this->_reply->integer <= 0) {
		std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}

	std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}
//右侧pop
bool RedisMgr::LPop(const std::string& key, std::string& value) {
	this->_reply = (redisReply*)redisCommand(this->_connect, "LPOP %s ", key.c_str());
	if (_reply == nullptr || _reply->type == REDIS_REPLY_NIL) {
		std::cout << "Execut command [ LPOP " << key << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}
	value = _reply->str;
	std::cout << "Execut command [ LPOP " << key << " ] success ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}
//右侧push
bool RedisMgr::RPush(const std::string& key, const std::string& value) {
	this->_reply = (redisReply*)redisCommand(this->_connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == this->_reply)
	{
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}

	if (this->_reply->type != REDIS_REPLY_INTEGER || this->_reply->integer <= 0) {
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}

	std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}
//右侧pop
bool RedisMgr::RPop(const std::string& key, std::string& value) {
	this->_reply = (redisReply*)redisCommand(this->_connect, "RPOP %s ", key.c_str());
	if (_reply == nullptr || _reply->type == REDIS_REPLY_NIL) {
		std::cout << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}
	value = _reply->str;
	std::cout << "Execut command [ RPOP " << key << " ] success ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}
//Hset
bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value) {
	this->_reply = (redisReply*)redisCommand(this->_connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (_reply == nullptr || _reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}
	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}


bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
	const char* argv[4];
	size_t argvlen[4];
	argv[0] = "HSET";
	argvlen[0] = 4;
	argv[1] = key;
	argvlen[1] = strlen(key);
	argv[2] = hkey;
	argvlen[2] = strlen(hkey);
	argv[3] = hvalue;
	argvlen[3] = hvaluelen;
	this->_reply = (redisReply*)redisCommandArgv(this->_connect, 4, argv, argvlen);
	if (_reply == nullptr || _reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}
	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] success ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}
//HGet
std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
	const char* argv[3];
	size_t argvlen[3];
	argv[0] = "HGET";
	argvlen[0] = 4;
	argv[1] = key.c_str();
	argvlen[1] = key.length();
	argv[2] = hkey.c_str();
	argvlen[2] = hkey.length();
	this->_reply = (redisReply*)redisCommandArgv(this->_connect, 3, argv, argvlen);
	if (this->_reply == nullptr || this->_reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(this->_reply);
		std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! " << std::endl;
		return "";
	}

	std::string value = this->_reply->str;
	freeReplyObject(this->_reply);
	std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << std::endl;
	return value;
}
//Del操作
bool RedisMgr::Del(const std::string& key)
{
	this->_reply = (redisReply*)redisCommand(this->_connect, "DEL %s", key.c_str());
	if (this->_reply == nullptr || this->_reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ Del " << key << " ] failure ! " << std::endl;
		freeReplyObject(this->_reply);
		return false;
	}
	std::cout << "Execut command [ Del " << key << " ] success ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}
//判断键值对是否存在
bool RedisMgr::ExistsKey(const std::string& key)
{
	if (this->_connect == nullptr)
	{
		std::cout << "Not Found [ Key " << key << " ]: redis not connected" << std::endl;
		return false;
	}
	this->_reply = (redisReply*)redisCommand(this->_connect, "exists %s", key.c_str());
	if (this->_reply == nullptr || this->_reply->type != REDIS_REPLY_INTEGER || this->_reply->integer == 0) {
		std::cout << "Not Found [ Key " << key << " ]  ! " << std::endl;
		if (this->_reply != nullptr)
		{
			freeReplyObject(this->_reply);
		}
		return false;
	}
	std::cout << " Found [ Key " << key << " ] exists ! " << std::endl;
	freeReplyObject(this->_reply);
	return true;
}
void RedisMgr::Close()
{
	if (_connect != nullptr)
	{
		redisFree(_connect);
		_connect = nullptr;
	}
}
RedisMgr::~RedisMgr()
{
	Close();
}
