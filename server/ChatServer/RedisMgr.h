#pragma once

#include "RedisConPool.h"
/*redis操作类，因为hredis提供的操作太别扭了，所以手动封装redis操作类，简化调用流程。后续*/

class RedisMgr : public Singleton<RedisMgr>,
    public std::enable_shared_from_this<RedisMgr>
{
    friend class Singleton<RedisMgr>;
public:
    ~RedisMgr();
    bool Get(const std::string& key, std::string& value);
    bool Set(const std::string& key, const std::string& value);
    bool LPush(const std::string& key, const std::string& value);
    bool LPop(const std::string& key, std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    bool RPop(const std::string& key, std::string& value);
    bool HSet(const std::string& key, const std::string& hkey, const std::string& value);
    bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
    std::string HGet(const std::string& key, const std::string& hkey);
    bool Del(const std::string& key);
    bool ExistsKey(const std::string& key);
    void Close();

    std::string acquireLock(const std::string& lockName,int lockTimeout, int acquireTimeout);
    bool releaseLock(const std::string& lockName,const std::string& identifier);
private:
    RedisMgr();

	std::unique_ptr<RedisConPool> _con_pool;
};
