// SPDX-License-Identifier: GPL-3.0-or-later
// Intelligence-gaps panel model (feature B20); see FeedGaps.h.
#include "OneMindCore/FeedGaps.h"

#include "OneMindCore/CoT.h"  // ParseCotTime (RFC 3339 with Z)

#include <algorithm>

namespace om {

namespace {
// fetchedAt is RFC 3339 in omos-ingest's status (Go time.Time) and epoch ms in the layer
// manifest (layer-snapshot.md §2). Accept both.
TimeMs TimeField(const Json& j, const std::string& key) {
  const Json& v = j[key];
  if (v.is_number()) return v.as_int();
  if (v.is_string()) {
    TimeMs t = 0;
    if (ParseCotTime(v.as_string(), t)) return t;
  }
  return 0;
}
int Rank(FeedGap::Status s) {
  switch (s) {
    case FeedGap::Status::Missing: return 0;
    case FeedGap::Status::Failed: return 1;
    case FeedGap::Status::Stale: return 2;
    default: return 3;
  }
}
}  // namespace

const char* FeedGapStatusName(FeedGap::Status s) {
  switch (s) {
    case FeedGap::Status::Fresh: return "fresh";
    case FeedGap::Status::Stale: return "stale";
    case FeedGap::Status::Failed: return "failed";
    case FeedGap::Status::Missing: return "missing";
  }
  return "?";
}

bool FeedGaps::IngestStatus(const std::string& body, TimeMs now) {
  Json j;
  if (!Json::Parse(body, j) || !j.is_object() || j.str("source").empty()) return false;
  FeedState& f = feeds_[j.str("source")];
  f.source = j.str("source");
  f.kind = j.str("kind");
  f.collector = j.str("collector");
  f.fetched_at = TimeField(j, "fetchedAt");
  f.count = j.i64("count");
  f.last_error = j["lastError"].is_string() ? j["lastError"].as_string() : std::string();
  if (j.has("staleAfterSec")) f.stale_after_s = j.num("staleAfterSec");
  f.stale_flag = j.boolean("stale");
  f.heard_at = now;
  return true;
}

void FeedGaps::IngestManifest(const std::string& layer, const Json& manifest, TimeMs now) {
  const Json& fr = manifest["freshness"];
  if (!fr.is_object()) return;
  double limit = fr.num("limit_s", 0);
  for (const auto& fd : fr["feeds"].items()) {
    std::string src = fd.str("source");
    if (src.empty()) continue;
    layer_feeds_[layer].insert(src);
    FeedState& f = feeds_[src];
    f.source = src;
    // The manifest copies the heartbeat; it never replaces a fresher direct one.
    TimeMs fa = TimeField(fd, "fetchedAt");
    if (fa > f.fetched_at) {
      f.fetched_at = fa;
      f.count = fd.i64("count");
      f.last_error = fd["lastError"].is_string() ? fd["lastError"].as_string() : std::string();
      f.stale_flag = fd.boolean("stale");
    }
    if (f.stale_after_s <= 0 && limit > 0) f.stale_after_s = limit;
    if (f.heard_at == 0) f.heard_at = now;
  }
}

void FeedGaps::SetLayerFeeds(const std::string& layer, const std::vector<std::string>& sources) {
  for (const auto& s : sources) layer_feeds_[layer].insert(s);
}

void FeedGaps::SetAlertFeeds(const std::string& alert_kind, const std::vector<std::string>& sources) {
  for (const auto& s : sources) alert_feeds_[alert_kind].insert(s);
}

void FeedGaps::ExpectFeed(const std::string& source, double stale_after_s) {
  FeedState& f = feeds_[source];
  f.source = source;
  f.expected = true;
  if (f.stale_after_s <= 0) f.stale_after_s = stale_after_s;
}

FeedGap FeedGaps::Evaluate(const FeedState& f, TimeMs now) const {
  FeedGap g;
  g.source = f.source;
  for (const auto& lf : layer_feeds_)
    if (lf.second.count(f.source)) g.layers.push_back(lf.first);
  for (const auto& af : alert_feeds_)
    if (af.second.count(f.source)) g.alerts.push_back(af.first);
  double limit = f.stale_after_s > 0 ? f.stale_after_s : 3.0 * heartbeat_s_;
  double silent_limit = (std::max)(limit, 3.0 * heartbeat_s_) + grace_s_;
  if (f.heard_at == 0) {
    g.status = FeedGap::Status::Missing;
    g.reason = "never heard";
    return g;
  }
  double silent = (now - f.heard_at) / 1000.0;
  if (f.fetched_at > 0) g.age_s = (now - f.fetched_at) / 1000.0;
  if (silent > silent_limit) {
    g.status = FeedGap::Status::Missing;
    g.age_s = silent;
    g.reason = "no heartbeat for " + std::to_string(static_cast<int>(silent)) + " s";
    return g;
  }
  if (!f.last_error.empty()) {
    g.status = FeedGap::Status::Failed;
    g.reason = f.last_error;
    return g;
  }
  if (f.fetched_at == 0 || f.stale_flag || (g.age_s >= 0 && g.age_s > limit)) {
    g.status = FeedGap::Status::Stale;
    g.reason = f.fetched_at == 0 ? "no successful fetch yet"
                                 : "data " + std::to_string(static_cast<int>(g.age_s)) + " s old (limit " +
                                       std::to_string(static_cast<int>(limit)) + " s)";
    return g;
  }
  g.status = FeedGap::Status::Fresh;
  return g;
}

std::vector<FeedGap> FeedGaps::All(TimeMs now) const {
  std::vector<FeedGap> out;
  std::set<std::string> sources;
  for (const auto& f : feeds_) sources.insert(f.first);
  for (const auto& lf : layer_feeds_) sources.insert(lf.second.begin(), lf.second.end());
  for (const auto& af : alert_feeds_) sources.insert(af.second.begin(), af.second.end());
  for (const auto& s : sources) {
    auto it = feeds_.find(s);
    FeedState f;
    if (it != feeds_.end()) f = it->second;
    f.source = s;
    out.push_back(Evaluate(f, now));
  }
  std::stable_sort(out.begin(), out.end(), [](const FeedGap& a, const FeedGap& b) { return Rank(a.status) < Rank(b.status); });
  return out;
}

std::vector<FeedGap> FeedGaps::Gaps(TimeMs now) const {
  std::vector<FeedGap> out;
  for (auto& g : All(now))
    if (g.status != FeedGap::Status::Fresh) out.push_back(g);
  return out;
}

std::vector<LayerHealth> FeedGaps::Layers(TimeMs now) const {
  std::vector<LayerHealth> out;
  for (const auto& lf : layer_feeds_) {
    LayerHealth h;
    h.layer = lf.first;
    size_t good = 0;
    for (const auto& s : lf.second) {
      auto it = feeds_.find(s);
      FeedState f;
      if (it != feeds_.end()) f = it->second;
      f.source = s;
      if (Evaluate(f, now).status == FeedGap::Status::Fresh) ++good;
      else h.bad_feeds.push_back(s);
    }
    h.status = lf.second.empty() ? "unknown" : good == lf.second.size() ? "fresh" : good == 0 ? "stale" : "degraded";
    out.push_back(h);
  }
  return out;
}

}  // namespace om
