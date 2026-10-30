// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/NatsProtocol.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace om {

namespace nats_detail {
bool IEquals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}
std::string TrimWs(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
  return s.substr(a, b - a);
}
std::vector<std::string> SplitArgs(const std::string& s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    size_t j = i;
    while (j < s.size() && s[j] != ' ' && s[j] != '\t') ++j;
    if (j > i) out.push_back(s.substr(i, j - i));
    i = j;
  }
  return out;
}
bool ToInt(const std::string& s, int64_t& v) {
  if (s.empty()) return false;
  v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
    if (v > (int64_t(1) << 40)) return false;
  }
  return true;
}
}  // namespace nats_detail

const std::string* NatsHeaders::Get(const std::string& key) const {
  for (const auto& p : kv)
    if (p.first == key) return &p.second;
  for (const auto& p : kv)
    if (nats_detail::IEquals(p.first, key)) return &p.second;
  return nullptr;
}

std::string NatsHeaders::GetOr(const std::string& key, const std::string& def) const {
  const std::string* v = Get(key);
  return v ? *v : def;
}

bool NatsHeaders::Parse(const std::string& block, NatsHeaders& out) {
  out = NatsHeaders();
  size_t eol = block.find("\r\n");
  if (eol == std::string::npos) return false;
  std::string first = block.substr(0, eol);
  if (first.rfind("NATS/1.0", 0) != 0) return false;
  std::string rest = nats_detail::TrimWs(first.substr(8));
  if (!rest.empty()) {
    size_t sp = rest.find(' ');
    std::string code = sp == std::string::npos ? rest : rest.substr(0, sp);
    int64_t c = 0;
    if (nats_detail::ToInt(code, c)) out.status = static_cast<int>(c);
    if (sp != std::string::npos) out.description = nats_detail::TrimWs(rest.substr(sp + 1));
  }
  size_t pos = eol + 2;
  while (pos < block.size()) {
    size_t e = block.find("\r\n", pos);
    if (e == std::string::npos) e = block.size();
    std::string line = block.substr(pos, e - pos);
    pos = e + 2;
    if (line.empty()) continue;
    size_t c = line.find(':');
    if (c == std::string::npos) continue;
    out.kv.emplace_back(nats_detail::TrimWs(line.substr(0, c)), nats_detail::TrimWs(line.substr(c + 1)));
  }
  return true;
}

bool NatsHeaders::ValidKey(const std::string& key) {
  if (key.empty()) return false;
  for (char ch : key) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (c < 33 || c > 126 || c == ':') return false;
  }
  return true;
}

bool NatsHeaders::ValidValue(const std::string& value) {
  return value.find_first_of(std::string("\r\n\0", 3)) == std::string::npos;
}

bool NatsHeaders::Add(const std::string& key, const std::string& value) {
  if (!ValidKey(key) || !ValidValue(value)) return false;
  kv.emplace_back(key, value);
  return true;
}

bool NatsHeaders::Valid() const {
  if (!ValidValue(description)) return false;
  for (const auto& p : kv)
    if (!ValidKey(p.first) || !ValidValue(p.second)) return false;
  return true;
}

std::string NatsHeaders::Serialize() const {
  if (!Valid()) return std::string();
  std::string s = "NATS/1.0";
  if (status) {
    s += " " + std::to_string(status);
    if (!description.empty()) s += " " + description;
  }
  s += "\r\n";
  for (const auto& p : kv) s += p.first + ": " + p.second + "\r\n";
  s += "\r\n";
  return s;
}

bool NatsParser::Feed(const char* data, size_t len, std::vector<NatsOp>& out, std::string* err) {
  using namespace nats_detail;
  auto fail = [&](const std::string& m) {
    if (err) *err = m;
    return false;
  };
  buf_.append(data, len);
  size_t pos = 0;
  while (pos < buf_.size()) {
    size_t eol = buf_.find("\r\n", pos);
    if (eol == std::string::npos) {
      if (buf_.size() - pos > 64 * 1024) return fail("control line too long");
      break;
    }
    std::string line = buf_.substr(pos, eol - pos);
    // Operation names are case-insensitive in the protocol.
    std::string verb;
    size_t sp = line.find_first_of(" \t");
    verb = sp == std::string::npos ? line : line.substr(0, sp);
    for (auto& c : verb) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    std::string args = sp == std::string::npos ? std::string() : line.substr(sp + 1);

    if (verb == "MSG" || verb == "HMSG") {
      bool has_hdr = verb == "HMSG";
      auto a = SplitArgs(args);
      // MSG <subject> <sid> [reply] <#bytes>; HMSG <subject> <sid> [reply] <#hdr> <#total>
      size_t need_min = has_hdr ? 4 : 3;
      if (a.size() < need_min || a.size() > need_min + 1) return fail("bad " + verb + " line: " + line);
      NatsOp op;
      op.kind = NatsOp::Kind::Msg;
      op.msg.subject = a[0];
      int64_t sid = 0, hdr = 0, total = 0;
      if (!ToInt(a[1], sid)) return fail("bad sid");
      op.msg.sid = sid;
      bool has_reply = a.size() == need_min + 1;
      if (has_reply) op.msg.reply = a[2];
      size_t idx = has_reply ? 3 : 2;
      if (has_hdr) {
        if (!ToInt(a[idx], hdr) || !ToInt(a[idx + 1], total) || hdr > total) return fail("bad HMSG sizes");
      } else {
        if (!ToInt(a[idx], total)) return fail("bad MSG size");
      }
      if (static_cast<size_t>(total) > max_payload) return fail("payload too large");
      size_t body_start = eol + 2;
      if (buf_.size() < body_start + static_cast<size_t>(total) + 2) break;  // wait for the rest
      if (has_hdr) {
        std::string hblock = buf_.substr(body_start, static_cast<size_t>(hdr));
        if (!NatsHeaders::Parse(hblock, op.msg.headers)) return fail("bad header block");
        op.msg.data = buf_.substr(body_start + static_cast<size_t>(hdr), static_cast<size_t>(total - hdr));
      } else {
        op.msg.data = buf_.substr(body_start, static_cast<size_t>(total));
      }
      if (buf_.compare(body_start + static_cast<size_t>(total), 2, "\r\n") != 0) return fail("missing CRLF after payload");
      pos = body_start + static_cast<size_t>(total) + 2;
      out.push_back(std::move(op));
      continue;
    }
    NatsOp op;
    if (verb == "PING") {
      op.kind = NatsOp::Kind::Ping;
    } else if (verb == "PONG") {
      op.kind = NatsOp::Kind::Pong;
    } else if (verb == "+OK") {
      op.kind = NatsOp::Kind::Ok;
    } else if (verb == "-ERR") {
      op.kind = NatsOp::Kind::Err;
      op.text = TrimWs(args);
    } else if (verb == "INFO") {
      op.kind = NatsOp::Kind::Info;
      op.text = TrimWs(args);
    } else if (line.empty()) {
      pos = eol + 2;
      continue;
    } else {
      return fail("unknown operation: " + line.substr(0, 40));
    }
    pos = eol + 2;
    out.push_back(std::move(op));
  }
  buf_.erase(0, pos);
  return true;
}

namespace nats_proto {

std::string Connect(const std::string& json) { return "CONNECT " + json + "\r\n"; }

std::string Pub(const std::string& subject, const std::string& reply, const std::string& data) {
  std::string s = "PUB " + subject;
  if (!reply.empty()) s += " " + reply;
  s += " " + std::to_string(data.size()) + "\r\n";
  s += data;
  s += "\r\n";
  return s;
}

std::string HPub(const std::string& subject, const std::string& reply, const NatsHeaders& h,
                 const std::string& data) {
  std::string hb = h.Serialize();
  if (hb.empty()) return std::string();  // invalid headers: refuse to frame
  std::string s = "HPUB " + subject;
  if (!reply.empty()) s += " " + reply;
  s += " " + std::to_string(hb.size()) + " " + std::to_string(hb.size() + data.size()) + "\r\n";
  s += hb;
  s += data;
  s += "\r\n";
  return s;
}

std::string Sub(const std::string& subject, const std::string& queue, int64_t sid) {
  std::string s = "SUB " + subject;
  if (!queue.empty()) s += " " + queue;
  s += " " + std::to_string(sid) + "\r\n";
  return s;
}

std::string Unsub(int64_t sid, int max_msgs) {
  std::string s = "UNSUB " + std::to_string(sid);
  if (max_msgs > 0) s += " " + std::to_string(max_msgs);
  return s + "\r\n";
}

std::vector<std::string> SplitTokens(const std::string& subject) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    size_t dot = subject.find('.', start);
    out.push_back(subject.substr(start, dot == std::string::npos ? std::string::npos : dot - start));
    if (dot == std::string::npos) break;
    start = dot + 1;
  }
  return out;
}

bool ValidSubject(const std::string& s, bool allow_wildcards) {
  if (s.empty()) return false;
  auto toks = SplitTokens(s);
  for (size_t i = 0; i < toks.size(); ++i) {
    const auto& t = toks[i];
    if (t.empty()) return false;
    for (char c : t) {
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return false;
    }
    if (t.find('*') != std::string::npos || t.find('>') != std::string::npos) {
      if (!allow_wildcards) return false;
      if (t != "*" && t != ">") return false;
      if (t == ">" && i + 1 != toks.size()) return false;
    }
  }
  return true;
}

bool SubjectMatches(const std::string& pattern, const std::string& subject) {
  auto p = SplitTokens(pattern);
  auto s = SplitTokens(subject);
  for (size_t i = 0; i < p.size(); ++i) {
    if (p[i] == ">") return s.size() > i;
    if (i >= s.size()) return false;
    if (p[i] != "*" && p[i] != s[i]) return false;
  }
  return p.size() == s.size();
}

}  // namespace nats_proto
}  // namespace om
