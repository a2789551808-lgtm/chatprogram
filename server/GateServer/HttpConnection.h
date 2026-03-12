#pragma once

#include "const.h"

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
	friend class LogicSystem;   //方便LogicSystem在处理请求时访问HttpConnection的私有成员
public:
    HttpConnection(boost::asio::io_context& ioc);
    void Start();
    tcp::socket& GetSocket();

private:
	void CheckDeadline();   //在go或者java里直接会有封装好的超时检测，但是c++里需要自己实现一个定时器来检测连接的超时
	void WriteResponse();   //应答请求
	void HandleReq();   //处理请求
    void PreParseGetParam();    //

    tcp::socket  _socket;
    // The buffer for performing reads.
    beast::flat_buffer  _buffer{ 8192 };

    // The request message.
    http::request<http::dynamic_body> _request;

    // The response message.
    http::response<http::dynamic_body> _response;

    // The timer for putting a deadline on connection processing.
    net::steady_timer deadline_{
        _socket.get_executor(), std::chrono::seconds(60) };

    std::string _get_url;
    std::unordered_map<std::string, std::string> _get_params;

};
