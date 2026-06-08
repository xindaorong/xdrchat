#include "HttpConnection.h"
#include "LogicSystem.h"
#include <utility>
#include <iostream>

using namespace std;

HttpConnection::HttpConnection(boost::asio::io_context& ioc)
    : _socket(ioc) {
}

// Convert a value in [0, 15] to its hex character.
unsigned char ToHex(unsigned char x)
{
    return x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}

std::string UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        // Keep unreserved URL characters as-is.
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~'))
        {
            strTemp += str[i];
        }
        else if (str[i] == ' ')
        {
            // Encode spaces as '+'.
            strTemp += "+";
        }
        else
        {
            // Percent-encode all other characters.
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] & 0x0F);
        }
    }
    return strTemp;
}

std::string UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        // Decode '+' back to space.
        if (str[i] == '+')
        {
            strTemp += ' ';
        }
        // Decode %xx escape sequences.
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high * 16 + low;
        }
        else
        {
            strTemp += str[i];
        }
    }
    return strTemp;
}

void HttpConnection::PreParseGetParam()
{
    // Split the URI into path and query string, e.g. /get_test?key1=value&key2=value2
    auto uri = _request.target();

    // '?' separates the path from the query string.
    auto query_pos = uri.find('?');
    if (query_pos == std::string::npos)
    {
        _get_url = uri;
        return;
    }

    _get_url = uri.substr(0, query_pos);
    std::string query_string = uri.substr(query_pos + 1);
    std::string key;
    std::string value;
    size_t pos = 0;

    while ((pos = query_string.find('&')) != std::string::npos)
    {
        auto pair = query_string.substr(0, pos);
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos)
        {
            key = UrlDecode(pair.substr(0, eq_pos)); // Decode the key before storing it.
            value = UrlDecode(pair.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        query_string.erase(0, pos + 1);
    }

    // Handle the final key=value pair after the loop ends.
    if (!query_string.empty())
    {
        size_t eq_pos = query_string.find('=');
        if (eq_pos != std::string::npos)
        {
            key = UrlDecode(query_string.substr(0, eq_pos));
            value = UrlDecode(query_string.substr(eq_pos + 1));
            _get_params[key] = value;
        }
    }
}

void HttpConnection::Start()
{
    auto self = shared_from_this();
    http::async_read(_socket, _buffer, _request, [self](beast::error_code ec,
        std::size_t bytes_transferred)
    {
        try
        {
            if (ec)
            {
                std::cout << "http read err is " << ec.what() << std::endl;
                return;
            }

            // bytes_transferred is intentionally unused here.
            boost::ignore_unused(bytes_transferred);
            self->HandleRequest();
            self->CheckDeadline();
        }
        catch (std::exception& exp)
        {
            std::cout << "exception is " << exp.what() << std::endl;
        }
    });
}

void HttpConnection::CheckDeadline()
{
    // Start an async wait so we can close the socket when the deadline expires.
    auto self = shared_from_this();
    deadline_.async_wait([self](beast::error_code ec)
    {
        if (!ec)
        {
            // Close the socket to cancel any outstanding operation.
            self->_socket.close(ec);
        }
    });
}

void HttpConnection::HandleRequest()
{
    // Set response metadata.
    _response.version(_request.version());
    _response.keep_alive(false);

    if (_request.method() == http::verb::get)
    {
        PreParseGetParam();
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
        if (!success)
        {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "url Not Found\r\n";
            WriteResponse();
            return;
        }

        _response.result(http::status::ok);
        _response.set(http::field::content_type, "Gateserver");
        WriteResponse();
        return;
    }
        if (_request.method() == http::verb::post) {
            bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
            if (!success) {
                _response.result(http::status::not_found);
                _response.set(http::field::content_type, "text/plain");
                beast::ostream(_response.body()) << "url not found\r\n";
                WriteResponse();
                return;
            }

            _response.result(http::status::ok);
            _response.set(http::field::server, "GateServer");
            WriteResponse();
            return;
        }

    
    _response.result(http::status::method_not_allowed);
    _response.set(http::field::content_type, "text/plain");
    beast::ostream(_response.body()) << "Method Not Allowed\r\n";
    WriteResponse();
    
}

void HttpConnection::WriteResponse()
{
    auto self = shared_from_this();
    _response.content_length(_response.body().size());
    http::async_write(_socket, _response, [self](beast::error_code ec, std::size_t)
    {
        self->_socket.shutdown(tcp::socket::shutdown_send, ec);
        self->deadline_.cancel();
    });
}
