#include "ChatConPool.h"
#include <grpcpp/grpcpp.h> // 确保引入了头文件

ChatConPool::ChatConPool(size_t poolSize, std::string host, std::string port) 
	: poolSize_(poolSize), host_(host), port_(port), b_stop_(false)
{
	for (size_t i = 0; i < poolSize_; ++i) {
		grpc::ChannelArguments args;
		//关闭全局 Subchannel Pool，强行要求每个 Channel 建立独立的底层 TCP 连接
		args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);

		// 使用 CreateCustomChannel，将 args 传进去
		std::shared_ptr<Channel> channel = grpc::CreateCustomChannel(
			host + ":" + port, grpc::InsecureChannelCredentials(), args);

		connections_.push(ChatService::NewStub(channel));
	}
}

ChatConPool::~ChatConPool() {
	std::lock_guard<std::mutex> lock(mutex_);
	Close();
	while (!connections_.empty()) {
		connections_.pop();
	}
}

std::unique_ptr<ChatService::Stub> ChatConPool::getConnection() {
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this] {
		if (b_stop_) {
			return true;
		}
		return !connections_.empty();
		});
	//如果停止则直接返回空指针
	if (b_stop_) {
		return  nullptr;
	}
	auto context = std::move(connections_.front());
	connections_.pop();
	return context;
}

void ChatConPool::returnConnection(std::unique_ptr<ChatService::Stub> context) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop_) {
		return;
	}
	connections_.push(std::move(context));
	cond_.notify_one();
}

void ChatConPool::Close() {
	b_stop_ = true;
	cond_.notify_all();
}