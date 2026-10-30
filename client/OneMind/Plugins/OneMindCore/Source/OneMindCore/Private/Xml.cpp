// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Xml.h"

#include <cstdlib>
#include <cstring>

namespace om {

const std::string* XmlNode::Attr(const std::string& name) const {
  for (const auto& a : attrs)
    if (a.first == name) return &a.second;
  return nullptr;
}

std::string XmlNode::AttrOr(const std::string& name, const std::string& def) const {
  const std::string* v = Attr(name);
  return v ? *v : def;
}

void XmlNode::SetAttr(const std::string& name, const std::string& value) {
  for (auto& a : attrs) {
    if (a.first == name) {
      a.second = value;
      return;
    }
  }
  attrs.emplace_back(name, value);
}

XmlNode* XmlNode::Child(const std::string& t) const {
  for (const auto& c : children)
    if (c->tag == t) return c.get();
  return nullptr;
}

std::vector<XmlNode*> XmlNode::Children(const std::string& t) const {
  std::vector<XmlNode*> out;
  for (const auto& c : children)
    if (c->tag == t) out.push_back(c.get());
  return out;
}

XmlNode* XmlNode::AddChild(const std::string& t) {
  children.push_back(std::unique_ptr<XmlNode>(new XmlNode()));
  children.back()->tag = t;
  return children.back().get();
}

void XmlNode::RemoveChildren(const std::string& t) {
  std::vector<std::unique_ptr<XmlNode>> keep;
  for (auto& c : children)
    if (c->tag != t) keep.push_back(std::move(c));
  children.swap(keep);
}

std::unique_ptr<XmlNode> XmlNode::Clone() const {
  std::unique_ptr<XmlNode> n(new XmlNode());
  n->tag = tag;
  n->attrs = attrs;
  n->text = text;
  for (const auto& c : children) n->children.push_back(c->Clone());
  return n;
}

std::string XmlEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default:
        // Control characters other than tab/LF/CR are not allowed in XML 1.0.
        if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n' && c != '\r') break;
        out += c;
    }
  }
  return out;
}

std::string XmlNode::ToString() const {
  std::string s = "<" + tag;
  for (const auto& a : attrs) s += " " + a.first + "=\"" + XmlEscape(a.second) + "\"";
  if (children.empty() && text.empty()) return s + "/>";
  s += ">";
  s += XmlEscape(text);
  for (const auto& c : children) s += c->ToString();
  s += "</" + tag + ">";
  return s;
}

namespace xml_detail {
struct Parser {
  const std::string& t;
  size_t p = 0;
  std::string err;
  XmlLimits lim;
  size_t elements = 0;

  bool Fail(const std::string& m) {
    if (err.empty()) err = m + " at " + std::to_string(p);
    return false;
  }
  static bool NameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '.' || c == ':' || static_cast<unsigned char>(c) >= 0x80;
  }
  void Ws() {
    while (p < t.size() && (t[p] == ' ' || t[p] == '\t' || t[p] == '\n' || t[p] == '\r')) ++p;
  }
  bool StartsWith(const char* s) const { return t.compare(p, std::strlen(s), s) == 0; }
  bool SkipPast(const char* s) {
    size_t e = t.find(s, p);
    if (e == std::string::npos) return Fail(std::string("unterminated ") + s);
    p = e + std::strlen(s);
    return true;
  }
  bool SkipMisc() {
    while (true) {
      Ws();
      if (StartsWith("<?")) {
        if (!SkipPast("?>")) return false;
      } else if (StartsWith("<!--")) {
        if (!SkipPast("-->")) return false;
      } else if (StartsWith("<!DOCTYPE") || StartsWith("<!doctype")) {
        // Skipped, never processed (no entity expansion).
        if (!SkipPast(">")) return false;
      } else {
        return true;
      }
    }
  }
  static void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  bool Decode(const std::string& raw, std::string& out) {
    out.clear();
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] != '&') {
        out += raw[i];
        continue;
      }
      size_t semi = raw.find(';', i);
      if (semi == std::string::npos || semi - i > 12) return Fail("bad entity");
      std::string ent = raw.substr(i + 1, semi - i - 1);
      if (ent == "amp") out += '&';
      else if (ent == "lt") out += '<';
      else if (ent == "gt") out += '>';
      else if (ent == "quot") out += '"';
      else if (ent == "apos") out += '\'';
      else if (!ent.empty() && ent[0] == '#') {
        uint32_t cp = ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')
                          ? static_cast<uint32_t>(std::strtoul(ent.c_str() + 2, nullptr, 16))
                          : static_cast<uint32_t>(std::strtoul(ent.c_str() + 1, nullptr, 10));
        AppendUtf8(out, cp);
      } else {
        return Fail("unknown entity &" + ent + ";");
      }
      i = semi;
    }
    return true;
  }
  bool Element(XmlNode& node, int depth) {
    if (depth > lim.max_depth) return Fail("too deep");
    if (++elements > lim.max_elements) return Fail("too many elements");
    ++p;  // '<'
    size_t s = p;
    while (p < t.size() && NameChar(t[p])) ++p;
    if (p == s) return Fail("expected element name");
    node.tag = t.substr(s, p - s);
    while (true) {
      Ws();
      if (p >= t.size()) return Fail("unterminated start tag");
      if (t[p] == '/') {
        if (p + 1 < t.size() && t[p + 1] == '>') {
          p += 2;
          return true;
        }
        return Fail("bad empty element");
      }
      if (t[p] == '>') {
        ++p;
        break;
      }
      size_t a = p;
      while (p < t.size() && NameChar(t[p])) ++p;
      if (p == a) return Fail("expected attribute name");
      std::string name = t.substr(a, p - a);
      Ws();
      if (p >= t.size() || t[p] != '=') return Fail("expected =");
      ++p;
      Ws();
      if (p >= t.size() || (t[p] != '"' && t[p] != '\'')) return Fail("expected quote");
      char q = t[p++];
      size_t e = t.find(q, p);
      if (e == std::string::npos) return Fail("unterminated attribute");
      std::string val;
      if (!Decode(t.substr(p, e - p), val)) return false;
      node.attrs.emplace_back(name, val);
      p = e + 1;
    }
    // Content.
    while (true) {
      if (p >= t.size()) return Fail("unterminated element " + node.tag);
      if (StartsWith("</")) {
        p += 2;
        size_t s2 = p;
        while (p < t.size() && NameChar(t[p])) ++p;
        if (t.compare(s2, p - s2, node.tag) != 0 || p - s2 != node.tag.size()) return Fail("mismatched end tag");
        Ws();
        if (p >= t.size() || t[p] != '>') return Fail("bad end tag");
        ++p;
        return true;
      }
      if (StartsWith("<!--")) {
        if (!SkipPast("-->")) return false;
        continue;
      }
      if (StartsWith("<![CDATA[")) {
        p += 9;
        size_t e = t.find("]]>", p);
        if (e == std::string::npos) return Fail("unterminated CDATA");
        node.text += t.substr(p, e - p);
        p = e + 3;
        continue;
      }
      if (StartsWith("<?")) {
        if (!SkipPast("?>")) return false;
        continue;
      }
      if (t[p] == '<') {
        node.children.push_back(std::unique_ptr<XmlNode>(new XmlNode()));
        if (!Element(*node.children.back(), depth + 1)) return false;
        continue;
      }
      size_t e = t.find('<', p);
      if (e == std::string::npos) return Fail("unterminated text");
      std::string txt;
      if (!Decode(t.substr(p, e - p), txt)) return false;
      // Whitespace-only runs between elements are formatting, not content.
      bool ws_only = txt.find_first_not_of(" \t\r\n") == std::string::npos;
      if (!ws_only) node.text += txt;
      p = e;
    }
  }
};
}  // namespace xml_detail

std::unique_ptr<XmlNode> XmlParse(const std::string& text, std::string* err, const XmlLimits& limits) {
  if (text.size() > limits.max_bytes) {
    if (err) *err = "document too large";
    return nullptr;
  }
  xml_detail::Parser ps{text, 0, std::string(), XmlLimits(), 0};
  ps.lim = limits;
  // Optional UTF-8 BOM.
  if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) ps.p = 3;
  if (!ps.SkipMisc()) {
    if (err) *err = ps.err;
    return nullptr;
  }
  if (ps.p >= text.size() || text[ps.p] != '<') {
    if (err) *err = "no root element";
    return nullptr;
  }
  std::unique_ptr<XmlNode> root(new XmlNode());
  if (!ps.Element(*root, 0)) {
    if (err) *err = ps.err;
    return nullptr;
  }
  return root;
}

}  // namespace om
