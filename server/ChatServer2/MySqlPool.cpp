#include "MySqlPool.h"

#include <chrono>
#include <iostream>

SqlConnection::SqlConnection(sql::Connection* con, int64_t lasttime)
	:_con(con), _last_oper_time(lasttime) {
}

MySqlPool::MySqlPool(const std::string& url, const std::string& user, const std::string& pass, const std::string& schema, int poolSize)
	: url_(url), user_(user), pass_(pass), schema_(schema), poolSize_(poolSize), b_stop_(false), _fail_count(0) {
	try {
		for (int i = 0; i < poolSize_; ++i) {
			sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
			auto* con = driver->connect(url_, user_, pass_);
			con->setSchema(schema_);

			auto currentTime = std::chrono::system_clock::now().time_since_epoch();
			long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
			pool_.push(std::make_unique<SqlConnection>(con, timestamp));
		}

		_check_thread = std::thread([this]() {
			while (!b_stop_) {
				checkConnectionPro();

				std::unique_lock<std::mutex> lock(mutex_);
				cond_.wait_for(lock, std::chrono::seconds(60), [this]() {
					return b_stop_.load();
				});
			}
		});
	}
	catch (sql::SQLException& e) {
		std::cout << "mysql pool init failed, error is " << e.what() << std::endl;
	}
}

void MySqlPool::checkConnectionPro() {
	size_t targetCount;
	{
		std::lock_guard<std::mutex> guard(mutex_);
		targetCount = pool_.size();
	}

	size_t processed = 0;
	auto now = std::chrono::system_clock::now().time_since_epoch();
	long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

	while (processed < targetCount) {
		std::unique_ptr<SqlConnection> con;
		{
			std::lock_guard<std::mutex> guard(mutex_);
			if (pool_.empty()) {
				break;
			}
			con = std::move(pool_.front());
			pool_.pop();
		}

		bool healthy = true;
		if (timestamp - con->_last_oper_time >= 5) {
			try {
				std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
				stmt->executeQuery("SELECT 1");
				con->_last_oper_time = timestamp;
			}
			catch (sql::SQLException& e) {
				std::cout << "Error keeping connection alive: " << e.what() << std::endl;
				healthy = false;
				_fail_count++;
			}
		}

		if (healthy) {
			std::lock_guard<std::mutex> guard(mutex_);
			pool_.push(std::move(con));
			cond_.notify_one();
		}

		++processed;
	}

	while (_fail_count > 0) {
		if (reconnect(timestamp)) {
			_fail_count--;
		}
		else {
			break;
		}
	}
}

bool MySqlPool::reconnect(long long timestamp) {
	try {
		sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
		auto* con = driver->connect(url_, user_, pass_);
		con->setSchema(schema_);

		auto newCon = std::make_unique<SqlConnection>(con, timestamp);
		{
			std::lock_guard<std::mutex> guard(mutex_);
			pool_.push(std::move(newCon));
		}

		std::cout << "mysql connection reconnect success" << std::endl;
		return true;
	}
	catch (sql::SQLException& e) {
		std::cout << "Reconnect failed, error is " << e.what() << std::endl;
		return false;
	}
}

std::unique_ptr<SqlConnection> MySqlPool::getConnection() {
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this]() {
		if (b_stop_) {
			return true;
		}
		return !pool_.empty();
	});

	if (b_stop_) {
		return nullptr;
	}

	std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
	pool_.pop();
	return con;
}

void MySqlPool::returnConnection(std::unique_ptr<SqlConnection> con) {
	std::unique_lock<std::mutex> lock(mutex_);
	if (b_stop_) {
		return;
	}
	pool_.push(std::move(con));
	cond_.notify_one();
}

void MySqlPool::Close() {
	b_stop_ = true;
	cond_.notify_all();
}

MySqlPool::~MySqlPool() {
	Close();

	if (_check_thread.joinable()) {
		_check_thread.join();
	}

	std::unique_lock<std::mutex> lock(mutex_);
	while (!pool_.empty()) {
		pool_.pop();
	}
}