// SPDX-License-Identifier: GPL-3.0-or-later
// TAK Protocol v1, from atak-civ takproto/README.txt and takproto/*.proto (see TakProto.h).
#include "OneMindCore/TakProto.h"

#include "OneMindCore/CoT.h"
#include "OneMindCore/Xml.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

namespace om {

namespace {

// --- protobuf wire helpers --------------------------------------------------------------

void PutTag(std::string& out, uint32_t field, uint32_t wire) { PutVarint(out, (static_cast<uint64_t>(field) << 3) | wire); }

void PutString(std::string& out, uint32_t field, const std::string& s) {
  if (s.empty()) return;  // proto3: default values are not written
  PutTag(out, field, 2);
  PutVarint(out, s.size());
  out += s;
}

void PutU64(std::string& out, uint32_t field, uint64_t v) {
  if (v == 0) return;
  PutTag(out, field, 0);
  PutVarint(out, v);
}

void PutDouble(std::string& out, uint32_t field, double v) {
  if (v == 0.0 && !std::signbit(v)) return;
  PutTag(out, field, 1);
  uint64_t bits;
  std::memcpy(&bits, &v, 8);
  for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>((bits >> (8 * i)) & 0xff));
}

void PutMessage(std::string& out, uint32_t field, const std::string& body, bool force = false) {
  if (body.empty() && !force) return;
  PutTag(out, field, 2);
  PutVarint(out, body.size());
  out += body;
}

struct Field {
  uint32_t num = 0;
  uint32_t wire = 0;
  uint64_t varint = 0;
  double dbl = 0;
  std::string bytes;
};

// Splits one message into fields; false on malformed input.
bool ReadFields(const std::string& msg, std::vector<Field>& out) {
  size_t pos = 0;
  const char* p = msg.data();
  while (pos < msg.size()) {
    uint64_t key;
    if (!GetVarint(p, msg.size(), pos, key)) return false;
    Field f;
    f.num = static_cast<uint32_t>(key >> 3);
    f.wire = static_cast<uint32_t>(key & 7);
    if (f.num == 0) return false;
    switch (f.wire) {
      case 0:
        if (!GetVarint(p, msg.size(), pos, f.varint)) return false;
        break;
      case 1: {
        if (msg.size() - pos < 8) return false;
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits |= static_cast<uint64_t>(static_cast<unsigned char>(p[pos + i])) << (8 * i);
        std::memcpy(&f.dbl, &bits, 8);
        pos += 8;
        break;
      }
      case 2: {
        uint64_t n;
        if (!GetVarint(p, msg.size(), pos, n) || n > msg.size() - pos) return false;
        f.bytes.assign(p + pos, static_cast<size_t>(n));
        pos += static_cast<size_t>(n);
        break;
      }
      case 5:
        if (msg.size() - pos < 4) return false;
        pos += 4;
        break;
      default:
        return false;  // groups (3, 4) are not used by TAK Protocol
    }
    out.push_back(std::move(f));
  }
  return true;
}

bool ParseDouble(const std::string& s, double& v) {
  if (s.empty()) return false;
  char* end = nullptr;
  v = std::strtod(s.c_str(), &end);
  return end && *end == 0 && std::isfinite(v);
}

bool ParseU32(const std::string& s, uint32_t& v) {
  if (s.empty() || s.size() > 10) return false;
  for (char c : s)
    if (c < '0' || c > '9') return false;
  unsigned long long x = std::strtoull(s.c_str(), nullptr, 10);
  if (x > 0xffffffffULL) return false;
  v = static_cast<uint32_t>(x);
  return true;
}

// Typed detail elements: tag -> (protobuf field, attributes in field order, optional set).
struct TypedSpec {
  uint32_t field;
  std::vector<std::string> attrs;
  std::set<std::string> optional;
  enum Kind { Strings, Track, Status } kind;
};

const std::map<std::string, TypedSpec>& TypedSpecs() {
  static const std::map<std::string, TypedSpec> s = {
      {"contact", {2, {"endpoint", "callsign"}, {"endpoint"}, TypedSpec::Strings}},
      {"__group", {3, {"name", "role"}, {}, TypedSpec::Strings}},
      {"precisionlocation", {4, {"geopointsrc", "altsrc"}, {}, TypedSpec::Strings}},
      {"status", {5, {"battery"}, {}, TypedSpec::Status}},
      {"takv", {6, {"device", "platform", "os", "version"}, {}, TypedSpec::Strings}},
      {"track", {7, {"speed", "course"}, {}, TypedSpec::Track}},
  };
  return s;
}

// Tries to encode one element as its typed message (whole-element rule).
bool EncodeTyped(const XmlNode& n, const TypedSpec& spec, std::string& body) {
  if (!n.children.empty() || !n.text.empty()) return false;
  std::map<std::string, std::string> have;
  for (const auto& a : n.attrs) {
    bool known = false;
    for (const auto& k : spec.attrs) known = known || k == a.first;
    if (!known || have.count(a.first)) return false;
    have[a.first] = a.second;
  }
  for (const auto& k : spec.attrs)
    if (!have.count(k) && !spec.optional.count(k)) return false;
  body.clear();
  if (spec.kind == TypedSpec::Track) {
    double speed, course;
    if (!ParseDouble(have["speed"], speed) || !ParseDouble(have["course"], course)) return false;
    PutDouble(body, 1, speed);
    PutDouble(body, 2, course);
    return true;
  }
  if (spec.kind == TypedSpec::Status) {
    uint32_t b;
    if (!ParseU32(have["battery"], b)) return false;
    PutU64(body, 1, b);
    return true;
  }
  for (size_t i = 0; i < spec.attrs.size(); ++i) {
    auto it = have.find(spec.attrs[i]);
    // A required string that is present but empty cannot be told apart from "missing"
    // after decoding, so keep such an element as XML.
    if (it != have.end() && it->second.empty() && !spec.optional.count(spec.attrs[i])) return false;
    if (it != have.end()) PutString(body, static_cast<uint32_t>(i + 1), it->second);
  }
  return true;
}

std::string DecodeTyped(const std::string& tag, const TypedSpec& spec, const std::string& body, bool& ok) {
  std::vector<Field> fs;
  ok = ReadFields(body, fs);
  if (!ok) return std::string();
  std::map<uint32_t, Field> by;
  for (auto& f : fs) by[f.num] = f;
  std::string s = "<" + tag;
  if (spec.kind == TypedSpec::Track) {
    s += " speed=\"" + FormatDoubleRoundTrip(by.count(1) ? by[1].dbl : 0) + "\"";
    s += " course=\"" + FormatDoubleRoundTrip(by.count(2) ? by[2].dbl : 0) + "\"";
  } else if (spec.kind == TypedSpec::Status) {
    s += " battery=\"" + std::to_string(by.count(1) ? by[1].varint : 0) + "\"";
  } else {
    for (size_t i = 0; i < spec.attrs.size(); ++i) {
      uint32_t fn = static_cast<uint32_t>(i + 1);
      bool present = by.count(fn) != 0;
      if (!present && spec.optional.count(spec.attrs[i])) continue;
      s += " " + spec.attrs[i] + "=\"" + XmlEscape(present ? by[fn].bytes : std::string()) + "\"";
    }
  }
  return s + "/>";
}

TimeMs AttrTime(const XmlNode& ev, const char* name) {
  TimeMs t = 0;
  ParseCotTime(ev.AttrOr(name), t);
  return t;
}

}  // namespace

void PutVarint(std::string& out, uint64_t v) {
  while (v >= 0x80) {
    out.push_back(static_cast<char>((v & 0x7f) | 0x80));
    v >>= 7;
  }
  out.push_back(static_cast<char>(v));
}

bool GetVarint(const char* p, size_t len, size_t& pos, uint64_t& v) {
  v = 0;
  for (int shift = 0, i = 0; i < 10; ++i, shift += 7) {
    if (pos >= len) return false;
    uint8_t b = static_cast<uint8_t>(p[pos++]);
    v |= static_cast<uint64_t>(b & 0x7f) << shift;
    if (!(b & 0x80)) return true;
  }
  return false;
}

std::string FormatDoubleRoundTrip(double v) {
  if (!std::isfinite(v)) return "0";
  char buf[40];
  for (int prec = 1; prec <= 17; ++prec) {
    std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
    if (std::strtod(buf, nullptr) == v) break;
  }
  std::string s(buf);
  // Exponent forms are valid XML attribute numbers but unusual in CoT; expand small ones.
  if (s.find('e') != std::string::npos && std::fabs(v) >= 1e-4 && std::fabs(v) < 1e15) {
    std::snprintf(buf, sizeof(buf), "%.10f", v);
    s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
  }
  return s;
}

bool CotXmlToTakPayload(const std::string& xml, std::string& payload, std::string* err) {
  std::unique_ptr<XmlNode> root = XmlParse(xml, err);
  if (!root) return false;
  if (root->tag != "event") {
    if (err) *err = "root is not <event>";
    return false;
  }
  std::string ev;
  PutString(ev, 1, root->AttrOr("type"));
  std::string access = root->AttrOr("access");
  if (access != "Undefined") PutString(ev, 2, access);  // proto comment: "Undefined" is omitted
  PutString(ev, 3, root->AttrOr("qos"));
  PutString(ev, 4, root->AttrOr("opex"));
  PutString(ev, 5, root->AttrOr("uid"));
  PutU64(ev, 6, static_cast<uint64_t>(AttrTime(*root, "time")));
  PutU64(ev, 7, static_cast<uint64_t>(AttrTime(*root, "start")));
  PutU64(ev, 8, static_cast<uint64_t>(AttrTime(*root, "stale")));
  PutString(ev, 9, root->AttrOr("how"));
  const XmlNode* pt = root->Child("point");
  double lat = 0, lon = 0, hae = 9999999.0, ce = 9999999.0, le = 9999999.0;
  if (pt) {
    ParseDouble(pt->AttrOr("lat"), lat);
    ParseDouble(pt->AttrOr("lon"), lon);
    ParseDouble(pt->AttrOr("hae"), hae);
    ParseDouble(pt->AttrOr("ce"), ce);
    ParseDouble(pt->AttrOr("le"), le);
  }
  PutDouble(ev, 10, lat);
  PutDouble(ev, 11, lon);
  PutDouble(ev, 12, hae);
  PutDouble(ev, 13, ce);
  PutDouble(ev, 14, le);

  std::string detail;
  if (const XmlNode* d = root->Child("detail")) {
    std::map<std::string, int> counts;
    for (const auto& c : d->children) counts[c->tag]++;
    std::string xml_rest;
    std::map<uint32_t, std::string> typed;
    for (const auto& c : d->children) {
      auto it = TypedSpecs().find(c->tag);
      std::string body;
      if (it != TypedSpecs().end() && counts[c->tag] == 1 && EncodeTyped(*c, it->second, body)) {
        typed[it->second.field] = body;
        continue;
      }
      xml_rest += c->ToString();
    }
    PutString(detail, 1, xml_rest);
    for (const auto& t : typed) PutMessage(detail, t.first, t.second, true);
  }
  PutMessage(ev, 15, detail);
  PutString(ev, 16, root->AttrOr("caveat"));
  PutString(ev, 17, root->AttrOr("releasableTo"));
  payload.clear();
  PutMessage(payload, 2, ev, true);
  return true;
}

std::string TakControlPayload(const TakControlMsg& c) {
  std::string body;
  PutU64(body, 1, c.min_version);
  PutU64(body, 2, c.max_version);
  PutString(body, 3, c.contact_uid);
  if (!c.extension_ids.empty()) {  // packed repeated uint32
    std::string packed;
    for (uint32_t id : c.extension_ids) PutVarint(packed, id);
    PutMessage(body, 4, packed, true);
  }
  std::string out;
  PutMessage(out, 1, body, true);
  return out;
}

bool DecodeTakPayload(const std::string& payload, TakDecoded& out, std::string* err) {
  out = TakDecoded();
  std::vector<Field> top;
  if (!ReadFields(payload, top)) {
    if (err) *err = "malformed TakMessage";
    return false;
  }
  for (const auto& f : top) {
    if (f.num == 1 && f.wire == 2) {
      std::vector<Field> cf;
      if (!ReadFields(f.bytes, cf)) {
        if (err) *err = "malformed TakControl";
        return false;
      }
      out.has_control = true;
      for (const auto& c : cf) {
        if (c.num == 1 && c.wire == 0) out.control.min_version = static_cast<uint32_t>(c.varint);
        if (c.num == 2 && c.wire == 0) out.control.max_version = static_cast<uint32_t>(c.varint);
        if (c.num == 3 && c.wire == 2) out.control.contact_uid = c.bytes;
        if (c.num == 4 && c.wire == 0) out.control.extension_ids.push_back(static_cast<uint32_t>(c.varint));
        if (c.num == 4 && c.wire == 2) {
          size_t pos = 0;
          uint64_t v;
          while (pos < c.bytes.size() && GetVarint(c.bytes.data(), c.bytes.size(), pos, v))
            out.control.extension_ids.push_back(static_cast<uint32_t>(v));
        }
      }
    } else if (f.num == 2 && f.wire == 2) {
      std::vector<Field> ef;
      if (!ReadFields(f.bytes, ef)) {
        if (err) *err = "malformed CotEvent";
        return false;
      }
      std::map<uint32_t, Field> e;
      for (auto& x : ef) e[x.num] = x;
      auto S = [&](uint32_t n) { return e.count(n) && e[n].wire == 2 ? e[n].bytes : std::string(); };
      auto U = [&](uint32_t n) { return e.count(n) && e[n].wire == 0 ? e[n].varint : 0; };
      auto D = [&](uint32_t n, double def) { return e.count(n) && e[n].wire == 1 ? e[n].dbl : def; };
      std::string x = "<event version=\"2.0\"";
      x += " uid=\"" + XmlEscape(S(5)) + "\" type=\"" + XmlEscape(S(1)) + "\"";
      if (!S(2).empty()) x += " access=\"" + XmlEscape(S(2)) + "\"";
      if (!S(3).empty()) x += " qos=\"" + XmlEscape(S(3)) + "\"";
      if (!S(4).empty()) x += " opex=\"" + XmlEscape(S(4)) + "\"";
      if (!S(16).empty()) x += " caveat=\"" + XmlEscape(S(16)) + "\"";
      if (!S(17).empty()) x += " releasableTo=\"" + XmlEscape(S(17)) + "\"";
      x += " how=\"" + XmlEscape(S(9)) + "\"";
      x += " time=\"" + CotTime(static_cast<TimeMs>(U(6))) + "\"";
      x += " start=\"" + CotTime(static_cast<TimeMs>(U(7))) + "\"";
      x += " stale=\"" + CotTime(static_cast<TimeMs>(U(8))) + "\">";
      // proto3 drops zeros, so a missing double is 0 (the spec's "unknown" is 9999999).
      x += "<point lat=\"" + FormatDoubleRoundTrip(D(10, 0)) + "\" lon=\"" + FormatDoubleRoundTrip(D(11, 0)) +
           "\" hae=\"" + FormatDoubleRoundTrip(D(12, 0)) + "\" ce=\"" + FormatDoubleRoundTrip(D(13, 0)) +
           "\" le=\"" + FormatDoubleRoundTrip(D(14, 0)) + "\"/>";
      std::string detail;
      if (e.count(15) && e[15].wire == 2) {
        std::vector<Field> df;
        if (!ReadFields(e[15].bytes, df)) {
          if (err) *err = "malformed Detail";
          return false;
        }
        std::string xml_detail;
        std::set<std::string> xml_tags;
        for (const auto& d : df)
          if (d.num == 1 && d.wire == 2) xml_detail += d.bytes;
        if (!xml_detail.empty()) {
          // Which element names does xmlDetail hold at its top level? (receiver rule 5)
          std::string err2;
          std::unique_ptr<XmlNode> wrapped = XmlParse("<detail>" + xml_detail + "</detail>", &err2);
          if (!wrapped) {
            if (err) *err = "xmlDetail is not valid XML: " + err2;
            return false;
          }
          for (const auto& c : wrapped->children) xml_tags.insert(c->tag);
          detail += xml_detail;
        }
        for (const auto& d : df) {
          if (d.num == 8) {
            out.undecoded_extensions++;
            continue;
          }
          for (const auto& sp : TypedSpecs()) {
            if (sp.second.field != d.num || d.wire != 2) continue;
            if (xml_tags.count(sp.first)) break;  // xmlDetail wins
            bool ok = false;
            std::string el = DecodeTyped(sp.first, sp.second, d.bytes, ok);
            if (!ok) {
              if (err) *err = "malformed typed detail " + sp.first;
              return false;
            }
            detail += el;
          }
        }
      }
      x += detail.empty() ? "<detail/>" : "<detail>" + detail + "</detail>";
      x += "</event>";
      out.has_event = true;
      out.xml = x;
    }
  }
  if (!out.has_event && !out.has_control) {
    if (err) *err = "empty TakMessage";
    return false;
  }
  return true;
}

std::string FrameTakMesh(const std::string& payload) {
  std::string out;
  out.push_back(static_cast<char>(kTakMagic));
  out.push_back(0x01);
  out.push_back(static_cast<char>(kTakMagic));
  return out + payload;
}

std::string FrameTakStream(const std::string& payload) {
  std::string out;
  out.push_back(static_cast<char>(kTakMagic));
  PutVarint(out, payload.size());
  return out + payload;
}

bool IsTakProtocol(const std::string& bytes) {
  return !bytes.empty() && static_cast<unsigned char>(bytes[0]) == kTakMagic;
}

bool DecodeTakMeshMessage(const std::string& bytes, TakDecoded& out, std::string* err) {
  out = TakDecoded();
  if (!IsTakProtocol(bytes)) {
    std::string e2;
    std::unique_ptr<XmlNode> root = XmlParse(bytes, &e2);
    if (!root || root->tag != "event") {
      if (err) *err = root ? "root is not <event>" : e2;
      return false;
    }
    out.has_event = true;
    out.xml = root->ToString();
    return true;
  }
  size_t pos = 1;
  uint64_t version = 0;
  if (!GetVarint(bytes.data(), bytes.size(), pos, version) || pos >= bytes.size() ||
      static_cast<unsigned char>(bytes[pos]) != kTakMagic) {
    if (err) *err = "bad mesh header";
    return false;
  }
  if (version != 1) {
    if (err) *err = "unsupported TAK protocol version " + std::to_string(version);
    return false;
  }
  return DecodeTakPayload(bytes.substr(pos + 1), out, err);
}

}  // namespace om
