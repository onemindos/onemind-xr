// Integration tests for the layer watch path against a contract fixture of the layer
// service (svc.layer.snapshot / svc.layer.watch / svc.layer.unwatch, layer-snapshot.md
// and layer-delta.md) running on a second connection to a real nats-server, and the
// 10,000-entity client-core load (B4/B5 client side; the 60 fps render check is the
// owner's, OWNER-STEPS.md).
//
// The fixture is test code that follows the M0 contracts; the real layer service is omos
// FEATURES A5 (not written yet). Where the contract is open, the fixture's choice is noted.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ItSupport.h"
#include "OneMindCore/Codec.h"
#include "OneMindCore/Layers.h"

#include <map>
#include <set>

using namespace om;
using namespace omit;

namespace {

class LayerServiceFixture {
 public:
  explicit LayerServiceFixture(NatsClient& nc) : nc_(nc) {
    nc_.Subscribe("svc.layer.snapshot", [this](const NatsMsg& m) { OnSnapshot(m); });
    nc_.Subscribe("svc.layer.watch", [this](const NatsMsg& m) { OnWatch(m); });
    nc_.Subscribe("svc.layer.unwatch", [this](const NatsMsg& m) {
      Json j;
      if (Json::Parse(m.data, j)) {
        watches_.erase(j.str("watch_id"));
        unwatched_++;
      }
    });
  }
  void Upsert(const std::string& id, double lat, double lon) {
    int64_t seq = ++seqs_[id];
    ent_seq_++;
    Json e = Json::MakeObject();
    e.set("entity_id", id);
    e.set("entity_type", "drone");
    e.set("lat", lat);
    e.set("lon", lon);
    e.set("hae", 120.0);
    e.set("course", 90);
    e.set("speed", 10);
    e.set("callsign", id);
    e.set("cot_type", "a-f-A-M-H-Q");
    e.set("disposition", "friendly");
    e.set("t", WallClockMs());
    e.set("stale", 60);
    e.set("expiry_t", WallClockMs() + 120000);
    e.set("seq", seq);
    e.set("live", true);
    e.set("origin", "site-mac");
    e.set("simulated", true);
    ents_[id] = e;
    for (auto& w : watches_) Queue(w.second, id, &e);
  }
  void Remove(const std::string& id, const std::string& reason) {
    ents_.erase(id);
    ent_seq_++;
    for (auto& w : watches_) {
      Json op = Json::MakeObject();
      op.set("op", "remove");
      op.set("id", id);
      op.set("reason", reason);
      w.second.ops[id] = op;
    }
  }
  /// Sends one batch per watch (<= 1000 ops each; more batches if needed), or a heartbeat.
  void Flush(bool heartbeat_if_empty = true) {
    for (auto& kv : watches_) {
      Watch& w = kv.second;
      if (w.ops.empty() && !heartbeat_if_empty) continue;
      std::vector<Json> all;
      for (auto& o : w.ops) all.push_back(o.second);
      w.ops.clear();
      size_t i = 0;
      do {
        Json ops = Json::MakeArray();
        for (size_t k = 0; k < 1000 && i < all.size(); ++k, ++i) ops.push_back(all[i]);
        Json b = Json::MakeObject();
        b.set("v", 1);
        b.set("watch_id", kv.first);
        b.set("layer", "fleet");
        b.set("n", ++w.n);
        b.set("from_seq", w.sent_seq + 1);
        b.set("to_seq", ent_seq_);
        b.set("as_of", WallClockMs());
        b.set("mode", "entities");
        b.set("ops", ops);
        b.set("meta", Json());
        nc_.Publish(w.inbox, b.Dump());
        batches_++;
      } while (i < all.size());
      w.sent_seq = ent_seq_;
    }
  }
  void SkipBatchNumber() {
    for (auto& w : watches_) w.second.n++;
  }
  size_t watches() const { return watches_.size(); }
  int unwatched() const { return unwatched_; }
  int snapshots() const { return snapshots_; }
  int renewals() const { return renewals_; }
  uint64_t batches() const { return batches_; }

 private:
  struct Watch {
    std::string inbox;
    int64_t n = 0;
    int64_t sent_seq = 0;
    Aoi aoi;
    std::map<std::string, Json> ops;  // coalesced: newest op per entity
  };
  static bool Inside(const Aoi& a, const Json& e) {
    if (!a.has_bbox) return true;
    double lat = e.num("lat"), lon = e.num("lon");
    return lat >= a.lat0 && lat <= a.lat1 && lon >= a.lon0 && lon <= a.lon1;
  }
  static Aoi AoiOf(const Json& req) {
    const Json& bb = req["aoi"]["bbox"];
    if (bb.size() == 4) return Aoi::Bbox(bb[0].as_number(), bb[1].as_number(), bb[2].as_number(), bb[3].as_number());
    return Aoi();
  }
  void Queue(Watch& w, const std::string& id, const Json* e) {
    Json op = Json::MakeObject();
    if (e && Inside(w.aoi, *e)) {
      op.set("op", "upsert");
      op.set("e", *e);
    } else {
      op.set("op", "exit");
      op.set("id", id);
      op.set("reason", "left_aoi");
    }
    w.ops[id] = op;
  }
  void OnSnapshot(const NatsMsg& m) {
    snapshots_++;
    Json req;
    Json::Parse(m.data, req);
    Aoi aoi = AoiOf(req);
    std::vector<const Json*> items;
    for (auto& kv : ents_)
      if (Inside(aoi, kv.second)) items.push_back(&kv.second);
    size_t parts = items.empty() ? 1 : (items.size() + 1999) / 2000;  // <= 2,000 entities per chunk
    for (size_t p = 0; p < parts; ++p) {
      Json arr = Json::MakeArray();
      for (size_t k = p * 2000; k < items.size() && k < (p + 1) * 2000; ++k) arr.push_back(*items[k]);
      Json c = Json::MakeObject();
      c.set("v", 1);
      c.set("layer", req.str("layer"));
      c.set("part", static_cast<int64_t>(p));
      c.set("last", p + 1 == parts);
      c.set("mode", "entities");
      c.set("ent_seq", ent_seq_);
      c.set("as_of", WallClockMs());
      c.set("truncated", false);
      c.set("items", arr);
      nc_.Publish(m.reply, c.Dump());
    }
  }
  void OnWatch(const NatsMsg& m) {
    Json req;
    Json::Parse(m.data, req);
    std::string wid = req.str("watch_id");
    Json ack = Json::MakeObject();
    ack.set("v", 1);
    ack.set("lease_s", req.i64("lease_s", 60));
    ack.set("ent_seq", ent_seq_);
    if (!wid.empty() && watches_.count(wid)) {
      // Renewal (fixture choice: batches keep going to the first request's inbox).
      renewals_++;
      Watch& w = watches_[wid];
      Aoi old = w.aoi;
      w.aoi = AoiOf(req);
      // enter/exit for the difference.
      for (auto& kv : ents_) {
        bool was = Inside(old, kv.second), now_in = Inside(w.aoi, kv.second);
        if (was == now_in) continue;
        Json op = Json::MakeObject();
        if (now_in) {
          op.set("op", "enter");
          op.set("e", kv.second);
        } else {
          op.set("op", "exit");
          op.set("id", kv.first);
          op.set("reason", "left_aoi");
        }
        w.ops[kv.first] = op;
      }
      ack.set("watch_id", wid);
      nc_.Publish(m.reply, ack.Dump());
      return;
    }
    wid = "w-" + NewToken().substr(0, 10);
    Watch w;
    w.inbox = m.reply;
    w.aoi = AoiOf(req);
    w.sent_seq = req.i64("from_seq") - 1;
    watches_[wid] = w;
    ack.set("watch_id", wid);
    nc_.Publish(m.reply, ack.Dump());
  }

  NatsClient& nc_;
  std::map<std::string, Json> ents_;
  std::map<std::string, int64_t> seqs_;
  std::map<std::string, Watch> watches_;
  int64_t ent_seq_ = 0;
  int snapshots_ = 0, unwatched_ = 0, renewals_ = 0;
  uint64_t batches_ = 0;
};

struct Rig {
  NatsServer srv{"layers"};
  std::unique_ptr<Client> svc_conn;
  std::unique_ptr<LayerServiceFixture> svc;
  std::unique_ptr<ClientCore> core;
  bool ok = false;
  explicit Rig(int lease_s = 60) {
    if (!srv.Start()) return;
    svc_conn.reset(new Client(srv.ws_url(), "layer-service-fixture"));
    svc.reset(new LayerServiceFixture(*svc_conn->nc));
    ClientConfig cc;
    cc.nats.urls = {srv.ws_url()};
    cc.nats.name = "onemind-client-under-test";
    LayerSpec spec;
    spec.id = "fleet";
    spec.entity_types = {"drone"};
    spec.path = LayerPath::Watch;
    spec.lease_s = lease_s;
    cc.layers = {spec};
    core.reset(new ClientCore(std::make_shared<omhost::PosixWsTransport>(), cc));
    core->Start(WallClockMs());
    ok = PollUntil({svc_conn->nc.get(), &core->nats()}, [&] { return svc_conn->nc->connected() && core->nats().connected(); },
                   5000);
    PollUntil({svc_conn->nc.get(), &core->nats()}, [] { return false; }, 200);
  }
  bool Run(std::function<bool()> done, int ms) {
    return PollUntil({svc_conn->nc.get()}, done, ms, [&](TimeMs now) { core->Tick(now); });
  }
};

}  // namespace

TEST(it_watch_path_snapshot_deltas_aoi_and_unwatch) {
  Rig r;
  EXPECT_TRUE(r.ok);
  if (!r.ok) return;
  for (int i = 0; i < 300; ++i) r.svc->Upsert("sim-" + std::to_string(i), 30.0 + i * 0.01, -97.7);
  r.core->layers().Toggle("fleet", true, WallClockMs());
  LayerSession* s = r.core->layers().Get("fleet");
  EXPECT_TRUE(r.Run([&] { return s->state() == LayerState::Live && r.svc->watches() == 1; }, 5000));
  EXPECT_EQ(s->store().size(), 300u);
  // Deltas: move one, add one, remove one.
  r.svc->Upsert("sim-1", 31.5, -97.7);
  r.svc->Upsert("sim-new", 30.5, -97.6);
  r.svc->Remove("sim-2", "expired");
  r.svc->Flush();
  EXPECT_TRUE(r.Run([&] { return s->store().Has("sim-new") && !s->store().Has("sim-2"); }, 3000));
  EXPECT_NEAR(s->store().lats()[static_cast<size_t>(s->store().IndexOf("sim-1"))], 31.5, 1e-9);
  // AOI change: same watch_id, the service answers with enter/exit.
  r.core->layers().SetAoi(Aoi::Bbox(-98, 30.0, -97, 30.995), 10, WallClockMs());
  EXPECT_TRUE(r.Run([&] { return r.svc->renewals() == 1; }, 3000));
  r.svc->Flush();
  EXPECT_TRUE(r.Run([&] { return !s->store().Has("sim-150"); }, 3000));
  bool none_outside = true;
  for (size_t i = 0; i < s->store().size(); ++i)
    if (s->store().lats()[i] > 30.995) none_outside = false;
  EXPECT_TRUE(none_outside);  // layer-delta.md §7 "AOI move"
  // A skipped batch number -> re-snapshot (layer-delta.md §2.2 "Gaps").
  int snaps_before = r.svc->snapshots();
  r.svc->SkipBatchNumber();
  r.svc->Flush();
  EXPECT_TRUE(r.Run([&] {
    return r.svc->snapshots() == snaps_before + 1 && s->state() == LayerState::Live && !s->stats().watch_id.empty();
  }, 3000));
  EXPECT_EQ(s->stats().resnapshots, 1u);
  EXPECT_EQ(r.svc->unwatched(), 1);  // the broken watch was released, not left to its lease
  EXPECT_EQ(r.svc->watches(), 1u);
  // Toggle off: unwatch reaches the service; no layer subscription is left.
  r.core->layers().Toggle("fleet", false, WallClockMs());
  EXPECT_TRUE(r.Run([&] { return r.svc->unwatched() >= 2; }, 3000));
  EXPECT_EQ(r.svc->watches(), 0u);
}

TEST(it_dead_watch_resnapshots_after_30s_silence) {
  Rig r;
  EXPECT_TRUE(r.ok);
  if (!r.ok) return;
  r.svc->Upsert("sim-x1", 30.2672, -97.7431);
  r.core->layers().Toggle("fleet", true, WallClockMs());
  LayerSession* s = r.core->layers().Get("fleet");
  EXPECT_TRUE(r.Run([&] { return s->state() == LayerState::Live; }, 5000));
  int before = r.svc->snapshots();
  TimeMs t0 = WallClockMs();
  // The fixture sends nothing (no heartbeat): the client must give up on the watch at 30 s.
  EXPECT_TRUE(r.Run([&] { return r.svc->snapshots() > before; }, 35000));
  std::printf("    silent watch -> new snapshot after %lld ms (contract: 30 s)\n",
              static_cast<long long>(WallClockMs() - t0));
  EXPECT_TRUE(WallClockMs() - t0 >= 29500);
}

TEST(it_10k_entities_snapshot_and_update_throughput) {
  Rig r;
  EXPECT_TRUE(r.ok);
  if (!r.ok) return;
  const int N = 10000;
  for (int i = 0; i < N; ++i) r.svc->Upsert("sim-" + std::to_string(i), 29.0 + (i % 100) * 0.03, -99.0 + (i / 100) * 0.03);
  TimeMs t0 = WallClockMs();
  r.core->layers().Toggle("fleet", true, t0);
  LayerSession* s = r.core->layers().Get("fleet");
  EXPECT_TRUE(r.Run([&] { return s->state() == LayerState::Live; }, 10000));
  TimeMs snap_ms = WallClockMs() - t0;
  EXPECT_EQ(s->store().size(), static_cast<size_t>(N));
  std::printf("    10,000-entity snapshot over svc.layer.snapshot (5 chunks): %lld ms wall, %lld ms in client "
              "(A5 contract target < 2 s)\n",
              static_cast<long long>(snap_ms), static_cast<long long>(s->stats().last_snapshot_ms));
  EXPECT_TRUE(snap_ms < 2000);
  EXPECT_TRUE(r.Run([&] { return r.svc->watches() == 1; }, 3000));
  s->store().DrainChanges();
  // Three rounds of moving every entity: 30,000 upserts in 1,000-op batches.
  TimeMs t1 = WallClockMs();
  for (int round = 1; round <= 3; ++round) {
    for (int i = 0; i < N; ++i)
      r.svc->Upsert("sim-" + std::to_string(i), 29.0 + (i % 100) * 0.03 + round * 0.001, -99.0 + (i / 100) * 0.03);
    r.svc->Flush(false);
  }
  EXPECT_TRUE(r.Run([&] { return s->store().counters().updated >= static_cast<uint64_t>(3 * N); }, 20000));
  double secs = static_cast<double>(WallClockMs() - t1) / 1000.0;
  std::printf("    30,000 delta upserts applied in %.2f s (%.0f updates/s, %llu batches) -> %zu journal changes\n", secs,
              3.0 * N / secs, static_cast<unsigned long long>(r.svc->batches()), s->store().DrainChanges().size());
  EXPECT_EQ(s->store().size(), static_cast<size_t>(N));
  bool all_moved = true;
  for (size_t i = 0; i < s->store().size(); ++i)
    if (s->store().Get(i).seq != 4) all_moved = false;
  EXPECT_TRUE(all_moved);
}
