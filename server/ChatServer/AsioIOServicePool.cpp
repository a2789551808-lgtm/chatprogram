#include "AsioIOServicePool.h"
#include <iostream>

// AsioIOServicePool 构造函数
// size 表示要创建的 io_context（IO 服务）数量，每个 io_context 通常由一个线程运行
AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : _ioServices(size),   // 构造 size 个 io_context 实例并存入向量
    _works(size),          // 构造 size 个 unique_ptr<Work>（初始为 nullptr）
    _nextIOService(0)      // 原子计数器，轮询索引用，初始为 0
{
    // 为每个 io_context 创建一个 executor_work_guard（keep-alive guard）
    // guard 的作用是防止对应的 io_context 在没有任务时立即退出 run()
    for (std::size_t i = 0; i < size; ++i) {
        // 使用 io_context 的执行器（executor）来构造 guard
        // 注意：传入的是执行器（轻量对象），不会拷贝 io_context 本体
        _works[i] = std::make_unique<Work>(_ioServices[i].get_executor());
    }

    // 为每个 io_context 创建并启动一个线程，线程函数为该 io_context 的 run()
    // 每个线程会阻塞在 run() 上，等待并处理该 io_context 的异步事件
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        // emplace_back 直接在 _threads 向量末尾构造 std::thread
        // 捕获列表 [this, i]：按指针捕获 this（访问成员），按值捕获 i（索引）
        _threads.emplace_back([this, i]() {
            _ioServices[i].run(); // 事件循环，直到 io_context 被 stop() 或没有工作项且 guard 被 reset()
            });
    }
}

// 析构函数：调用 Stop() 做优雅停机，然后输出调试信息
AsioIOServicePool::~AsioIOServicePool() {
    Stop();
    std::cout << "AsioIOServicePool destruct" << std::endl;
}

// 轮询返回一个 io_context 的引用，供外部（例如 accept 时）将 socket 绑定到该 io_context
boost::asio::io_context& AsioIOServicePool::GetIOService() {
    // 使用原子自增获取索引，fetch_add 返回的是“增加前”的旧值（pre-increment value）
    // 使用 memory_order_relaxed 表示我们只需要原子性，不需要跨线程的严格内存序
    // 取模 _ioServices.size() 将原子值映射到合法索引范围
    auto idx = _nextIOService.fetch_add(1, std::memory_order_relaxed) % _ioServices.size();
    return _ioServices[idx];
}

// 停止所有 io_context 并等待线程退出
void AsioIOServicePool::Stop() {
    // 先释放所有 executor_work_guard（reset），允许对应的 io_context 在任务完成后自然退出 run()
    for (auto& guard : _works) {
        if (guard) guard->reset();
    }
    // 为了确保能强制退出（例如有阻塞或长期等待的任务），显式调用每个 io_context 的 stop()
    // stop() 会使对应的 run() 尽快返回
    for (auto& io : _ioServices) {
        io.stop();
    }
    // 等待所有线程结束并回收资源：join() 会阻塞直到线程函数返回
    for (auto& t : _threads) {
        if (t.joinable()) t.join();
    }
}