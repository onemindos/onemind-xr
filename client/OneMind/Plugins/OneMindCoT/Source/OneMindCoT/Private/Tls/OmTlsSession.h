// A TLS client session over memory BIOs (OpenSSL), with no sockets and no threads, so the
// same code runs in the Unreal transport (FSocket) and in the host test transport (POSIX
// sockets), where it is built and tested by HostTests/CMakeLists.txt. Also RSA key and CSR
// generation for TAK enrollment (TakEnroll.h).
//
// OpenSSL API used is common to 1.1.1 and 3.x (Unreal ships its own OpenSSL; which version
// UE 5.8 bundles for Mac is unverified, so nothing 3.x-only is used).
// No exceptions; errors come back as strings. PEM inputs are kept only inside OpenSSL objects.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <utility>
#include <vector>

struct ssl_ctx_st;
struct ssl_st;
struct bio_st;

namespace omtls {

struct TlsConfig {
  std::string cert_pem;     // client certificate (and optional chain), PEM; empty = no client cert
  std::string key_pem;      // client private key, PEM
  std::string ca_pem;       // trust anchors for the server, PEM (one or more)
  std::string server_name;  // SNI; also checked against the certificate when verify_host
  bool verify_peer = true;  // false only for local tests; logged by the caller
  bool verify_host = false; // Common Commo checks the chain only (tak-protocols.md S4)
};

class TlsSession {
 public:
  enum class State { Idle, Handshaking, Open, Closed, Failed };
  TlsSession() = default;
  ~TlsSession();
  TlsSession(const TlsSession&) = delete;
  TlsSession& operator=(const TlsSession&) = delete;

  bool Init(const TlsConfig& cfg, std::string& err);
  /// Ciphertext read from the socket.
  void FeedNetwork(const char* data, size_t len);
  /// Ciphertext to write to the socket (drains the buffer).
  std::string TakeNetwork();
  /// Plaintext to send; buffered until the handshake completes.
  void Write(const std::string& plain);
  /// Drives the handshake and reads; appends decrypted bytes to `plain_out`.
  State Pump(std::string& plain_out);
  /// Sends close_notify (call TakeNetwork afterwards).
  void Shutdown();
  State state() const { return state_; }
  const std::string& error() const { return err_; }
  /// Subject CN of the server certificate (after the handshake).
  std::string PeerCommonName() const;

 private:
  void Reset();
  ssl_ctx_st* ctx_ = nullptr;
  ssl_st* ssl_ = nullptr;
  bio_st* rbio_ = nullptr;  // network -> ssl (owned by ssl_)
  bio_st* wbio_ = nullptr;  // ssl -> network (owned by ssl_)
  std::string pending_plain_;
  State state_ = State::Idle;
  std::string err_;
};

/// RSA key pair and PEM CSR (SHA-256) with the ordered subject entries. Returns the
/// private key as unencrypted PKCS#8 PEM; the caller stores it in the OS keychain.
bool MakeRsaKeyAndCsr(const std::vector<std::pair<std::string, std::string>>& subject, int bits, std::string& key_pem,
                      std::string& csr_pem, std::string& err);

}  // namespace omtls
