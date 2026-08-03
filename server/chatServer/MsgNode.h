#pragma once
#include<string>
#include"const.h"
#include<iostream>
#include<boost/asio.hpp>
using boost::asio::ip::tcp;
class LogicSystem;
class MsgNode
{
public:
	MsgNode(short max_len) :_total_len(max_len), _cur_len(0)
	{
		_data = new char[_total_len + 1]();
		_data[_total_len] = '\0';
	}
	~MsgNode() {
		std::cout << "destruct MsgNode" << std::endl;
		delete[] _data;
	}

	void Clear() {
		::memset(_data, 0, _total_len);
		_cur_len = 0;
	}

	int _total_len;
	int _cur_len;
	char* _data;
};

//存放接收消息体id
class RecvNode :public MsgNode {
	friend class LogicSystem;
public:
	RecvNode(short max_len, short msg_id);
private:
	short _msg_id;
};

//存放发送消息体id
class SendNode :public MsgNode {
	friend class LogicSystem;
public:
	SendNode(const char* msg, short max_len, short msg_id);
private:
	short _msg_id;
};
