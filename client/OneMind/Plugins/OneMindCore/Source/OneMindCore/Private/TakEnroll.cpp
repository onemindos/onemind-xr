// SPDX-License-Identifier: GPL-3.0-or-later
// TAK Server certificate enrollment (feature C3); see TakEnroll.h for sources.
#include "OneMindCore/TakEnroll.h"

#include "OneMindCore/Codec.h"
#include "OneMindCore/Json.h"
#include "OneMindCore/Xml.h"

#include <cctype>
#include <cstdlib>

namespace om {

namespace {
std::string LocalName(const std::string& tag) {
  size_t c = tag.find(':');
  return c == std::string::npos ? tag : tag.substr(c + 1);
}

std::string StripWs(const std::string& s) {
  std::string out;
  for (char c : s)
    if (!std::isspace(static_cast<unsigned char>(c))) out += c;
  return out;
}
}  // namespace

bool ParseCertificateConfig(const std::string& xml, std::vector<std::pair<std::string, std::string>>& entries,
                            std::string* err) {
  entries.clear();
  std::unique_ptr<XmlNode> root = XmlParse(xml, err);
  if (!root) return false;
  if (LocalName(root->tag) != "certificateConfig") {
    if (err) *err = "root is not certificateConfig";
    return false;
  }
  const XmlNode* names = nullptr;
  for (const auto& c : root->children)
    if (LocalName(c->tag) == "nameEntries") names = c.get();
  if (!names) {
    if (err) *err = "missing nameEntries";
    return false;
  }
  for (const auto& c : names->children) {
    if (LocalName(c->tag) != "nameEntry") continue;
    const std::string* n = c->Attr("name");
    const std::string* v = c->Attr("value");
    if (n && v) entries.push_back({*n, *v});
  }
  return true;
}

std::string CsrBodyFromPem(const std::string& pem) {
  std::string out;
  size_t pos = 0;
  while (pos < pem.size()) {
    size_t nl = pem.find('\n', pos);
    std::string line = pem.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = nl == std::string::npos ? pem.size() : nl + 1;
    if (line.find("-----") != std::string::npos) continue;
    out += StripWs(line);
  }
  return out;
}

std::string ToCertPem(const std::string& body_or_pem) {
  if (body_or_pem.find("-----BEGIN") != std::string::npos) return body_or_pem;
  std::string b = StripWs(body_or_pem);
  std::string out = "-----BEGIN CERTIFICATE-----\n";
  for (size_t i = 0; i < b.size(); i += 64) out += b.substr(i, 64) + "\n";
  return out + "-----END CERTIFICATE-----\n";
}

bool ParseSignClientV2(const std::string& body, const std::string& content_type, EnrollResult& out) {
  out.cert_pem.clear();
  out.ca_pems.clear();
  size_t first = body.find_first_not_of(" \t\r\n");
  bool is_xml = content_type.find("xml") != std::string::npos || (first != std::string::npos && body[first] == '<');
  auto valid = [](const std::string& pem) {
    std::string b = CsrBodyFromPem(pem), raw;
    return !b.empty() && Base64Decode(b, raw) && raw.size() > 16;
  };
  if (is_xml) {
    std::string err;
    std::unique_ptr<XmlNode> root = XmlParse(body, &err);
    if (!root || LocalName(root->tag) != "enrollment") {
      out.error = "enrollment XML: " + (root ? std::string("unexpected root") : err);
      return false;
    }
    for (const auto& c : root->children) {
      if (LocalName(c->tag) == "signedCert") out.cert_pem = ToCertPem(c->text);
      if (LocalName(c->tag) == "ca") out.ca_pems.push_back(ToCertPem(c->text));
    }
  } else {
    Json j;
    std::string err;
    if (!Json::Parse(body, j, &err) || !j.is_object()) {
      out.error = "enrollment JSON: " + err;
      return false;
    }
    out.cert_pem = j.has("signedCert") ? ToCertPem(j.str("signedCert")) : std::string();
    for (int i = 0; j.has("ca" + std::to_string(i)); ++i) out.ca_pems.push_back(ToCertPem(j.str("ca" + std::to_string(i))));
  }
  if (out.cert_pem.empty() || !valid(out.cert_pem)) {
    out.error = "no signed certificate in the reply";
    return false;
  }
  for (const auto& ca : out.ca_pems)
    if (!valid(ca)) {
      out.error = "malformed CA certificate in the reply";
      return false;
    }
  return true;
}

std::string BasicAuthHeader(const std::string& user, const std::string& password) {
  return "Basic " + Base64Encode(user + ":" + password);
}

std::string UrlEncode(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

bool ParseHttpResponse(const std::string& raw, HttpResponse& r) {
  size_t he = raw.find("\r\n\r\n");
  if (raw.compare(0, 5, "HTTP/") != 0 || he == std::string::npos) {
    r.error = "malformed HTTP response";
    return false;
  }
  size_t sp = raw.find(' ');
  r.status = std::atoi(raw.c_str() + sp + 1);
  std::string head = raw.substr(0, he), rest = raw.substr(he + 4);
  bool chunked = false;
  size_t content_length = std::string::npos;
  size_t pos = 0;
  while (pos < head.size()) {
    size_t nl = head.find("\r\n", pos);
    std::string line = head.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = nl == std::string::npos ? head.size() : nl + 2;
    size_t c = line.find(':');
    if (c == std::string::npos) continue;
    std::string k = line.substr(0, c), v = line.substr(c + 1);
    while (!v.empty() && v[0] == ' ') v.erase(0, 1);
    for (auto& ch : k) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (k == "content-type") r.content_type = v;
    if (k == "transfer-encoding" && v.find("chunked") != std::string::npos) chunked = true;
    if (k == "content-length") content_length = static_cast<size_t>(std::strtoul(v.c_str(), nullptr, 10));
  }
  if (chunked) {
    std::string out;
    size_t p2 = 0;
    while (p2 < rest.size()) {
      size_t nl = rest.find("\r\n", p2);
      if (nl == std::string::npos) break;
      size_t n = std::strtoul(rest.substr(p2, nl - p2).c_str(), nullptr, 16);
      if (n == 0) break;
      out += rest.substr(nl + 2, n);
      p2 = nl + 2 + n + 2;
    }
    rest = out;
  } else if (content_length != std::string::npos && rest.size() > content_length) {
    rest.resize(content_length);
  }
  r.body = rest;
  return true;
}

std::string BuildHttpRequest(const std::string& method, const std::string& host, int port, const std::string& path,
                             const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body) {
  std::string req = method + " " + path + " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
                    "\r\nConnection: close\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
  for (const auto& h : headers) req += h.first + ": " + h.second + "\r\n";
  return req + "\r\n" + body;
}

HttpExchange::HttpExchange(std::shared_ptr<ITransport> t) : t_(std::move(t)) { t_->SetListener(this); }

HttpExchange::~HttpExchange() {
  t_->Close();
  t_->SetListener(nullptr);
}

bool HttpExchange::Start(const std::string& method, const std::string& https_url,
                         const std::vector<std::pair<std::string, std::string>>& headers, const std::string& body,
                         TimeMs now, int timeout_ms) {
  const std::string pre = "https://";
  if (https_url.compare(0, pre.size(), pre) != 0) return false;
  std::string rest = https_url.substr(pre.size());
  size_t slash = rest.find('/');
  std::string hp = rest.substr(0, slash), path = slash == std::string::npos ? "/" : rest.substr(slash);
  size_t colon = hp.rfind(':');
  if (colon == std::string::npos) return false;
  std::string host = hp.substr(0, colon);
  int port = std::atoi(hp.c_str() + colon + 1);
  request_ = BuildHttpRequest(method, host, port, path, headers, body);
  deadline_ = now + timeout_ms;
  return t_->Open("tls://" + host + ":" + std::to_string(port));
}

void HttpExchange::OnTransportOpen() {
  std::lock_guard<std::mutex> l(mu_);
  opened_ = true;
}
void HttpExchange::OnTransportData(const char* data, size_t len) {
  std::lock_guard<std::mutex> l(mu_);
  if (overflow_) return;
  if (rx_.size() + len > kMaxResponseBytes) {
    overflow_ = true;
    rx_.clear();
    return;
  }
  rx_.append(data, len);
}
void HttpExchange::OnTransportClosed(const std::string& reason) {
  std::lock_guard<std::mutex> l(mu_);
  closed_ = true;
  close_reason_ = reason;
}

bool HttpExchange::Poll(TimeMs now, HttpResponse& out) {
  if (done_) return true;
  bool opened, closed, overflow, fresh;
  std::string rx, reason;
  {
    std::lock_guard<std::mutex> l(mu_);
    opened = opened_;
    closed = closed_;
    overflow = overflow_;
    fresh = rx_.size() != checked_;
    if (fresh || closed) rx = rx_;  // copy only when something new arrived or at the end
    checked_ = rx_.size();
    reason = close_reason_;
  }
  if (overflow) {
    out = HttpResponse();
    out.error = "response too large";
    done_ = true;
    t_->Close();
    return true;
  }
  if (opened && !sent_) {
    t_->Send(request_);
    sent_ = true;
  }
  // Done when the server closes (Connection: close), or once a Content-Length body is complete.
  bool complete = false;
  if (fresh && !rx.empty()) {
    HttpResponse probe;
    size_t he = rx.find("\r\n\r\n");
    if (he != std::string::npos && ParseHttpResponse(rx, probe)) {
      std::string lower = rx.substr(0, he);
      for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      size_t cl = lower.find("content-length:");
      if (cl != std::string::npos) complete = rx.size() - (he + 4) >= std::strtoul(lower.c_str() + cl + 15, nullptr, 10);
    }
  }
  if (closed || complete) {
    out = HttpResponse();
    if (rx.empty()) out.error = reason.empty() ? "connection closed" : reason;
    else ParseHttpResponse(rx, out);
    done_ = true;
    t_->Close();
    return true;
  }
  if (now > deadline_) {
    out = HttpResponse();
    out.error = "timeout";
    done_ = true;
    t_->Close();
    return true;
  }
  return false;
}

std::string TakEnrollment::ConfigUrl() const {
  return "https://" + cfg_.host + ":" + std::to_string(cfg_.port) + "/Marti/api/tls/config";
}

std::string TakEnrollment::SignUrl() const {
  std::string u = "https://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                  "/Marti/api/tls/signClient/v2?clientUid=" + UrlEncode(cfg_.client_uid);
  if (!cfg_.version.empty()) u += "&version=" + UrlEncode(cfg_.version);
  return u;
}

void TakEnrollment::Start(Done done) {
  done_ = std::move(done);
  result_ = EnrollResult();
  if (cfg_.host.empty() || cfg_.user.empty() || cfg_.client_uid.empty()) {
    Finish("host, user and client uid are required");
    return;
  }
  platform_.Https("GET", ConfigUrl(), {{"Authorization", BasicAuthHeader(cfg_.user, cfg_.password)}}, std::string(),
                  [this](const HttpResponse& r) { OnConfig(r); });
}

void TakEnrollment::OnConfig(const HttpResponse& r) {
  if (!r.error.empty()) return Finish("tls/config: " + r.error);
  if (r.status == 401 || r.status == 403) return Finish("tls/config: TAK user rejected (HTTP " + std::to_string(r.status) + ")");
  if (r.status != 200) return Finish("tls/config: HTTP " + std::to_string(r.status));
  std::vector<std::pair<std::string, std::string>> entries;
  std::string err;
  if (!ParseCertificateConfig(r.body, entries, &err)) return Finish("tls/config: " + err);
  std::vector<std::pair<std::string, std::string>> subject{{"CN", cfg_.user}};
  for (const auto& e : entries) subject.push_back(e);
  std::string csr;
  if (!platform_.MakeKeyAndCsr(subject, cfg_.key_bits, result_.key_ref, csr, err)) return Finish("CSR: " + err);
  platform_.Https("POST", SignUrl(),
                  {{"Authorization", BasicAuthHeader(cfg_.user, cfg_.password)},
                   {"Accept", "application/json"},
                   {"Content-Type", "text/plain"}},
                  CsrBodyFromPem(csr), [this](const HttpResponse& s) { OnSigned(s); });
}

void TakEnrollment::OnSigned(const HttpResponse& r) {
  if (!r.error.empty()) return Finish("signClient/v2: " + r.error);
  if (r.status != 200) return Finish("signClient/v2: HTTP " + std::to_string(r.status));
  EnrollResult parsed;
  if (!ParseSignClientV2(r.body, r.content_type, parsed)) return Finish("signClient/v2: " + parsed.error);
  result_.cert_pem = parsed.cert_pem;
  result_.ca_pems = parsed.ca_pems;
  Finish(std::string());
}

void TakEnrollment::Finish(const std::string& error) {
  result_.ok = error.empty();
  result_.error = error;
  if (done_) {
    Done d = std::move(done_);
    done_ = nullptr;
    d(result_);
  }
}

}  // namespace om
