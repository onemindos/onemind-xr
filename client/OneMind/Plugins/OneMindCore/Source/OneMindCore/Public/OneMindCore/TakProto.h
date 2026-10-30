// TAK Protocol version 1 for the OneMind client (feature C3): the protobuf TakMessage
// (TakControl + CotEvent), CoT XML <-> TakMessage conversion, and the mesh and streaming
// headers. Written from the published spec, atak-civ `takproto/README.txt` and
// `takproto/*.proto` at commit 9f6893dd (docs/research/tak-protocols.md S1, S2); no Commo
// or TAK Server code is used. Hand-written protobuf wire format, no protobuf library.
//
// Encoding: the whole <detail> goes into xmlDetail except the six elements that have a
// typed message (contact, __group, precisionlocation, status, takv, track), which are
// converted only when the whole element fits the message (detail.proto "WHOLE ELEMENTS"
// rule): exactly one occurrence, no children or text, only the message's attributes and
// every required one present. Anything else stays XML.
// Decoding: typed messages are merged back as XML elements unless xmlDetail already holds
// an element of the same name (receiver rule 5 in detail.proto).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

#include <string>
#include <vector>

namespace om {

constexpr unsigned char kTakMagic = 0xbf;

struct OM_CORE_API TakControlMsg {
  uint32_t min_version = 0;
  uint32_t max_version = 0;
  std::string contact_uid;
  std::vector<uint32_t> extension_ids;
};

struct OM_CORE_API TakDecoded {
  bool has_event = false;
  std::string xml;  // CoT XML without declaration
  bool has_control = false;
  TakControlMsg control;
  int undecoded_extensions = 0;  // extensionDetails entries the client cannot decode
};

/// Unsigned protobuf varint (README.txt "varint" section).
OM_CORE_API void PutVarint(std::string& out, uint64_t v);
/// Reads a varint at `pos`; advances `pos`. Returns false if truncated or over 10 bytes.
OM_CORE_API bool GetVarint(const char* p, size_t len, size_t& pos, uint64_t& v);

/// CoT XML (one <event>) -> TakMessage bytes with field cotEvent set.
OM_CORE_API bool CotXmlToTakPayload(const std::string& xml, std::string& payload, std::string* err = nullptr);
/// TakMessage bytes with only takControl set.
OM_CORE_API std::string TakControlPayload(const TakControlMsg& c);
/// TakMessage bytes -> XML and/or control. Rejects malformed protobuf.
OM_CORE_API bool DecodeTakPayload(const std::string& payload, TakDecoded& out, std::string* err = nullptr);

/// Mesh / directed v1 message: 0xbf 0x01 0xbf + payload.
OM_CORE_API std::string FrameTakMesh(const std::string& payload);
/// Streaming v1 message: 0xbf + varint(len) + payload.
OM_CORE_API std::string FrameTakStream(const std::string& payload);
/// True if the bytes start with the TAK Protocol magic byte (else legacy XML).
OM_CORE_API bool IsTakProtocol(const std::string& bytes);
/// Decodes one mesh or directed message (v1 frame or legacy XML) into `out`.
OM_CORE_API bool DecodeTakMeshMessage(const std::string& bytes, TakDecoded& out, std::string* err = nullptr);

/// Shortest decimal that parses back to the same double (CoT attributes from protobuf).
OM_CORE_API std::string FormatDoubleRoundTrip(double v);

}  // namespace om
