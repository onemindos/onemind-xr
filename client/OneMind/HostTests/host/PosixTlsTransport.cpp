// SPDX-License-Identifier: GPL-3.0-or-later
#include "PosixTlsTransport.h"

#include "OneMindCore/Codec.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace omhost {

namespace {
enum { kOpen = 0, kData = 1, kClosed = 2 };

// Non-blocking connect with a deadline, polled in 50 ms slices so Close() (close_req, wake
// pipe) never waits for the OS connect timeout (about 75 s on macOS).
int ConnectTcp(const std::string& host, int port, std::string& err, const std::atomic<bool>& close_req, int wake_fd,
               int timeout_ms) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
    err = "resolve " + host;
    return -1;
  }
  int fd = -1;
  err.clear();
  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  for (addrinfo* a = res; a && !close_req; a = a->ai_next) {
    fd = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (fd < 0) continue;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    int rc = ::connect(fd, a->ai_addr, a->ai_addrlen);
    int soerr = rc == 0 ? 0 : errno;
    if (rc != 0 && errno == EINPROGRESS) {
      soerr = ETIMEDOUT;
      while (!close_req && std::chrono::steady_clock::now() < end) {
        pollfd p[2] = {{fd, POLLOUT, 0}, {wake_fd, POLLIN, 0}};
        int n = ::poll(p, wake_fd >= 0 ? 2 : 1, 50);
        if (n > 0 && (p[1].revents & POLLIN)) {  // Send() before the connection is up: drain, keep waiting
          char drain[64];
          while (::read(wake_fd, drain, sizeof(drain)) > 0) {
          }
        }
        if (n > 0 && (p[0].revents & (POLLOUT | POLLERR | POLLHUP))) {
          socklen_t len = sizeof(soerr);
          if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) != 0) soerr = errno;
          break;
        }
      }
    }
    if (soerr == 0 && !close_req) break;
    err = close_req ? std::string("connect cancelled") : std::string("connect: ") + std::strerror(soerr);
    ::close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) {
    if (err.empty()) err = close_req ? "connect cancelled" : "connect: no address";
    return -1;
  }
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
  return fd;
}

bool SendAll(int fd, const std::string& data, int timeout_ms) {
  size_t off = 0;
  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (off < data.size()) {
    ssize_t n = ::send(fd, data.data() + off, data.size() - off, 0);
    if (n > 0) {
      off += static_cast<size_t>(n);
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (std::chrono::steady_clock::now() > end) return false;
      pollfd p{fd, POLLOUT, 0};
      ::poll(&p, 1, 20);
      continue;
    }
    return false;
  }
  return true;
}
}  // namespace

bool SplitUrl(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path) {
  size_t s = url.find("://");
  if (s == std::string::npos) return false;
  scheme = url.substr(0, s);
  std::string rest = url.substr(s + 3);
  size_t slash = rest.find('/');
  path = slash == std::string::npos ? "/" : rest.substr(slash);
  std::string hp = rest.substr(0, slash);
  size_t c = hp.rfind(':');
  if (c == std::string::npos) return false;
  host = hp.substr(0, c);
  port = std::atoi(hp.c_str() + c + 1);
  return !host.empty() && port > 0 && port < 65536;
}

PosixTlsTransport::~PosixTlsTransport() {
  SetListener(nullptr);
  Close();
}

void PosixTlsTransport::SetListener(Listener* l) {
  std::lock_guard<std::mutex> lock(mu_);
  listener_ = l;
}

bool PosixTlsTransport::Open(const std::string& url) {
  std::string scheme, host, path;
  int port = 0;
  if (!SplitUrl(url, scheme, host, port, path) || (scheme != "tcp" && scheme != "tls")) return false;
  Close();
  std::lock_guard<std::mutex> lock(life_mu_);
  if (::pipe(wake_) != 0) return false;
  fcntl(wake_[0], F_SETFL, O_NONBLOCK);
  fcntl(wake_[1], F_SETFL, O_NONBLOCK);
  close_req_ = false;
  {
    std::lock_guard<std::mutex> l(mu_);
    out_.clear();
    overflow_ = false;
  }
  uint64_t gen = ++gen_;
  thread_ = std::thread([this, host, port, scheme, gen] { Run(host, port, scheme == "tls", gen); });
  return true;
}

void PosixTlsTransport::Send(const std::string& bytes) {
  {
    std::lock_guard<std::mutex> l(mu_);
    if (out_.size() + bytes.size() > kMaxPendingBytes) overflow_ = true;  // Run fails the connection
    else out_ += bytes;
  }
  if (wake_[1] >= 0) {
    char c = 1;
    (void)!::write(wake_[1], &c, 1);
  }
}

void PosixTlsTransport::Close() {
  std::lock_guard<std::mutex> lock(life_mu_);
  close_req_ = true;
  if (wake_[1] >= 0) {
    char c = 1;
    (void)!::write(wake_[1], &c, 1);
  }
  if (thread_.joinable()) {
    if (thread_.get_id() == std::this_thread::get_id()) thread_.detach();
    else thread_.join();
  }
  ++gen_;  // no listener calls for the old connection
  for (int& w : wake_) {
    if (w >= 0) ::close(w);
    w = -1;
  }
}

void PosixTlsTransport::KillSocketForTest() {
  int fd = fd_.load();
  if (fd >= 0) ::shutdown(fd, SHUT_RDWR);
}

std::string PosixTlsTransport::peer_cn() const {
  std::lock_guard<std::mutex> l(cn_mu_);
  return peer_cn_;
}

void PosixTlsTransport::Deliver(uint64_t gen, int kind, const std::string& data) {
  std::lock_guard<std::mutex> l(mu_);
  if (!listener_ || gen != gen_.load() || close_req_) return;
  if (kind == kOpen) listener_->OnTransportOpen();
  else if (kind == kData) listener_->OnTransportData(data.data(), data.size());
  else listener_->OnTransportClosed(data);
}

void PosixTlsTransport::Run(std::string host, int port, bool use_tls, uint64_t gen) {
  std::string err;
  int fd = ConnectTcp(host, port, err, close_req_, wake_[0], connect_timeout_ms_);
  if (fd < 0) {
    Deliver(gen, kClosed, err);
    return;
  }
  fd_ = fd;
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  omtls::TlsSession tls;
  bool opened = false;
  if (use_tls) {
    omtls::TlsConfig cfg = tls_;
    if (cfg.server_name.empty()) cfg.server_name = host;
    if (!tls.Init(cfg, err)) {
      ::close(fd);
      fd_ = -1;
      Deliver(gen, kClosed, "tls: " + err);
      return;
    }
  } else {
    opened = true;
    Deliver(gen, kOpen, std::string());
  }
  std::string net_out, reason;
  char buf[16384];
  while (true) {
    if (close_req_) break;
    pollfd p[2] = {{fd, static_cast<short>(POLLIN | (net_out.empty() ? 0 : POLLOUT)), 0}, {wake_[0], POLLIN, 0}};
    ::poll(p, 2, 50);
    if (p[1].revents & POLLIN)
      while (::read(wake_[0], buf, sizeof(buf)) > 0) {
      }
    std::string plain_out;
    bool overflow;
    {
      std::lock_guard<std::mutex> l(mu_);
      plain_out.swap(out_);
      overflow = overflow_;
    }
    if (!plain_out.empty()) {
      if (use_tls) tls.Write(plain_out);
      else net_out += plain_out;
    }
    bool eof = false;
    if (p[0].revents & (POLLIN | POLLHUP | POLLERR)) {
      while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
          if (use_tls) tls.FeedNetwork(buf, static_cast<size_t>(n));
          else Deliver(gen, kData, std::string(buf, static_cast<size_t>(n)));
          continue;
        }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) eof = true;
        break;
      }
    }
    if (use_tls) {
      std::string plain_in;
      omtls::TlsSession::State st = tls.Pump(plain_in);
      net_out += tls.TakeNetwork();
      if (st == omtls::TlsSession::State::Open && !opened) {
        opened = true;
        {
          std::lock_guard<std::mutex> l(cn_mu_);
          peer_cn_ = tls.PeerCommonName();
        }
        Deliver(gen, kOpen, std::string());
      }
      if (!plain_in.empty()) Deliver(gen, kData, plain_in);
      if (st == omtls::TlsSession::State::Failed) {
        reason = tls.error();
        SendAll(fd, net_out, 200);  // let the alert reach the peer
        break;
      }
      if (st == omtls::TlsSession::State::Closed) {
        reason = "tls closed by peer";
        break;
      }
    }
    if (!net_out.empty()) {
      ssize_t n = ::send(fd, net_out.data(), net_out.size(), 0);
      if (n > 0) net_out.erase(0, static_cast<size_t>(n));
      else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) eof = true;
    }
    if (overflow || net_out.size() > kMaxPendingBytes) {
      // The peer stopped reading (TCP window full) while we keep queueing: fail instead of growing.
      reason = "send buffer overflow (peer not reading)";
      break;
    }
    if (eof) {
      reason = opened ? "connection closed" : "connection closed during handshake";
      break;
    }
  }
  if (close_req_) {
    // Graceful: flush what the owner queued before Close (directed sends rely on it).
    std::string rest;
    {
      std::lock_guard<std::mutex> l(mu_);
      rest.swap(out_);
    }
    if (use_tls) {
      if (!rest.empty()) tls.Write(rest);
      std::string ignore;
      tls.Pump(ignore);
      tls.Shutdown();
      net_out += tls.TakeNetwork();
    } else {
      net_out += rest;
    }
    SendAll(fd, net_out, 2000);
    ::shutdown(fd, SHUT_WR);
  }
  fd_ = -1;
  ::close(fd);
  if (!close_req_) Deliver(gen, kClosed, reason);
}

// --- HostEnrollPlatform ----------------------------------------------------------------------

void HostEnrollPlatform::Https(const std::string& method, const std::string& url,
                               const std::vector<std::pair<std::string, std::string>>& headers,
                               const std::string& body, HttpCallback cb) {
  // The same om::HttpExchange the Unreal plugin uses, over the host TLS transport.
  omtls::TlsConfig cfg;
  cfg.ca_pem = ca_pem_;
  cfg.verify_peer = verify_;
  om::HttpExchange ex(std::make_shared<PosixTlsTransport>(cfg));
  om::HttpResponse r;
  if (!ex.Start(method, url, headers, body, om::WallClockMs())) {
    r.error = "bad url";
    cb(r);
    return;
  }
  while (!ex.Poll(om::WallClockMs(), r)) std::this_thread::sleep_for(std::chrono::milliseconds(2));
  cb(r);
}

bool HostEnrollPlatform::MakeKeyAndCsr(const std::vector<std::pair<std::string, std::string>>& subject, int key_bits,
                                       std::string& key_ref, std::string& csr_pem, std::string& err) {
  std::string key_pem;
  if (!omtls::MakeRsaKeyAndCsr(subject, key_bits, key_pem, csr_pem, err)) return false;
  key_ref = "key-" + om::NewToken();
  keys_[key_ref] = key_pem;
  return true;
}

std::string HostEnrollPlatform::KeyPem(const std::string& key_ref) const {
  auto it = keys_.find(key_ref);
  return it == keys_.end() ? std::string() : it->second;
}

}  // namespace omhost
