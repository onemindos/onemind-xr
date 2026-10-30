// TAK Server certificate enrollment for the client's own TAK identity (feature C3).
//
// The flow is the one Common Commo's EnrollmentManager runs (atak-civ
// commoncommo/core/impl/enrollmentmanager.cpp, read at 9f6893dd) against the endpoints in
// TAK Server's CertManagerApi (TAK-Product-Center/Server tag 5.7-RELEASE-14,
// src/takserver-core/takserver-war/src/main/java/com/bbn/tak/tls/CertManagerApi.java):
//   1. generate a key pair (RSA, default 2048 bits);
//   2. GET  https://{host}:{port}/Marti/api/tls/config with HTTP Basic auth (the TAK user);
//      the reply is XML <certificateConfig><nameEntries><nameEntry name= value=/>...;
//   3. build a CSR whose subject is CN={user} followed by every nameEntry, in order;
//   4. POST https://{host}:{port}/Marti/api/tls/signClient/v2?clientUid={uid}&version={v}
//      with the base64 CSR (PEM banners removed, as Commo does) as the body;
//   5. the reply is JSON {"signedCert": PEM-body, "ca0": PEM-body, ...} (Accept */* or
//      application/json) or XML <enrollment><signedCert/><ca/>...</enrollment>
//      (Accept application/xml). CertManagerApi writes these with Util.certToPEM(cert, false);
//      whether that omits the BEGIN/END lines is unverified, so both forms are accepted.
// Port 8446 is TAK Server's HTTPS connector without client certificates, used for
// enrollment (CoreConfig.example.xml, tak-protocols.md §5); 5.8 behaviour is unverified
// (5.8 source is not public).
//
// Key generation, CSR signing and HTTPS are platform services (OpenSSL on the host and in
// Unreal), passed in through IEnrollPlatform. The private key never leaves the platform's
// secure store (OS keychain on the Mac); nothing here writes files or logs secrets.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"
#include "OneMindCore/NatsClient.h"  // om::ITransport

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace om {

struct OM_CORE_API HttpResponse {
  int status = 0;
  std::string content_type;
  std::string body;
  std::string error;  // transport error ("" when an HTTP status was received)
};

class OM_CORE_API IEnrollPlatform {
 public:
  virtual ~IEnrollPlatform() = default;
  using HttpCallback = std::function<void(const HttpResponse&)>;
  /// HTTPS request; the platform verifies the server against its configured trust store.
  virtual void Https(const std::string& method, const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body,
                     HttpCallback cb) = 0;
  /// Generates a key pair and a PEM CSR with `subject` (ordered name entries). The key
  /// stays in the platform, addressed by `key_ref`.
  virtual bool MakeKeyAndCsr(const std::vector<std::pair<std::string, std::string>>& subject, int key_bits,
                             std::string& key_ref, std::string& csr_pem, std::string& err) = 0;
};

struct OM_CORE_API EnrollConfig {
  std::string host;
  int port = 8446;
  std::string user;      // TAK user (from the keychain or environment, never Git)
  std::string password;  // idem
  std::string client_uid;
  std::string version = "OneMind 0.4.0";
  int key_bits = 2048;
};

struct OM_CORE_API EnrollResult {
  bool ok = false;
  std::string error;
  std::string key_ref;               // platform handle for the private key
  std::string cert_pem;              // signed client certificate (PEM with banners)
  std::vector<std::string> ca_pems;  // chain returned by the server (PEM with banners)
};

/// Parses /Marti/api/tls/config. Namespace prefixes on element names are ignored.
OM_CORE_API bool ParseCertificateConfig(const std::string& xml, std::vector<std::pair<std::string, std::string>>& entries,
                                        std::string* err = nullptr);
/// Removes "-----BEGIN CERTIFICATE REQUEST-----" / END lines and line breaks.
OM_CORE_API std::string CsrBodyFromPem(const std::string& pem);
/// Parses a signClient/v2 reply (JSON or XML) into PEM certificates.
OM_CORE_API bool ParseSignClientV2(const std::string& body, const std::string& content_type, EnrollResult& out);
/// Wraps a bare base64 certificate in PEM banners (64-character lines); PEM passes through.
OM_CORE_API std::string ToCertPem(const std::string& body_or_pem);
OM_CORE_API std::string BasicAuthHeader(const std::string& user, const std::string& password);
/// Percent-encodes a query parameter value (RFC 3986 unreserved characters kept).
OM_CORE_API std::string UrlEncode(const std::string& s);

/// Parses a raw HTTP/1.1 response (Content-Length or chunked body).
OM_CORE_API bool ParseHttpResponse(const std::string& raw, HttpResponse& out);
/// The request bytes for an HTTP/1.1 exchange with Connection: close.
OM_CORE_API std::string BuildHttpRequest(const std::string& method, const std::string& host, int port,
                                         const std::string& path,
                                         const std::vector<std::pair<std::string, std::string>>& headers,
                                         const std::string& body);

/// One HTTPS request over an om::ITransport ("tls://host:port"), for platforms whose HTTP
/// stack cannot trust a TAK Server's private CA (the Unreal transport is TLS with our own
/// trust store). Poll from one thread until it returns true.
class OM_CORE_API HttpExchange : public ITransport::Listener {
 public:
  /// Larger replies fail with "response too large" (enrollment replies are a few KiB).
  static constexpr size_t kMaxResponseBytes = 1u << 20;
  explicit HttpExchange(std::shared_ptr<ITransport> t);
  ~HttpExchange() override;
  bool Start(const std::string& method, const std::string& https_url,
             const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body, TimeMs now,
             int timeout_ms = 15000);
  /// True once finished; `out` then holds the response or the error.
  bool Poll(TimeMs now, HttpResponse& out);
  void OnTransportOpen() override;
  void OnTransportData(const char* data, size_t len) override;
  void OnTransportClosed(const std::string& reason) override;

 private:
  std::shared_ptr<ITransport> t_;
  std::mutex mu_;
  bool opened_ = false, closed_ = false, sent_ = false, done_ = false, overflow_ = false;
  size_t checked_ = 0;  // rx_ size at the last completeness check (Poll copies only new data)
  std::string rx_, close_reason_, request_;
  TimeMs deadline_ = 0;
};

/// Runs steps 1-5 once. The callback fires exactly once.
class OM_CORE_API TakEnrollment {
 public:
  using Done = std::function<void(const EnrollResult&)>;
  TakEnrollment(IEnrollPlatform& platform, EnrollConfig cfg) : platform_(platform), cfg_(std::move(cfg)) {}
  void Start(Done done);
  std::string ConfigUrl() const;
  std::string SignUrl() const;

 private:
  void OnConfig(const HttpResponse& r);
  void OnSigned(const HttpResponse& r);
  void Finish(const std::string& error);
  IEnrollPlatform& platform_;
  EnrollConfig cfg_;
  EnrollResult result_;
  Done done_;
};

}  // namespace om
