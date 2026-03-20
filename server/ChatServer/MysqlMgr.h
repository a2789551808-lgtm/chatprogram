#pragma once
#include "const.h"
#include "MysqlDao.h"
#include "Singleton.h"
class MysqlMgr : public Singleton<MysqlMgr>
{
    friend class Singleton<MysqlMgr>;
public:
    ~MysqlMgr();
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);

    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name, const std::string& pwd);

    bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);

    bool TestProcedure(const std::string& email, int& uid, std::string& name);
private:
    MysqlMgr();
    MysqlDao  _dao;
};
