// MIL-STD-2525 helpers for symbology (B7 later; B5 symbol classes now).
//
// The icon-name <-> CoT-type conversion and the parent/child hierarchy are ported from
// VR-TAK's UVH_MilStd2525 (VH_COT plugin):
//   TAK-Product-Center/VR, Game/Plugins/VH_COT/Source/VH_COT/Private/VH_MilStd2525.cpp
//   at 4bd384978b48b0a46e7f17b272933a152a61c013 (GPL-3.0), functions
//   ConvertCOTTypeToID, PopulateIcons (idToCOTType lambda, icon file-name normalisation,
//   ParentID = id minus its last "-X"), GetIconData/GetParentData.
//   Modified by OneMind on 2026-10-29: no textures, no symbols.dat loading, plain C++.
// ATAK's icon set and symbols.dat are not shipped (their licence is not recorded here);
// the registry is filled at run time from whatever icon set the owner installs.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

#include <map>
#include <string>
#include <vector>

namespace om {

/// Coarse classes the batched renderer uses to pick a mesh/icon (one ISM per class).
enum class SymbolClass : uint8_t { Unknown, Air, Ground, Sea, Subsurface, Space, Person, Shape, Point, Event };

struct OM_CORE_API MilStd2525 {
  /// "a-f-A-M-F" -> "f-A-M-F" (VH: ConvertCOTTypeToID removes the initial "a-").
  static std::string CotTypeToId(const std::string& cot_type);
  /// Icon file name (e.g. "sfapmf----*****.png" style, "s" + affiliation + "p" + function)
  /// to CoT-style id "f-A-M-F" (VH: PopulateIcons normalisation + idToCOTType).
  static std::string IconFileToId(const std::string& file_name);
  /// Parent id: drop the last "-X" (VH: ParentID = Left(Len - 2)). "" at the top.
  static std::string ParentId(const std::string& id);
  /// Renderer class from a CoT type and the entity type token.
  static SymbolClass ClassFor(const std::string& cot_type, const std::string& entity_type);
};

/// Registry of known icon ids with fallback to the nearest ancestor (VH GetIconData,
/// with the hierarchy walk OneMind adds so unknown leaves still get a parent icon).
class OM_CORE_API SymbolRegistry {
 public:
  void Add(const std::string& id, const std::string& asset);
  /// Asset for a CoT type, walking up parents; "" if nothing matches.
  std::string Lookup(const std::string& cot_type) const;
  std::vector<std::string> ChildrenOf(const std::string& id) const;
  size_t size() const { return icons_.size(); }

 private:
  std::map<std::string, std::string> icons_;
};

}  // namespace om
