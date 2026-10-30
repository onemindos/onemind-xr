// RFC 6455 client-side framing and handshake, engine-independent.
//
// Inside Unreal the engine's WebSockets module does the framing, so the OneMindBus
// plugin hands whole binary messages to the NATS client. This codec is used by the
// host transport (HostTests/host) and is unit-tested on its own, so both paths feed
// the same NATS protocol core.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

#include <functional>
#include <string>
#include <vector>

namespace om {

struct OM_CORE_API WsUrl {
  bool secure = false;
  std::string host;
  int port = 80;
  std::string path = "/";
  /// Parses ws://host[:port][/path] and wss://...; returns false otherwise.
  static bool Parse(const std::string& url, WsUrl& out);
};

/// Builds the HTTP/1.1 upgrade request. `key_b64` is the Sec-WebSocket-Key.
OM_CORE_API std::string WsBuildHandshake(const WsUrl& url, const std::string& key_b64);
/// Expected Sec-WebSocket-Accept for a key (RFC 6455 §4.2.2: SHA-1 of key + GUID, Base64).
OM_CORE_API std::string WsAcceptFor(const std::string& key_b64);
/// Validates the server's response headers (status 101, Upgrade, Connection, Accept).
OM_CORE_API bool WsCheckHandshakeResponse(const std::string& response_headers,
                                          const std::string& key_b64, std::string* err);

enum class WsOpcode : uint8_t { Continuation = 0, Text = 1, Binary = 2, Close = 8, Ping = 9, Pong = 10 };

/// Encodes one client frame (always masked, FIN set). `mask_key` is 4 bytes.
OM_CORE_API std::string WsEncodeFrame(WsOpcode op, const std::string& payload, const uint8_t mask_key[4]);

/// Incremental decoder for server frames (unmasked per RFC 6455 §5.1, masked accepted).
/// Reassembles fragmented messages. Control frames are delivered immediately.
class OM_CORE_API WsDecoder {
 public:
  struct Message {
    WsOpcode op;
    std::string payload;
  };
  /// Feed raw bytes; complete messages are appended to `out`.
  /// Returns false on a protocol error (the connection must be closed).
  bool Feed(const char* data, size_t len, std::vector<Message>& out, std::string* err = nullptr);
  void Reset();
  size_t max_message_bytes = 16u * 1024u * 1024u;

 private:
  std::string buf_;
  std::string frag_;
  WsOpcode frag_op_ = WsOpcode::Continuation;
  bool in_frag_ = false;
};

}  // namespace om
