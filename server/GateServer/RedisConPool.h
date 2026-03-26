#pragma once

#include "const.h"
#include "hiredis.h"
#include <queue>
#include <atomic>
#include <mutex>
#include "Singleton.h"
#include <cstring>

class RedisConPool {
public:
    RedisConPool(size_t poolSize, const char* host, int port, const char* pwd);
    ~RedisConPool();

    redisContext* getConnection();
    void returnConnection(redisContext* context);
    void Close();

private:
    std::atomic<bool> b_stop_;
    size_t poolSize_;
    const char* host_;
    int port_;
    std::queue<redisContext*> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
};