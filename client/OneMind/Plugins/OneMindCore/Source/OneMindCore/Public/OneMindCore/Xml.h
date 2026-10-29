// Small XML DOM for CoT (elements, attributes, text; comments, processing instructions
// and DOCTYPE are skipped; no external entities are ever resolved). CoT arriving from the
// mesh is untrusted input (security-and-trust.md T4), so the parser bounds depth, size
// and element count and never expands anything but the five predefined entities and
// numeric character references.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace om {

struct OM_CORE_API XmlNode {
  std::string tag;
  std::vector<std::pair<std::string, std::string>> attrs;
  std::string text;  // concatenated character data directly inside this element
  std::vector<std::unique_ptr<XmlNode>> children;

  const std::string* Attr(const std::string& name) const;
  std::string AttrOr(const std::string& name, const std::string& def = std::string()) const;
  void SetAttr(const std::string& name, const std::string& value);
  XmlNode* Child(const std::string& tag) const;
  std::vector<XmlNode*> Children(const std::string& tag) const;
  XmlNode* AddChild(const std::string& tag);
  void RemoveChildren(const std::string& tag);
  std::unique_ptr<XmlNode> Clone() const;

  /// Serialise this element and its subtree (no XML declaration).
  std::string ToString() const;
};

struct OM_CORE_API XmlLimits {
  size_t max_bytes = 256 * 1024;
  int max_depth = 32;
  size_t max_elements = 4096;
};

/// Parses one document; returns the root element or nullptr (error in `err`).
OM_CORE_API std::unique_ptr<XmlNode> XmlParse(const std::string& text, std::string* err = nullptr,
                                              const XmlLimits& limits = XmlLimits());
OM_CORE_API std::string XmlEscape(const std::string& s);

}  // namespace om
