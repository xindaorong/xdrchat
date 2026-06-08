#pragma once
#include"const.h"

class HttpConnection:public std::enable_shared_from_this<HttpConnection>
{
	friend class LogicSystem;
public:
	HttpConnection(boost::asio::io_context& ioc);
	void Start();
	tcp::socket& GetSocket() {
		return _socket;
	}
private:
	void PreParseGetParam();
	void CheckDeadline();
	void WriteResponse();
	void HandleRequest();
	tcp::socket _socket;
	std::string _get_url;
	std::unordered_map<std::string, std::string> _get_params;
	//
	beast::flat_buffer _buffer{ 8192 };
   
	//
	http::request<http::dynamic_body> _request;

	//
	http::response<http::dynamic_body> _response;

	//
	net::steady_timer deadline_{
		_socket.get_executor(),//给定时器指定上下文
		std::chrono::seconds(60)
	};


};

