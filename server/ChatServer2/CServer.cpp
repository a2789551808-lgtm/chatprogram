#include "CServer.h"
#include <iostream>
#include "AsioIOServicePool.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"
CServer::CServer(boost::asio::io_context& io_context, short port) :_io_context(io_context), _port(port),
_acceptor(io_context, tcp::endpoint(tcp::v4(), port))
{
	auto& cfg = ConfigMgr::Inst();
	_server_name = cfg["SelfServer"]["Name"];

	cout << "Server start success, listen on port : " << _port << endl;
	StartAccept();
}

CServer::~CServer() {
	cout << "Server destruct listen on port : " << _port << endl;
}

void CServer::HandleAccept(shared_ptr<CSession> new_session, const boost::system::error_code& error) {
	if (!error) {
		new_session->Start();
		lock_guard<mutex> lock(_mutex);
		_sessions.insert(make_pair(new_session->GetSessionId(), new_session));
	}
	else {
		cout << "session accept failed, error is " << error.what() << endl;
	}

	StartAccept();
}

void CServer::StartAccept() {
	auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
	shared_ptr<CSession> new_session = make_shared<CSession>(io_context, this);
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, placeholders::_1));
}

void CServer::ClearSession(std::string session_id) {
	lock_guard<mutex> lock(_mutex);

	auto iter = _sessions.find(session_id);
	if (iter == _sessions.end()) {
		return;
	}
	// 删除用户会话关联关系
	if (_sessions.find(session_id) != _sessions.end()) {
		UserMgr::GetInstance()->RmvUserSession(_sessions[session_id]->GetUid());
	}
	auto session = iter->second;
	_sessions.erase(iter);

	// 仅对已登录会话做 -1
	if (session && session->IsAuthed()) {
		long long new_value = 0;
		bool ok = RedisMgr::GetInstance()->HIncrBy(LOGIN_COUNT, _server_name, -1, new_value);
		if (!ok) {
			std::cout << "decrement login count failed, server = " << _server_name << std::endl;
		}
	}
}
