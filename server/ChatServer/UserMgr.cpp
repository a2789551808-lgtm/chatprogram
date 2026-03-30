#include "UserMgr.h"
#include "CSession.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
UserMgr:: ~UserMgr() {
	_uid_to_session.clear();
}

std::shared_ptr<CSession> UserMgr::GetSession(int uid)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	auto iter = _uid_to_session.find(uid);
	if (iter == _uid_to_session.end()) {
		return nullptr;
	}

	return iter->second;
}

void UserMgr::SetUserSession(int uid, std::shared_ptr<CSession> session)
{
	{
		std::lock_guard<std::mutex> lock(_session_mtx);
		_uid_to_session[uid] = session;
	}

	auto uid_str = std::to_string(uid);
	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	RedisMgr::GetInstance()->Set(USERIPPREFIX + uid_str, self_name);
}


bool UserMgr::RmvUserSession(int uid, const std::shared_ptr<CSession>& session)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	auto iter = _uid_to_session.find(uid);
	if (iter == _uid_to_session.end()) {
		return false;
	}

	// 会话已被新连接覆盖，旧连接不允许删除当前映射
	if (iter->second != session) {
		return false;
	}

	_uid_to_session.erase(iter);

	auto uid_str = std::to_string(uid);
	RedisMgr::GetInstance()->Del(USERIPPREFIX + uid_str);
	return true;
}

UserMgr::UserMgr()
{

}
