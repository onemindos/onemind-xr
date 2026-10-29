// Intelligence-gaps panel model (feature B20): which feeds are stale, failing or missing,
// and which layers and alerts they weaken.
//
// Inputs (both already defined on the bus):
//  - `tel.feed.{source}.status`: the omos-ingest freshness heartbeat (PR #553,
//    services/bridge/ingest/internal/ingest/status.go, schema
//    services/schemas/ingest/feed-status.v1.schema.json), sent every 10 s by the lease holder:
//    {source, kind, collector, instance, leader, fetchedAt, count, published, lastError,
//     errorAt, staleAfterSec, ageSec, stale, at}.
//  - the layer manifest's `freshness` block (layer-snapshot.md §2): {status, limit_s,
//    feeds:[{source, fetchedAt, count, lastError, stale}]}, which also tells the client which
//    feeds each layer depends on.
// A feed is
//  - failed   when its last status carries `lastError` (the last fetch failed);
//  - stale    when its data is older than `staleAfterSec` (the collector's own flag, or
//             fetchedAt age by the local clock as a cross-check);
//  - missing  when no heartbeat arrived for max(staleAfterSec, 3 heartbeats) plus a grace,
//             timed by the client's receive clock, so collector clock skew cannot hide it
//             (a stopped collector stops sending heartbeats, so "missing" is how a killed
//             feed shows up within its limit);
//  - expected but never heard, when configured with ExpectFeed.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Json.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace om {

struct OM_CORE_API FeedState {
  std::string source, kind, collector;
  TimeMs fetched_at = 0;   // collector's last good fetch (epoch ms)
  TimeMs heard_at = 0;     // local receive time of the last heartbeat
  int64_t count = 0;
  std::string last_error;
  double stale_after_s = 0;
  bool stale_flag = false;
  bool expected = false;
};

struct OM_CORE_API FeedGap {
  enum class Status { Fresh, Stale, Failed, Missing };
  std::string source;
  Status status = Status::Fresh;
  std::string reason;
  double age_s = -1;  // seconds since the last good fetch (or heartbeat for Missing), -1 unknown
  std::vector<std::string> layers;  // layers this feed weakens
  std::vector<std::string> alerts;  // alert kinds it weakens
};

struct OM_CORE_API LayerHealth {
  std::string layer;
  std::string status;  // fresh | degraded | stale (all feeds bad) | unknown (no feeds known)
  std::vector<std::string> bad_feeds;
};

OM_CORE_API const char* FeedGapStatusName(FeedGap::Status s);

class OM_CORE_API FeedGaps {
 public:
  explicit FeedGaps(int heartbeat_s = 10, int grace_s = 5) : heartbeat_s_(heartbeat_s), grace_s_(grace_s) {}

  /// A `tel.feed.{source}.status` message body. Returns false if it is not a status.
  bool IngestStatus(const std::string& body, TimeMs now);
  /// A layer manifest (`world-state` `{layer}.meta`): records the layer's feeds.
  void IngestManifest(const std::string& layer, const Json& manifest, TimeMs now);
  /// Configured dependencies (DefaultGame.ini), for layers without a manifest yet.
  void SetLayerFeeds(const std::string& layer, const std::vector<std::string>& sources);
  void SetAlertFeeds(const std::string& alert_kind, const std::vector<std::string>& sources);
  /// A feed that should exist; it shows as missing until heard.
  void ExpectFeed(const std::string& source, double stale_after_s);

  /// The panel: every feed that is not fresh, worst first (missing, failed, stale).
  std::vector<FeedGap> Gaps(TimeMs now) const;
  /// Every feed with its status (for a full list view).
  std::vector<FeedGap> All(TimeMs now) const;
  std::vector<LayerHealth> Layers(TimeMs now) const;
  const std::map<std::string, FeedState>& feeds() const { return feeds_; }

 private:
  FeedGap Evaluate(const FeedState& f, TimeMs now) const;
  int heartbeat_s_, grace_s_;
  std::map<std::string, FeedState> feeds_;
  std::map<std::string, std::set<std::string>> layer_feeds_;  // layer -> sources
  std::map<std::string, std::set<std::string>> alert_feeds_;  // alert kind -> sources
};

}  // namespace om
