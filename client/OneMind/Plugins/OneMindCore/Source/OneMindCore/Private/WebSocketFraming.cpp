// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/WebSocketFraming.h"

#include "OneMindCore/Codec.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace om {

namespace ws_detail {
std::string Lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
}  // namespace ws_detail

bool WsUrl::Parse(const std::string& url, WsUrl& out) {
  WsUrl u;
  std::string rest;
  if (url.rfind("ws://", 0) == 0) {
    rest = url.substr(5);
  } else if (url.rfind("wss://", 0) == 0) {
    u.secure = true;
    u.port = 443;
    rest = url.substr(6);
  } else {
    return false;
  }
  size_t slash = rest.find('/');
  std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
  u.path = slash == std::string::npos ? "/" : rest.substr(slash);
  size_t at = hostport.rfind('@');
  if (at != std::string::npos) hostport = hostport.substr(at + 1);  // credentials never go in the URL we send
  size_t colon = hostport.rfind(':');
  if (colon != std::string::npos && hostport.find(']') == std::string::npos) {
    u.host = hostport.substr(0, colon);
    u.port = std::atoi(hostport.substr(colon + 1).c_str());
  } else {
    u.host = hostport;
  }
  if (u.host.empty() || u.port <= 0 || u.port > 65535) return false;
  out = u;
  return true;
}

std::string WsBuildHandshake(const WsUrl& url, const std::string& key_b64) {
  std::string host = url.host;
  bool default_port = (!url.secure && url.port == 80) || (url.secure && url.port == 443);
  if (!default_port) host += ":" + std::to_string(url.port);
  std::string req;
  req += "GET " + url.path + " HTTP/1.1\r\n";
  req += "Host: " + host + "\r\n";
  req += "Upgrade: websocket\r\n";
  req += "Connection: Upgrade\r\n";
  req += "Sec-WebSocket-Key: " + key_b64 + "\r\n";
  req += "Sec-WebSocket-Version: 13\r\n";
  req += "User-Agent: OneMind-Client/0.2\r\n";
  req += "\r\n";
  return req;
}

std::string WsAcceptFor(const std::string& key_b64) {
  std::string s = key_b64 + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  auto digest = Sha1(reinterpret_cast<const uint8_t*>(s.data()), s.size());
  return Base64Encode(digest.data(), digest.size());
}

bool WsCheckHandshakeResponse(const std::string& resp, const std::string& key_b64, std::string* err) {
  auto fail = [&](const char* m) {
    if (err) *err = m;
    return false;
  };
  size_t eol = resp.find("\r\n");
  if (eol == std::string::npos) return fail("no status line");
  std::string status = resp.substr(0, eol);
  if (status.find(" 101") == std::string::npos) return fail("status is not 101");
  bool upgrade = false, connection = false, accept = false;
  size_t pos = eol + 2;
  while (pos < resp.size()) {
    size_t e = resp.find("\r\n", pos);
    if (e == std::string::npos) e = resp.size();
    std::string line = resp.substr(pos, e - pos);
    pos = e + 2;
    if (line.empty()) break;
    size_t c = line.find(':');
    if (c == std::string::npos) continue;
    std::string k = ws_detail::Lower(ws_detail::Trim(line.substr(0, c)));
    std::string v = ws_detail::Trim(line.substr(c + 1));
    if (k == "upgrade" && ws_detail::Lower(v) == "websocket") upgrade = true;
    if (k == "connection" && ws_detail::Lower(v).find("upgrade") != std::string::npos) connection = true;
    if (k == "sec-websocket-accept" && v == WsAcceptFor(key_b64)) accept = true;
  }
  if (!upgrade) return fail("missing Upgrade: websocket");
  if (!connection) return fail("missing Connection: Upgrade");
  if (!accept) return fail("bad Sec-WebSocket-Accept");
  return true;
}

std::string WsEncodeFrame(WsOpcode op, const std::string& payload, const uint8_t mask_key[4]) {
  std::string f;
  f.reserve(payload.size() + 14);
  f += static_cast<char>(0x80 | static_cast<uint8_t>(op));
  size_t n = payload.size();
  if (n < 126) {
    f += static_cast<char>(0x80 | n);
  } else if (n <= 0xFFFF) {
    f += static_cast<char>(0x80 | 126);
    f += static_cast<char>((n >> 8) & 0xFF);
    f += static_cast<char>(n & 0xFF);
  } else {
    f += static_cast<char>(0x80 | 127);
    for (int i = 7; i >= 0; --i) f += static_cast<char>((static_cast<uint64_t>(n) >> (i * 8)) & 0xFF);
  }
  for (int i = 0; i < 4; ++i) f += static_cast<char>(mask_key[i]);
  for (size_t i = 0; i < n; ++i) f += static_cast<char>(payload[i] ^ static_cast<char>(mask_key[i % 4]));
  return f;
}

void WsDecoder::Reset() {
  buf_.clear();
  frag_.clear();
  in_frag_ = false;
}

bool WsDecoder::Feed(const char* data, size_t len, std::vector<Message>& out, std::string* err) {
  auto fail = [&](const char* m) {
    if (err) *err = m;
    return false;
  };
  buf_.append(data, len);
  size_t pos = 0;
  while (true) {
    size_t avail = buf_.size() - pos;
    if (avail < 2) break;
    const uint8_t* b = reinterpret_cast<const uint8_t*>(buf_.data() + pos);
    bool fin = (b[0] & 0x80) != 0;
    if (b[0] & 0x70) return fail("reserved bits set (compression was not negotiated)");
    auto op = static_cast<WsOpcode>(b[0] & 0x0F);
    bool masked = (b[1] & 0x80) != 0;
    uint64_t plen = b[1] & 0x7F;
    size_t hdr = 2;
    if (plen == 126) {
      if (avail < 4) break;
      plen = (uint64_t(b[2]) << 8) | b[3];
      hdr = 4;
    } else if (plen == 127) {
      if (avail < 10) break;
      plen = 0;
      for (int i = 0; i < 8; ++i) plen = (plen << 8) | b[2 + i];
      hdr = 10;
    }
    if (plen > max_message_bytes) return fail("frame too large");
    size_t need = hdr + (masked ? 4 : 0) + static_cast<size_t>(plen);
    if (avail < need) break;
    std::string payload(buf_.data() + pos + hdr + (masked ? 4 : 0), static_cast<size_t>(plen));
    if (masked) {
      const uint8_t* mk = b + hdr;
      for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(payload[i] ^ mk[i % 4]);
    }
    pos += need;
    bool control = (static_cast<uint8_t>(op) & 0x08) != 0;
    if (control) {
      if (!fin || plen > 125) return fail("bad control frame");
      out.push_back(Message{op, std::move(payload)});
      continue;
    }
    if (op == WsOpcode::Continuation) {
      if (!in_frag_) return fail("continuation without start");
      frag_ += payload;
      if (frag_.size() > max_message_bytes) return fail("message too large");
      if (fin) {
        out.push_back(Message{frag_op_, std::move(frag_)});
        frag_.clear();
        in_frag_ = false;
      }
    } else if (op == WsOpcode::Text || op == WsOpcode::Binary) {
      if (in_frag_) return fail("new message inside fragmented message");
      if (fin) {
        out.push_back(Message{op, std::move(payload)});
      } else {
        in_frag_ = true;
        frag_op_ = op;
        frag_ = std::move(payload);
      }
    } else {
      return fail("unknown opcode");
    }
  }
  buf_.erase(0, pos);
  return true;
}

}  // namespace om
