// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Entity.h"

#include "OneMindCore/Geo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace om {

const char* RemoveReasonName(RemoveReason r) {
  switch (r) {
    case RemoveReason::Expired: return "expired";
    case RemoveReason::Deleted: return "deleted";
    case RemoveReason::NotLive: return "not_live";
    case RemoveReason::LeftAoi: return "left_aoi";
    case RemoveReason::Released: return "released_out";
    case RemoveReason::LayerOff: return "layer_off";
  }
  return "deleted";
}

bool RenderEntity::FromJson(const Json& j, RenderEntity& e) {
  if (!j.is_object()) return false;
  e = RenderEntity();
  e.entity_id = j.str("entity_id", j.str("uid"));
  if (e.entity_id.empty()) return false;
  e.entity_type = j.str("entity_type");
  const Json& loc = j["location"];
  e.lat = j.num("lat", loc.num("lat", 0));
  e.lon = j.num("lon", loc.num("lon", 0));
  e.hae = j.num("hae", loc.num("hae_m", -1));
  e.course = j.num("course", loc.num("course_deg", -1));
  e.speed = j.num("speed", loc.num("speed_mps", -1));
  e.callsign = j.str("callsign", j["aliases"].str("name"));
  e.cot_type = j.str("cot_type");
  e.disposition = j.str("disposition");
  e.origin = j.str("origin");
  e.lifecycle = j.str("lifecycle");
  e.confidence = j.num("confidence", -1);
  e.t = j.i64("t");
  e.stale_s = j.i64("stale");
  e.expiry_t = j["expiry_t"].is_number() ? j.i64("expiry_t") : 0;
  e.seq = j.i64("seq");
  e.live = j.boolean("live", true);
  e.simulated = j.has("simulated") ? j.boolean("simulated") : j["indicators"].boolean("simulated", false);
  // Non-located types carry the reserved lat 0, lon 0 and no "locatable" trait
  // (entity.md §4.1 Group A): never draw them.
  if (j["traits"].is_array()) {
    bool loc_trait = false;
    for (const auto& t : j["traits"].items())
      if (t.as_string() == "locatable") loc_trait = true;
    e.locatable = loc_trait;
  }
  if (!std::isfinite(e.lat) || !std::isfinite(e.lon) || e.lat < -90 || e.lat > 90 || e.lon < -180 || e.lon > 180)
    e.locatable = false;
  return true;
}

// ---------------------------------------------------------------------------------------------

int64_t EntityStore::CellKey(double lat, double lon) const {
  // 0.1 degree cells (about 11 km); picking searches the 3x3 neighbourhood.
  int64_t a = static_cast<int64_t>(std::floor((lat + 90.0) * 10.0));
  int64_t b = static_cast<int64_t>(std::floor((lon + 180.0) * 10.0));
  return a * 4000 + b;
}

void EntityStore::GridAdd(size_t i) {
  if (!locatable_[i]) {
    cell_[i] = -1;
    return;
  }
  cell_[i] = CellKey(lat_[i], lon_[i]);
  grid_[cell_[i]].push_back(i);
}

void EntityStore::GridRemove(size_t i) {
  if (cell_[i] < 0) return;
  auto it = grid_.find(cell_[i]);
  if (it == grid_.end()) return;
  auto& v = it->second;
  auto f = std::find(v.begin(), v.end(), i);
  if (f != v.end()) {
    *f = v.back();
    v.pop_back();
  }
  if (v.empty()) grid_.erase(it);
}

void EntityStore::Journal(Change::Kind k, const std::string& id, RemoveReason r) {
  auto it = journal_index_.find(id);
  if (it != journal_index_.end()) {
    Change& c = journal_[it->second];
    // Added then Updated stays Added; Added then Removed cancels out to Removed (the
    // renderer may not have the entity, and removing an unknown id is a no-op).
    if (!(c.kind == Change::Kind::Added && k == Change::Kind::Updated)) c.kind = k;
    c.reason = r;
    return;
  }
  journal_index_[id] = journal_.size();
  journal_.push_back(Change{k, id, r});
}

std::vector<EntityStore::Change> EntityStore::DrainChanges() {
  std::vector<Change> out;
  out.swap(journal_);
  journal_index_.clear();
  return out;
}

void EntityStore::Set(size_t i, const RenderEntity& e) {
  etype_[i] = e.entity_type;
  callsign_[i] = e.callsign;
  cot_type_[i] = e.cot_type;
  disposition_[i] = e.disposition;
  origin_[i] = e.origin;
  lifecycle_[i] = e.lifecycle;
  lat_[i] = e.lat;
  lon_[i] = e.lon;
  hae_[i] = e.hae;
  course_[i] = e.course;
  speed_[i] = e.speed;
  conf_[i] = e.confidence;
  t_[i] = e.t;
  expiry_[i] = e.expiry_t;
  stale_s_[i] = e.stale_s;
  seq_[i] = e.seq;
  sim_[i] = e.simulated ? 1 : 0;
  locatable_[i] = e.locatable ? 1 : 0;
  cls_[i] = MilStd2525::ClassFor(e.cot_type, e.entity_type);
}

EntityStore::Result EntityStore::Upsert(const RenderEntity& e, TimeMs now) {
  if (e.entity_id.empty()) {
    counters_.ignored_invalid++;
    return Result::IgnoredInvalid;
  }
  auto it = index_.find(e.entity_id);
  if (!e.live) {
    if (it != index_.end() && e.seq != 0 && e.seq < seq_[it->second]) {
      counters_.ignored_older++;
      return Result::IgnoredOlder;
    }
    if (it != index_.end()) {
      RemoveAt(it->second);
      Journal(Change::Kind::Removed, e.entity_id, RemoveReason::NotLive);
      counters_.removed++;
    }
    tomb_[e.entity_id] = {e.seq, now};
    return Result::Removed;
  }
  if (it == index_.end()) {
    auto tb = tomb_.find(e.entity_id);
    if (tb != tomb_.end()) {
      if (e.seq != 0 && e.seq <= tb->second.first) {
        counters_.ignored_older++;
        return Result::IgnoredOlder;
      }
      tomb_.erase(tb);
    }
    if (e.expiry_t != 0 && e.expiry_t <= now) {
      counters_.ignored_older++;  // already expired on arrival
      return Result::IgnoredOlder;
    }
    size_t i = ids_.size();
    ids_.push_back(e.entity_id);
    etype_.emplace_back();
    callsign_.emplace_back();
    cot_type_.emplace_back();
    disposition_.emplace_back();
    origin_.emplace_back();
    lifecycle_.emplace_back();
    lat_.push_back(0);
    lon_.push_back(0);
    hae_.push_back(0);
    course_.push_back(0);
    speed_.push_back(0);
    conf_.push_back(0);
    t_.push_back(0);
    expiry_.push_back(0);
    stale_s_.push_back(0);
    seq_.push_back(0);
    sim_.push_back(0);
    locatable_.push_back(0);
    cls_.push_back(SymbolClass::Unknown);
    cell_.push_back(-1);
    Set(i, e);
    GridAdd(i);
    index_[e.entity_id] = i;
    Journal(Change::Kind::Added, e.entity_id, RemoveReason::Deleted);
    counters_.added++;
    return Result::Added;
  }
  size_t i = it->second;
  if (e.seq != 0 && e.seq <= seq_[i]) {
    counters_.ignored_older++;
    return Result::IgnoredOlder;
  }
  GridRemove(i);
  Set(i, e);
  GridAdd(i);
  Journal(Change::Kind::Updated, e.entity_id, RemoveReason::Deleted);
  counters_.updated++;
  return Result::Updated;
}

void EntityStore::RemoveAt(size_t i) {
  GridRemove(i);
  size_t last = ids_.size() - 1;
  index_.erase(ids_[i]);
  if (i != last) {
    GridRemove(last);
    auto mv = [&](auto& v) { v[i] = std::move(v[last]); };
    mv(ids_); mv(etype_); mv(callsign_); mv(cot_type_); mv(disposition_); mv(origin_); mv(lifecycle_);
    mv(lat_); mv(lon_); mv(hae_); mv(course_); mv(speed_); mv(conf_);
    mv(t_); mv(expiry_); mv(stale_s_); mv(seq_); mv(sim_); mv(locatable_); mv(cls_); mv(cell_);
    index_[ids_[i]] = i;
    cell_[i] = -1;
    GridAdd(i);
  }
  auto pop = [](auto& v) { v.pop_back(); };
  pop(ids_); pop(etype_); pop(callsign_); pop(cot_type_); pop(disposition_); pop(origin_); pop(lifecycle_);
  pop(lat_); pop(lon_); pop(hae_); pop(course_); pop(speed_); pop(conf_);
  pop(t_); pop(expiry_); pop(stale_s_); pop(seq_); pop(sim_); pop(locatable_); pop(cls_); pop(cell_);
}

bool EntityStore::Remove(const std::string& id, RemoveReason reason, TimeMs now) {
  auto it = index_.find(id);
  if (it == index_.end()) return false;
  int64_t seq = seq_[it->second];
  RemoveAt(it->second);
  Journal(Change::Kind::Removed, id, reason);
  if (reason == RemoveReason::Expired) counters_.expired++;
  else counters_.removed++;
  if (reason != RemoveReason::LeftAoi && reason != RemoveReason::LayerOff) tomb_[id] = {seq, now};
  return true;
}

size_t EntityStore::Expire(TimeMs now) {
  std::vector<std::string> gone;
  for (size_t i = 0; i < ids_.size(); ++i)
    if (expiry_[i] != 0 && expiry_[i] <= now) gone.push_back(ids_[i]);
  for (const auto& id : gone) Remove(id, RemoveReason::Expired, now);
  // Prune tombstones older than 10 minutes.
  for (auto it = tomb_.begin(); it != tomb_.end();) {
    if (now - it->second.second > 600000) it = tomb_.erase(it);
    else ++it;
  }
  return gone.size();
}

void EntityStore::Clear(RemoveReason reason) {
  for (const auto& id : ids_) Journal(Change::Kind::Removed, id, reason);
  index_.clear();
  grid_.clear();
  for (auto* v : {&ids_, &etype_, &callsign_, &cot_type_, &disposition_, &origin_, &lifecycle_}) v->clear();
  for (auto* v : {&lat_, &lon_, &hae_, &course_, &speed_, &conf_}) v->clear();
  t_.clear(); expiry_.clear(); stale_s_.clear(); seq_.clear(); sim_.clear(); locatable_.clear(); cls_.clear(); cell_.clear();
  tomb_.clear();
}

int64_t EntityStore::IndexOf(const std::string& id) const {
  auto it = index_.find(id);
  return it == index_.end() ? -1 : static_cast<int64_t>(it->second);
}

int64_t EntityStore::SeqOf(const std::string& id) const {
  auto it = index_.find(id);
  return it == index_.end() ? -1 : seq_[it->second];
}

RenderEntity EntityStore::Get(size_t i) const {
  RenderEntity e;
  if (i >= ids_.size()) return e;
  e.entity_id = ids_[i];
  e.entity_type = etype_[i];
  e.callsign = callsign_[i];
  e.cot_type = cot_type_[i];
  e.disposition = disposition_[i];
  e.origin = origin_[i];
  e.lifecycle = lifecycle_[i];
  e.lat = lat_[i];
  e.lon = lon_[i];
  e.hae = hae_[i];
  e.course = course_[i];
  e.speed = speed_[i];
  e.confidence = conf_[i];
  e.t = t_[i];
  e.expiry_t = expiry_[i];
  e.stale_s = stale_s_[i];
  e.seq = seq_[i];
  e.simulated = sim_[i] != 0;
  e.locatable = locatable_[i] != 0;
  return e;
}

bool EntityStore::IsStale(size_t i, TimeMs now) const {
  if (i >= ids_.size()) return false;
  if (lifecycle_[i] == "stale" || lifecycle_[i] == "lost") return true;
  return stale_s_[i] > 0 && t_[i] > 0 && now - t_[i] > stale_s_[i] * 1000;
}

std::string EntityStore::Pick(double lat, double lon, double radius_m) const {
  int64_t a0 = static_cast<int64_t>(std::floor((lat + 90.0) * 10.0));
  int64_t b0 = static_cast<int64_t>(std::floor((lon + 180.0) * 10.0));
  int span = radius_m > 11000 ? static_cast<int>(std::ceil(radius_m / 11000.0)) : 1;
  std::string best;
  double best_d = radius_m;
  for (int64_t da = -span; da <= span; ++da) {
    for (int64_t db = -span; db <= span; ++db) {
      auto it = grid_.find((a0 + da) * 4000 + (b0 + db));
      if (it == grid_.end()) continue;
      for (size_t i : it->second) {
        double d = Geo::HaversineM(lat, lon, lat_[i], lon_[i]);
        if (d <= best_d) {
          best_d = d;
          best = ids_[i];
        }
      }
    }
  }
  return best;
}

std::vector<std::string> EntityStore::InBox(double lon0, double lat0, double lon1, double lat1) const {
  std::vector<std::string> out;
  for (size_t i = 0; i < ids_.size(); ++i) {
    if (locatable_[i] && lat_[i] >= lat0 && lat_[i] <= lat1 && lon_[i] >= lon0 && lon_[i] <= lon1)
      out.push_back(ids_[i]);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------

void EntityDetails::Update(const Json& rec) {
  record = rec;
  RenderEntity e;
  if (!RenderEntity::FromJson(rec, e) || !e.locatable) return;
  if (!history.empty() && history.back().t == e.t) return;
  history.push_back(Fix{e.t, e.lat, e.lon, e.hae});
  while (history.size() > max_history) history.pop_front();
}

namespace entity_detail {
std::string Num(double v, int prec = 2) {
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%.*f", prec, v);
  return buf;
}
std::string Age(TimeMs now, TimeMs t) {
  if (t <= 0) return "unknown";
  double s = static_cast<double>(now - t) / 1000.0;
  return Num(s, 1) + " s ago";
}
}  // namespace entity_detail

std::vector<InspectorRow> BuildInspectorRows(const EntityDetails& d, TimeMs now) {
  using entity_detail::Age;
  using entity_detail::Num;
  std::vector<InspectorRow> rows;
  const Json& r = d.record;
  auto add = [&](const std::string& s, const std::string& k, const std::string& v) { rows.push_back({s, k, v}); };
  add("Identity", "entity_id", r.str("entity_id", r.str("uid")));
  add("Identity", "entity_type", r.str("entity_type"));
  add("Identity", "callsign", r.str("callsign", r["aliases"].str("name")));
  if (r.has("kind")) add("Identity", "kind", r.str("kind"));
  add("Identity", "cot_type", r.str("cot_type"));
  add("Identity", "disposition", r.str("disposition", "unknown"));
  add("Identity", "simulated", r["indicators"].boolean("simulated", r.boolean("simulated")) ? "yes" : "no");
  for (const auto& a : r["aliases"]["ids"].items()) add("Identity", "alias " + a.str("type"), a.str("id"));
  add("State", "lifecycle", r.str("lifecycle"));
  add("State", "seq", std::to_string(r.i64("seq")));
  add("State", "updated", Age(now, r.i64("t")));
  if (r["expiry_t"].is_number()) add("State", "expires in", Num(static_cast<double>(r.i64("expiry_t") - now) / 1000.0, 1) + " s");
  else add("State", "expires", r.boolean("pinned") ? "never (pinned)" : "unknown");
  const Json& loc = r["location"];
  add("Position", "lat, lon", Num(r.num("lat", loc.num("lat")), 6) + ", " + Num(r.num("lon", loc.num("lon")), 6));
  add("Position", "hae m", Num(r.num("hae", loc.num("hae_m", -1)), 1));
  add("Position", "course, speed", Num(r.num("course", -1), 0) + " deg, " + Num(r.num("speed", -1), 1) + " m/s");
  const Json& u = r["uncertainty"];
  if (u.is_object()) add("Position", "ce, le m (1-sigma)", Num(u.num("ce_m", -1), 1) + ", " + Num(u.num("le_m", -1), 1));
  add("Confidence", "confidence", Num(r.num("confidence", -1), 3));
  const Json& cp = r["confidence_parts"];
  add("Confidence", "source_prior", Num(cp.num("source_prior", -1), 3));
  add("Confidence", "track_quality", Num(cp.num("track_quality", -1), 3));
  add("Confidence", "corroboration", Num(cp.num("corroboration", -1), 3));
  size_t n = 0;
  for (const auto& s : r["sources"].items()) {
    std::string k = "source " + std::to_string(++n);
    add("Sources", k, s.str("integration") + " (" + s.str("data_type") + ")");
    add("Sources", k + " subject", s.str("subject"));
    add("Sources", k + " stream/seq", s.str("stream") + " #" + std::to_string(s.i64("seq")));
    add("Sources", k + " last seen", Age(now, s.i64("last_seen_t")));
  }
  if (n == 0) add("Sources", "sources", "none listed");
  const Json& prov = r["provenance"];
  add("Provenance", "primary", prov.str("primary"));
  add("Provenance", "how", prov.str("how"));
  for (const auto& c : r["conflicts"].items())
    add("Conflicts", c.str("attribute"), "kept " + c["kept"].Dump() + ", rejected " + c["rejected"].Dump());
  add("Marking", "tlp", r["marking"].str("tlp"));
  add("History", "fixes kept", std::to_string(d.history.size()));
  if (!d.history.empty()) {
    const auto& f = d.history.front();
    add("History", "oldest", Age(now, f.t) + " at " + Num(f.lat, 5) + ", " + Num(f.lon, 5));
  }
  std::string stream, subject;
  uint64_t seq = 0;
  if (FindRawCotSource(r, stream, seq, subject))
    add("Raw CoT", "from", stream + " #" + std::to_string(seq) + " (" + subject + ")");
  return rows;
}

bool FindRawCotSource(const Json& record, std::string& stream, uint64_t& seq, std::string& subject) {
  for (const auto& s : record["sources"].items()) {
    std::string st = s.str("stream");
    std::string sub = s.str("subject");
    if ((st == "COT" || st == "SITE_COT") && s.i64("seq") > 0) {
      stream = st;
      seq = static_cast<uint64_t>(s.i64("seq"));
      subject = sub;
      return true;
    }
  }
  return false;
}

}  // namespace om
