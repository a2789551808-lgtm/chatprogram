#pragma once

#include "const.h"
#include <vector>

class AsioIOServicePool : public Singleton<AsioIOServicePool>
{
    friend Singleton<AsioIOServicePool>;
public:
    using IOService = boost::asio::io_context;
    using Work = boost::asio::executor_work_guard<IOService::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;

    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

    // 使用 round-robin 的方式返回一个 io_service
    IOService& GetIOService();

	void Stop();    // 先释放 work guard，允许 io_context 在无任务时退出 run()，再主动 stop 每个 io_context（以防有阻塞等任务），最后 join 线程
private:
	AsioIOServicePool(std::size_t size = 2/*std::thread::hardware_concurrency()*/); //std::thread::hardware_concurrency()可以根据系统的CPU核心数自动设置线程池大小，但在某些环境下可能返回0，所以这里默认设置为2
    std::vector<IOService> _ioServices;
    std::vector<WorkPtr> _works;
    std::vector<std::thread> _threads;
    std::atomic<std::size_t> _nextIOService;    //轮询索引 _nextIOService 若非原子，在多线程环境下会产生数据竞态，GetIOService() 可能被并发调用，需要 std::atomic 或锁保护。
};