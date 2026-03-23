#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "ChatServiceImpl.h"
bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

int main()
{
    auto& cfg = ConfigMgr::Inst();
    std::string self_name = cfg["SelfServer"]["Name"];
    try {
        auto pool = AsioIOServicePool::GetInstance();
		//初始化登录服务器的在线人数为0，后续登录登出时会在这个基础上增减
        RedisMgr::GetInstance()->HSet(LOGIN_COUNT, self_name, "0");

		//初始化RPC服务器
        std::string server_address(cfg["SelfServer"]["Host"] + ":" + cfg["SelfServer"]["RPCPort"]);
        ChatServiceImpl service;
        grpc::ServerBuilder builder;
        // 监听端口和添加服务
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        // 构建并启动gRPC服务器
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cout << "RPC Server listening on " << server_address << std::endl;
        //单独启动一个线程处理grpc服务
        std::thread  grpc_server_thread([&server]() {
            server->Wait();
            });

        boost::asio::io_context  io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, pool, &server](auto, auto) {
            io_context.stop();
            pool->Stop();
            server->Shutdown();
            });
        auto port_str = cfg["SelfServer"]["Port"];
        CServer s(io_context, atoi(port_str.c_str()));
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << endl;

        // 异常退出：从 LOGIN_COUNT 中移除当前节点，避免被状态服务继续选中
        bool ok = RedisMgr::GetInstance()->HDel(LOGIN_COUNT, self_name);
        if (!ok) {
            std::cerr << "HDel failed, key=" << LOGIN_COUNT << ", field=" << self_name << endl;
        }

        RedisMgr::GetInstance()->Close();
    }

}