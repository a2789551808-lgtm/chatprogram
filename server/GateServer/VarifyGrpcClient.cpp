#include "VarifyGrpcClient.h"
#include "ConfigMgr.h"
// ===================== RPConPool =====================

RPConPool::RPConPool(size_t poolSize, std::string host, std::string port)
    : poolSize_(poolSize), host_(host), port_(port), b_stop_(false)
{
    for (size_t i = 0; i < poolSize_; ++i) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(
            host + ":" + port, grpc::InsecureChannelCredentials());
        connections_.push(VarifyService::NewStub(channel));
    }
}

RPConPool::~RPConPool()
{
    std::lock_guard<std::mutex> lock(mutex_);
    Close();
    while (!connections_.empty()) {
        connections_.pop();
    }
}

std::unique_ptr<VarifyService::Stub> RPConPool::getConnection()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] {
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

void RPConPool::returnConnection(std::unique_ptr<VarifyService::Stub> context)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (b_stop_) {
        return;
    }
    connections_.push(std::move(context));
    cond_.notify_one();
}

void RPConPool::Close()
{
    b_stop_ = true;
    cond_.notify_all();
}


// ===================== VerifyGrpcClient =====================

VerifyGrpcClient::VerifyGrpcClient()
{
    auto& gCfgMgr = ConfigMgr::Inst();
    std::string host = gCfgMgr["VarifyServer"]["Host"];
    std::string port = gCfgMgr["VarifyServer"]["Port"];
    pool_.reset(new RPConPool(5, host, port));
}

GetVarifyRsp VerifyGrpcClient::GetVarifyCode(std::string email)
{
    ClientContext context;
    GetVarifyRsp reply;
    GetVarifyReq request;
    request.set_email(email);
	auto stub = pool_->getConnection();
    Status status = stub->GetVarifyCode(&context, request, &reply);

    if (status.ok()) {
		pool_->returnConnection(std::move(stub));
        return reply;
    }
    else {
        reply.set_error(ErrorCodes::RPCFailed);
        pool_->returnConnection(std::move(stub));
        return reply;
    }
}










/*
结论先行
?	是的，RPConPool 的目的是维护一组可复用的 VarifyService::Stub（每个 stub 绑定一个 channel），通过 getConnection / returnConnection 控制并发调用数量并复用资源。
?	但注意：在 gRPC C++ 中，单个 Stub/Channel 本身通常可以被多个线程并发使用（对普通 unary RPC 来说是安全的），所以没有池也能发起多个并发 RPC。池的价值在于控制并发、降低连接/资源开销或为特殊场景提供隔离。
详细说明（要点）
?	单个 Stub/Channel 行为
?	gRPC 的 Channel 是线程安全的，多个线程可以使用同一个 Stub 发起并发的普通（unary）RPC 调用（每个调用用自己的 ClientContext）。
?	所以“没有池就只有一个 RPC 在跑”这种理解不准确：没有池也能并发，只要线程各自调用 RPC。
?	为什么还会用 RPConPool（常见原因）
1.	控制并发量：通过池大小限制同时在外部服务上发起的并发请求数，避免把后端压垮或自己占用过多资源。
2.	多 channel 分流/性能：创建多个 channel（可能映射到不同 TCP 连接）可以在某些网络/负载场景下提高吞吐或避免单连接瓶颈。
3.	资源复用与开销控制：创建 channel/stub 可能有开销，池令你复用已创建的连接而不是频繁创建销毁。
4.	隔离：某些 RPC（如长期的 streaming RPC）可能会与短时请求互相干扰，给它们分配独立的 stub 更安全。
5.	与非线程安全代码兼容：如果你的包装层或某些状态不是线程安全，池可以把并发访问序列化到有限的实例上。
6.	使用同步 API 且想限制并发时比直接共享 stub 更直观（用 getConnection 阻塞等待空闲 stub）。
?	代码中池的行为（如何实现上面目的）
?	poolSize_ 决定同时可用的 stub 数量 —— 超过的线程会在 getConnection() 里等待（cond_.wait）。
?	线程拿到 stub 后负责调用 RPC，完成后通过 returnConnection() 把 stub 放回，唤醒等待者。
?	Close() 用来安全停止并唤醒所有等待线程。
?	何时可以移除池
?	如果你的 RPC 都是简单的短请求（unary），后端能承受高并发，并且你不需要单独的 channel 分流或并发限制，那么直接共享一个 Stub 是更简单且常见的做法。
?	如果需要控制并发、做限流、或者遇到性能问题或流/认证隔离要求，则保留或使用池/信号量更合适。
替代方案（简短）
?	如果只是限制并发，可以用 std::counting_semaphore / std::atomic + 条件变量 或 std::mutex + 计数器来限制同时在飞的请求数，而不必为每个“slot”创建独立 channel。
?	若需要高吞吐且复杂并发，考虑 gRPC 的异步 API（completion queue）配合线程池。
如果需要，我可以：
?	根据你的使用场景（unary vs streaming、是否需要限流）给出推荐（保留单 stub / 建池 / 用异步）；
?	或把 VerifyGrpcClient 改成用连接池并演示一个使用示例。

*/