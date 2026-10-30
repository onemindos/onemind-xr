// Integration-test support: a disposable nats-server (JetStream, WebSocket, monitor) on
// free ports with a temporary store, clients over the host WebSocket transport, a poll
// loop, JetStream stream setup through the client itself, and a monitor-port HTTP GET.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/ClientCore.h"
#include "OneMindCore/JetStream.h"
#include "OneMindCore/NatsClient.h"
#include "PosixWsTransport.h"
#include "TestHarness.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>

extern char** environ;

namespace omit {

inline int FreePort() {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;
  ::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a));
  socklen_t len = sizeof(a);
  ::getsockname(fd, reinterpret_cast<sockaddr*>(&a), &len);
  int port = ntohs(a.sin_port);
  ::close(fd);
  return port;
}

inline bool PortOpen(int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(static_cast<uint16_t>(port));
  bool ok = ::connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
  ::close(fd);
  return ok;
}

inline std::string HttpGet(int port, const std::string& path) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(static_cast<uint16_t>(port));
  if (::connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
    ::close(fd);
    return std::string();
  }
  std::string req = "GET " + path + " HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
  ::send(fd, req.data(), req.size(), 0);
  std::string resp;
  char buf[8192];
  ssize_t n;
  while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) resp.append(buf, static_cast<size_t>(n));
  ::close(fd);
  size_t b = resp.find("\r\n\r\n");
  return b == std::string::npos ? std::string() : resp.substr(b + 4);
}

/// A nats-server child process. Binary: $NATS_SERVER_BIN, else nats-server on PATH.
/// Scratch directory: $OM_IT_TMP, else $TMPDIR (or /tmp).
class NatsServer {
 public:
  explicit NatsServer(const std::string& name) {
    const char* bin = std::getenv("NATS_SERVER_BIN");
    bin_ = bin ? bin : "nats-server";
    const char* root = std::getenv("OM_IT_TMP");
    if (!root) root = std::getenv("TMPDIR");
    dir_ = std::string(root ? root : "/tmp") + "/it-" + name + "-" +
           std::to_string(::getpid());
    std::string mk = "mkdir -p '" + dir_ + "/js'";
    if (std::system(mk.c_str()) != 0) std::printf("    mkdir failed\n");
    port_ = FreePort();
    ws_port_ = FreePort();
    mon_port_ = FreePort();
    std::ofstream c(dir_ + "/nats.conf");
    c << "listen: 127.0.0.1:" << port_ << "\n"
      << "http: 127.0.0.1:" << mon_port_ << "\n"
      << "jetstream { store_dir: \"" << dir_ << "/js\", max_file_store: 256MB, max_memory_store: 128MB }\n"
      << "websocket { listen: \"127.0.0.1:" << ws_port_ << "\", no_tls: true }\n"
      << "max_payload: 1MB\n";
  }
  ~NatsServer() {
    Stop();
    std::string rm = "rm -rf '" + dir_ + "'";
    if (std::system(rm.c_str()) != 0) std::printf("    cleanup failed\n");
  }
  bool Start() {
    std::string conf = dir_ + "/nats.conf";
    std::string log = dir_ + "/server.log";
    const char* argv[] = {bin_.c_str(), "-c", conf.c_str(), "-l", log.c_str(), nullptr};
    if (posix_spawnp(&pid_, bin_.c_str(), nullptr, nullptr, const_cast<char* const*>(argv), environ) != 0) return false;
    for (int i = 0; i < 100; ++i) {
      if (PortOpen(ws_port_) && PortOpen(port_)) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
  }
  void Stop() {
    if (pid_ <= 0) return;
    ::kill(pid_, SIGKILL);  // abrupt, like a crashed leaf
    int st = 0;
    ::waitpid(pid_, &st, 0);
    pid_ = -1;
    for (int i = 0; i < 50 && PortOpen(ws_port_); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  std::string ws_url() const { return "ws://127.0.0.1:" + std::to_string(ws_port_); }
  int mon_port() const { return mon_port_; }
  const std::string& dir() const { return dir_; }

 private:
  std::string bin_, dir_;
  int port_ = 0, ws_port_ = 0, mon_port_ = 0;
  pid_t pid_ = -1;
};

struct Client {
  std::shared_ptr<omhost::PosixWsTransport> t;
  std::unique_ptr<om::NatsClient> nc;
  std::unique_ptr<om::JetStream> js;
  explicit Client(const std::string& url, const std::string& name = "it") {
    t = std::make_shared<omhost::PosixWsTransport>();
    om::NatsOptions o;
    o.urls = {url};
    o.name = name;
    o.reconnect_wait_min_ms = 100;
    o.reconnect_wait_max_ms = 500;
    nc.reset(new om::NatsClient(t, o));
    js.reset(new om::JetStream(*nc));
    nc->Start(om::WallClockMs());
  }
};

/// Polls every client until `done` returns true or the timeout passes.
inline bool PollUntil(const std::vector<om::NatsClient*>& ncs, std::function<bool()> done, int timeout_ms,
                      std::function<void(om::TimeMs)> each = nullptr) {
  om::TimeMs end = om::WallClockMs() + timeout_ms;
  while (om::WallClockMs() < end) {
    om::TimeMs now = om::WallClockMs();
    for (auto* n : ncs) n->Poll(now);
    if (each) each(now);
    if (done()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return done();
}

/// JetStream API request through the client; returns the parsed reply.
inline om::Json JsApi(om::NatsClient& nc, const std::string& subject, const std::string& body) {
  om::Json out;
  bool done = false;
  nc.Request(subject, body, [&](const om::NatsMsg* m, const std::string& err) {
    if (m) om::Json::Parse(m->data, out);
    else out.set("client_error", err);
    done = true;
    return true;
  }, 5000);
  PollUntil({&nc}, [&] { return done; }, 6000);
  return out;
}

/// The ENT stream with the settings of omos platform/nats/streams/ent.yaml (PR #552),
/// file storage replaced by memory and max_bytes 1 GiB by 64 MiB for the test, R1.
inline om::Json CreateEntStream(om::NatsClient& nc) {
  return JsApi(nc, "$JS.API.STREAM.CREATE.ENT",
               R"({"name":"ENT","subjects":["ent.*.*.state"],"storage":"memory","retention":"limits","discard":"old",
                   "max_age":604800000000000,"max_bytes":67108864,"max_msgs_per_subject":1,
                   "duplicate_window":120000000000,"allow_direct":true,"allow_msg_ttl":true,
                   "subject_delete_marker_ttl":900000000000,"num_replicas":1})");
}

inline om::Json CreateKv(om::NatsClient& nc, const std::string& bucket, bool allow_direct,
                         const std::string& storage = "memory") {
  return JsApi(nc, "$JS.API.STREAM.CREATE.KV_" + bucket,
               std::string(R"({"name":"KV_)") + bucket + R"(","subjects":["$KV.)" + bucket +
                   R"(.>"],"storage":")" + storage + R"(","max_msgs_per_subject":1,"discard":"new","allow_rollup_hdrs":true,
                   "deny_delete":true,"num_replicas":1,"allow_direct":)" + (allow_direct ? "true" : "false") + "}");
}

}  // namespace omit
