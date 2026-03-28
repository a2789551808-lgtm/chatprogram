#include <boost/asio.hpp>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

static const uint16_t ID_HEART_BEAT_REQ = 1023;
static const uint16_t ID_HEARTBEAT_RSP = 1024;

struct Options {
	std::string host = "127.0.0.1";
	int port = 8090;
	int threads = 8;
	int connections = 2000;
	int durationSec = 60;
	int intervalMs = 1000;
	std::string mode = "connect"; // connect | ping
};

struct Stats {
	std::atomic<long long> connectOk{ 0 };
	std::atomic<long long> connectFail{ 0 };
	std::atomic<long long> pingOk{ 0 };
	std::atomic<long long> pingFail{ 0 };

	std::mutex latMtx;
	std::vector<double> latMs;
};

static uint16_t read_be16(const unsigned char* p) {
	return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static void write_be16(unsigned char* p, uint16_t v) {
	p[0] = static_cast<unsigned char>((v >> 8) & 0xFF);
	p[1] = static_cast<unsigned char>(v & 0xFF);
}

static bool send_packet(tcp::socket& sock, uint16_t msgId, const std::string& body) {
	unsigned char hdr[4];
	write_be16(hdr, msgId);
	write_be16(hdr + 2, static_cast<uint16_t>(body.size()));
	boost::system::error_code ec;
	boost::asio::write(sock, boost::asio::buffer(hdr, 4), ec);
	if (ec) return false;
	if (!body.empty()) {
		boost::asio::write(sock, boost::asio::buffer(body.data(), body.size()), ec);
		if (ec) return false;
	}
	return true;
}

static bool wait_available(tcp::socket& sock, std::size_t n, int timeoutMs) {
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	boost::system::error_code ec;
	while (std::chrono::steady_clock::now() < deadline) {
		std::size_t avail = sock.available(ec);
		if (ec) return false;
		if (avail >= n) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return false;
}

static bool recv_packet(tcp::socket& sock, uint16_t& msgId, std::string& body, int timeoutMs) {
	if (!wait_available(sock, 4, timeoutMs)) return false;

	unsigned char hdr[4];
	boost::system::error_code ec;
	boost::asio::read(sock, boost::asio::buffer(hdr, 4), ec);
	if (ec) return false;

	msgId = read_be16(hdr);
	uint16_t len = read_be16(hdr + 2);

	if (len > 0) {
		if (!wait_available(sock, len, timeoutMs)) return false;
		body.resize(len);
		boost::asio::read(sock, boost::asio::buffer(&body[0], len), ec);
		if (ec) return false;
	}
	else {
		body.clear();
	}
	return true;
}

static void worker(int idx, const Options& opt, int connCount, Stats& st) {
	boost::asio::io_context ioc;
	tcp::endpoint ep(boost::asio::ip::make_address(opt.host), static_cast<unsigned short>(opt.port));

	std::vector<std::unique_ptr<tcp::socket>> sockets;
	sockets.reserve(connCount);

	for (int i = 0; i < connCount; ++i) {
		std::unique_ptr<tcp::socket> s(new tcp::socket(ioc));
		boost::system::error_code ec;
		s->connect(ep, ec);
		if (ec) {
			st.connectFail.fetch_add(1);
			continue;
		}
		s->set_option(tcp::no_delay(true), ec);
		sockets.push_back(std::move(s));
		st.connectOk.fetch_add(1);
	}

	auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(opt.durationSec);

	if (opt.mode == "connect") {
		while (std::chrono::steady_clock::now() < endTime) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		return;
	}

	while (std::chrono::steady_clock::now() < endTime) {
		for (std::size_t i = 0; i < sockets.size(); ++i) {
			auto& s = *sockets[i];
			std::string payload = "{\"ping\":1}";
			auto t1 = std::chrono::steady_clock::now();
			if (!send_packet(s, ID_HEART_BEAT_REQ, payload)) {
				st.pingFail.fetch_add(1);
				continue;
			}
			uint16_t rid = 0;
			std::string rsp;
			if (!recv_packet(s, rid, rsp, 2000) || rid != ID_HEARTBEAT_RSP) {
				st.pingFail.fetch_add(1);
				continue;
			}
			auto t2 = std::chrono::steady_clock::now();
			double ms = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.0;
			st.pingOk.fetch_add(1);
			{
				std::lock_guard<std::mutex> g(st.latMtx);
				st.latMs.push_back(ms);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(opt.intervalMs));
	}
}

static void print_latency(Stats& st) {
	std::vector<double> v;
	{
		std::lock_guard<std::mutex> g(st.latMtx);
		v = st.latMs;
	}
	if (v.empty()) {
		std::cout << "no latency samples\n";
		return;
	}
	std::sort(v.begin(), v.end());
	auto pct = [&](double p) {
		std::size_t idx = static_cast<std::size_t>(p * (v.size() - 1));
		return v[idx];
		};
	double avg = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
	std::cout << "Latency(ms): avg=" << avg
		<< " p50=" << pct(0.50)
		<< " p95=" << pct(0.95)
		<< " p99=" << pct(0.99) << "\n";
}

static void parse_args(int argc, char* argv[], Options& opt) {
	for (int i = 1; i + 1 < argc; i += 2) {
		std::string k = argv[i];
		std::string v = argv[i + 1];
		if (k == "--host") opt.host = v;
		else if (k == "--port") opt.port = std::atoi(v.c_str());
		else if (k == "--threads") opt.threads = std::atoi(v.c_str());
		else if (k == "--connections") opt.connections = std::atoi(v.c_str());
		else if (k == "--duration") opt.durationSec = std::atoi(v.c_str());
		else if (k == "--interval") opt.intervalMs = std::atoi(v.c_str());
		else if (k == "--mode") opt.mode = v;
	}
}

int main(int argc, char* argv[]) {
	Options opt;
	parse_args(argc, argv, opt);

	std::cout << "host=" << opt.host
		<< " port=" << opt.port
		<< " threads=" << opt.threads
		<< " connections=" << opt.connections
		<< " duration=" << opt.durationSec
		<< " mode=" << opt.mode << std::endl;

	Stats st;
	std::vector<std::thread> ts;
	ts.reserve(opt.threads);

	int base = opt.connections / opt.threads;
	int rem = opt.connections % opt.threads;

	for (int i = 0; i < opt.threads; ++i) {
		int cnt = base + (i < rem ? 1 : 0);
		ts.emplace_back(worker, i, std::ref(opt), cnt, std::ref(st));
	}
	for (std::size_t i = 0; i < ts.size(); ++i) {
		ts[i].join();
	}

	std::cout << "connect_ok=" << st.connectOk.load()
		<< " connect_fail=" << st.connectFail.load()
		<< " ping_ok=" << st.pingOk.load()
		<< " ping_fail=" << st.pingFail.load()
		<< std::endl;

	if (opt.mode == "ping") {
		print_latency(st);
	}
	return 0;
}