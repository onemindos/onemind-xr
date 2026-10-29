// NATS client protocol (text protocol with headers), transport-independent.
// Reference: https://docs.nats.io/reference/reference-protocols/nats-protocol
// and ADR-4 (headers: "NATS/1.0[ status [description]]\r\n" then MIME-style lines).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

#include <string>
#include <utility>
#include <vector>

namespace om {

/// Ordered multimap of NATS headers plus the optional status line.
struct OM_CORE_API NatsHeaders {
  int status = 0;            // 0 = no status
  std::string description;   // e.g. "Idle Heartbeat", "No Messages"
  std::vector<std::pair<std::string, std::string>> kv;

  bool empty() const { return status == 0 && kv.empty(); }
  /// Case-sensitive lookup first, then case-insensitive (servers use canonical case).
  const std::string* Get(const std::string& key) const;
  std::string GetOr(const std::string& key, const std::string& def = std::string()) const;
  void Add(const std::string& key, const std::string& value) { kv.emplace_back(key, value); }

  static bool Parse(const std::string& block, NatsHeaders& out);
  std::string Serialize() const;
};

struct OM_CORE_API NatsMsg {
  std::string subject;
  std::string reply;
  int64_t sid = 0;
  NatsHeaders headers;
  std::string data;
};

/// One parsed server operation.
struct OM_CORE_API NatsOp {
  enum class Kind { Info, Msg, Ping, Pong, Ok, Err };
  Kind kind = Kind::Ok;
  std::string text;  // INFO json or -ERR text
  NatsMsg msg;       // MSG / HMSG
};

/// Streaming parser for server-to-client operations. Feed arbitrary chunks.
class OM_CORE_API NatsParser {
 public:
  /// Appends complete operations to `out`. Returns false on a protocol error.
  bool Feed(const char* data, size_t len, std::vector<NatsOp>& out, std::string* err = nullptr);
  void Reset() { buf_.clear(); }
  size_t max_payload = 64u * 1024u * 1024u;

 private:
  std::string buf_;
};

/// Client-to-server operations.
namespace nats_proto {
OM_CORE_API std::string Connect(const std::string& json);
OM_CORE_API std::string Pub(const std::string& subject, const std::string& reply, const std::string& data);
OM_CORE_API std::string HPub(const std::string& subject, const std::string& reply, const NatsHeaders& h,
                             const std::string& data);
OM_CORE_API std::string Sub(const std::string& subject, const std::string& queue, int64_t sid);
OM_CORE_API std::string Unsub(int64_t sid, int max_msgs = 0);
inline std::string Ping() { return "PING\r\n"; }
inline std::string Pong() { return "PONG\r\n"; }
/// Subject token rules (no whitespace, no empty tokens). Wildcards allowed only if `allow_wildcards`.
OM_CORE_API bool ValidSubject(const std::string& s, bool allow_wildcards);
/// True if `subject` matches `pattern` with NATS wildcards `*` and `>`.
OM_CORE_API bool SubjectMatches(const std::string& pattern, const std::string& subject);
OM_CORE_API std::vector<std::string> SplitTokens(const std::string& subject);
}  // namespace nats_proto

}  // namespace om
