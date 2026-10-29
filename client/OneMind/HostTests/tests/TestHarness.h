// A tiny test registry (no third-party framework: nothing to download, no exceptions).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace omtest {

struct Case {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<Case>& Registry() {
  static std::vector<Case> r;
  return r;
}

struct Registrar {
  Registrar(const char* n, std::function<void()> f) { Registry().push_back(Case{n, std::move(f)}); }
};

inline int& Failures() {
  static int f = 0;
  return f;
}
inline int& Checks() {
  static int c = 0;
  return c;
}

inline void Report(bool ok, const char* expr, const char* file, int line, const std::string& extra = std::string()) {
  ++Checks();
  if (!ok) {
    ++Failures();
    std::printf("    FAIL %s:%d: %s %s\n", file, line, expr, extra.c_str());
  }
}

template <typename A, typename B>
std::string Show(const A& a, const B& b) {
  std::ostringstream os;
  os << "(got " << a << ", want " << b << ")";
  return os.str();
}

}  // namespace omtest

#define OM_CAT2(a, b) a##b
#define OM_CAT(a, b) OM_CAT2(a, b)
#define TEST(name)                                                                   \
  static void OM_CAT(test_fn_, name)();                                              \
  static omtest::Registrar OM_CAT(test_reg_, name)(#name, &OM_CAT(test_fn_, name)); \
  static void OM_CAT(test_fn_, name)()

#define EXPECT_TRUE(x) omtest::Report(static_cast<bool>(x), #x, __FILE__, __LINE__)
#define EXPECT_FALSE(x) omtest::Report(!static_cast<bool>(x), "!(" #x ")", __FILE__, __LINE__)
// Each argument is evaluated once (a call with side effects, such as a send, runs once).
#define EXPECT_EQ(a, b)                                                                          \
  do {                                                                                           \
    auto&& om_a_ = (a);                                                                          \
    auto&& om_b_ = (b);                                                                          \
    omtest::Report(om_a_ == om_b_, #a " == " #b, __FILE__, __LINE__, omtest::Show(om_a_, om_b_)); \
  } while (0)
#define EXPECT_NEAR(a, b, eps)                                                                    \
  do {                                                                                            \
    auto&& om_a_ = (a);                                                                           \
    auto&& om_b_ = (b);                                                                           \
    omtest::Report(std::fabs(om_a_ - om_b_) <= (eps), #a " ~= " #b, __FILE__, __LINE__,           \
                   omtest::Show(om_a_, om_b_));                                                   \
  } while (0)
