// SPDX-License-Identifier: GPL-3.0-or-later
// Memory-BIO TLS client session and CSR generation (OpenSSL); see OmTlsSession.h.
#include "OmTlsSession.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace omtls {

namespace {
std::string LastSslError(const char* what) {
  unsigned long e = ERR_get_error();
  std::string s = what;
  if (e) {
    char buf[256];
    ERR_error_string_n(e, buf, sizeof(buf));
    s += ": ";
    s += buf;
  }
  ERR_clear_error();
  return s;
}

std::string BioToString(BIO* b) {
  char* data = nullptr;
  long n = BIO_get_mem_data(b, &data);
  return n > 0 ? std::string(data, static_cast<size_t>(n)) : std::string();
}
}  // namespace

TlsSession::~TlsSession() { Reset(); }

void TlsSession::Reset() {
  if (ssl_) SSL_free(ssl_);  // frees both BIOs
  if (ctx_) SSL_CTX_free(ctx_);
  ssl_ = nullptr;
  ctx_ = nullptr;
  rbio_ = wbio_ = nullptr;
  pending_plain_.clear();
  state_ = State::Idle;
}

bool TlsSession::Init(const TlsConfig& cfg, std::string& err) {
  Reset();
  ctx_ = SSL_CTX_new(TLS_client_method());
  if (!ctx_) {
    err = LastSslError("SSL_CTX_new");
    return false;
  }
  SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
  if (!cfg.cert_pem.empty()) {
    BIO* b = BIO_new_mem_buf(cfg.cert_pem.data(), static_cast<int>(cfg.cert_pem.size()));
    X509* leaf = PEM_read_bio_X509(b, nullptr, nullptr, nullptr);
    if (!leaf || SSL_CTX_use_certificate(ctx_, leaf) != 1) {
      if (leaf) X509_free(leaf);
      BIO_free(b);
      err = LastSslError("client certificate");
      return false;
    }
    X509_free(leaf);
    while (X509* extra = PEM_read_bio_X509(b, nullptr, nullptr, nullptr)) {
      if (SSL_CTX_add_extra_chain_cert(ctx_, extra) != 1) X509_free(extra);  // owned by ctx on success
    }
    ERR_clear_error();  // end of PEM input
    BIO_free(b);
    BIO* kb = BIO_new_mem_buf(cfg.key_pem.data(), static_cast<int>(cfg.key_pem.size()));
    EVP_PKEY* key = PEM_read_bio_PrivateKey(kb, nullptr, nullptr, nullptr);
    BIO_free(kb);
    if (!key || SSL_CTX_use_PrivateKey(ctx_, key) != 1 || SSL_CTX_check_private_key(ctx_) != 1) {
      if (key) EVP_PKEY_free(key);
      err = LastSslError("client key");
      return false;
    }
    EVP_PKEY_free(key);
  }
  if (cfg.verify_peer) {
    if (cfg.ca_pem.empty()) {
      err = "verify_peer needs a CA";
      return false;
    }
    X509_STORE* store = SSL_CTX_get_cert_store(ctx_);
    BIO* b = BIO_new_mem_buf(cfg.ca_pem.data(), static_cast<int>(cfg.ca_pem.size()));
    int added = 0;
    while (X509* ca = PEM_read_bio_X509(b, nullptr, nullptr, nullptr)) {
      if (X509_STORE_add_cert(store, ca) == 1) ++added;
      X509_free(ca);
    }
    ERR_clear_error();
    BIO_free(b);
    if (added == 0) {
      err = "no CA certificate in ca_pem";
      return false;
    }
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
  } else {
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
  }
  ssl_ = SSL_new(ctx_);
  rbio_ = BIO_new(BIO_s_mem());
  wbio_ = BIO_new(BIO_s_mem());
  if (!ssl_ || !rbio_ || !wbio_) {
    err = LastSslError("SSL_new");
    return false;
  }
  SSL_set_bio(ssl_, rbio_, wbio_);
  if (!cfg.server_name.empty()) {
    SSL_set_tlsext_host_name(ssl_, cfg.server_name.c_str());
    if (cfg.verify_host && SSL_set1_host(ssl_, cfg.server_name.c_str()) != 1) {
      err = LastSslError("SSL_set1_host");
      return false;
    }
  }
  SSL_set_connect_state(ssl_);
  state_ = State::Handshaking;
  return true;
}

void TlsSession::FeedNetwork(const char* data, size_t len) {
  if (rbio_ && len) BIO_write(rbio_, data, static_cast<int>(len));
}

std::string TlsSession::TakeNetwork() {
  std::string out;
  if (!wbio_) return out;
  char buf[16384];
  int n;
  while ((n = BIO_read(wbio_, buf, sizeof(buf))) > 0) out.append(buf, static_cast<size_t>(n));
  return out;
}

void TlsSession::Write(const std::string& plain) { pending_plain_ += plain; }

TlsSession::State TlsSession::Pump(std::string& plain_out) {
  if (!ssl_ || state_ == State::Failed || state_ == State::Closed) return state_;
  if (state_ == State::Handshaking) {
    int r = SSL_do_handshake(ssl_);
    if (r == 1) {
      state_ = State::Open;
    } else {
      int e = SSL_get_error(ssl_, r);
      if (e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE) {
        long vr = SSL_get_verify_result(ssl_);
        err_ = LastSslError("TLS handshake");
        if (vr != X509_V_OK) err_ += std::string(" (verify: ") + X509_verify_cert_error_string(vr) + ")";
        state_ = State::Failed;
        return state_;
      }
      return state_;
    }
  }
  while (!pending_plain_.empty()) {
    int w = SSL_write(ssl_, pending_plain_.data(), static_cast<int>(pending_plain_.size()));
    if (w <= 0) {
      int e = SSL_get_error(ssl_, w);
      if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) break;
      err_ = LastSslError("SSL_write");
      state_ = State::Failed;
      return state_;
    }
    pending_plain_.erase(0, static_cast<size_t>(w));
  }
  char buf[16384];
  while (true) {
    int n = SSL_read(ssl_, buf, sizeof(buf));
    if (n > 0) {
      plain_out.append(buf, static_cast<size_t>(n));
      continue;
    }
    int e = SSL_get_error(ssl_, n);
    if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) break;
    if (e == SSL_ERROR_ZERO_RETURN) {
      state_ = State::Closed;
      break;
    }
    err_ = LastSslError("SSL_read");
    state_ = State::Failed;
    break;
  }
  return state_;
}

void TlsSession::Shutdown() {
  if (ssl_ && state_ == State::Open) {
    SSL_shutdown(ssl_);
    state_ = State::Closed;
  }
}

std::string TlsSession::PeerCommonName() const {
  if (!ssl_) return std::string();
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  X509* peer = SSL_get1_peer_certificate(ssl_);
#else
  X509* peer = SSL_get_peer_certificate(ssl_);
#endif
  if (!peer) return std::string();
  char cn[256] = {0};
  X509_NAME_get_text_by_NID(X509_get_subject_name(peer), NID_commonName, cn, sizeof(cn));
  X509_free(peer);
  return cn;
}

bool MakeRsaKeyAndCsr(const std::vector<std::pair<std::string, std::string>>& subject, int bits, std::string& key_pem,
                      std::string& csr_pem, std::string& err) {
  EVP_PKEY* pkey = nullptr;
  EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  bool ok = kctx && EVP_PKEY_keygen_init(kctx) == 1 && EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, bits) > 0 &&
            EVP_PKEY_keygen(kctx, &pkey) == 1;
  if (kctx) EVP_PKEY_CTX_free(kctx);
  if (!ok) {
    err = LastSslError("RSA keygen");
    return false;
  }
  X509_REQ* req = X509_REQ_new();
  X509_NAME* name = X509_NAME_new();
  ok = req && name && X509_REQ_set_version(req, 0) == 1;
  for (const auto& e : subject) {
    if (!ok) break;
    ok = X509_NAME_add_entry_by_txt(name, e.first.c_str(), MBSTRING_UTF8,
                                    reinterpret_cast<const unsigned char*>(e.second.c_str()), -1, -1, 0) == 1;
    if (!ok) err = "bad subject entry " + e.first;
  }
  ok = ok && X509_REQ_set_subject_name(req, name) == 1 && X509_REQ_set_pubkey(req, pkey) == 1 &&
       X509_REQ_sign(req, pkey, EVP_sha256()) > 0;
  if (ok) {
    BIO* b = BIO_new(BIO_s_mem());
    ok = PEM_write_bio_X509_REQ(b, req) == 1;
    if (ok) csr_pem = BioToString(b);
    BIO_free(b);
    BIO* kb = BIO_new(BIO_s_mem());
    ok = ok && PEM_write_bio_PrivateKey(kb, pkey, nullptr, nullptr, 0, nullptr, nullptr) == 1;
    if (ok) key_pem = BioToString(kb);
    BIO_free(kb);
  }
  if (!ok && err.empty()) err = LastSslError("CSR");
  if (name) X509_NAME_free(name);
  if (req) X509_REQ_free(req);
  EVP_PKEY_free(pkey);
  return ok;
}

}  // namespace omtls
