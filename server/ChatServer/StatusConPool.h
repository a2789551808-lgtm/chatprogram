#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"

using grpc::Channel;
using message::StatusService;

class StatusConPool {
public:
	StatusConPool(size_t poolSize, const std::string& host, const std::string& port);
	~StatusConPool();

	std::unique_ptr<StatusService::Stub> getConnection();
	void returnConnection(std::unique_ptr<StatusService::Stub> context);
	void Close();

private:
	std::atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	std::queue<std::unique_ptr<StatusService::Stub>> connections_;
	std::mutex mutex_;
	std::condition_variable cond_;
};