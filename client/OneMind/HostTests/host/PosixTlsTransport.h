// Host-only byte-stream ITransport over POSIX sockets: "tcp://host:port" (plain; TAK
// directed messages and local tests) and "tls://host:port" (TLS client with an optional
// client certificate, through omtls::TlsSession, the same code the Unreal transport uses).
// Plus the host IEnrollPlatform: blocking HTTPS for /Marti/api/tls/* and OpenSSL key/CSR.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/NatsClient.h"
#include "OneMindCore/TakEnroll.h"
#include "Tls/OmTlsSession.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace omhost {

class PosixTlsTransport : public om::ITransport {
 public:
  explicit PosixTlsTransport(omtls::TlsConfig tls = omtls::TlsConfig()) : tls_(std::move(tls)) {}
  ~PosixTlsTransport() override;
  void SetListener(Listener* l) override;
  bool Open(const std::string& url) override;
  void Send(const std::string& bytes) override;
  /// Flushes queued bytes (up to 2 s), closes, and joins the I/O thread.
  void Close() override;
  /// Test hook: drop the TCP connection abruptly.
  void KillSocketForTest();
  std::string peer_cn() const;

 private:
  void Run(std::string host, int port, bool tls, uint64_t gen);
  void Deliver(uint64_t gen, int kind, const std::string& data);
  omtls::TlsConfig tls_;
  std::mutex mu_;       // listener_, out_
  std::mutex life_mu_;  // Open/Close
  Listener* listener_ = nullptr;
  std::string out_;
  std::thread thread_;
  std::atomic<uint64_t> gen_{0};
  std::atomic<bool> close_req_{false};
  std::atomic<int> fd_{-1};
  int wake_[2] = {-1, -1};
  mutable std::mutex cn_mu_;
  std::string peer_cn_;
};

/// IEnrollPlatform for the host: blocking HTTPS (HTTP/1.1, Connection: close, Content-Length
/// or chunked replies), keys kept in memory by reference.
class HostEnrollPlatform : public om::IEnrollPlatform {
 public:
  explicit HostEnrollPlatform(std::string ca_pem, bool verify_peer = true) : ca_pem_(std::move(ca_pem)), verify_(verify_peer) {}
  void Https(const std::string& method, const std::string& url,
             const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body,
             HttpCallback cb) override;
  bool MakeKeyAndCsr(const std::vector<std::pair<std::string, std::string>>& subject, int key_bits, std::string& key_ref,
                     std::string& csr_pem, std::string& err) override;
  std::string KeyPem(const std::string& key_ref) const;

 private:
  std::string ca_pem_;
  bool verify_;
  std::map<std::string, std::string> keys_;
};

/// Splits "scheme://host:port[/path]"; false if malformed.
bool SplitUrl(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path);

}  // namespace omhost
