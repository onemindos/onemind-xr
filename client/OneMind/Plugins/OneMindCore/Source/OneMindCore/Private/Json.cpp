// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Json.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace om {

TimeMs WallClockMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

namespace json_detail {
const Json& NullValue() {
  static const Json kNull;
  return kNull;
}
const std::string& EmptyString() {
  static const std::string kEmpty;
  return kEmpty;
}
const Json::Array& EmptyArray() {
  static const Json::Array kArr;
  return kArr;
}
const Json::Object& EmptyObject() {
  static const Json::Object kObj;
  return kObj;
}

struct Parser {
  const char* p;
  const char* end;
  std::string err;
  int depth = 0;

  void Ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
  }
  bool Fail(const char* msg) {
    if (err.empty()) err = msg;
    return false;
  }
  static void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  bool Hex4(uint32_t& v) {
    if (end - p < 4) return Fail("short \\u escape");
    v = 0;
    for (int i = 0; i < 4; ++i) {
      char c = *p++;
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
      else return Fail("bad \\u escape");
    }
    return true;
  }
  bool String(std::string& out) {
    ++p;  // opening quote
    while (p < end) {
      char c = *p++;
      if (c == '"') return true;
      if (static_cast<unsigned char>(c) < 0x20) return Fail("control character in string");
      if (c != '\\') {
        out += c;
        continue;
      }
      if (p >= end) return Fail("unterminated escape");
      char e = *p++;
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          uint32_t cp = 0;
          if (!Hex4(cp)) return false;
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
              p += 2;
              uint32_t lo = 0;
              if (!Hex4(lo)) return false;
              if (lo < 0xDC00 || lo > 0xDFFF) return Fail("bad surrogate pair");
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            } else {
              return Fail("lone surrogate");
            }
          }
          AppendUtf8(out, cp);
          break;
        }
        default: return Fail("bad escape");
      }
    }
    return Fail("unterminated string");
  }
  bool Number(Json& out) {
    const char* start = p;
    if (p < end && *p == '-') ++p;
    if (p >= end) return Fail("bad number");
    if (*p == '0') {
      ++p;
    } else if (*p >= '1' && *p <= '9') {
      while (p < end && *p >= '0' && *p <= '9') ++p;
    } else {
      return Fail("bad number");
    }
    if (p < end && *p == '.') {
      ++p;
      if (p >= end || *p < '0' || *p > '9') return Fail("bad fraction");
      while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
      ++p;
      if (p < end && (*p == '+' || *p == '-')) ++p;
      if (p >= end || *p < '0' || *p > '9') return Fail("bad exponent");
      while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    std::string tmp(start, static_cast<size_t>(p - start));
    out = Json(std::strtod(tmp.c_str(), nullptr));
    return true;
  }
  bool Literal(const char* word, Json v, Json& out) {
    size_t n = std::strlen(word);
    if (static_cast<size_t>(end - p) < n || std::memcmp(p, word, n) != 0) return Fail("bad literal");
    p += n;
    out = std::move(v);
    return true;
  }
  bool Value(Json& out) {
    if (++depth > 256) return Fail("nesting too deep");
    Ws();
    if (p >= end) return Fail("unexpected end");
    bool ok = false;
    switch (*p) {
      case '{': ok = ObjectV(out); break;
      case '[': ok = ArrayV(out); break;
      case '"': {
        std::string s;
        ok = String(s);
        if (ok) out = Json(std::move(s));
        break;
      }
      case 't': ok = Literal("true", Json(true), out); break;
      case 'f': ok = Literal("false", Json(false), out); break;
      case 'n': ok = Literal("null", Json(), out); break;
      default: ok = Number(out); break;
    }
    --depth;
    return ok;
  }
  bool ArrayV(Json& out) {
    ++p;
    Json::Array arr;
    Ws();
    if (p < end && *p == ']') {
      ++p;
      out = Json(std::move(arr));
      return true;
    }
    while (true) {
      Json v;
      if (!Value(v)) return false;
      arr.push_back(std::move(v));
      Ws();
      if (p >= end) return Fail("unterminated array");
      if (*p == ',') { ++p; continue; }
      if (*p == ']') { ++p; break; }
      return Fail("expected , or ]");
    }
    out = Json(std::move(arr));
    return true;
  }
  bool ObjectV(Json& out) {
    ++p;
    Json::Object obj;
    Ws();
    if (p < end && *p == '}') {
      ++p;
      out = Json(std::move(obj));
      return true;
    }
    while (true) {
      Ws();
      if (p >= end || *p != '"') return Fail("expected key");
      std::string key;
      if (!String(key)) return false;
      Ws();
      if (p >= end || *p != ':') return Fail("expected :");
      ++p;
      Json v;
      if (!Value(v)) return false;
      obj[key] = std::move(v);
      Ws();
      if (p >= end) return Fail("unterminated object");
      if (*p == ',') { ++p; continue; }
      if (*p == '}') { ++p; break; }
      return Fail("expected , or }");
    }
    out = Json(std::move(obj));
    return true;
  }
};
}  // namespace json_detail

const std::string& Json::as_string() const { return is_string() ? s_ : json_detail::EmptyString(); }

size_t Json::size() const {
  if (is_array()) return a_->size();
  if (is_object()) return o_->size();
  return 0;
}

const Json& Json::operator[](size_t i) const {
  if (!is_array() || i >= a_->size()) return json_detail::NullValue();
  return (*a_)[i];
}

void Json::push_back(Json v) {
  if (!is_array()) {
    type_ = Type::Array;
    a_ = std::make_shared<Array>();
  } else if (a_.use_count() > 1) {
    a_ = std::make_shared<Array>(*a_);
  }
  a_->push_back(std::move(v));
}

const Json::Array& Json::items() const { return is_array() ? *a_ : json_detail::EmptyArray(); }

const Json& Json::operator[](const std::string& key) const {
  if (!is_object()) return json_detail::NullValue();
  auto it = o_->find(key);
  return it == o_->end() ? json_detail::NullValue() : it->second;
}

bool Json::has(const std::string& key) const { return is_object() && o_->count(key) > 0; }

void Json::set(const std::string& key, Json v) {
  if (!is_object()) {
    type_ = Type::Object;
    o_ = std::make_shared<Object>();
  } else if (o_.use_count() > 1) {
    o_ = std::make_shared<Object>(*o_);
  }
  (*o_)[key] = std::move(v);
}

const Json::Object& Json::fields() const { return is_object() ? *o_ : json_detail::EmptyObject(); }

std::string Json::str(const std::string& key, const std::string& def) const {
  const Json& v = (*this)[key];
  return v.is_string() ? v.s_ : def;
}
double Json::num(const std::string& key, double def) const {
  const Json& v = (*this)[key];
  return v.is_number() ? v.n_ : def;
}
int64_t Json::i64(const std::string& key, int64_t def) const {
  const Json& v = (*this)[key];
  return v.is_number() ? static_cast<int64_t>(v.n_) : def;
}
bool Json::boolean(const std::string& key, bool def) const {
  const Json& v = (*this)[key];
  return v.is_bool() ? v.b_ : def;
}

bool Json::Parse(const std::string& text, Json& out, std::string* err) {
  return Parse(text.data(), text.size(), out, err);
}

bool Json::Parse(const char* data, size_t len, Json& out, std::string* err) {
  json_detail::Parser ps{data, data + len, std::string(), 0};
  Json v;
  bool ok = ps.Value(v);
  if (ok) {
    ps.Ws();
    if (ps.p != ps.end) ok = ps.Fail("trailing characters");
  }
  if (!ok) {
    if (err) *err = ps.err;
    return false;
  }
  out = std::move(v);
  return true;
}

std::string JsonQuote(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  out += '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  out += '"';
  return out;
}

void Json::DumpTo(std::string& out) const {
  switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += b_ ? "true" : "false"; break;
    case Type::Number: {
      if (!std::isfinite(n_)) {
        out += "null";
      } else if (std::fabs(n_) < 9.007199254740992e15 && std::floor(n_) == n_) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n_));
        out += buf;
      } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.17g", n_);
        out += buf;
      }
      break;
    }
    case Type::String: out += JsonQuote(s_); break;
    case Type::Array: {
      out += '[';
      bool first = true;
      for (const auto& v : *a_) {
        if (!first) out += ',';
        first = false;
        v.DumpTo(out);
      }
      out += ']';
      break;
    }
    case Type::Object: {
      out += '{';
      bool first = true;
      for (const auto& kv : *o_) {
        if (!first) out += ',';
        first = false;
        out += JsonQuote(kv.first);
        out += ':';
        kv.second.DumpTo(out);
      }
      out += '}';
      break;
    }
  }
}

std::string Json::Dump() const {
  std::string out;
  DumpTo(out);
  return out;
}

}  // namespace om
