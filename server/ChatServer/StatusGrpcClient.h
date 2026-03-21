#pragma once

#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "Singleton.h"
#include "message.grpc.pb.h"
#include "message.pb.h"

using grpc::ClientContext;
using grpc::Status;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;

class StatusConPool;

class StatusGrpcClient : public Singleton<StatusGrpcClient>
{
	friend class Singleton<StatusGrpcClient>;

public:
	~StatusGrpcClient();

	GetChatServerRsp GetChatServer(int uid);
	LoginRsp Login(int uid, std::string token);

private:
	StatusGrpcClient();
	std::unique_ptr<StatusConPool> pool_;
};