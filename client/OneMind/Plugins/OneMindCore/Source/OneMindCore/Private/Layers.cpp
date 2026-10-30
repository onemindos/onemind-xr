// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Layers.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace om {

Json Aoi::ToJson() const {
  if (has_bbox) {
    Json b = Json::MakeArray();
    b.push_back(lon0);
    b.push_back(lat0);
    b.push_back(lon1);
    b.push_back(lat1);
    Json o = Json::MakeObject();
    o.set("bbox", b);
    return o;
  }
  if (!zone_id.empty()) {
    Json o = Json::MakeObject();
    o.set("zone_id", zone_id);
    return o;
  }
  return Json();
}

Aoi Aoi::Bbox(double lon0, double lat0, double lon1, double lat1) {
  Aoi a;
  a.has_bbox = true;
  a.lon0 = lon0;
  a.lat0 = lat0;
  a.lon1 = lon1;
  a.lat1 = lat1;
  return a;
}

const char* LayerPathName(LayerPath p) {
  switch (p) {
    case LayerPath::Watch: return "watch";
    case LayerPath::Direct: return "direct";
    case LayerPath::Core: return "core";
  }
  return "watch";
}

bool ParseLayerPath(const std::string& s, LayerPath& out) {
  if (s == "watch") out = LayerPath::Watch;
  else if (s == "direct") out = LayerPath::Direct;
  else if (s == "core") out = LayerPath::Core;
  else return false;
  return true;
}

const char* LayerStateName(LayerState s) {
  switch (s) {
    case LayerState::Off: return "off";
    case LayerState::Snapshotting: return "snapshotting";
    case LayerState::Live: return "live";
    case LayerState::Resyncing: return "resyncing";
    case LayerState::Error: return "error";
  }
  return "off";
}

bool LayerManifest::FromJson(const Json& j, LayerManifest& m) {
  if (!j.is_object() || j.str("layer").empty()) return false;
  m = LayerManifest();
  m.present = true;
  m.layer = j.str("layer");
  m.title = j.str("title");
  m.tier = j.str("tier");
  m.filter = j.str("filter");
  m.site = j.str("site");
  m.marking = j.str("marking");
  m.style = j.str("style");
  for (const auto& t : j["entity_types"].items()) m.entity_types.push_back(t.as_string());
  m.count = j.i64("count");
  m.cap = j.i64("cap");
  m.ent_seq = j.i64("ent_seq");
  m.as_of = j.i64("as_of");
  m.status = j["freshness"].str("status", "fresh");
  m.limit_s = j["freshness"].i64("limit_s");
  return true;
}

// ---------------------------------------------------------------------------------------------

LayerSession::LayerSession(NatsClient& nc, JetStream& js, LayerSpec spec)
    : nc_(nc), js_(js), spec_(std::move(spec)), alive_(std::make_shared<bool>(true)) {}

LayerSession::~LayerSession() {
  Stop();
  *alive_ = false;
}

void LayerSession::Start(TimeMs now) {
  now_ = now;
  gen_++;
  seen_reconnects_ = nc_.stats().reconnects;
  last_error_.clear();
  switch (spec_.path) {
    case LayerPath::Watch: StartSnapshot(now); break;
    case LayerPath::Direct: StartDirect(now); break;
    case LayerPath::Core: StartCore(now); break;
  }
}

void LayerSession::Stop() {
  if (state_ == LayerState::Off) return;
  gen_++;
  if (snap_req_) nc_.CancelRequest(snap_req_);
  if (watch_req_) nc_.CancelRequest(watch_req_);
  snap_req_ = watch_req_ = 0;
  if (!stats_.watch_id.empty() && nc_.connected()) {
    Json u = Json::MakeObject();
    u.set("v", 1);
    u.set("watch_id", stats_.watch_id);
    nc_.Publish("svc.layer.unwatch", u.Dump());
  }
  stats_.watch_id.clear();
  for (auto& c : consumers_) c->Stop();
  consumers_.clear();
  for (int64_t sid : core_sids_) nc_.Unsubscribe(sid);
  core_sids_.clear();
  store_.Clear(RemoveReason::LayerOff);
  cells_.clear();
  state_ = LayerState::Off;
}

void LayerSession::Fail(const std::string& why, TimeMs now) {
  last_error_ = why;
  state_ = LayerState::Error;
  retry_at_ = now + 5000;
}

void LayerSession::Resnapshot(const std::string& why, TimeMs now) {
  stats_.resnapshots++;
  last_error_ = why;
  if (snap_req_) nc_.CancelRequest(snap_req_);
  if (watch_req_) nc_.CancelRequest(watch_req_);
  snap_req_ = watch_req_ = 0;
  if (!stats_.watch_id.empty() && nc_.connected()) {
    // Release the old watch now instead of letting its lease run out.
    Json u = Json::MakeObject();
    u.set("v", 1);
    u.set("watch_id", stats_.watch_id);
    nc_.Publish("svc.layer.unwatch", u.Dump());
  }
  stats_.watch_id.clear();
  gen_++;
  state_ = LayerState::Resyncing;
  if (nc_.connected()) StartSnapshot(now);
  else retry_at_ = now + 500;
}

void LayerSession::StartSnapshot(TimeMs now) {
  if (!nc_.connected()) {
    state_ = LayerState::Resyncing;
    retry_at_ = now + 500;
    return;
  }
  if (state_ != LayerState::Resyncing) state_ = LayerState::Snapshotting;
  snap_items_.clear();
  snap_truncated_ = false;
  expect_part_ = 0;
  snap_started_ = WallClockMs();
  Json req = Json::MakeObject();
  req.set("v", 1);
  req.set("layer", spec_.id);
  req.set("aoi", spec_.aoi.ToJson());
  req.set("zoom", spec_.zoom);
  req.set("fields", "render");
  req.set("max", spec_.max);
  std::weak_ptr<bool> alive = alive_;
  uint64_t gen = gen_;
  snap_req_ = nc_.Request(
      "svc.layer.snapshot", req.Dump(),
      [this, alive, gen](const NatsMsg* m, const std::string& error) {
        auto a = alive.lock();
        if (!a || !*a || gen != gen_) return true;
        if (!m) {
          snap_req_ = 0;
          if (error == "disconnected") {
            state_ = LayerState::Resyncing;
            retry_at_ = now_ + 500;
          } else {
            Fail("snapshot: " + error, now_);
          }
          return true;
        }
        Json j;
        if (!Json::Parse(m->data, j)) {
          snap_req_ = 0;
          Fail("snapshot: bad JSON", now_);
          return true;
        }
        if (j.has("error")) {
          snap_req_ = 0;
          Fail("snapshot: " + j.str("error") + " " + j.str("detail"), now_);
          return true;
        }
        OnSnapshotChunk(j, now_);
        if (gen != gen_) return true;  // the chunk handler restarted us
        if (j.boolean("last")) {
          snap_req_ = 0;
          FinishSnapshot(now_);
          return true;
        }
        return false;
      },
      10000, true);
}

void LayerSession::OnSnapshotChunk(const Json& j, TimeMs now) {
  if (j.i64("part", -1) != expect_part_) {
    Resnapshot("snapshot chunk out of order", now);
    return;
  }
  expect_part_++;
  mode_ = j.str("mode", "entities");
  stats_.ent_seq = j.i64("ent_seq", stats_.ent_seq);
  if (j.boolean("truncated")) snap_truncated_ = true;
  if (mode_ == "cells") {
    if (j.i64("part") == 0) cells_.clear();
    for (const auto& c : j["items"].items()) {
      std::vector<double> row;
      for (const auto& v : c.items()) row.push_back(v.as_number());
      cells_.push_back(row);
    }
    return;
  }
  for (const auto& it : j["items"].items()) {
    RenderEntity e;
    if (RenderEntity::FromJson(it, e)) snap_items_.push_back(std::move(e));
  }
}

void LayerSession::FinishSnapshot(TimeMs now) {
  std::set<std::string> in_snap;
  for (const auto& e : snap_items_) in_snap.insert(e.entity_id);
  std::vector<std::string> drop;
  for (const auto& id : store_.ids())
    if (!in_snap.count(id)) drop.push_back(id);
  for (const auto& id : drop) store_.Remove(id, RemoveReason::LeftAoi, now);
  if (mode_ == "cells") {
    store_.Clear(RemoveReason::LeftAoi);
  } else {
    cells_.clear();
  }
  for (const auto& e : snap_items_) store_.Upsert(e, now);
  stats_.snapshots++;
  stats_.snapshot_items = snap_items_.size();
  stats_.last_snapshot_ms = WallClockMs() - snap_started_;
  snap_items_.clear();
  state_ = LayerState::Live;
  StartWatch(now);
}

Json LayerSession::WatchRequest() const {
  Json req = Json::MakeObject();
  req.set("v", 1);
  req.set("layer", spec_.id);
  req.set("aoi", spec_.aoi.ToJson());
  req.set("zoom", spec_.zoom);
  req.set("fields", "render");
  req.set("from_seq", stats_.ent_seq + 1);
  req.set("lease_s", spec_.lease_s);
  req.set("max_rate_hz", spec_.max_rate_hz);
  req.set("watch_id", stats_.watch_id.empty() ? Json() : Json(stats_.watch_id));
  return req;
}

void LayerSession::StartWatch(TimeMs now) {
  stats_.last_batch_n = 0;
  stats_.watch_id.clear();
  last_batch_at_ = now;
  std::weak_ptr<bool> alive = alive_;
  uint64_t gen = gen_;
  // A batch (or heartbeat) arrives at least every 10 s; 30 s of silence means a dead
  // watch and a new snapshot (layer-delta.md §2.2 "Gaps").
  watch_req_ = nc_.Request(
      "svc.layer.watch", WatchRequest().Dump(),
      [this, alive, gen](const NatsMsg* m, const std::string& error) {
        auto a = alive.lock();
        if (!a || !*a || gen != gen_) return true;
        if (!m) {
          watch_req_ = 0;
          if (error == "disconnected") {
            state_ = LayerState::Resyncing;
            retry_at_ = now_ + 500;
          } else {
            Resnapshot("watch: " + error, now_);
          }
          return true;
        }
        Json j;
        if (!Json::Parse(m->data, j)) return false;
        OnWatchMsg(j, now_);
        return gen != gen_;
      },
      30000, true);
}

void LayerSession::OnWatchMsg(const Json& j, TimeMs now) {
  if (j.has("error")) {
    if (j.str("error") == "resnapshot") Resnapshot("service asked for a new snapshot", now);
    else Fail("watch: " + j.str("error") + " " + j.str("detail"), now);
    return;
  }
  last_batch_at_ = now;
  if (!j.has("ops")) {
    // First reply (or renewal ack): {v, watch_id, lease_s, ent_seq}.
    stats_.watch_id = j.str("watch_id", stats_.watch_id);
    int lease = static_cast<int>(j.i64("lease_s", spec_.lease_s));
    renew_at_ = now + static_cast<TimeMs>(lease) * 500;  // renew at half the lease
    return;
  }
  int64_t n = j.i64("n");
  if (stats_.last_batch_n != 0 && n != stats_.last_batch_n + 1) {
    Resnapshot("delta batch gap (" + std::to_string(stats_.last_batch_n) + " -> " + std::to_string(n) + ")", now);
    return;
  }
  stats_.last_batch_n = n;
  stats_.batches++;
  stats_.ent_seq = (std::max)(stats_.ent_seq, j.i64("to_seq"));
  if (j["ops"].size() == 0) stats_.heartbeats++;
  mode_ = j.str("mode", mode_);
  ApplyOps(j["ops"], now);
}

void LayerSession::ApplyOps(const Json& ops, TimeMs now) {
  for (const auto& op : ops.items()) {
    std::string kind = op.str("op");
    stats_.ops++;
    if (kind == "upsert" || kind == "enter") {
      RenderEntity e;
      if (RenderEntity::FromJson(op["e"], e)) store_.Upsert(e, now);
    } else if (kind == "exit") {
      store_.Remove(op.str("id"), RemoveReason::LeftAoi, now);
    } else if (kind == "remove") {
      std::string reason = op.str("reason");
      RemoveReason r = reason == "expired"        ? RemoveReason::Expired
                       : reason == "not_live"     ? RemoveReason::NotLive
                       : reason == "released_out" ? RemoveReason::Released
                                                  : RemoveReason::Deleted;
      store_.Remove(op.str("id"), r, now);
    } else if (kind == "cells") {
      // Changed cells replace cells with the same x,y.
      for (const auto& c : op["cells"].items()) {
        std::vector<double> row;
        for (const auto& v : c.items()) row.push_back(v.as_number());
        if (row.size() < 3) continue;
        bool replaced = false;
        for (auto& existing : cells_) {
          if (existing.size() >= 2 && existing[0] == row[0] && existing[1] == row[1]) {
            existing = row;
            replaced = true;
            break;
          }
        }
        if (!replaced) cells_.push_back(row);
      }
    }
  }
}

void LayerSession::Renew(TimeMs now) {
  if (stats_.watch_id.empty() || !nc_.connected()) return;
  renew_at_ = now + static_cast<TimeMs>(spec_.lease_s) * 500;
  std::weak_ptr<bool> alive = alive_;
  uint64_t gen = gen_;
  nc_.Request(
      "svc.layer.watch", WatchRequest().Dump(),
      [this, alive, gen](const NatsMsg* m, const std::string& error) {
        auto a = alive.lock();
        if (!a || !*a || gen != gen_) return true;
        if (!m) {
          if (error != "disconnected") Resnapshot("renew: " + error, now_);
          return true;
        }
        Json j;
        if (Json::Parse(m->data, j)) OnWatchMsg(j, now_);
        return true;
      },
      5000);
}

void LayerSession::SetAoi(const Aoi& aoi, int zoom, TimeMs now) {
  spec_.aoi = aoi;
  spec_.zoom = zoom;
  if (state_ == LayerState::Off) return;
  switch (spec_.path) {
    case LayerPath::Watch:
      if (state_ == LayerState::Live && !stats_.watch_id.empty()) {
        Renew(now);  // same watch_id, new aoi/zoom: the service answers with enter/exit ops
      } else {
        Resnapshot("aoi changed", now);
      }
      break;
    case LayerPath::Direct:
      // Client-side AOI (layer-delta.md §1): entities that left are dropped now; entities
      // that entered are only in ENT, so the ordered consumers restart at last_per_subject.
      PruneOutsideAoi(now);
      stats_.resnapshots++;
      StartDirect(now, true);
      break;
    case LayerPath::Core:
      PruneOutsideAoi(now);
      stats_.resnapshots++;
      gen_++;  // ignore pages of the query for the old box
      // Snapshotting, not Resyncing: Tick re-sends the query in Resyncing.
      state_ = LayerState::Snapshotting;
      snap_started_ = WallClockMs();
      QueryPage(std::string(), now);
      break;
  }
}

void LayerSession::PruneOutsideAoi(TimeMs now) {
  if (!spec_.aoi.has_bbox) return;
  const Aoi& a = spec_.aoi;
  std::vector<std::string> drop;
  for (size_t i = 0; i < store_.size(); ++i) {
    if (!store_.locatables()[i]) continue;
    double lat = store_.lats()[i], lon = store_.lons()[i];
    if (lat < a.lat0 || lat > a.lat1 || lon < a.lon0 || lon > a.lon1) drop.push_back(store_.ids()[i]);
  }
  for (const auto& id : drop) store_.Remove(id, RemoveReason::LeftAoi, now);
}

void LayerSession::OnEntityRecord(const std::string& subject, const NatsMsg& m, const std::string& marker, TimeMs now) {
  auto toks = nats_proto::SplitTokens(subject);
  if (toks.size() != 4 || toks[0] != "ent" || toks[3] != "state") return;
  if (!marker.empty() || m.data.empty()) {
    store_.Remove(toks[2], marker == "MaxAge" ? RemoveReason::Expired : RemoveReason::Deleted, now);
    return;
  }
  Json j;
  if (!Json::Parse(m.data, j)) return;
  RenderEntity e;
  if (!RenderEntity::FromJson(j, e)) return;
  if (spec_.aoi.has_bbox && e.locatable &&
      (e.lat < spec_.aoi.lat0 || e.lat > spec_.aoi.lat1 || e.lon < spec_.aoi.lon0 || e.lon > spec_.aoi.lon1)) {
    store_.Remove(e.entity_id, RemoveReason::LeftAoi, now);
    return;
  }
  store_.Upsert(e, now);
  if (record_sink_) record_sink_(j);
}

void LayerSession::StartDirect(TimeMs now, bool resync) {
  state_ = resync ? LayerState::Resyncing : LayerState::Snapshotting;
  snap_started_ = WallClockMs();
  for (auto& c : consumers_) c->Stop();
  consumers_.clear();
  auto caught = std::make_shared<size_t>(0);
  size_t total = spec_.entity_types.size();
  for (const auto& type : spec_.entity_types) {
    OrderedConsumer::Config cfg;
    cfg.stream = spec_.ent_stream;
    cfg.filter_subject = "ent." + type + ".*.state";
    cfg.deliver_policy = "last_per_subject";
    std::weak_ptr<bool> alive = alive_;
    consumers_.emplace_back(new OrderedConsumer(
        js_, cfg,
        [this, alive](const OrderedConsumer::Delivery& d) {
          auto a = alive.lock();
          if (!a || !*a) return;
          OnEntityRecord(d.msg->subject, *d.msg, d.marker_reason, now_);
        },
        [this, alive, caught, total]() {
          auto a = alive.lock();
          if (!a || !*a) return;
          if (++*caught >= total && state_ != LayerState::Live) {
            state_ = LayerState::Live;
            stats_.snapshots++;
            stats_.snapshot_items = store_.size();
            stats_.last_snapshot_ms = WallClockMs() - snap_started_;
          }
        },
        [this, alive](const std::string& err) {
          auto a = alive.lock();
          if (a && *a) last_error_ = err;
        }));
    consumers_.back()->Start(now);
  }
}

void LayerSession::StartCore(TimeMs now) {
  state_ = LayerState::Snapshotting;
  snap_started_ = WallClockMs();
  for (int64_t sid : core_sids_) nc_.Unsubscribe(sid);
  core_sids_.clear();
  std::weak_ptr<bool> alive = alive_;
  // Subscribe first, then query: the revision rule drops anything older the query returns.
  for (const auto& type : spec_.entity_types) {
    core_sids_.push_back(nc_.Subscribe("ent." + type + ".*.state", [this, alive](const NatsMsg& m) {
      auto a = alive.lock();
      if (!a || !*a) return;
      OnEntityRecord(m.subject, m, std::string(), now_);
    }));
  }
  QueryPage(std::string(), now);
}

void LayerSession::QueryPage(const std::string& cursor, TimeMs now) {
  if (!nc_.connected()) {
    state_ = LayerState::Resyncing;
    retry_at_ = now + 500;
    return;
  }
  Json q = Json::MakeObject();
  Json types = Json::MakeArray();
  for (const auto& t : spec_.entity_types) types.push_back(Json(t));
  q.set("types", types);
  if (spec_.aoi.has_bbox) {
    Json b = Json::MakeArray();
    b.push_back(spec_.aoi.lon0);
    b.push_back(spec_.aoi.lat0);
    b.push_back(spec_.aoi.lon1);
    b.push_back(spec_.aoi.lat1);
    q.set("bbox", b);
  }
  q.set("limit", 200);
  if (!cursor.empty()) q.set("cursor", cursor);
  std::weak_ptr<bool> alive = alive_;
  uint64_t gen = gen_;
  nc_.Request(
      "svc.entity.query", q.Dump(),
      [this, alive, gen](const NatsMsg* m, const std::string& error) {
        auto a = alive.lock();
        if (!a || !*a || gen != gen_) return true;
        if (!m) {
          // No entity service: stay live on the core subscription and expiry alone.
          last_error_ = "svc.entity.query: " + error;
          state_ = LayerState::Live;
          return true;
        }
        if (const std::string* code = m->headers.Get("Nats-Service-Error-Code")) {
          last_error_ = "svc.entity.query error " + *code;
          state_ = LayerState::Live;
          return true;
        }
        Json j;
        if (!Json::Parse(m->data, j)) {
          last_error_ = "svc.entity.query: bad JSON";
          state_ = LayerState::Live;
          return true;
        }
        for (const auto& rec : j["entities"].items()) {
          RenderEntity e;
          if (RenderEntity::FromJson(rec, e)) {
            store_.Upsert(e, now_);
            if (record_sink_) record_sink_(rec);
          }
        }
        if (j["cursor"].is_string()) {
          QueryPage(j["cursor"].as_string(), now_);
        } else {
          state_ = LayerState::Live;
          stats_.snapshots++;
          stats_.snapshot_items = store_.size();
          stats_.last_snapshot_ms = WallClockMs() - snap_started_;
        }
        return true;
      },
      5000);
}

void LayerSession::Tick(TimeMs now) {
  now_ = now;
  if (state_ == LayerState::Off) return;
  for (auto& c : consumers_) c->Tick(now);
  store_.Expire(now);
  bool reconnected = nc_.stats().reconnects != seen_reconnects_;
  if (reconnected && nc_.connected()) {
    seen_reconnects_ = nc_.stats().reconnects;
    if (spec_.path == LayerPath::Watch) Resnapshot("reconnected", now);
    else if (spec_.path == LayerPath::Core) QueryPage(std::string(), now);
    // Direct: the ordered consumers reset themselves on reconnect.
    return;
  }
  if (spec_.path == LayerPath::Watch) {
    if ((state_ == LayerState::Error || state_ == LayerState::Resyncing) && now >= retry_at_ && !snap_req_ &&
        !watch_req_ && nc_.connected()) {
      gen_++;
      StartSnapshot(now);
    }
    if (state_ == LayerState::Live && !stats_.watch_id.empty() && now >= renew_at_) Renew(now);
  } else if (spec_.path == LayerPath::Core) {
    if ((state_ == LayerState::Resyncing || state_ == LayerState::Error) && now >= retry_at_ && nc_.connected())
      QueryPage(std::string(), now);
  }
}

// ---------------------------------------------------------------------------------------------

void LayerManager::Register(const LayerSpec& spec) { specs_[spec.id] = spec; }

bool LayerManager::Toggle(const std::string& id, bool on, TimeMs now) {
  auto sit = specs_.find(id);
  if (sit == specs_.end()) return false;
  auto it = sessions_.find(id);
  if (on) {
    if (it != sessions_.end()) return true;
    LayerSpec spec = sit->second;
    if (zoom_ >= 0 && spec.UsesCameraAoi()) {
      spec.aoi = aoi_;
      spec.zoom = zoom_;
    }
    std::unique_ptr<LayerSession> s(new LayerSession(nc_, js_, spec));
    if (sink_) s->SetRecordSink(sink_);
    s->Start(now);
    sessions_[id] = std::move(s);
    return true;
  }
  if (it != sessions_.end()) {
    it->second->Stop();
    sessions_.erase(it);
  }
  return true;
}

bool LayerManager::IsOn(const std::string& id) const { return sessions_.count(id) > 0; }

void LayerManager::SetAoi(const Aoi& aoi, int zoom, TimeMs now) {
  // Ignore camera jitter: only a zoom change or a move of more than 10 % of the box size
  // renews watches (each renewal costs the layer service an enter/exit diff).
  if (zoom == zoom_ && aoi.has_bbox && aoi_.has_bbox) {
    double w = (std::max)(1e-6, aoi_.lon1 - aoi_.lon0), h = (std::max)(1e-6, aoi_.lat1 - aoi_.lat0);
    bool similar = std::fabs(aoi.lon0 - aoi_.lon0) < 0.1 * w && std::fabs(aoi.lon1 - aoi_.lon1) < 0.1 * w &&
                   std::fabs(aoi.lat0 - aoi_.lat0) < 0.1 * h && std::fabs(aoi.lat1 - aoi_.lat1) < 0.1 * h;
    if (similar) return;
  }
  aoi_ = aoi;
  zoom_ = zoom;
  // Only layers that follow the camera get it (LayerSpec::UsesCameraAoi): direct/core
  // small and medium layers keep the whole layer whatever the view.
  for (auto& kv : sessions_)
    if (kv.second->spec().UsesCameraAoi()) kv.second->SetAoi(aoi, zoom, now);
}

void LayerManager::Tick(TimeMs now) {
  for (auto& kv : sessions_) kv.second->Tick(now);
}

LayerSession* LayerManager::Get(const std::string& id) {
  auto it = sessions_.find(id);
  return it == sessions_.end() ? nullptr : it->second.get();
}

std::vector<std::string> LayerManager::Ids() const {
  std::vector<std::string> out;
  for (const auto& kv : specs_) out.push_back(kv.first);
  return out;
}

size_t LayerManager::TotalEntities() const {
  size_t n = 0;
  for (const auto& kv : sessions_) n += kv.second->store().size();
  return n;
}

void LayerManager::OnManifest(const std::string& layer, const LayerManifest& m) { manifests_[layer] = m; }

const LayerManifest* LayerManager::Manifest(const std::string& layer) const {
  auto it = manifests_.find(layer);
  return it == manifests_.end() ? nullptr : &it->second;
}

bool LayerManager::ParseSpecs(const std::string& json, std::vector<LayerSpec>& out, std::string* err) {
  Json j;
  if (!Json::Parse(json, j, err)) return false;
  const Json& arr = j.is_object() ? j["layers"] : j;
  if (!arr.is_array()) {
    if (err) *err = "expected an array of layers";
    return false;
  }
  for (const auto& l : arr.items()) {
    LayerSpec s;
    s.id = l.str("id");
    if (s.id.empty()) continue;
    s.title = l.str("title", s.id);
    for (const auto& t : l["entity_types"].items()) s.entity_types.push_back(t.as_string());
    LayerPath p;
    if (ParseLayerPath(l.str("path", "watch"), p)) s.path = p;
    s.tier = l.str("tier", "small");
    const Json& bb = l["aoi"]["bbox"];
    if (bb.size() == 4) s.aoi = Aoi::Bbox(bb[0].as_number(), bb[1].as_number(), bb[2].as_number(), bb[3].as_number());
    s.zoom = static_cast<int>(l.i64("zoom", 6));
    s.max = static_cast<int>(l.i64("max", 5000));
    s.style = l.str("style");
    s.ent_stream = l.str("ent_stream", "ENT");
    out.push_back(s);
  }
  return true;
}

}  // namespace om
