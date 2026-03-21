#include "StatusConPool.h"

StatusConPool::StatusConPool(size_t poolSize, const std::string& host, const std::string& port)
	: b_stop_(false), poolSize_(poolSize), host_(host), port_(port) {
	for (size_t i = 0; i < poolSize_; ++i) {
		std::shared_ptr<Channel> channel = grpc::CreateChannel(
			host_ + ":" + port_,
			grpc::InsecureChannelCredentials());

		connections_.push(StatusService::NewStub(channel));
	}
}

StatusConPool::~StatusConPool() {
	std::lock_guard<std::mutex> lock(mutex_);
	Close();
	while (!connections_.empty()) {
		connections_.pop();
	}
}

std::unique_ptr<StatusService::Stub> StatusConPool::getConnection() {
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this]() {
		if (b_stop_) {
			return true;
		}
		return !connections_.empty();
		});

	if (b_stop_) {
		return nullptr;
	}

	auto context = std::move(connections_.front());
	connections_.pop();
	return context;
}

void StatusConPool::returnConnection(std::unique_ptr<StatusService::Stub> context) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop_ || context == nullptr) {
		return;
	}
	connections_.push(std::move(context));
	cond_.notify_one();
}

void StatusConPool::Close() {
	b_stop_ = true;
	cond_.notify_all();
}