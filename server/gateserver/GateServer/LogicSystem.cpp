#include "LogicSystem.h"
#include "HttpConnection.h"
#include <iostream>
#include"VerifyGrpcClient.h"
#include"RedisMgr.h"
#include"MysqlMgr.h"
#include"MysqlDao.h"
LogicSystem::~LogicSystem() = default;

void LogicSystem::RegGet(std::string url, HttpHandler handler)
{
    _get_handlers.insert(std::make_pair(url, handler));
}
//函数声明
void LogicSystem::RegPost(std::string url, HttpHandler handler) {
    _post_handlers.insert(make_pair(url, handler));
}

// Register built-in routes.
LogicSystem::LogicSystem()
{
    RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection)
    {
        beast::ostream(connection->_response.body()) << "receive get_test request";
    });
    RegPost("/get_verifycode", [](std::shared_ptr<HttpConnection> connection) {
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type, "text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        auto email = src_root["email"].asString();
        GetVerifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVarifyCode(email);
        cout << "email is " << email << endl;
        root["error"] = rsp.error();
        root["email"] = src_root["email"];
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
        });
    //函数的具体实现
    //接收前端发来的注册请求 → 校验验证码 → 校验用户名是否重复 → 返回注册结果。
    RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
        //把http请求的原始body字节转成string
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        //设置响应头，告诉客户端返回的是JSON格式
        connection->_response.set(http::field::content_type, "text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        //转成json格式
        bool parse_success = reader.parse(body_str, src_root);
        if (!parse_success) {
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            //把jsonstr写到body
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //3第一步校验 --先查找redis中email对应的验证码是否过期
        std::string  varify_code;
        //用‘code_prefix_’+邮箱作为key去Redis中查找之前发给用户邮箱的验证码。找不到说明已经过期
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX+src_root["email"].asString(), varify_code);
        if (!b_get_varify) {
            std::cout << " get varify code expired" << std::endl;
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //4 第二步校验--验证码是否匹配
        if (varify_code != src_root["varifycode"].asString()) {
            std::cout << " varify code error" << std::endl;
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //5 校验用户名是否已经存在
        bool b_usr_exist = RedisMgr::GetInstance()->ExistsKey(src_root["user"].asString());
        if (b_usr_exist) {
            std::cout << " user exist" << std::endl;
            root["error"] = ErrorCodes::UserExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //需要先把这些数据取出来
        auto name = src_root["user"].asString();
        auto email = src_root["email"].asString();
        auto pwd = src_root["passwd"].asString();

        //查找数据库判断用户是否存在
        int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd);
        if (uid==0) {
            std::cout << " user  or email exist" << std::endl;
            root["error"] = ErrorCodes::UserExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        else if (uid == -1) {
            std::cout << "mysql error" << std::endl;
            root["error"] = ErrorCodes::SqlError;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //6校验通过，返回成功

        root["error"] = 0;
        root["email"] = src_root["email"];
        root["uid"] = uid;//否则就是注册成功
        root["user"] = src_root["user"].asString();
        root["passwd"] = src_root["passwd"].asString();
        root["confirm"] = src_root["confirm"].asString();
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
        });
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> connection)
{
    auto it = _get_handlers.find(path);
    if (it == _get_handlers.end())
    {
        return false;
    }

    _get_handlers[path](connection);
    return true;
}
bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con) {
    if (_post_handlers.find(path) == _post_handlers.end()) {
        return false;
    }

    _post_handlers[path](con);
    return true;
}