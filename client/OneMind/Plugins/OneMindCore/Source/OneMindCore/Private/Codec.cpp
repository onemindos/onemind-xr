// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Codec.h"

#include <chrono>
#include <cstring>
#include <mutex>
#include <random>
#include <thread>

namespace om {

namespace codec_detail {
const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline uint32_t Rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
inline uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::vector<uint8_t> PadMessage(const uint8_t* data, size_t len) {
  std::vector<uint8_t> msg(data, data + len);
  uint64_t bits = static_cast<uint64_t>(len) * 8u;
  msg.push_back(0x80);
  while (msg.size() % 64 != 56) msg.push_back(0);
  for (int i = 7; i >= 0; --i) msg.push_back(static_cast<uint8_t>(bits >> (i * 8)));
  return msg;
}
}  // namespace codec_detail

std::string Base64Encode(const uint8_t* data, size_t len) {
  using codec_detail::kB64;
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  for (; i + 2 < len; i += 3) {
    uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
    out += kB64[(v >> 18) & 63];
    out += kB64[(v >> 12) & 63];
    out += kB64[(v >> 6) & 63];
    out += kB64[v & 63];
  }
  if (i < len) {
    uint32_t v = uint32_t(data[i]) << 16;
    if (i + 1 < len) v |= uint32_t(data[i + 1]) << 8;
    out += kB64[(v >> 18) & 63];
    out += kB64[(v >> 12) & 63];
    out += (i + 1 < len) ? kB64[(v >> 6) & 63] : '=';
    out += '=';
  }
  return out;
}

std::string Base64Encode(const std::string& s) {
  return Base64Encode(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

bool Base64Decode(const std::string& in, std::string& out) {
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  for (char c : in) {
    int v;
    if (c >= 'A' && c <= 'Z') v = c - 'A';
    else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
    else if (c >= '0' && c <= '9') v = c - '0' + 52;
    else if (c == '+' || c == '-') v = 62;
    else if (c == '/' || c == '_') v = 63;
    else if (c == '=') break;
    else if (c == '\r' || c == '\n' || c == ' ') continue;
    else return false;
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((acc >> bits) & 0xFF);
    }
  }
  return true;
}

std::string Base32LowerNoPad(const uint8_t* data, size_t len) {
  static const char kAlpha[] = "abcdefghijklmnopqrstuvwxyz234567";
  std::string out;
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = 0; i < len; ++i) {
    acc = (acc << 8) | data[i];
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      out += kAlpha[(acc >> bits) & 31];
    }
  }
  if (bits > 0) out += kAlpha[(acc << (5 - bits)) & 31];
  return out;
}

std::array<uint8_t, 20> Sha1(const uint8_t* data, size_t len) {
  using codec_detail::Rotl;
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::vector<uint8_t> msg = codec_detail::PadMessage(data, len);
  for (size_t off = 0; off < msg.size(); off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
      w[i] = (uint32_t(msg[off + i * 4]) << 24) | (uint32_t(msg[off + i * 4 + 1]) << 16) |
             (uint32_t(msg[off + i * 4 + 2]) << 8) | uint32_t(msg[off + i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) w[i] = Rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
      uint32_t t = Rotl(a, 5) + f + e + k + w[i];
      e = d; d = c; c = Rotl(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  }
  std::array<uint8_t, 20> out{};
  for (int i = 0; i < 5; ++i) {
    out[i * 4] = static_cast<uint8_t>(h[i] >> 24);
    out[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
    out[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
    out[i * 4 + 3] = static_cast<uint8_t>(h[i]);
  }
  return out;
}

std::array<uint8_t, 32> Sha256(const uint8_t* data, size_t len) {
  using codec_detail::Rotr;
  static const uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::vector<uint8_t> msg = codec_detail::PadMessage(data, len);
  for (size_t off = 0; off < msg.size(); off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (uint32_t(msg[off + i * 4]) << 24) | (uint32_t(msg[off + i * 4 + 1]) << 16) |
             (uint32_t(msg[off + i * 4 + 2]) << 8) | uint32_t(msg[off + i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + s1 + ch + k[i] + w[i];
      uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
      uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = s0 + mj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  std::array<uint8_t, 32> out{};
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<uint8_t>(h[i] >> 24);
    out[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
    out[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
    out[i * 4 + 3] = static_cast<uint8_t>(h[i]);
  }
  return out;
}

std::string HexLower(const uint8_t* data, size_t len) {
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out += kHex[data[i] >> 4];
    out += kHex[data[i] & 15];
  }
  return out;
}

std::string NewToken() {
  static std::mutex mu;
  static std::mt19937_64 rng([] {
    std::random_device rd;
    uint64_t seed = (static_cast<uint64_t>(rd()) << 32) ^ rd();
    seed ^= static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    seed ^= std::hash<std::thread::id>()(std::this_thread::get_id());
    return seed;
  }());
  static const char kAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  std::lock_guard<std::mutex> lock(mu);
  std::string out(22, 'A');
  for (auto& c : out) c = kAlpha[rng() % 62];
  return out;
}

}  // namespace om
