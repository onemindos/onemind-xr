// Unit tests: entity parse and store (revision rule, live:false, expiry, tombstones,
// change journal, picking), inspector rows, render planning (bands, horizon, clusters),
// and the layer watch path against a scripted layer service on a fake transport
// (snapshot chunks, batches, gap -> resnapshot, dead watch, renew, unwatch on toggle off).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Entity.h"
#include "OneMindCore/Layers.h"
#include "OneMindCore/RenderPlan.h"
#include "TestHarness.h"

#include <fstream>
#include <map>
#include <sstream>

using namespace om;

namespace {
std::string ReadFixture(const char* name) {
  std::ifstream f(std::string(OM_FIXTURES_DIR) + "/" + name);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
RenderEntity Ent(const std::string& id, int64_t seq, double lat = 30, double lon = -97, TimeMs expiry = 0) {
  RenderEntity e;
  e.entity_id = id;
  e.entity_type = "drone";
  e.seq = seq;
  e.lat = lat;
  e.lon = lon;
  e.expiry_t = expiry;
  e.t = 1000;
  e.stale_s = 60;
  e.cot_type = "a-f-A-M-H-Q";
  e.simulated = true;
  return e;
}
}  // namespace

TEST(render_entity_from_full_record_and_render_item) {
  Json j;
  EXPECT_TRUE(Json::Parse(ReadFixture("entity_sim_x1.json"), j));
  RenderEntity e;
  EXPECT_TRUE(RenderEntity::FromJson(j, e));
  EXPECT_EQ(e.entity_id, std::string("sim-x1"));
  EXPECT_EQ(e.entity_type, std::string("drone"));
  EXPECT_TRUE(e.simulated);
  EXPECT_EQ(e.seq, 5);
  EXPECT_EQ(e.cot_type, std::string("a-f-A-M-H-Q"));
  EXPECT_TRUE(e.locatable);
  Json r;
  EXPECT_TRUE(Json::Parse(R"({"entity_id":"icao-a1","entity_type":"aircraft","lat":40.6,"lon":-73.8,"hae":3000,
    "course":90,"speed":200,"callsign":"N1","cot_type":"a-n-A-C-F","disposition":"neutral","t":1,"stale":60,
    "expiry_t":121000,"seq":3,"live":true,"origin":"hub","simulated":false})", r));
  EXPECT_TRUE(RenderEntity::FromJson(r, e));
  EXPECT_EQ(e.origin, std::string("hub"));
  EXPECT_EQ(e.expiry_t, 121000);
  Json team;
  EXPECT_TRUE(Json::Parse(R"({"entity_id":"team-cyan","entity_type":"team","lat":0,"lon":0,"traits":[]})", team));
  EXPECT_TRUE(RenderEntity::FromJson(team, e));
  EXPECT_FALSE(e.locatable);  // reserved 0,0 without the locatable trait: never drawn
}

TEST(entity_store_revision_rule_live_false_expiry_tombstones) {
  EntityStore s;
  TimeMs now = 1000;
  EXPECT_TRUE(s.Upsert(Ent("sim-x1", 5), now) == EntityStore::Result::Added);
  EXPECT_TRUE(s.Upsert(Ent("sim-x1", 5), now) == EntityStore::Result::IgnoredOlder);
  EXPECT_TRUE(s.Upsert(Ent("sim-x1", 4), now) == EntityStore::Result::IgnoredOlder);
  EXPECT_TRUE(s.Upsert(Ent("sim-x1", 6, 31), now) == EntityStore::Result::Updated);
  EXPECT_NEAR(s.lats()[static_cast<size_t>(s.IndexOf("sim-x1"))], 31.0, 1e-9);
  // live:false removes, and an older record cannot resurrect it.
  RenderEntity dead = Ent("sim-x1", 7);
  dead.live = false;
  EXPECT_TRUE(s.Upsert(dead, now) == EntityStore::Result::Removed);
  EXPECT_FALSE(s.Has("sim-x1"));
  EXPECT_TRUE(s.Upsert(Ent("sim-x1", 6), now) == EntityStore::Result::IgnoredOlder);
  EXPECT_TRUE(s.Upsert(Ent("sim-x1", 8), now) == EntityStore::Result::Added);
  // Expiry at expiry_t (the client's own expiry when no delete marker arrives).
  s.Upsert(Ent("icao-1", 1, 40, -73, 5000), now);
  s.Upsert(Ent("icao-2", 1, 40, -73, 9000), now);
  EXPECT_EQ(s.Expire(4999), 0u);
  EXPECT_EQ(s.Expire(5000), 1u);
  EXPECT_FALSE(s.Has("icao-1"));
  EXPECT_TRUE(s.Has("icao-2"));
  // Arriving already expired is ignored.
  EXPECT_TRUE(s.Upsert(Ent("icao-3", 1, 40, -73, 100), 200) == EntityStore::Result::IgnoredOlder);
  // LeftAoi leaves no tombstone: the same revision may enter again.
  EXPECT_TRUE(s.Remove("icao-2", RemoveReason::LeftAoi, now));
  EXPECT_TRUE(s.Upsert(Ent("icao-2", 1, 40, -73, 9000), now) == EntityStore::Result::Added);
  // Stale dimming.
  size_t i = static_cast<size_t>(s.IndexOf("sim-x1"));
  EXPECT_FALSE(s.IsStale(i, 1000 + 59000));
  EXPECT_TRUE(s.IsStale(i, 1000 + 61000));
}

TEST(entity_store_journal_swap_remove_and_pick) {
  EntityStore s;
  for (int k = 0; k < 100; ++k) s.Upsert(Ent("e" + std::to_string(k), 1, 30 + k * 0.01, -97), 0);
  auto ch = s.DrainChanges();
  EXPECT_EQ(ch.size(), 100u);
  s.Upsert(Ent("e5", 2, 35, -97), 0);
  s.Remove("e0", RemoveReason::Deleted, 0);
  s.Upsert(Ent("e7", 2), 0);
  s.Upsert(Ent("e7", 3), 0);  // coalesced
  ch = s.DrainChanges();
  EXPECT_EQ(ch.size(), 3u);
  // After swap-remove, every id still maps to its own row.
  bool consistent = true;
  for (size_t k = 0; k < s.size(); ++k)
    if (s.IndexOf(s.ids()[k]) != static_cast<int64_t>(k)) consistent = false;
  EXPECT_TRUE(consistent);
  EXPECT_EQ(s.Pick(35.0001, -97.0001, 100), std::string("e5"));
  EXPECT_EQ(s.Pick(10, 10, 1000), std::string());
  EXPECT_EQ(s.InBox(-97.5, 30.0, -96.5, 30.105).size(), 9u);  // e1..e10, minus removed e0 and moved e5
}

TEST(inspector_rows_for_tak_person_record) {
  Json j;
  EXPECT_TRUE(Json::Parse(ReadFixture("entity_tak_person.json"), j));
  EntityDetails d;
  d.Update(j);
  auto rows = BuildInspectorRows(d, j.i64("t") + 5000);
  std::map<std::string, std::string> kv;
  for (const auto& r : rows) kv[r.section + "/" + r.key] = r.value;
  EXPECT_EQ(kv["Identity/entity_type"], std::string("person"));
  EXPECT_TRUE(kv.count("Confidence/source_prior") == 1);
  EXPECT_TRUE(kv.count("Sources/source 1") == 1);
  EXPECT_TRUE(kv["Sources/source 1 stream/seq"].find("COT #") == 0);
  EXPECT_EQ(kv["Marking/tlp"], std::string("AMBER+STRICT"));
  std::string stream, subject;
  uint64_t seq = 0;
  EXPECT_TRUE(FindRawCotSource(j, stream, seq, subject));
  EXPECT_EQ(stream, std::string("COT"));
  EXPECT_TRUE(seq > 0);
  EXPECT_EQ(d.history.size(), 1u);
}

TEST(render_plan_bands_horizon_and_clusters) {
  EntityStore s;
  // Camera 1,000 m above (30, -97). One entity right below (actor), one 20 km away
  // (model), one 300 km away (icon), one on the far side of the Earth (culled), and a
  // dense knot of 50 entities 400 km away (cluster).
  s.Upsert(Ent("near", 1, 30.0, -97.0), 0);
  s.Upsert(Ent("mid", 1, 30.18, -97.0), 0);
  s.Upsert(Ent("far", 1, 32.7, -97.0), 0);
  s.Upsert(Ent("antipode", 1, -30.0, 83.0), 0);
  for (int k = 0; k < 50; ++k) s.Upsert(Ent("knot" + std::to_string(k), 1, 33.61 + k * 1e-4, -97.0), 0);
  RenderPlanConfig cfg;
  cfg.cluster_cell_deg = 0.05;
  cfg.cluster_min = 8;
  RenderPlan p = PlanRender(s, 30.0, -97.0, 1000, cfg);
  auto band = [&](const std::string& id) { return p.band[static_cast<size_t>(s.IndexOf(id))]; };
  EXPECT_TRUE(band("near") == Band::Actor);
  EXPECT_TRUE(band("mid") == Band::Model);
  EXPECT_TRUE(band("antipode") == Band::Culled);
  // From 1 km up the horizon is ~113 km away: "far" and the knot are below it.
  EXPECT_TRUE(band("far") == Band::Culled);
  RenderPlan high = PlanRender(s, 30.0, -97.0, 2000000, cfg);  // 2,000 km up
  EXPECT_TRUE(high.band[static_cast<size_t>(s.IndexOf("far"))] == Band::Icon);
  EXPECT_EQ(high.clusters.size(), 1u);
  if (!high.clusters.empty()) EXPECT_EQ(high.clusters[0].count, 50u);
  EXPECT_TRUE(high.band[static_cast<size_t>(s.IndexOf("antipode"))] == Band::Culled);
}

TEST(render_plan_10k_entities_budget) {
  // 10,000 sim entities spread over a state: planning must fit well inside a 16.7 ms frame.
  EntityStore s;
  for (int k = 0; k < 10000; ++k)
    s.Upsert(Ent("sim-" + std::to_string(k), 1, 29 + (k % 100) * 0.05, -99 + (k / 100) * 0.05), 0);
  TimeMs t0 = WallClockMs();
  RenderPlan p;
  for (int r = 0; r < 10; ++r) p = PlanRender(s, 31.5, -96.5, 400000);
  double per = static_cast<double>(WallClockMs() - t0) / 10.0;
  std::printf("    10k render plan: %.2f ms per frame (icons %zu models %zu actors %zu clustered %zu culled %zu)\n", per,
              p.icons, p.models, p.actors, p.clustered, p.culled);
  EXPECT_TRUE(per < 8.0);
  EXPECT_EQ(p.icons + p.models + p.actors + p.clustered + p.culled, 10000u);
}

namespace {
// Fake transport + a scripted layer service that speaks the layer contracts.
class LayerFake : public ITransport {
 public:
  Listener* l = nullptr;
  std::string out;
  void SetListener(Listener* x) override { l = x; }
  bool Open(const std::string&) override { return true; }
  void Send(const std::string& b) override { out += b; }
  void Close() override {}
  void Accept() {
    l->OnTransportOpen();
    Inject("INFO {\"server_id\":\"fake\",\"headers\":true}\r\n");
  }
  void Inject(const std::string& s) { l->OnTransportData(s.data(), s.size()); }
  std::string mux_sid;
  // Returns {reply, body} of every PUB to `subject` seen since the last call.
  std::vector<std::pair<std::string, std::string>> Take(const std::string& subject) {
    std::vector<std::pair<std::string, std::string>> res;
    size_t pos = 0;
    while (true) {
      size_t p = out.find("PUB " + subject + " ", pos);
      if (p == std::string::npos) break;
      size_t eol = out.find("\r\n", p);
      std::string line = out.substr(p, eol - p);
      std::vector<std::string> a;
      std::stringstream ss(line);
      std::string w;
      while (ss >> w) a.push_back(w);
      size_t n = static_cast<size_t>(std::stoul(a.back()));
      std::string reply = a.size() == 4 ? a[2] : "";
      res.emplace_back(reply, out.substr(eol + 2, n));
      pos = eol + 2 + n;
    }
    if (mux_sid.empty()) {
      size_t sp = out.find("SUB _INBOX.");
      if (sp != std::string::npos) {
        size_t e = out.find("\r\n", sp);
        std::string line = out.substr(sp, e - sp);
        mux_sid = line.substr(line.rfind(' ') + 1);
      }
    }
    out.clear();
    return res;
  }
  void Reply(const std::string& reply, const std::string& body) {
    Inject("MSG " + reply + " " + mux_sid + " " + std::to_string(body.size()) + "\r\n" + body + "\r\n");
  }
};
std::string Item(const std::string& id, int seq, double lat) {
  return "{\"entity_id\":\"" + id + "\",\"entity_type\":\"drone\",\"lat\":" + std::to_string(lat) +
         ",\"lon\":-97,\"seq\":" + std::to_string(seq) + ",\"live\":true,\"t\":1,\"stale\":60,\"simulated\":true}";
}
}  // namespace

TEST(layer_watch_path_snapshot_deltas_gap_and_toggle) {
  auto t = std::make_shared<LayerFake>();
  NatsOptions no;
  no.ping_interval_ms = 1000000000;  // the fake server never answers PINGs
  NatsClient nc(t, no);
  JetStream js(nc);
  LayerManager lm(nc, js);
  LayerSpec spec;
  spec.id = "fleet";
  spec.entity_types = {"drone"};
  spec.path = LayerPath::Watch;
  spec.lease_s = 60;
  lm.Register(spec);
  TimeMs now = 0;
  nc.Start(now);
  t->Accept();
  nc.Poll(now);
  size_t base_subs = nc.subscription_count();
  EXPECT_TRUE(lm.Toggle("fleet", true, now));
  nc.Poll(now);
  auto snaps = t->Take("svc.layer.snapshot");
  EXPECT_EQ(snaps.size(), 1u);
  Json req;
  EXPECT_TRUE(Json::Parse(snaps[0].second, req));
  EXPECT_EQ(req.str("layer"), std::string("fleet"));
  EXPECT_EQ(req.str("fields"), std::string("render"));
  // Two chunks.
  t->Reply(snaps[0].first, "{\"v\":1,\"layer\":\"fleet\",\"part\":0,\"last\":false,\"mode\":\"entities\",\"ent_seq\":100,\"items\":[" +
                               Item("sim-a", 1, 30) + "," + Item("sim-b", 1, 30.1) + "]}");
  t->Reply(snaps[0].first, "{\"v\":1,\"layer\":\"fleet\",\"part\":1,\"last\":true,\"mode\":\"entities\",\"ent_seq\":100,\"items\":[" +
                               Item("sim-c", 1, 30.2) + "]}");
  nc.Poll(now += 10);
  LayerSession* s = lm.Get("fleet");
  EXPECT_TRUE(s != nullptr);
  if (!s) return;
  EXPECT_EQ(s->store().size(), 3u);
  EXPECT_TRUE(s->state() == LayerState::Live);
  auto watches = t->Take("svc.layer.watch");
  EXPECT_EQ(watches.size(), 1u);
  Json w;
  EXPECT_TRUE(Json::Parse(watches[0].second, w));
  EXPECT_EQ(w.i64("from_seq"), 101);  // snapshot ent_seq + 1 (layer-snapshot.md §6)
  std::string winbox = watches[0].first;
  t->Reply(winbox, R"({"v":1,"watch_id":"w-1","lease_s":60,"ent_seq":100})");
  t->Reply(winbox, "{\"v\":1,\"watch_id\":\"w-1\",\"layer\":\"fleet\",\"n\":1,\"from_seq\":101,\"to_seq\":105,\"mode\":\"entities\",\"ops\":["
                   "{\"op\":\"upsert\",\"e\":" + Item("sim-a", 2, 31) + "},"
                   "{\"op\":\"enter\",\"e\":" + Item("sim-d", 1, 30.3) + "},"
                   "{\"op\":\"exit\",\"id\":\"sim-b\",\"reason\":\"left_aoi\"},"
                   "{\"op\":\"remove\",\"id\":\"sim-c\",\"reason\":\"expired\"}],\"meta\":null}");
  nc.Poll(now += 10);
  lm.Tick(now);
  EXPECT_EQ(s->store().size(), 2u);
  EXPECT_TRUE(s->store().Has("sim-d"));
  EXPECT_FALSE(s->store().Has("sim-b"));
  EXPECT_NEAR(s->store().lats()[static_cast<size_t>(s->store().IndexOf("sim-a"))], 31.0, 1e-9);
  EXPECT_EQ(s->stats().watch_id, std::string("w-1"));
  // Heartbeat (empty ops) keeps the watch alive.
  t->Reply(winbox, R"({"v":1,"watch_id":"w-1","layer":"fleet","n":2,"from_seq":105,"to_seq":105,"ops":[]})");
  nc.Poll(now += 10);
  EXPECT_EQ(s->stats().heartbeats, 1u);
  // Heartbeats every ~10 s; the renewal goes out at half the lease (30 s) with the same watch_id.
  for (int hb = 3; hb <= 5; ++hb) {
    now += 10500;
    t->Reply(winbox, "{\"v\":1,\"watch_id\":\"w-1\",\"layer\":\"fleet\",\"n\":" + std::to_string(hb) +
                         ",\"from_seq\":105,\"to_seq\":105,\"ops\":[]}");
    nc.Poll(now);
    lm.Tick(now);
  }
  auto renews = t->Take("svc.layer.watch");
  EXPECT_EQ(renews.size(), 1u);
  Json rw;
  if (!renews.empty()) EXPECT_TRUE(Json::Parse(renews[0].second, rw));
  EXPECT_EQ(rw.str("watch_id"), std::string("w-1"));
  if (!renews.empty()) t->Reply(renews[0].first, R"({"v":1,"watch_id":"w-1","lease_s":60,"ent_seq":105})");
  t->Reply(winbox, R"({"v":1,"watch_id":"w-1","layer":"fleet","n":6,"from_seq":105,"to_seq":105,"ops":[]})");
  nc.Poll(now += 10);
  // A skipped batch number means lost deltas: the client re-snapshots.
  t->Reply(winbox, R"({"v":1,"watch_id":"w-1","layer":"fleet","n":8,"from_seq":106,"to_seq":110,"ops":[]})");
  nc.Poll(now += 10);
  EXPECT_EQ(s->stats().resnapshots, 1u);
  auto snaps2 = t->Take("svc.layer.snapshot");
  EXPECT_EQ(snaps2.size(), 1u);
  if (snaps2.empty()) return;
  t->Reply(snaps2[0].first, "{\"v\":1,\"layer\":\"fleet\",\"part\":0,\"last\":true,\"mode\":\"entities\",\"ent_seq\":110,\"items\":[" +
                                Item("sim-a", 3, 32) + "]}");
  nc.Poll(now += 10);
  EXPECT_EQ(s->store().size(), 1u);  // sim-d was not in the new snapshot: swept
  auto w2 = t->Take("svc.layer.watch");
  EXPECT_EQ(w2.size(), 1u);
  // Dead watch: 30 s without any batch -> new snapshot.
  nc.Poll(now += 30001);
  lm.Tick(now);
  EXPECT_EQ(s->stats().resnapshots, 2u);
  t->Take("svc.layer.snapshot");
  // Toggle off: unwatch, no layer subscriptions left, store cleared.
  t->Take("svc.layer.unwatch");
  lm.Toggle("fleet", false, now);
  EXPECT_TRUE(lm.Get("fleet") == nullptr);
  EXPECT_EQ(nc.subscription_count(), base_subs + 1);  // only the shared request inbox remains
}

TEST(layer_specs_parse) {
  std::vector<LayerSpec> specs;
  std::string err;
  EXPECT_TRUE(LayerManager::ParseSpecs(
      R"({"layers":[{"id":"fleet","entity_types":["drone","robot"],"path":"direct"},
                    {"id":"air","entity_types":["aircraft"],"tier":"large","aoi":{"bbox":[-74.3,40.5,-73.6,41.0]},"zoom":9}]})",
      specs, &err));
  EXPECT_EQ(specs.size(), 2u);
  EXPECT_TRUE(specs[0].path == LayerPath::Direct);
  EXPECT_EQ(specs[0].entity_types.size(), 2u);
  EXPECT_TRUE(specs[1].aoi.has_bbox);
  EXPECT_EQ(specs[1].zoom, 9);
  Json a = specs[1].aoi.ToJson();
  EXPECT_EQ(a["bbox"].size(), 4u);
}
