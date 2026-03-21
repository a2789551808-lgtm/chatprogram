#pragma once

#include "const.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/mysql_driver.h>

class SqlConnection {
public:
	SqlConnection(sql::Connection* con, int64_t lasttime);

	std::unique_ptr<sql::Connection> _con;
	int64_t _last_oper_time;
};

class MySqlPool {
public:
	MySqlPool(const std::string& url, const std::string& user, const std::string& pass, const std::string& schema, int poolSize);
	~MySqlPool();

	void checkConnectionPro();
	bool reconnect(long long timestamp);

	std::unique_ptr<SqlConnection> getConnection();
	void returnConnection(std::unique_ptr<SqlConnection> con);
	void Close();

private:
	std::string url_;
	std::string user_;
	std::string pass_;
	std::string schema_;
	int poolSize_;

	std::queue<std::unique_ptr<SqlConnection>> pool_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::atomic<bool> b_stop_;
	std::thread _check_thread;
	std::atomic<int> _fail_count;
};