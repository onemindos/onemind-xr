// Internal helpers shared by the CoT builders (defined in CoT.cpp). Not part of the public API.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/CoT.h"

namespace om {
namespace cot_detail {
std::string FormatNum(double v);
bool ParseLinkPoint(const std::string& s, GeoPoint& p);
std::string PointStr(const GeoPoint& p);
CotEvent Base(const CotBuild& b, const std::string& type, const GeoPoint& p, TimeMs now);
void Remarks(CotEvent& ev, const CotBuild& b);
std::string Argb(uint32_t argb);
}  // namespace cot_detail
}  // namespace om
