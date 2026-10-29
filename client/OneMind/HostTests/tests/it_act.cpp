// Integration tests against a disposable nats-server (WebSocket):
//  - C7: the client's mission view against a stub of the proposed tak-client mission RPC
//    (client/OneMind/Docs/svc-tak-mission.md). A feature an ATAK user adds (the stub plays
//    tak-client relaying TAK Server's t-x-m-c change) appears in the client's view, and a
//    feature the client publishes appears in the mission for everyone else.
//  - B20: a feed heartbeat on tel.feed.{source}.status that stops shows as a gap within its
//    limit, naming the layer it weakens.
//  - D3 (stub policy): the client proposes, cannot confirm before the dialog is shown, refuses
//    a policy reply whose params_hash differs from its own, and follows evt.so.action.* to
//    "executed". The real policy service is exercised in scripts/e2e_d3_policy.py.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ItSupport.h"
#include "OneMindCore/AssetTasking.h"
#include "OneMindCore/Codec.h"
#include "OneMindCore/FeedGaps.h"
#include "OneMindCore/TakMissions.h"

#include <map>
#include <vector>

namespace {

// tak-client stand-in: an in-memory TAK Server mission store behind svc.tak.mission.*.
struct MissionStub {
  struct Item {
    std::string uid, type, callsign, creator;
    double lat = 0, lon = 0;
  };
  std::map<std::string, std::vector<Item>> missions{{"patrol-1", {}}};
  std::map<std::string, std::vector<std::string>> subs;
  std::vector<std::string> requests;
  om::NatsClient& nc;
  explicit MissionStub(om::NatsClient& n) : nc(n) {
    nc.Subscribe("svc.tak.mission.>", [this](const om::NatsMsg& m) { Handle(m); });
  }
  om::Json MissionJson(const std::string& name) {
    om::Json j = om::Json::MakeObject();
    j.set("name", name);
    j.set("token", om::TakMissionToken(name));
    j.set("description", "stub mission");
    j.set("subscribed", !subs[name].empty());
    om::Json uids = om::Json::MakeArray();
    for (const auto& it : missions[name]) {
      om::Json u = om::Json::MakeObject();
      u.set("uid", it.uid);
      u.set("type", it.type);
      u.set("callsign", it.callsign);
      u.set("lat", it.lat);
      u.set("lon", it.lon);
      u.set("creator_uid", it.creator);
      uids.push_back(u);
    }
    j.set("uids", uids);
    j.set("contents", om::Json::MakeArray());
    j.set("uid_count", static_cast<int64_t>(missions[name].size()));
    return j;
  }
  void Changed(const std::string& name, const Item& it) {
    om::Json ch = om::Json::MakeObject();
    ch.set("type", "ADD_CONTENT");
    ch.set("uid", it.uid);
    ch.set("callsign", it.callsign);
    ch.set("cot_type", it.type);
    ch.set("creator_uid", it.creator);
    om::Json data = om::Json::MakeObject();
    data.set("name", name);
    data.set("token", om::TakMissionToken(name));
    data.set("change", ch);
    om::Json env = om::Json::MakeObject();
    env.set("v", 1);
    env.set("id", om::NewUuid());
    env.set("type", "evt.mission.changed");
    env.set("source", "tak-client-stub");
    env.set("subject", "evt.mission." + om::TakMissionToken(name) + ".changed");
    env.set("data", data);
    nc.Publish("evt.mission." + om::TakMissionToken(name) + ".changed", env.Dump());
  }
  // What an ATAK user does in Data Sync: add a marker to the mission.
  void AtakAdds(const std::string& name, const std::string& uid, const std::string& callsign) {
    Item it{uid, "a-u-G", callsign, "ANDROID-atak1", 30.27, -97.74};
    missions[name].push_back(it);
    Changed(name, it);
  }
  void Handle(const om::NatsMsg& m) {
    requests.push_back(m.subject);
    om::Json q;
    om::Json::Parse(m.data, q);
    std::string ep = m.subject.substr(std::string("svc.tak.mission.").size());
    std::string name = q.str("name");
    om::Json r = om::Json::MakeObject();
    r.set("v", 1);
    if (ep != "list" && !missions.count(name)) {
      r.set("error", "not_found");
    } else if (ep == "list") {
      om::Json arr = om::Json::MakeArray();
      for (auto& kv : missions) arr.push_back(MissionJson(kv.first));
      r.set("missions", arr);
    } else if (ep == "get") {
      r.set("mission", MissionJson(name));
    } else if (ep == "changes") {
      r.set("changes", om::Json::MakeArray());
    } else if (ep == "logs") {
      om::Json logs = om::Json::MakeArray();
      om::Json l = om::Json::MakeObject();
      l.set("id", "log1");
      l.set("content", "mission opened");
      l.set("creator_uid", "ANDROID-atak1");
      logs.push_back(l);
      r.set("logs", logs);
    } else if (ep == "subscribe") {
      subs[name].push_back(q.str("client_uid"));
      r.set("ok", true);
    } else if (ep == "unsubscribe") {
      subs[name].clear();
      r.set("ok", true);
    } else if (ep == "content.put") {
      om::CotInfo c;
      if (!om::ParseCot(q.str("cot"), c)) {
        r.set("error", "invalid");
      } else {
        Item it{c.uid, c.type, c.callsign, q.str("client_uid"), c.point.lat, c.point.lon};
        missions[name].push_back(it);
        r.set("ok", true);
        r.set("uid", c.uid);
        Changed(name, it);
      }
    } else {
      r.set("error", "unknown endpoint");
    }
    nc.Publish(m.reply, r.Dump());
  }
};

bool Has(const om::TakMissionView* v, const std::string& uid) {
  if (!v) return false;
  for (const auto& u : v->uids)
    if (u.uid == uid) return true;
  return false;
}

}  // namespace

TEST(ItMissionsBothWays) {
  omit::NatsServer srv("missions");
  EXPECT_TRUE(srv.Start());
  omit::Client stub_c(srv.ws_url(), "tak-client-stub"), cl(srv.ws_url(), "client"), other(srv.ws_url(), "other-client");
  std::vector<om::NatsClient*> all{stub_c.nc.get(), cl.nc.get(), other.nc.get()};
  EXPECT_TRUE(omit::PollUntil(all, [&] { return stub_c.nc->connected() && cl.nc->connected() && other.nc->connected(); }, 5000));
  MissionStub stub(*stub_c.nc);
  om::TakMissionClient mc(*cl.nc, "ONEMIND-mac"), mo(*other.nc, "ONEMIND-other");
  mc.Watch();
  mo.Watch();
  omit::PollUntil(all, [] { return false; }, 200);
  std::string err = "pending";
  mc.List([&](const std::string& e) { err = e; });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return err != "pending"; }, 3000));
  EXPECT_EQ(err, std::string());
  EXPECT_EQ(mc.Missions().size(), 1u);
  err = "pending";
  mc.Subscribe("patrol-1", [&](const std::string& e) { err = e; });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return err != "pending"; }, 3000));
  EXPECT_EQ(stub.subs["patrol-1"].size(), 1u);
  err = "pending";
  mc.Open("patrol-1", [&](const std::string& e) { err = e; });
  std::string err2 = "pending";
  mo.Open("patrol-1", [&](const std::string& e) { err2 = e; });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return err != "pending" && err2 != "pending"; }, 3000));
  EXPECT_EQ(err, std::string());
  EXPECT_TRUE(omit::PollUntil(all, [&] { return mc.View("patrol-1") && mc.View("patrol-1")->logs.size() == 1; }, 3000));
  // ATAK -> client.
  stub.AtakAdds("patrol-1", "atak-marker-1", "Car at gate");
  EXPECT_TRUE(omit::PollUntil(all, [&] { return Has(mc.View("patrol-1"), "atak-marker-1"); }, 3000));
  EXPECT_TRUE(mc.change_events() >= 1);
  EXPECT_TRUE(!mc.View("patrol-1")->changes.empty() && mc.View("patrol-1")->changes.back().uid == "atak-marker-1");
  // Client -> mission (seen by every other subscriber, as ATAK would be).
  om::CotBuild b;
  b.uid = "onemind-mac-m1";
  b.callsign = "Water point";
  err = "pending";
  mc.PutFeature("patrol-1", om::BuildMarker(b, "a-n-G", om::GeoPoint{30.28, -97.75, 9999999.0}, om::WallClockMs()),
                [&](const std::string& e) { err = e; });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return err != "pending"; }, 3000));
  EXPECT_EQ(err, std::string());
  EXPECT_TRUE(omit::PollUntil(all, [&] { return Has(mo.View("patrol-1"), "onemind-mac-m1"); }, 3000));
  EXPECT_TRUE(Has(mc.View("patrol-1"), "onemind-mac-m1"));
  // Refusals: a hostile marker never leaves the client; unknown missions report the error.
  b.uid = "onemind-mac-h";
  size_t before = stub.requests.size();
  err = "pending";
  mc.PutFeature("patrol-1", om::BuildMarker(b, "a-h-G", om::GeoPoint{30.28, -97.75, 0}, om::WallClockMs()),
                [&](const std::string& e) { err = e; });
  EXPECT_FALSE(err.empty() || err == "pending");
  EXPECT_EQ(stub.requests.size(), before);
  err = "pending";
  mc.Open("nope", [&](const std::string& e) { err = e; });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return err != "pending"; }, 3000));
  EXPECT_EQ(err, std::string("not_found"));
  // OneMind's own mission facts on evt.mission.msn-* are ignored by the TAK mission view.
  uint64_t ev_before = mc.change_events();
  stub_c.nc->Publish("evt.mission.msn-01jbx7a0000000000000000000.changed", "{\"v\":1}");
  omit::PollUntil(all, [] { return false; }, 200);
  EXPECT_EQ(mc.change_events(), ev_before);
}

TEST(ItFeedGapStoppedFeed) {
  omit::NatsServer srv("gaps");
  EXPECT_TRUE(srv.Start());
  omit::Client feed(srv.ws_url(), "omos-ingest-standin"), cl(srv.ws_url(), "client");
  std::vector<om::NatsClient*> all{feed.nc.get(), cl.nc.get()};
  EXPECT_TRUE(omit::PollUntil(all, [&] { return feed.nc->connected() && cl.nc->connected(); }, 5000));
  // Heartbeat every 200 ms with a 1 s limit (omos-ingest uses 10 s and the source's staleAfter);
  // the panel's heartbeat assumption is scaled the same way.
  om::FeedGaps gaps(/*heartbeat_s=*/0, /*grace_s=*/0);
  gaps.SetLayerFeeds("weather", {"nws-alerts"});
  gaps.SetAlertFeeds("severe_weather", {"nws-alerts"});
  cl.nc->Subscribe("tel.feed.*.status", [&](const om::NatsMsg& m) { gaps.IngestStatus(m.data, om::WallClockMs()); });
  omit::PollUntil(all, [] { return false; }, 100);
  auto beat = [&] {
    om::Json j = om::Json::MakeObject();
    j.set("source", "nws-alerts");
    j.set("kind", "rest_poll");
    j.set("collector", "omos-ingest");
    j.set("leader", true);
    j.set("fetchedAt", om::CotTime(om::WallClockMs()));
    j.set("count", 3);
    j.set("lastError", om::Json());
    j.set("staleAfterSec", 1.0);
    j.set("stale", false);
    feed.nc->Publish("tel.feed.nws-alerts.status", j.Dump());
  };
  om::TimeMs last = 0;
  omit::PollUntil(all, [] { return false; }, 1200, [&](om::TimeMs now) {
    if (now - last >= 200) {
      beat();
      last = now;
    }
  });
  EXPECT_TRUE(gaps.Gaps(om::WallClockMs()).empty());
  // Stop the feed. It must show within its limit (1 s) plus the allowed margin.
  om::TimeMs stopped = om::WallClockMs();
  om::TimeMs shown = 0;
  omit::PollUntil(all, [&] {
    auto g = gaps.Gaps(om::WallClockMs());
    if (!g.empty() && !shown) shown = om::WallClockMs();
    return !g.empty();
  }, 3000);
  EXPECT_TRUE(shown > 0);
  EXPECT_TRUE(shown - stopped <= 1500);
  auto g = gaps.Gaps(om::WallClockMs());
  EXPECT_EQ(g.size(), 1u);
  if (!g.empty()) {
    EXPECT_EQ(g[0].source, std::string("nws-alerts"));
    EXPECT_TRUE(g[0].status == om::FeedGap::Status::Stale || g[0].status == om::FeedGap::Status::Missing);
    EXPECT_EQ(g[0].layers.size(), 1u);
    EXPECT_EQ(g[0].layers[0], std::string("weather"));
    EXPECT_EQ(g[0].alerts[0], std::string("severe_weather"));
    std::printf("    gap shown %lld ms after the feed stopped: %s %s (%s), weakens layer %s\n",
                static_cast<long long>(shown - stopped), g[0].source.c_str(), om::FeedGapStatusName(g[0].status),
                g[0].reason.c_str(), g[0].layers[0].c_str());
  }
  // The feed comes back: the gap clears.
  beat();
  EXPECT_TRUE(omit::PollUntil(all, [&] { return gaps.Gaps(om::WallClockMs()).empty(); }, 2000));
}

TEST(ItTaskingStubPolicy) {
  omit::NatsServer srv("tasking");
  EXPECT_TRUE(srv.Start());
  omit::Client pol(srv.ws_url(), "policy-stub"), cl(srv.ws_url(), "client");
  std::vector<om::NatsClient*> all{pol.nc.get(), cl.nc.get()};
  EXPECT_TRUE(omit::PollUntil(all, [&] { return pol.nc->connected() && cl.nc->connected(); }, 5000));
  bool tamper = false;
  int confirms = 0;
  std::string last_action;
  pol.nc->Subscribe("svc.policy.command.propose", [&](const om::NatsMsg& m) {
    om::Json q;
    om::Json::Parse(m.data, q);
    om::Json r = om::Json::MakeObject();
    last_action = "act-" + om::NewToken();
    r.set("action_id", last_action);
    r.set("status", "awaiting_confirmation");
    r.set("level", "A3");
    om::Json params = q["params"];
    if (tamper) params.set("lat", 31.0);  // a policy that would dispatch something else
    r.set("params_hash", om::ParamsHash(q.str("action_type"), 1, q["target"], params));
    r.set("confirm_by", om::CotTime(om::WallClockMs() + 120000));
    r.set("approvals_needed", 1);
    r.set("denial", om::Json());
    // Like the real service: the audit event goes out before the reply.
    om::Json env = om::Json::MakeObject();
    om::Json d = om::Json::MakeObject();
    d.set("action_id", last_action);
    env.set("data", d);
    pol.nc->Publish("evt.so.action.proposed", env.Dump());
    pol.nc->Publish(m.reply, r.Dump());
  });
  pol.nc->Subscribe("svc.policy.command.confirm", [&](const om::NatsMsg& m) {
    ++confirms;
    om::Json q;
    om::Json::Parse(m.data, q);
    om::Json r = om::Json::MakeObject();
    r.set("action_id", q.str("action_id"));
    r.set("status", "approved");
    pol.nc->Publish(m.reply, r.Dump());
    for (const char* e : {"approved", "dispatched", "acknowledged", "executed"}) {
      om::Json env = om::Json::MakeObject();
      om::Json d = om::Json::MakeObject();
      d.set("action_id", q.str("action_id"));
      env.set("data", d);
      pol.nc->Publish(std::string("evt.so.action.") + e, env.Dump());
    }
  });
  om::AssetTasking t(*cl.nc, "user:zeus");
  t.Watch();
  omit::PollUntil(all, [] { return false; }, 100);
  om::PendingAction got;
  std::string err = "pending";
  t.Propose(om::GotoDraft("sim-x1", "drone", {30.27, -97.74, 0}, 120, 0, "clicked"), om::WallClockMs(),
            [&](const om::PendingAction& a, const std::string& e) {
              got = a;
              err = e;
            });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return err != "pending"; }, 3000));
  EXPECT_EQ(err, std::string());
  EXPECT_EQ(got.status, std::string("awaiting_confirmation"));
  EXPECT_EQ(got.params_hash, got.local_hash);
  // Not shown yet -> no confirm leaves the client.
  err = "pending";
  t.Confirm(got.action_id, true, "", om::WallClockMs(), [&](const om::PendingAction&, const std::string& e) { err = e; });
  EXPECT_TRUE(err.find("not shown") != std::string::npos);
  EXPECT_EQ(confirms, 0);
  t.MarkShown(got.action_id);
  err = "pending";
  t.Confirm(got.action_id, true, "", om::WallClockMs(), [&](const om::PendingAction&, const std::string& e) { err = e; });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return t.Find(got.action_id) && t.Find(got.action_id)->status == "executed"; }, 3000));
  EXPECT_EQ(confirms, 1);
  EXPECT_EQ(t.Find(got.action_id)->timeline.size(), 5u);
  // A second confirm of a decided action is refused locally.
  err = "pending";
  t.Confirm(got.action_id, true, "", om::WallClockMs(), [&](const om::PendingAction&, const std::string& e) { err = e; });
  EXPECT_TRUE(err.find("not awaiting") != std::string::npos);
  // A policy reply whose hash differs from what the human sees is never confirmable.
  tamper = true;
  err = "pending";
  t.Propose(om::GotoDraft("sim-x1", "drone", {30.27, -97.74, 0}, 120, 0, "clicked"), om::WallClockMs(),
            [&](const om::PendingAction& a, const std::string& e) {
              got = a;
              err = e;
            });
  EXPECT_TRUE(omit::PollUntil(all, [&] { return err != "pending"; }, 3000));
  EXPECT_EQ(got.denial_code, std::string("hash_mismatch"));
  t.MarkShown(got.action_id);
  t.Confirm(got.action_id, true, "", om::WallClockMs(), [&](const om::PendingAction&, const std::string& e) { err = e; });
  EXPECT_EQ(confirms, 1);
  // A non-sim asset never reaches policy.
  err = "pending";
  std::string rid = t.Propose(om::GotoDraft("x1", "drone", {30.27, -97.74, 0}, 120, 0, ""), om::WallClockMs(),
                              [&](const om::PendingAction& a, const std::string& e) {
                                got = a;
                                err = e;
                              });
  EXPECT_EQ(rid, std::string());
  EXPECT_EQ(got.denial_code, std::string("not_simulated"));
}
