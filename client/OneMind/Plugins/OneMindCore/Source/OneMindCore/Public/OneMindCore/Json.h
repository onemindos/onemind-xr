// Minimal JSON value, parser and writer (RFC 8259) without exceptions.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace om {

class OM_CORE_API Json {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json>;

  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool b) : type_(Type::Bool), b_(b) {}
  Json(int v) : type_(Type::Number), n_(v) {}
  Json(int64_t v) : type_(Type::Number), n_(static_cast<double>(v)) {}
  Json(double v) : type_(Type::Number), n_(v) {}
  Json(const char* s) : type_(Type::String), s_(s) {}
  Json(std::string s) : type_(Type::String), s_(std::move(s)) {}
  Json(Array a) : type_(Type::Array), a_(std::make_shared<Array>(std::move(a))) {}
  Json(Object o) : type_(Type::Object), o_(std::make_shared<Object>(std::move(o))) {}

  static Json MakeArray() { return Json(Array{}); }
  static Json MakeObject() { return Json(Object{}); }

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_bool() const { return type_ == Type::Bool; }
  bool is_number() const { return type_ == Type::Number; }
  bool is_string() const { return type_ == Type::String; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_object() const { return type_ == Type::Object; }

  bool as_bool(bool def = false) const { return is_bool() ? b_ : def; }
  double as_number(double def = 0) const { return is_number() ? n_ : def; }
  int64_t as_int(int64_t def = 0) const { return is_number() ? static_cast<int64_t>(n_) : def; }
  const std::string& as_string() const;
  std::string as_string_or(const std::string& def) const { return is_string() ? s_ : def; }

  // Array access. Out-of-range or wrong type returns a shared null value.
  size_t size() const;
  const Json& operator[](size_t i) const;
  const Json& operator[](int i) const { return (*this)[static_cast<size_t>(i < 0 ? 0 : i)]; }
  void push_back(Json v);
  const Array& items() const;

  // Object access. Missing key or wrong type returns a shared null value.
  const Json& operator[](const std::string& key) const;
  const Json& operator[](const char* key) const { return (*this)[std::string(key)]; }
  bool has(const std::string& key) const;
  void set(const std::string& key, Json v);
  const Object& fields() const;

  // Convenience typed getters on objects.
  std::string str(const std::string& key, const std::string& def = std::string()) const;
  double num(const std::string& key, double def = 0) const;
  int64_t i64(const std::string& key, int64_t def = 0) const;
  bool boolean(const std::string& key, bool def = false) const;

  /// Parse UTF-8 JSON text. Returns false and sets `err` on failure.
  static bool Parse(const std::string& text, Json& out, std::string* err = nullptr);
  static bool Parse(const char* data, size_t len, Json& out, std::string* err = nullptr);

  /// Compact serialisation. Numbers that are integral print without a fraction.
  std::string Dump() const;

 private:
  void DumpTo(std::string& out) const;
  Type type_ = Type::Null;
  bool b_ = false;
  double n_ = 0;
  std::string s_;
  std::shared_ptr<Array> a_;
  std::shared_ptr<Object> o_;
};

/// Escape a string as a JSON string literal including the quotes.
OM_CORE_API std::string JsonQuote(const std::string& s);

}  // namespace om
