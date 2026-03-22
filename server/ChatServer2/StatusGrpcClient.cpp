#include "StatusGrpcClient.h"

#include "ConfigMgr.h"
#include "StatusConPool.h"
#include "const.h"

StatusGrpcClient::StatusGrpcClient()
{
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host = gCfgMgr["StatusServer"]["Host"];
	std::string port = gCfgMgr["StatusServer"]["Port"];
	pool_.reset(new StatusConPool(5, host, port));
}

StatusGrpcClient::~StatusGrpcClient()
{
	if (pool_) {
		pool_->Close();
	}
}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
	ClientContext context;
	GetChatServerRsp reply;
	GetChatServerReq request;
	request.set_uid(uid);

	auto stub = pool_->getConnection();
	if (!stub) {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}

	Defer defer([&stub, this]() {
		pool_->returnConnection(std::move(stub));
		});

	Status status = stub->GetChatServer(&context, request, &reply);
	if (status.ok()) {
		return reply;
	}

	reply.set_error(ErrorCodes::RPCFailed);
	return reply;
}

LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
	ClientContext context;
	LoginRsp reply;
	LoginReq request;
	request.set_uid(uid);
	request.set_token(token);

	auto stub = pool_->getConnection();
	if (!stub) {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}

	Defer defer([&stub, this]() {
		pool_->returnConnection(std::move(stub));
		});

	Status status = stub->Login(&context, request, &reply);
	if (status.ok()) {
		return reply;
	}

	reply.set_error(ErrorCodes::RPCFailed);
	return reply;
}