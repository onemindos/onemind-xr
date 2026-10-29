// Runs every registered TEST. Optional argument: substring filter on test names.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHarness.h"

#include <chrono>
#include <cstring>

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int run = 0, failed_cases = 0;
  auto t0 = std::chrono::steady_clock::now();
  for (auto& c : omtest::Registry()) {
    if (filter && !std::strstr(c.name, filter)) continue;
    int before = omtest::Failures();
    auto s = std::chrono::steady_clock::now();
    std::printf("[ RUN  ] %s\n", c.name);
    std::fflush(stdout);
    c.fn();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - s).count();
    bool ok = omtest::Failures() == before;
    std::printf("[ %s ] %s (%lld ms)\n", ok ? " OK " : "FAIL", c.name, static_cast<long long>(ms));
    std::fflush(stdout);
    ++run;
    if (!ok) ++failed_cases;
  }
  auto total = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  std::printf("\n%d tests, %d passed, %d failed, %d checks, %lld ms\n", run, run - failed_cases, failed_cases,
              omtest::Checks(), static_cast<long long>(total));
  return failed_cases == 0 ? 0 : 1;
}
