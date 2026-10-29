// Integration tests for the host TAK byte transport (PosixTlsTransport, twin of the Unreal
// FOneMindUeTlsTransport):
//  - Close() during a connect to an unreachable address returns within about 50 ms instead
//    of the OS connect timeout, and no listener call follows;
//  - a peer that accepts but never reads fails the connection once the outbound high-water
//    mark is passed, instead of letting the queue grow without limit.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PosixTlsTransport.h"
#include "TestHarness.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <mutex>
#include <string>
#include <thread>

namespace {

struct Recorder : public om::ITransport::Listener {
  std::mutex mu;
  bool opened = false, closed = false;
  std::string reason;
  void OnTransportOpen() override {
    std::lock_guard<std::mutex> l(mu);
    opened = true;
  }
  void OnTransportData(const char*, size_t) override {}
  void OnTransportClosed(const std::string& r) override {
    std::lock_guard<std::mutex> l(mu);
    closed = true;
    reason = r;
  }
};

long long MsSince(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
}

}  // namespace

TEST(ItTransportCloseDuringConnectIsPrompt) {
  // 192.0.2.1 is TEST-NET-1 (RFC 5737): never routed, so the SYN goes unanswered (or the OS
  // reports unreachable at once; either way Close must not wait).
  Recorder rec;
  omhost::PosixTlsTransport t(omtls::TlsConfig(), 60000);
  t.SetListener(&rec);
  EXPECT_TRUE(t.Open("tcp://192.0.2.1:9"));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  auto start = std::chrono::steady_clock::now();
  t.Close();
  long long ms = MsSince(start);
  std::printf("    Close() during connect returned in %lld ms\n", ms);
  EXPECT_TRUE(ms < 1000);
  std::lock_guard<std::mutex> l(rec.mu);
  EXPECT_FALSE(rec.opened);
}

TEST(ItTransportConnectDeadline) {
  Recorder rec;
  omhost::PosixTlsTransport t(omtls::TlsConfig(), 300);
  t.SetListener(&rec);
  auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(t.Open("tcp://192.0.2.1:9"));
  bool closed = false;
  while (MsSince(start) < 3000 && !closed) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::lock_guard<std::mutex> l(rec.mu);
    closed = rec.closed;
  }
  EXPECT_TRUE(closed);
  std::printf("    connect gave up after %lld ms: %s\n", MsSince(start), rec.reason.c_str());
  t.Close();
}

TEST(ItTransportOutboundHighWaterMark) {
  int ls = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;
  EXPECT_EQ(::bind(ls, reinterpret_cast<sockaddr*>(&a), sizeof(a)), 0);
  EXPECT_EQ(::listen(ls, 4), 0);
  socklen_t len = sizeof(a);
  ::getsockname(ls, reinterpret_cast<sockaddr*>(&a), &len);
  int port = ntohs(a.sin_port);
  // The peer accepts (the kernel completes the handshake) and never reads.
  int peer = -1;
  std::thread acceptor([&] { peer = ::accept(ls, nullptr, nullptr); });
  Recorder rec;
  omhost::PosixTlsTransport t;
  t.SetListener(&rec);
  EXPECT_TRUE(t.Open("tcp://127.0.0.1:" + std::to_string(port)));
  auto start = std::chrono::steady_clock::now();
  bool opened = false;
  while (!opened && MsSince(start) < 2000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::lock_guard<std::mutex> l(rec.mu);
    opened = rec.opened;
  }
  EXPECT_TRUE(opened);
  const std::string chunk(64 * 1024, 'x');
  size_t queued = 0;
  bool closed = false;
  while (!closed && queued < (64u << 20) && MsSince(start) < 10000) {
    t.Send(chunk);
    queued += chunk.size();
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    std::lock_guard<std::mutex> l(rec.mu);
    closed = rec.closed;
  }
  for (int i = 0; i < 100 && !closed; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::lock_guard<std::mutex> l(rec.mu);
    closed = rec.closed;
  }
  EXPECT_TRUE(closed);
  {
    std::lock_guard<std::mutex> l(rec.mu);
    std::printf("    peer not reading: failed after %zu KiB queued: %s\n", queued / 1024, rec.reason.c_str());
    EXPECT_TRUE(rec.reason.find("send buffer overflow") != std::string::npos);
  }
  t.Close();
  acceptor.join();
  if (peer >= 0) ::close(peer);
  ::close(ls);
}
