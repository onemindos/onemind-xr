// Integration tests against a real nats-server over its WebSocket listener (spike S5
// fallback, feature B3): pub/sub, request/reply, KV get (Direct Get and fallback), KV watch
// with delete, reconnect after the server restarts, ordered consumer on an ENT stream with
// per-message TTL delete markers, and server-side proof that a layer off leaves no
// subscription.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ItSupport.h"
#include "OneMindCore/Layers.h"

using namespace om;
using namespace omit;

TEST(it_ws_connect_pubsub_request_reply) {
  NatsServer srv("pubsub");
  EXPECT_TRUE(srv.Start());
  Client a(srv.ws_url(), "it-a"), b(srv.ws_url(), "it-b");
  EXPECT_TRUE(PollUntil({a.nc.get(), b.nc.get()}, [&] { return a.nc->connected() && b.nc->connected(); }, 5000));
  EXPECT_EQ(a.nc->server_info().str("version"), std::string("2.15.2"));
  std::vector<std::string> got;
  a.nc->Subscribe("tel.fleet.drone.sim-x1.position", [&](const NatsMsg& m) { got.push_back(m.data); });
  // Responder on b; header round trip too.
  b.nc->Subscribe("svc.echo.ping", [&](const NatsMsg& m) {
    NatsHeaders h;
    h.Add("OneMind-Echo", m.headers.GetOr("OneMind-Test"));
    b.nc->Publish(m.reply, "pong:" + m.data, std::string(), &h);
  });
  PollUntil({a.nc.get(), b.nc.get()}, [] { return false; }, 200);  // let SUBs land
  b.nc->Publish("tel.fleet.drone.sim-x1.position", R"({"lat":30.2672,"lon":-97.7431})");
  std::string reply, hdr, err;
  NatsHeaders req_h;
  req_h.Add("OneMind-Test", "42");
  a.nc->Request("svc.echo.ping", "hi", [&](const NatsMsg* m, const std::string& e) {
    if (m) {
      reply = m->data;
      hdr = m->headers.GetOr("OneMind-Echo");
    }
    err = e;
    return true;
  }, 3000, false, &req_h);
  EXPECT_TRUE(PollUntil({a.nc.get(), b.nc.get()}, [&] { return got.size() == 1 && !reply.empty(); }, 3000));
  EXPECT_EQ(reply, std::string("pong:hi"));
  EXPECT_EQ(hdr, std::string("42"));
  // No responders comes back fast as an error, not a timeout.
  std::string nr;
  TimeMs t0 = WallClockMs();
  a.nc->Request("svc.nobody.home", "", [&](const NatsMsg*, const std::string& e) {
    nr = e;
    return true;
  }, 3000);
  EXPECT_TRUE(PollUntil({a.nc.get()}, [&] { return !nr.empty(); }, 3000));
  EXPECT_EQ(nr, std::string("no_responders"));
  EXPECT_TRUE(WallClockMs() - t0 < 1000);
  // A 512 KiB payload crosses the WebSocket in one piece.
  std::string big(512 * 1024, 'z');
  size_t big_got = 0;
  a.nc->Subscribe("evt.test.big.blob", [&](const NatsMsg& m) { big_got = m.data.size(); });
  PollUntil({a.nc.get()}, [] { return false; }, 100);
  b.nc->Publish("evt.test.big.blob", big);
  EXPECT_TRUE(PollUntil({a.nc.get(), b.nc.get()}, [&] { return big_got == big.size(); }, 3000));
}

TEST(it_kv_get_direct_fallback_and_watch) {
  NatsServer srv("kv");
  EXPECT_TRUE(srv.Start());
  Client c(srv.ws_url());
  EXPECT_TRUE(PollUntil({c.nc.get()}, [&] { return c.nc->connected(); }, 5000));
  Json r1 = CreateKv(*c.nc, "probe", true);
  Json r2 = CreateKv(*c.nc, "nodirect", false);
  EXPECT_TRUE(r1["config"].is_object());
  EXPECT_TRUE(r2["config"].is_object());
  c.nc->Publish("$KV.probe.fleet.meta", R"({"v":1,"layer":"fleet","count":3})");
  c.nc->Publish("$KV.nodirect.k1", "v1");
  PollUntil({c.nc.get()}, [] { return false; }, 200);
  StreamMsg got, missing, fb;
  std::string e1 = "x", e2 = "x", e3 = "x";
  c.js->KvGet("probe", "fleet.meta", [&](const StreamMsg& m, const std::string& e) { got = m; e1 = e; });
  c.js->KvGet("probe", "nope", [&](const StreamMsg& m, const std::string& e) { missing = m; e2 = e; });
  c.js->KvGet("nodirect", "k1", [&](const StreamMsg& m, const std::string& e) { fb = m; e3 = e; });
  EXPECT_TRUE(PollUntil({c.nc.get()}, [&] { return e1 != "x" && e2 != "x" && e3 != "x"; }, 5000));
  EXPECT_TRUE(got.found);
  EXPECT_EQ(e1, std::string());
  EXPECT_TRUE(got.data.find("\"layer\":\"fleet\"") != std::string::npos);
  EXPECT_TRUE(got.seq > 0);
  EXPECT_FALSE(missing.found);
  EXPECT_EQ(e2, std::string());
  EXPECT_TRUE(fb.found);  // STREAM.MSG.GET fallback for a bucket without allow_direct
  EXPECT_EQ(fb.data, std::string("v1"));
  EXPECT_EQ(e3, std::string());

  // Watch: initial value, live update, delete.
  ClientConfig cc;
  cc.nats.urls = {srv.ws_url()};
  ClientCore core(std::make_shared<omhost::PosixWsTransport>(), cc);
  core.Start(WallClockMs());
  std::vector<std::string> events;
  EXPECT_TRUE(PollUntil({&core.nats()}, [&] { return core.nats().connected(); }, 5000));
  core.WatchKv("probe", ">", [&](const std::string& k, const std::string& v, bool del) {
    events.push_back((del ? "DEL " : "PUT ") + k + "=" + v);
  }, WallClockMs());
  auto tick = [&](TimeMs now) { core.Tick(now); };
  EXPECT_TRUE(PollUntil({c.nc.get()}, [&] { return events.size() == 1; }, 5000, tick));
  TimeMs t_put = WallClockMs();
  c.nc->Publish("$KV.probe.ent.drone.sim-x1.state", R"({"lat":40.7128,"lon":-74.0060})");
  EXPECT_TRUE(PollUntil({c.nc.get()}, [&] { return events.size() == 2; }, 5000, tick));
  TimeMs latency = WallClockMs() - t_put;
  std::printf("    kv put -> watch delivery: %lld ms (B3 target: marker within 1 s)\n", static_cast<long long>(latency));
  EXPECT_TRUE(latency < 1000);
  NatsHeaders del;
  del.Add("KV-Operation", "DEL");
  c.nc->Publish("$KV.probe.fleet.meta", "", std::string(), &del);
  EXPECT_TRUE(PollUntil({c.nc.get()}, [&] { return events.size() == 3; }, 5000, tick));
  if (events.size() == 3) {
    EXPECT_EQ(events[0].substr(0, 15), std::string("PUT fleet.meta="));
    EXPECT_EQ(events[1].substr(0, 29), std::string("PUT ent.drone.sim-x1.state={\""));
    EXPECT_EQ(events[2], std::string("DEL fleet.meta="));
  }
  core.Stop();
}

TEST(it_reconnect_after_server_restart) {
  NatsServer srv("reconnect");
  EXPECT_TRUE(srv.Start());
  ClientConfig cc;
  cc.nats.urls = {srv.ws_url()};
  cc.nats.reconnect_wait_min_ms = 100;
  cc.nats.reconnect_wait_max_ms = 1000;
  ClientCore core(std::make_shared<omhost::PosixWsTransport>(), cc);
  core.Start(WallClockMs());
  auto tick = [&](TimeMs now) { core.Tick(now); };
  EXPECT_TRUE(PollUntil({}, [&] { return core.nats().connected(); }, 5000, tick));
  {
    Client setup(srv.ws_url());
    PollUntil({setup.nc.get()}, [&] { return setup.nc->connected(); }, 5000);
    CreateKv(*setup.nc, "world-state", true, "file");  // survives the restart
  }
  std::vector<std::string> events;
  core.WatchKv("world-state", "probe.>", [&](const std::string& k, const std::string& v, bool) {
    events.push_back(k + "=" + v);
  }, WallClockMs());
  int msgs = 0;
  core.nats().Subscribe("tel.fleet.drone.*.position", [&](const NatsMsg&) { msgs++; });
  PollUntil({}, [] { return false; }, 300, tick);
  // The leaf restarts (killed, then started again on the same ports and store).
  srv.Stop();
  EXPECT_TRUE(PollUntil({}, [&] { return !core.nats().connected(); }, 5000, tick));
  TimeMs down_at = WallClockMs();
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  EXPECT_TRUE(srv.Start());
  EXPECT_TRUE(PollUntil({}, [&] { return core.nats().connected(); }, 10000, tick));
  std::printf("    reconnected %lld ms after the server went down (server restarted after 1.5 s)\n",
              static_cast<long long>(WallClockMs() - down_at));
  EXPECT_EQ(core.nats().stats().reconnects, 1u);
  Client pub(srv.ws_url());
  PollUntil({pub.nc.get()}, [&] { return pub.nc->connected(); }, 5000);
  PollUntil({pub.nc.get()}, [] { return false; }, 300, tick);  // resubscribe settles
  pub.nc->Publish("tel.fleet.drone.sim-x1.position", "{}");
  TimeMs t_put = WallClockMs();
  pub.nc->Publish("$KV.world-state.probe.sim-x1", R"({"lat":40.72})");
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return msgs == 1 && !events.empty(); }, 5000, tick));
  std::printf("    after reconnect: kv put -> client in %lld ms\n", static_cast<long long>(WallClockMs() - t_put));
  EXPECT_EQ(msgs, 1);
  EXPECT_FALSE(events.empty());
  core.Stop();
}

TEST(it_direct_path_ent_snapshot_live_ttl_markers_and_live_false) {
  NatsServer srv("ent");
  EXPECT_TRUE(srv.Start());
  Client pub(srv.ws_url(), "it-entity-service-stand-in");
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return pub.nc->connected(); }, 5000));
  Json created = CreateEntStream(*pub.nc);
  EXPECT_TRUE(created["config"].is_object());
  if (!created["config"].is_object()) std::printf("    stream create: %s\n", created.Dump().c_str());
  TimeMs now0 = WallClockMs();
  auto rec = [&](const std::string& id, int seq, double lat, bool live = true) {
    return std::string("{\"entity_id\":\"") + id + "\",\"entity_type\":\"drone\",\"uid\":\"" + id +
           "\",\"lat\":" + std::to_string(lat) + ",\"lon\":-97.74,\"seq\":" + std::to_string(seq) +
           ",\"live\":" + (live ? "true" : "false") + ",\"t\":" + std::to_string(now0) +
           ",\"stale\":60,\"expiry_t\":" + std::to_string(now0 + 600000) +
           ",\"indicators\":{\"simulated\":true},\"traits\":[\"locatable\"]}";
  };
  for (int i = 0; i < 1000; ++i) {
    NatsHeaders h;
    h.Add("Nats-Msg-Id", "sim-" + std::to_string(i) + ":1");
    pub.nc->Publish("ent.drone.sim-" + std::to_string(i) + ".state", rec("sim-" + std::to_string(i), 1, 30.0 + i * 1e-4),
                    std::string(), &h);
  }
  PollUntil({pub.nc.get()}, [] { return false; }, 300);

  ClientConfig cc;
  cc.nats.urls = {srv.ws_url()};
  LayerSpec spec;
  spec.id = "fleet";
  spec.entity_types = {"drone"};
  spec.path = LayerPath::Direct;
  cc.layers = {spec};
  ClientCore core(std::make_shared<omhost::PosixWsTransport>(), cc);
  core.Start(WallClockMs());
  auto tick = [&](TimeMs now) { core.Tick(now); };
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return core.nats().connected(); }, 5000, tick));
  core.layers().Toggle("fleet", true, WallClockMs());
  LayerSession* s = core.layers().Get("fleet");
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return s->state() == LayerState::Live; }, 10000, tick));
  EXPECT_EQ(s->store().size(), 1000u);
  std::printf("    direct path: 1000-entity snapshot in %lld ms\n", static_cast<long long>(s->stats().last_snapshot_ms));
  // Live update.
  pub.nc->Publish("ent.drone.sim-5.state", rec("sim-5", 2, 31.5));
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] {
    int64_t i = s->store().IndexOf("sim-5");
    return i >= 0 && s->store().lats()[static_cast<size_t>(i)] > 31.0;
  }, 5000, tick));
  // live:false removes (entity.md §5 Delete).
  pub.nc->Publish("ent.drone.sim-6.state", rec("sim-6", 2, 30.0, false));
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return !s->store().Has("sim-6"); }, 5000, tick));
  // Per-message TTL: the server removes the message at its TTL and writes a subject delete
  // marker (Nats-Marker-Reason: MaxAge) that the ordered consumer sees (layer-delta.md §3).
  NatsHeaders ttl;
  ttl.Add("Nats-TTL", "1s");
  pub.nc->Publish("ent.drone.sim-7.state", rec("sim-7", 2, 30.0), std::string(), &ttl);
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return s->store().SeqOf("sim-7") == 2; }, 5000, tick));
  TimeMs t_ttl = WallClockMs();
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return !s->store().Has("sim-7"); }, 10000, tick));
  std::printf("    Nats-TTL 1s -> removed by delete marker after %lld ms (expired counter %llu)\n",
              static_cast<long long>(WallClockMs() - t_ttl),
              static_cast<unsigned long long>(s->store().counters().expired));
  EXPECT_EQ(s->store().counters().expired, 1u);
  EXPECT_EQ(s->store().size(), 998u);
  // The inspector already has the full record from the direct path.
  EXPECT_TRUE(core.Details("sim-5") != nullptr);
  core.Stop();
}

namespace {
int ServerSubsFor(int mon_port, const std::string& conn_name) {
  Json j;
  if (!Json::Parse(HttpGet(mon_port, "/connz?subs=1"), j)) return -1;
  for (const auto& c : j["connections"].items()) {
    if (c.str("name") == conn_name) return static_cast<int>(c.i64("subscriptions"));
  }
  return -1;
}
}  // namespace

TEST(it_layer_off_means_no_subscription_on_the_server) {
  NatsServer srv("toggle");
  EXPECT_TRUE(srv.Start());
  {
    Client setup(srv.ws_url());
    PollUntil({setup.nc.get()}, [&] { return setup.nc->connected(); }, 5000);
    CreateEntStream(*setup.nc);
  }
  ClientConfig cc;
  cc.nats.urls = {srv.ws_url()};
  cc.nats.name = "onemind-toggle-probe";
  LayerSpec direct, core_path;
  direct.id = "fleet";
  direct.entity_types = {"drone", "robot"};
  direct.path = LayerPath::Direct;
  core_path.id = "tak";
  core_path.entity_types = {"person", "place", "zone"};
  core_path.path = LayerPath::Core;
  cc.layers = {direct, core_path};
  ClientCore core(std::make_shared<omhost::PosixWsTransport>(), cc);
  core.Start(WallClockMs());
  auto tick = [&](TimeMs now) { core.Tick(now); };
  EXPECT_TRUE(PollUntil({}, [&] { return core.nats().connected(); }, 5000, tick));
  PollUntil({}, [] { return false; }, 300, tick);
  int base = ServerSubsFor(srv.mon_port(), "onemind-toggle-probe");
  core.layers().Toggle("fleet", true, WallClockMs());
  core.layers().Toggle("tak", true, WallClockMs());
  PollUntil({}, [] { return false; }, 800, tick);
  int on = ServerSubsFor(srv.mon_port(), "onemind-toggle-probe");
  Json consumers;
  Json::Parse(HttpGet(srv.mon_port(), "/jsz?consumers=true"), consumers);
  core.layers().Toggle("fleet", false, WallClockMs());
  core.layers().Toggle("tak", false, WallClockMs());
  PollUntil({}, [] { return false; }, 800, tick);
  int off = ServerSubsFor(srv.mon_port(), "onemind-toggle-probe");
  Json jsz;
  Json::Parse(HttpGet(srv.mon_port(), "/jsz"), jsz);
  std::printf("    server-side subscriptions: before=%d on=%d off=%d; ENT consumers after off=%lld\n", base, on, off,
              static_cast<long long>(jsz.i64("consumers", -1)));
  // fleet: 2 ordered-consumer inboxes; tak: 3 core subjects (+1 request inbox at most once).
  EXPECT_TRUE(on >= base + 5);
  EXPECT_TRUE(off <= base + 1);  // the shared request inbox may remain; no layer subscription
  EXPECT_EQ(jsz.i64("consumers", -1), 0);  // ordered consumers deleted on toggle off
  core.Stop();
}

// Review finding (B4/C1): the camera AOI must not empty direct small/medium layers, and a
// layer that does filter by the camera on the client must reload entities on a pan.
TEST(it_direct_layers_camera_aoi_small_keeps_all_large_reloads_on_pan) {
  NatsServer srv("aoi");
  EXPECT_TRUE(srv.Start());
  Client pub(srv.ws_url(), "it-entity-service-stand-in");
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return pub.nc->connected(); }, 5000));
  CreateEntStream(*pub.nc);
  TimeMs now0 = WallClockMs();
  auto rec = [&](const std::string& type, const std::string& id, double lat, double lon, int seq = 1) {
    return std::string("{\"entity_id\":\"") + id + "\",\"entity_type\":\"" + type + "\",\"uid\":\"" + id +
           "\",\"lat\":" + std::to_string(lat) + ",\"lon\":" + std::to_string(lon) +
           ",\"seq\":" + std::to_string(seq) + ",\"live\":true,\"t\":" + std::to_string(now0) + ",\"stale\":60,\"expiry_t\":" +
           std::to_string(now0 + 600000) + ",\"indicators\":{\"simulated\":true},\"traits\":[\"locatable\"]}";
  };
  // 400 drones and 400 aircraft on a 20 x 20 grid, 0.05 degrees apart, around Austin.
  for (int i = 0; i < 400; ++i) {
    double lat = 30.0 + (i % 20) * 0.05, lon = -98.2 + (i / 20) * 0.05;
    pub.nc->Publish("ent.drone.sim-d" + std::to_string(i) + ".state", rec("drone", "sim-d" + std::to_string(i), lat, lon));
    pub.nc->Publish("ent.aircraft.sim-a" + std::to_string(i) + ".state",
                    rec("aircraft", "sim-a" + std::to_string(i), lat, lon));
  }
  PollUntil({pub.nc.get()}, [] { return false; }, 300);

  ClientConfig cc;
  cc.nats.urls = {srv.ws_url()};
  LayerSpec small, large;
  small.id = "fleet";
  small.entity_types = {"drone"};
  small.path = LayerPath::Direct;
  small.tier = "small";
  large.id = "air-direct";
  large.entity_types = {"aircraft"};
  large.path = LayerPath::Direct;
  large.tier = "large";
  cc.layers = {small, large};
  ClientCore core(std::make_shared<omhost::PosixWsTransport>(), cc);
  core.Start(WallClockMs());
  auto tick = [&](TimeMs now) { core.Tick(now); };
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return core.nats().connected(); }, 5000, tick));
  // A camera-sized box (what UpdateAoiFromCamera computes at 8 km) over the grid's corner,
  // set before the layers start, as the Unreal subsystem does.
  auto count_in = [](const EntityStore& st, const Aoi& a) {
    size_t n = 0;
    for (size_t i = 0; i < st.size(); ++i)
      if (st.lats()[i] >= a.lat0 && st.lats()[i] <= a.lat1 && st.lons()[i] >= a.lon0 && st.lons()[i] <= a.lon1) n++;
    return n;
  };
  Aoi view1 = Aoi::Bbox(-98.22, 29.98, -98.03, 30.17);  // 4 x 4 grid points
  core.layers().SetAoi(view1, 12, WallClockMs());
  core.layers().Toggle("fleet", true, WallClockMs());
  core.layers().Toggle("air-direct", true, WallClockMs());
  LayerSession* fs = core.layers().Get("fleet");
  LayerSession* as = core.layers().Get("air-direct");
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return fs->state() == LayerState::Live && as->state() == LayerState::Live; },
                        10000, tick));
  EXPECT_EQ(fs->store().size(), 400u);  // small layer: whole layer, camera ignored
  EXPECT_EQ(as->store().size(), 16u);   // large layer: camera box only
  EXPECT_FALSE(fs->spec().aoi.has_bbox);
  // Pan to the opposite corner (more than 10 % of the box: the manager applies it).
  Aoi view2 = Aoi::Bbox(-97.57, 30.73, -97.38, 30.92);
  core.layers().SetAoi(view2, 12, WallClockMs());
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] {
    return as->state() == LayerState::Live && as->store().size() == 16u && count_in(as->store(), view2) == 16u;
  }, 10000, tick));
  std::printf("    after pan: large direct layer %zu entities (%zu in the new view), small layer %zu\n",
              as->store().size(), count_in(as->store(), view2), fs->store().size());
  EXPECT_EQ(count_in(as->store(), view2), 16u);
  EXPECT_EQ(count_in(as->store(), view1), 0u);
  EXPECT_EQ(fs->store().size(), 400u);
  // Pan back: the entities dropped by the first pan reload without waiting for an update.
  core.layers().SetAoi(view1, 12, WallClockMs());
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return count_in(as->store(), view1) == 16u; }, 10000, tick));
  EXPECT_EQ(as->store().size(), 16u);
  // A small-layer entity far outside every view still updates.
  pub.nc->Publish("ent.drone.sim-d399.state", rec("drone", "sim-d399", 31.5, -96.0, 2));
  EXPECT_TRUE(PollUntil({pub.nc.get()}, [&] { return fs->store().SeqOf("sim-d399") == 2; }, 5000, tick));
  core.Stop();
}
