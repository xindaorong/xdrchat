#pragma once
#include"const.h"
#include"MysqlDao.h"
//业务层入口，`MysqlMgr` 的作用是把数据库操作再包一层，让 `LogicSystem` 不直接依赖 `MysqlDao`。
class MysqlMgr:public Singleton<MysqlMgr>
{
	//singleton需要访问MysqlMgr的构造函数，所以要声明为友元
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	//注册接口
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& pwd);
	bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
private:
	//构造函数私有化，防止外部直接new MysqlMgr
	MysqlMgr();

	//MysqlDao 内部持有一个MysqlDao，由Dao负责真正访问数据库
	MysqlDao _dao;
};
