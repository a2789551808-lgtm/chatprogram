#pragma once

#include "data.h"

#include <memory>
#include <string>

class MySqlPool;

class MysqlDao
{
public:
	MysqlDao();
	~MysqlDao();

	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);
	bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
	std::shared_ptr<UserInfo> GetUser(int uid);

private:
	std::unique_ptr<MySqlPool> pool_;
};