// SPDX-License-Identifier: GPL-3.0-or-later
#include "PosixWsTransport.h"

#include "OneMindCore/Codec.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <random>

namespace omhost {

PosixWsTransport::~PosixWsTransport() {
  SetListener(nullptr);
  Close();
}

void PosixWsTransport::SetListener(Listener* l) {
  std::lock_guard<std::mutex> lock(mu_);
  listener_ = l;
}

bool PosixWsTransport::Open(const std::string& url) {
  om::WsUrl u;
  if (!om::WsUrl::Parse(url, u) || u.secure) return false;  // wss:// is the engine transport's job
  Close();
  std::lock_guard<std::mutex> lock(life_mu_);
  uint64_t gen = ++gen_;
  thread_ = std::thread([this, u, gen] { Run(u, gen); });
  return true;
}

void PosixWsTransport::SendFrame(om::WsOpcode op, const std::string& payload) {
  static thread_local std::mt19937 rng(std::random_device{}());
  uint8_t mask[4];
  for (auto& m : mask) m = static_cast<uint8_t>(rng());
  std::string frame = om::WsEncodeFrame(op, payload, mask);
  std::lock_guard<std::mutex> lock(mu_);
  if (fd_ < 0) return;
  size_t off = 0;
  while (off < frame.size()) {
    ssize_t n = ::send(fd_, frame.data() + off, frame.size() - off, 0);
    if (n <= 0) return;  // the reader notices the dead socket and reports the close
    off += static_cast<size_t>(n);
  }
}

void PosixWsTransport::Send(const std::string& bytes) {
  if (!open_) return;
  SendFrame(om::WsOpcode::Binary, bytes);
}

void PosixWsTransport::Close() {
  std::lock_guard<std::mutex> lock(life_mu_);
  ++gen_;  // silences the reader for the old connection
  open_ = false;
  {
    std::lock_guard<std::mutex> l2(mu_);
    if (fd_ >= 0) ::shutdown(fd_, SHUT_RDWR);
  }
  if (thread_.joinable()) {
    if (thread_.get_id() == std::this_thread::get_id()) thread_.detach();
    else thread_.join();
  }
  std::lock_guard<std::mutex> l2(mu_);
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

void PosixWsTransport::KillSocketForTest() {
  std::lock_guard<std::mutex> l2(mu_);
  if (fd_ >= 0) ::shutdown(fd_, SHUT_RDWR);
}

void PosixWsTransport::Run(om::WsUrl url, uint64_t gen) {
  auto notify_closed = [&](const std::string& why) {
    open_ = false;
    std::lock_guard<std::mutex> lock(mu_);
    if (listener_ && gen == gen_) listener_->OnTransportClosed(why);
  };
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  if (::getaddrinfo(url.host.c_str(), std::to_string(url.port).c_str(), &hints, &res) != 0 || !res) {
    notify_closed("resolve failed");
    return;
  }
  int fd = -1;
  for (addrinfo* ai = res; ai; ai = ai->ai_next) {
    fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) continue;
    if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
    ::close(fd);
    fd = -1;
  }
  ::freeaddrinfo(res);
  if (fd < 0) {
    notify_closed("connect failed");
    return;
  }
  int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (gen != gen_) {
      ::close(fd);
      return;
    }
    fd_ = fd;
  }
  uint8_t keybytes[16];
  std::random_device rd;
  for (auto& b : keybytes) b = static_cast<uint8_t>(rd());
  std::string key = om::Base64Encode(keybytes, sizeof(keybytes));
  std::string req = om::WsBuildHandshake(url, key);
  if (::send(fd, req.data(), req.size(), 0) != static_cast<ssize_t>(req.size())) {
    notify_closed("handshake send failed");
    return;
  }
  std::string acc;
  char buf[65536];
  size_t hdr_end = std::string::npos;
  while (hdr_end == std::string::npos) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      notify_closed("handshake recv failed");
      return;
    }
    acc.append(buf, static_cast<size_t>(n));
    hdr_end = acc.find("\r\n\r\n");
    if (acc.size() > 16384 && hdr_end == std::string::npos) {
      notify_closed("handshake too long");
      return;
    }
  }
  std::string err;
  if (!om::WsCheckHandshakeResponse(acc.substr(0, hdr_end + 4), key, &err)) {
    notify_closed("handshake rejected: " + err);
    return;
  }
  open_ = true;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (listener_ && gen == gen_) listener_->OnTransportOpen();
  }
  om::WsDecoder dec;
  std::vector<om::WsDecoder::Message> msgs;
  std::string rest = acc.substr(hdr_end + 4);
  auto handle = [&](const char* p, size_t n) -> bool {
    msgs.clear();
    if (!dec.Feed(p, n, msgs, &err)) {
      notify_closed("websocket protocol error: " + err);
      return false;
    }
    for (auto& m : msgs) {
      switch (m.op) {
        case om::WsOpcode::Binary:
        case om::WsOpcode::Text: {
          std::lock_guard<std::mutex> lock(mu_);
          if (listener_ && gen == gen_) listener_->OnTransportData(m.payload.data(), m.payload.size());
          break;
        }
        case om::WsOpcode::Ping:
          SendFrame(om::WsOpcode::Pong, m.payload);
          break;
        case om::WsOpcode::Close:
          SendFrame(om::WsOpcode::Close, m.payload.substr(0, 2));
          notify_closed("server closed websocket");
          return false;
        default:
          break;
      }
    }
    return true;
  };
  if (!rest.empty() && !handle(rest.data(), rest.size())) return;
  while (gen == gen_) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      notify_closed(n == 0 ? "connection closed by peer" : "socket error");
      return;
    }
    if (!handle(buf, static_cast<size_t>(n))) return;
  }
}

}  // namespace omhost
