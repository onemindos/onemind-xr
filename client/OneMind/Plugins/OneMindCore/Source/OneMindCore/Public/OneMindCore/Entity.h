// Entities on the client (OneMindEntities, features B4, B5, B6, C1).
//  - RenderEntity: the render subset of layer-snapshot.md §4, parsed from either a render
//    item or a full entity.v1 record (entity.md §4.1).
//  - EntityStore: structure-of-arrays cache for one layer with the revision rule
//    (ignore seq <= held), live:false removal, expiry at expiry_t, stale dimming, a
//    change journal for batched renderers and a grid index for picking.
//  - EntityDetails / InspectorRows: what the inspector shows (sources, confidence
//    breakdown, history, raw CoT pointer).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Json.h"
#include "OneMindCore/MilStd2525.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace om {

struct OM_CORE_API RenderEntity {
  std::string entity_id, entity_type, callsign, cot_type, disposition, origin, lifecycle;
  double lat = 0, lon = 0, hae = -1, course = -1, speed = -1;
  double confidence = -1;
  TimeMs t = 0;
  int64_t stale_s = 0;
  TimeMs expiry_t = 0;  // 0 = no expiry (pinned or unknown)
  int64_t seq = 0;
  bool live = true;
  bool simulated = false;
  bool locatable = true;

  /// Accepts a render item or a full entity record. Returns false without entity_id.
  static bool FromJson(const Json& j, RenderEntity& out);
};

enum class RemoveReason : uint8_t { Expired, Deleted, NotLive, LeftAoi, Released, LayerOff };
OM_CORE_API const char* RemoveReasonName(RemoveReason r);

class OM_CORE_API EntityStore {
 public:
  enum class Result { Added, Updated, Removed, IgnoredOlder, IgnoredInvalid };
  struct Change {
    enum class Kind : uint8_t { Added, Updated, Removed } kind;
    std::string id;
    RemoveReason reason = RemoveReason::Deleted;
  };

  Result Upsert(const RenderEntity& e, TimeMs now);
  bool Remove(const std::string& id, RemoveReason reason, TimeMs now);
  /// Removes entities whose expiry_t has passed. Returns the number removed.
  size_t Expire(TimeMs now);
  void Clear(RemoveReason reason);

  size_t size() const { return ids_.size(); }
  bool Has(const std::string& id) const { return index_.count(id) > 0; }
  /// Index of an entity, or -1.
  int64_t IndexOf(const std::string& id) const;
  RenderEntity Get(size_t i) const;
  bool IsStale(size_t i, TimeMs now) const;
  int64_t SeqOf(const std::string& id) const;

  // SoA columns for renderers (valid until the next mutation).
  const std::vector<std::string>& ids() const { return ids_; }
  const std::vector<double>& lats() const { return lat_; }
  const std::vector<double>& lons() const { return lon_; }
  const std::vector<double>& haes() const { return hae_; }
  const std::vector<double>& courses() const { return course_; }
  const std::vector<SymbolClass>& classes() const { return cls_; }
  const std::vector<std::string>& callsigns() const { return callsign_; }
  const std::vector<std::string>& cot_types() const { return cot_type_; }
  const std::vector<uint8_t>& locatables() const { return locatable_; }

  /// Change journal since the last drain (coalesced per id, newest kind wins).
  std::vector<Change> DrainChanges();
  size_t pending_changes() const { return journal_.size(); }

  /// Nearest locatable entity to (lat, lon) within radius_m, or "".
  std::string Pick(double lat, double lon, double radius_m) const;
  /// All entity ids inside a lon/lat box.
  std::vector<std::string> InBox(double lon0, double lat0, double lon1, double lat1) const;

  struct Counters {
    uint64_t added = 0, updated = 0, removed = 0, expired = 0, ignored_older = 0, ignored_invalid = 0;
  };
  const Counters& counters() const { return counters_; }

 private:
  void Set(size_t i, const RenderEntity& e);
  void RemoveAt(size_t i);
  void Journal(Change::Kind k, const std::string& id, RemoveReason r);
  int64_t CellKey(double lat, double lon) const;
  void GridAdd(size_t i);
  void GridRemove(size_t i);

  std::unordered_map<std::string, size_t> index_;
  std::vector<std::string> ids_, etype_, callsign_, cot_type_, disposition_, origin_, lifecycle_;
  std::vector<double> lat_, lon_, hae_, course_, speed_, conf_;
  std::vector<TimeMs> t_, expiry_;
  std::vector<int64_t> stale_s_, seq_;
  std::vector<uint8_t> sim_, locatable_;
  std::vector<SymbolClass> cls_;
  std::vector<int64_t> cell_;
  std::unordered_map<int64_t, std::vector<size_t>> grid_;
  // Tombstones keep the last seq of removed entities so a late, older record cannot
  // resurrect them (entity revision rule). LeftAoi does not leave a tombstone.
  std::unordered_map<std::string, std::pair<int64_t, TimeMs>> tomb_;
  std::vector<Change> journal_;
  std::unordered_map<std::string, size_t> journal_index_;
  Counters counters_;
};

/// Full record and short position history for the inspector (B6).
struct OM_CORE_API EntityDetails {
  Json record;
  struct Fix {
    TimeMs t;
    double lat, lon, hae;
  };
  std::deque<Fix> history;
  size_t max_history = 240;
  void Update(const Json& rec);
};

struct OM_CORE_API InspectorRow {
  std::string section, key, value;
};

/// Inspector rows from a full entity.v1 record: identity, freshness, confidence and its
/// parts, every sources[] entry, provenance, marking, history size.
OM_CORE_API std::vector<InspectorRow> BuildInspectorRows(const EntityDetails& d, TimeMs now);

/// Where the raw CoT of a TAK-sourced entity lives: the sources[] entry whose stream
/// is COT/SITE_COT (stream + seq for a Direct Get). Returns false if none.
OM_CORE_API bool FindRawCotSource(const Json& record, std::string& stream, uint64_t& seq, std::string& subject);

}  // namespace om
