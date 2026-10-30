// Layer system core (OneMindLayers, feature B4): a layer is a snapshot plus deltas, a
// store, a renderer, a style and a filter. Toggling a layer on subscribes; off
// unsubscribes and clears it ("a layer off means no subscription", B4 acceptance).
//
// Three read paths, chosen per layer:
//  - Watch  (required path for every client): svc.layer.snapshot then svc.layer.watch
//           (contracts layer-snapshot.md §3.1, layer-delta.md §2).
//  - Direct (trusted site clients): ordered consumer on ENT, last_per_subject, with
//           delete markers (layer-snapshot.md §3.2, layer-delta.md §3).
//  - Core   (interim until ENT/the layer service exist): svc.entity.query pages, then a
//           core subscription to ent.{type}.*.state, expiring from expiry_t
//           (entity.md §8 "Interim").
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Entity.h"
#include "OneMindCore/JetStream.h"
#include "OneMindCore/NatsClient.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace om {

struct OM_CORE_API Aoi {
  bool has_bbox = false;
  double lon0 = 0, lat0 = 0, lon1 = 0, lat1 = 0;
  std::string zone_id;
  Json ToJson() const;  // null when unset
  static Aoi Bbox(double lon0, double lat0, double lon1, double lat1);
};

enum class LayerPath { Watch, Direct, Core };
OM_CORE_API const char* LayerPathName(LayerPath p);
OM_CORE_API bool ParseLayerPath(const std::string& s, LayerPath& out);

struct OM_CORE_API LayerSpec {
  std::string id;      // layer id (layer-snapshot.md §1), e.g. "fleet", "tak", "air"
  std::string title;
  std::vector<std::string> entity_types;
  LayerPath path = LayerPath::Watch;
  std::string tier = "small";  // small | medium | large | static
  Aoi aoi;
  int zoom = 6;
  int max = 5000;
  int lease_s = 60;
  int max_rate_hz = 4;
  std::string ent_stream = "ENT";
  std::string style;  // renderer style key, e.g. "air.default.v1"

  /// Whether the camera AOI (LayerManager::SetAoi) applies to this layer. Watch-path
  /// layers pass it to the layer service; large layers on any path need it. Direct and
  /// core small/medium/static layers load whole and cull on the GPU (layer-snapshot.md
  /// §1 tier rules), so the camera never filters them. An AOI set in the spec itself
  /// (config) still applies to every path.
  bool UsesCameraAoi() const { return path == LayerPath::Watch || tier == "large"; }
};

/// Layer manifest (KV world-state `{layer}.meta`, layer-snapshot.md §2).
struct OM_CORE_API LayerManifest {
  bool present = false;
  std::string layer, title, tier, filter, site, status, marking, style;
  std::vector<std::string> entity_types;
  int64_t count = 0, cap = 0, ent_seq = 0, limit_s = 0;
  TimeMs as_of = 0;
  static bool FromJson(const Json& j, LayerManifest& out);
};

enum class LayerState { Off, Snapshotting, Live, Resyncing, Error };
OM_CORE_API const char* LayerStateName(LayerState s);

/// One layer's live session over the bus. Owns its EntityStore.
class OM_CORE_API LayerSession {
 public:
  LayerSession(NatsClient& nc, JetStream& js, LayerSpec spec);
  ~LayerSession();
  LayerSession(const LayerSession&) = delete;
  LayerSession& operator=(const LayerSession&) = delete;

  void Start(TimeMs now);
  void Stop();
  void Tick(TimeMs now);
  /// Changes the area of interest. Watch path: renews the watch with the new AOI so the
  /// service sends enter/exit. Direct/core paths (client-side AOI filter): drops what is
  /// now outside and re-runs the snapshot so entities that came back into view reload.
  void SetAoi(const Aoi& aoi, int zoom, TimeMs now);

  const LayerSpec& spec() const { return spec_; }
  LayerState state() const { return state_; }
  EntityStore& store() { return store_; }
  const EntityStore& store() const { return store_; }
  const std::string& last_error() const { return last_error_; }
  /// Mode of the last snapshot/batch ("entities" or "cells").
  const std::string& mode() const { return mode_; }
  const std::vector<std::vector<double>>& cells() const { return cells_; }

  struct Stats {
    uint64_t snapshots = 0, snapshot_items = 0, batches = 0, ops = 0, resnapshots = 0, heartbeats = 0;
    int64_t last_snapshot_ms = 0;  // wall time of the last snapshot load
    int64_t ent_seq = 0;
    int64_t last_batch_n = 0;
    std::string watch_id;
  };
  const Stats& stats() const { return stats_; }

  /// Full records seen on the direct/core paths (for the inspector without a request).
  using RecordSink = std::function<void(const Json& record)>;
  void SetRecordSink(RecordSink s) { record_sink_ = std::move(s); }

 private:
  void StartSnapshot(TimeMs now);
  void OnSnapshotChunk(const Json& j, TimeMs now);
  void FinishSnapshot(TimeMs now);
  void StartWatch(TimeMs now);
  void Renew(TimeMs now);
  void OnWatchMsg(const Json& j, TimeMs now);
  void ApplyOps(const Json& ops, TimeMs now);
  void Resnapshot(const std::string& why, TimeMs now);
  void Fail(const std::string& why, TimeMs now);
  void StartDirect(TimeMs now, bool resync = false);
  void PruneOutsideAoi(TimeMs now);
  void StartCore(TimeMs now);
  void QueryPage(const std::string& cursor, TimeMs now);
  void OnEntityRecord(const std::string& subject, const NatsMsg& m, const std::string& marker, TimeMs now);
  Json WatchRequest() const;

  NatsClient& nc_;
  JetStream& js_;
  LayerSpec spec_;
  EntityStore store_;
  LayerState state_ = LayerState::Off;
  std::string last_error_;
  std::string mode_ = "entities";
  std::vector<std::vector<double>> cells_;
  std::shared_ptr<bool> alive_;
  uint64_t gen_ = 0;
  TimeMs now_ = 0;
  TimeMs retry_at_ = 0;
  TimeMs snap_started_ = 0;
  TimeMs renew_at_ = 0;
  TimeMs last_batch_at_ = 0;
  uint64_t snap_req_ = 0, watch_req_ = 0;
  int64_t expect_part_ = 0;
  std::vector<RenderEntity> snap_items_;
  bool snap_truncated_ = false;
  std::vector<std::unique_ptr<OrderedConsumer>> consumers_;
  std::vector<int64_t> core_sids_;
  std::map<std::string, bool> snap_seen_;
  uint64_t seen_reconnects_ = 0;
  RecordSink record_sink_;
  Stats stats_;
};

/// Registry of layers; Toggle subscribes/unsubscribes.
class OM_CORE_API LayerManager {
 public:
  LayerManager(NatsClient& nc, JetStream& js) : nc_(nc), js_(js) {}
  void Register(const LayerSpec& spec);
  bool Toggle(const std::string& id, bool on, TimeMs now);
  bool IsOn(const std::string& id) const;
  void SetAoi(const Aoi& aoi, int zoom, TimeMs now);
  void Tick(TimeMs now);
  LayerSession* Get(const std::string& id);
  std::vector<std::string> Ids() const;
  size_t TotalEntities() const;
  /// Applies a manifest from KV world-state (`{layer}.meta`).
  void OnManifest(const std::string& layer, const LayerManifest& m);
  const LayerManifest* Manifest(const std::string& layer) const;
  /// Full entity records seen by any session (direct/core paths) go here.
  void SetRecordSink(LayerSession::RecordSink sink) { sink_ = std::move(sink); }

  /// Parses layer specs from JSON: [{"id","title","entity_types":[],"path","tier",
  /// "aoi":{"bbox":[...]},"zoom","max"}]. Unknown fields are ignored.
  static bool ParseSpecs(const std::string& json, std::vector<LayerSpec>& out, std::string* err = nullptr);

 private:
  NatsClient& nc_;
  JetStream& js_;
  std::map<std::string, LayerSpec> specs_;
  std::map<std::string, std::unique_ptr<LayerSession>> sessions_;
  std::map<std::string, LayerManifest> manifests_;
  Aoi aoi_;
  int zoom_ = -1;
  LayerSession::RecordSink sink_;
};

}  // namespace om
