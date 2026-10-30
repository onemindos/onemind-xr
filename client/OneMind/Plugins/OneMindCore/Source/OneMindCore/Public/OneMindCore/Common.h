// OneMind client core: engine-independent C++17 shared by the OneMind* Unreal plugins
// and built by CMake on the host for unit and integration tests (client/OneMind/HostTests).
//
// Rules for every file under OneMindCore (so the same source compiles inside Unreal):
//  - no Unreal headers, no exceptions, no RTTI (Unreal builds with both off by default);
//  - no identifiers that collide with Unreal macros (check, verify, ensure, TEXT, PI);
//  - std::min/std::max written as (std::min)(a, b) for Windows builds;
//  - nothing here owns a thread; I/O goes through om::ITransport.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>

#if defined(ONEMINDCORE_API)
#define OM_CORE_API ONEMINDCORE_API
#else
#define OM_CORE_API
#endif

namespace om {

/// Milliseconds since the Unix epoch (UTC). The core never reads a clock itself:
/// callers pass `now_ms` so tests are deterministic.
using TimeMs = int64_t;

/// Wall-clock helper for hosts that want one (std::chrono, no platform API).
OM_CORE_API TimeMs WallClockMs();

}  // namespace om
