#include "ChatGrpcClient.h"
#include "RedisMgr.h"

ChatGrpcClient::ChatGrpcClient()
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

		_pools[cfg[word]["Name"]] = std::make_unique<ChatConPool>(5, cfg[word]["Host"], cfg[word]["Port"]);
	}

}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(const AddFriendReq& req)
{
	auto to_uid = req.touid();
	std::string uid_str = std::to_string(to_uid);

	AddFriendRsp rsp;
	rsp.set_error(ErrorCodes::RPCFailed);
	rsp.set_applyuid(req.applyuid());
	rsp.set_touid(to_uid);

	// 从 Redis 取目标用户所在聊天服名（uip_<uid> -> server_name）
	std::string server_name = "";
	auto key = USERIPPREFIX + uid_str;
	bool ok = RedisMgr::GetInstance()->Get(key, server_name);
	if (!ok || server_name.empty()) {
		rsp.set_error(ErrorCodes::UidInvalid);
		return rsp;
	}

	auto it = _pools.find(server_name);
	if (it == _pools.end() || !it->second) {
		return rsp;
	}

	auto stub = it->second->getConnection();
	if (!stub) {
		return rsp;
	}

	Defer defer([&]() {
		it->second->returnConnection(std::move(stub));
		});

	ClientContext context;
	Status status = stub->NotifyAddFriend(&context, req, &rsp);
	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPCFailed);
	}

	return rsp;
}
