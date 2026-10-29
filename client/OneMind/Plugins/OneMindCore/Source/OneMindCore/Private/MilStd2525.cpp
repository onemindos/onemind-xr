// SPDX-License-Identifier: GPL-3.0-or-later
// Ported in part from VR-TAK VH_MilStd2525.cpp (GPL-3.0); see MilStd2525.h and THIRD_PARTY.md.
#include "OneMindCore/MilStd2525.h"

#include <cctype>

namespace om {

std::string MilStd2525::CotTypeToId(const std::string& cot_type) {
  if (cot_type.rfind("a-", 0) == 0) return cot_type.substr(2);
  return cot_type;
}

std::string MilStd2525::IconFileToId(const std::string& file_name) {
  // remove extension
  std::string name = file_name.substr(0, file_name.find('.'));
  if (name.empty()) return std::string();
  // remove initial 's'
  name.erase(0, 1);
  // special case for icons that end with h----
  if (name.size() >= 5 && name.compare(name.size() - 5, 5, "h----") == 0) name.erase(name.size() - 5);
  // remove trailing -'s
  while (!name.empty() && name.back() == '-') name.pop_back();
  // remove the status letter ('p', present) at the 2nd index
  if (name.size() > 2) name.erase(2, 1);
  if (name.empty()) return std::string();
  // idToCOTType: first char kept, the rest upper-cased and joined with '-'
  std::string out(1, name[0]);
  for (size_t i = 1; i < name.size(); ++i) {
    out += '-';
    out += static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
  }
  return out;
}

std::string MilStd2525::ParentId(const std::string& id) {
  if (id.size() <= 2) return std::string();
  return id.substr(0, id.size() - 2);
}

SymbolClass MilStd2525::ClassFor(const std::string& cot_type, const std::string& entity_type) {
  if (entity_type == "person") return SymbolClass::Person;
  if (entity_type == "zone" || entity_type == "structure") return SymbolClass::Shape;
  if (entity_type == "place") return SymbolClass::Point;
  if (entity_type == "event" || entity_type == "alert") return SymbolClass::Event;
  if (cot_type.rfind("u-d-", 0) == 0 || cot_type == "b-m-r") return SymbolClass::Shape;
  if (cot_type.rfind("b-m-p", 0) == 0) return SymbolClass::Point;
  if (cot_type.size() >= 5 && cot_type[0] == 'a' && cot_type[1] == '-' && cot_type[3] == '-') {
    switch (cot_type[4]) {
      case 'A': return SymbolClass::Air;
      case 'G': return SymbolClass::Ground;
      case 'S': return SymbolClass::Sea;
      case 'U': return SymbolClass::Subsurface;
      case 'P': return SymbolClass::Space;
      default: break;
    }
  }
  if (entity_type == "aircraft" || entity_type == "drone") return SymbolClass::Air;
  if (entity_type == "vessel") return SymbolClass::Sea;
  if (entity_type == "vehicle" || entity_type == "robot") return SymbolClass::Ground;
  if (entity_type == "satellite") return SymbolClass::Space;
  return SymbolClass::Unknown;
}

void SymbolRegistry::Add(const std::string& id, const std::string& asset) { icons_[id] = asset; }

std::string SymbolRegistry::Lookup(const std::string& cot_type) const {
  std::string id = MilStd2525::CotTypeToId(cot_type);
  while (!id.empty()) {
    auto it = icons_.find(id);
    if (it != icons_.end()) return it->second;
    id = MilStd2525::ParentId(id);
  }
  return std::string();
}

std::vector<std::string> SymbolRegistry::ChildrenOf(const std::string& id) const {
  // VH findChildrenIcons: keys exactly two characters longer that start with the id.
  std::vector<std::string> out;
  for (const auto& kv : icons_) {
    if (kv.first.size() == id.size() + 2 && kv.first.compare(0, id.size(), id) == 0) out.push_back(kv.first);
  }
  return out;
}

}  // namespace om
