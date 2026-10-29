// Small codecs the core needs without third-party libraries: Base64 (RFC 4648),
// Base32 (RFC 4648, lowercase output), SHA-1 (FIPS 180-4, used only for the
// WebSocket handshake check, RFC 6455 §4.2.2) and SHA-256 (entity id hashing,
// entity.md §3), plus unique ids for NATS inboxes.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

#include <array>
#include <string>
#include <vector>

namespace om {

OM_CORE_API std::string Base64Encode(const uint8_t* data, size_t len);
OM_CORE_API std::string Base64Encode(const std::string& s);
/// Decodes standard Base64 (padding optional). Returns false on invalid input.
OM_CORE_API bool Base64Decode(const std::string& in, std::string& out);

/// RFC 4648 Base32, lowercase, no padding.
OM_CORE_API std::string Base32LowerNoPad(const uint8_t* data, size_t len);

OM_CORE_API std::array<uint8_t, 20> Sha1(const uint8_t* data, size_t len);
OM_CORE_API std::array<uint8_t, 32> Sha256(const uint8_t* data, size_t len);
OM_CORE_API std::string HexLower(const uint8_t* data, size_t len);

/// Unique token of [A-Za-z0-9], 22 characters, from a per-process seeded PRNG.
/// Used for inbox subjects and request tokens, never for security.
OM_CORE_API std::string NewToken();

}  // namespace om
