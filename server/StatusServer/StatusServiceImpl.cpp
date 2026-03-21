#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include <climits>
#include <limits>

std::string generate_unique_string() {
    // 创建UUID对象
    boost::uuids::random_generator gen;
    boost::uuids::uuid uuid = gen();

    // 将UUID转换为字符串
    std::string unique_string = boost::uuids::to_string(uuid);

    return unique_string;
}

Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* reply)
{
	std::string prefix("llfc status server has received :  ");
	const auto& server = getChatServer();

	// 无可用聊天服（例如配置为空）
	if (server.name.empty()) {
		reply->set_error(ErrorCodes::RPCFailed);
		return Status::OK;
	}

	reply->set_host(server.host);
	reply->set_port(server.port);
	reply->set_error(ErrorCodes::Success);
	reply->set_token(generate_unique_string());
	insertToken(request->uid(), reply->token());
	return Status::OK;
}

StatusServiceImpl::StatusServiceImpl()
{
	auto& cfg = ConfigMgr::Inst();
	auto server_list = cfg["chatservers"]["Name"];

	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;

	while (std::getline(ss, word, ',')) {
		words.push_back(word);
	}

	for (auto& word : words) {
		if (cfg[word]["Name"].empty()) {
			continue;
		}

		ChatServer server;
		server.port = cfg[word]["Port"];
		server.host = cfg[word]["Host"];
		server.name = cfg[word]["Name"];
		_servers[server.name] = server;

		// 初始化 Redis 连接计数字段（不存在时置 0）
		auto cnt = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.name);
		if (cnt.empty()) {
			RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server.name, "0");
		}
	}
}

ChatServer StatusServiceImpl::getChatServer() {
	std::lock_guard<std::mutex> guard(_server_mtx);

	// 防止空 map 解引用崩溃
	if (_servers.empty()) {
		return ChatServer();
	}

	ChatServer minServer;
	long long minCount = std::numeric_limits<long long>::max();
	bool hasCandidate = false;

	// 最小连接数策略：遍历所有 chat server，选择 LOGIN_COUNT 最小者
	for (const auto& kv : _servers) {
		const ChatServer& server = kv.second;
		auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.name);

		// Redis 中不存在计数时，视为高负载，避免优先选中未初始化/异常节点
		long long curCount = std::numeric_limits<long long>::max();
		if (!count_str.empty()) {
			try {
				curCount = std::stoll(count_str);
			}
			catch (...) {
				curCount = std::numeric_limits<long long>::max();
			}
		}

		if (!hasCandidate || curCount < minCount) {
			minCount = curCount;
			minServer = server;
			hasCandidate = true;
		}
	}

	// 理论上 hasCandidate 必定为 true；这里保底回退
	if (!hasCandidate) {
		return _servers.begin()->second;
	}

	return minServer;
}

Status StatusServiceImpl::Login(ServerContext* context, const LoginReq* request, LoginRsp* reply)
{
	auto uid = request->uid();
	auto token = request->token();

	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);

	// Redis中不存在该uid对应token，或者读取失败
	if (!success) {
		reply->set_error(ErrorCodes::UidInvalid);
		return Status::OK;
	}

	// token不匹配
	if (token_value != token) {
		reply->set_error(ErrorCodes::TokenInvalid);
		return Status::OK;
	}

	reply->set_error(ErrorCodes::Success);
	reply->set_uid(uid);
	reply->set_token(token);
	return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, std::string token)
{
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	RedisMgr::GetInstance()->Set(token_key, token);
}

