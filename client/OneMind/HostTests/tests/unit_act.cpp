// Unit tests: drawing and tactical graphics (B11), intelligence gaps (B20), asset tasking
// and JCS params_hash (D3), TAK mission parsing (C7).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/AssetTasking.h"
#include "OneMindCore/Drawing.h"
#include "OneMindCore/FeedGaps.h"
#include "OneMindCore/TakMissions.h"
#include "OneMindCore/TakProto.h"
#include "TestHarness.h"

namespace {
const om::GeoPoint kC{30.2672, -97.7431, 9999999.0};
om::CotBuild B(const std::string& uid, const std::string& label = "Shape") {
  om::CotBuild b;
  b.uid = uid;
  b.callsign = label;
  return b;
}
om::Shape RoundTrip(const om::CotEvent& ev) {
  // Through TAK Protocol v1 and back, as a shape travels on a negotiated stream.
  std::string payload;
  om::CotXmlToTakPayload(ev.ToXML(), payload);
  om::TakDecoded d;
  om::DecodeTakPayload(payload, d);
  om::Shape s;
  om::ParseShape(d.xml, s);
  return s;
}
}  // namespace

TEST(GeoMathDestinationBearing) {
  om::GeoPoint p = om::GeoMath::Destination(kC, 90, 1000);
  EXPECT_NEAR(om::GeoMath::DistanceM(kC, p), 1000.0, 0.01);
  EXPECT_NEAR(om::GeoMath::Bearing(kC, p), 90.0, 0.01);
  om::GeoPoint n = om::GeoMath::Destination(kC, 0, 111195);  // ~1 degree of latitude
  EXPECT_NEAR(n.lat - kC.lat, 1.0, 1e-3);
  EXPECT_TRUE(om::GeoMath::Contains({{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}, 0.5, 0.5));
  EXPECT_FALSE(om::GeoMath::Contains({{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}, 1.5, 0.5));
}

TEST(DrawingCircleRectanglePolygonLine) {
  om::ShapeStyle st;
  st.stroke_argb = 0xffff0000u;
  st.fill_argb = 0x40ff0000u;
  om::CotEvent c = om::BuildCircle(B("onemind-c1", "Circle 1"), kC, 250, st, 1793275200000);
  EXPECT_EQ(c.GetAttribute("type"), std::string("u-d-c-c"));
  EXPECT_EQ(c.GetAttribute("detail\tshape\tellipse\tmajor"), std::string("250.0"));
  EXPECT_EQ(c.GetAttribute("detail\tshape\tellipse\tangle"), std::string("360"));
  EXPECT_EQ(c.GetAttribute("detail\tstrokeColor\tvalue"), std::string("-65536"));  // as the R&B example writes red
  EXPECT_EQ(c.GetAttribute("detail\tlabels_on\tvalue"), std::string("true"));
  EXPECT_EQ(om::ValidateOutgoingCot(c), std::string());
  om::Shape s = RoundTrip(c);
  EXPECT_TRUE(s.kind == om::ShapeKind::Circle);
  EXPECT_NEAR(s.radius_m, 250.0, 1e-9);
  EXPECT_NEAR(s.center.lat, kC.lat, 1e-7);
  EXPECT_EQ(s.style.fill_argb, 0x40ff0000u);

  om::CotEvent r = om::BuildRectangle(B("onemind-r1"), {30.0, -97.0, 0}, {30.1, -97.2, 0}, st, 0);
  s = RoundTrip(r);
  EXPECT_TRUE(s.kind == om::ShapeKind::Rectangle);
  EXPECT_EQ(s.points.size(), 4u);
  // NW, NE, SE, SW, the order of the TAK example.
  EXPECT_NEAR(s.points[0].lat, 30.1, 1e-9);
  EXPECT_NEAR(s.points[0].lon, -97.2, 1e-9);
  EXPECT_NEAR(s.points[2].lat, 30.0, 1e-9);
  EXPECT_NEAR(s.points[2].lon, -97.0, 1e-9);

  om::CotEvent poly = om::BuildPolygon(B("onemind-p1"), {{30, -97, 0}, {30.1, -97, 0}, {30.1, -97.1, 0}}, st, 0);
  s = RoundTrip(poly);
  EXPECT_TRUE(s.kind == om::ShapeKind::Polygon);
  EXPECT_EQ(s.points.size(), 4u);  // closed

  om::CotEvent line = om::BuildPolyline(B("onemind-l1"), {{30, -97, 0}, {30.1, -97, 0}, {30.2, -97.05, 0}}, st, 0);
  s = RoundTrip(line);
  EXPECT_TRUE(s.kind == om::ShapeKind::Line);
  EXPECT_EQ(s.points.size(), 3u);
  EXPECT_FALSE(line.HasNode("detail\tfillColor"));
}

TEST(DrawingSectorRingsRangeBearingPoint) {
  om::ShapeStyle st;
  om::CotEvent sec = om::BuildSector(B("onemind-s1"), kC, 600, 30, 90, st, 0, 12);
  om::Shape s = RoundTrip(sec);
  EXPECT_TRUE(s.kind == om::ShapeKind::Polygon);
  EXPECT_EQ(s.points.size(), 15u);  // centre + 13 arc points + centre
  EXPECT_NEAR(om::GeoMath::DistanceM(kC, s.points[1]), 600, 0.5);
  EXPECT_NEAR(om::GeoMath::Bearing(kC, s.points[1]), 30, 0.05);
  EXPECT_NEAR(om::GeoMath::Bearing(kC, s.points[13]), 90, 0.05);
  // Sector across north (350 -> 10 degrees) spans 20 degrees, not 340.
  om::Shape wrap = RoundTrip(om::BuildSector(B("onemind-s2"), kC, 600, 350, 10, st, 0, 4));
  EXPECT_NEAR(om::GeoMath::Bearing(kC, wrap.points[3]), 0, 0.05);

  auto rings = om::BuildRangeRings(B("onemind-rr", "Rings"), kC, {100, 200, 300}, st, 0);
  EXPECT_EQ(rings.size(), 3u);
  EXPECT_EQ(rings[2].GetAttribute("uid"), std::string("onemind-rr.r3"));
  EXPECT_EQ(rings[2].GetAttribute("type"), std::string("u-r-b-c-c"));
  EXPECT_NEAR(RoundTrip(rings[1]).radius_m, 200, 1e-9);

  om::GeoPoint to = om::GeoMath::Destination(kC, 60, 1000);
  om::Shape rb = RoundTrip(om::BuildRangeBearing(B("onemind-rb"), kC, to, st, 0));
  EXPECT_TRUE(rb.kind == om::ShapeKind::RangeBearing);
  EXPECT_NEAR(rb.range_m, 1000, 0.01);
  EXPECT_NEAR(rb.bearing_deg, 60, 0.01);

  om::Shape pt = RoundTrip(om::BuildPoint(B("onemind-pt", "Gate"), kC, 0));
  EXPECT_TRUE(pt.kind == om::ShapeKind::Point);
  EXPECT_EQ(pt.label, std::string("Gate"));
}

TEST(DrawingBufferAndLasso) {
  // Point buffer: every vertex at the distance.
  auto ring = om::BufferRing({kC}, 100);
  EXPECT_TRUE(ring.size() > 32);
  double worst = 0;
  for (const auto& p : ring) worst = (std::max)(worst, std::fabs(om::GeoMath::DistanceM(kC, p) - 100));
  EXPECT_TRUE(worst < 0.5);
  // Line buffer: a capsule; points 50 m off the middle are inside, 150 m off are outside.
  om::GeoPoint e = om::GeoMath::Destination(kC, 90, 800);
  auto cap = om::BufferRing({kC, e}, 100);
  om::GeoPoint mid = om::GeoMath::Destination(kC, 90, 400);
  om::GeoPoint in = om::GeoMath::Destination(mid, 0, 50), out = om::GeoMath::Destination(mid, 0, 150);
  om::GeoPoint beyond = om::GeoMath::Destination(e, 90, 90), far_beyond = om::GeoMath::Destination(e, 90, 120);
  EXPECT_TRUE(om::GeoMath::Contains(cap, in.lat, in.lon));
  EXPECT_FALSE(om::GeoMath::Contains(cap, out.lat, out.lon));
  EXPECT_TRUE(om::GeoMath::Contains(cap, beyond.lat, beyond.lon));
  EXPECT_FALSE(om::GeoMath::Contains(cap, far_beyond.lat, far_beyond.lon));
  // An L-shaped line keeps the corner covered on the outer side.
  om::GeoPoint n = om::GeoMath::Destination(e, 0, 800);
  auto l = om::BufferRing({kC, e, n}, 100);
  om::GeoPoint outer = om::GeoMath::Destination(om::GeoMath::Destination(e, 90, 60), 180, 60);
  EXPECT_TRUE(om::GeoMath::Contains(l, outer.lat, outer.lon));
  om::CotEvent buf = om::BuildBuffer(B("onemind-b1"), {kC, e}, 100, om::ShapeStyle(), 0);
  EXPECT_EQ(buf.GetAttribute("type"), std::string("u-d-f"));
  EXPECT_EQ(om::ValidateOutgoingCot(buf), std::string());
  // Lasso.
  std::vector<om::LassoCandidate> cands{{"in-1", in.lat, in.lon}, {"out-1", out.lat, out.lon}, {"in-2", mid.lat, mid.lon}};
  auto sel = om::LassoSelect(cap, cands);
  EXPECT_EQ(sel.size(), 2u);
  EXPECT_EQ(sel[0], std::string("in-1"));
  EXPECT_TRUE(om::LassoSelect({kC, e}, cands).empty());
}

// --- B20 ------------------------------------------------------------------------------------

namespace {
std::string Status(const std::string& src, const std::string& fetched_at, double stale_after, const char* last_error,
                   bool stale) {
  om::Json j = om::Json::MakeObject();
  j.set("source", src);
  j.set("kind", "rest_poll");
  j.set("collector", "omos-ingest");
  j.set("instance", "t");
  j.set("leader", true);
  j.set("fetchedAt", fetched_at.empty() ? om::Json() : om::Json(fetched_at));
  j.set("count", 12);
  j.set("published", 3);
  j.set("lastError", last_error ? om::Json(last_error) : om::Json());
  j.set("staleAfterSec", stale_after);
  j.set("stale", stale);
  j.set("at", "2026-10-29T12:00:10Z");
  return j.Dump();
}
}  // namespace

TEST(FeedGapsStatesAndLayers) {
  om::TimeMs t0 = 0;
  om::ParseCotTime("2026-10-29T12:00:10Z", t0);
  om::FeedGaps g(10, 5);
  g.SetLayerFeeds("weather", {"nws-alerts"});
  g.SetLayerFeeds("air", {"opensky", "adsb-local"});
  g.SetAlertFeeds("severe_weather", {"nws-alerts"});
  g.ExpectFeed("adsb-local", 60);
  EXPECT_TRUE(g.IngestStatus(Status("nws-alerts", "2026-10-29T12:00:00Z", 300, nullptr, false), t0));
  EXPECT_TRUE(g.IngestStatus(Status("opensky", "2026-10-29T12:00:05.5Z", 60, nullptr, false), t0));
  EXPECT_FALSE(g.IngestStatus("{\"x\":1}", t0));
  auto gaps = g.Gaps(t0);
  EXPECT_EQ(gaps.size(), 1u);  // adsb-local: expected, never heard
  EXPECT_EQ(gaps[0].source, std::string("adsb-local"));
  EXPECT_TRUE(gaps[0].status == om::FeedGap::Status::Missing);
  EXPECT_EQ(gaps[0].layers.size(), 1u);
  EXPECT_EQ(gaps[0].layers[0], std::string("air"));
  // A failed fetch names the error and the weakened layer and alert.
  g.IngestStatus(Status("nws-alerts", "2026-10-29T12:00:00Z", 300, "HTTP 503", false), t0 + 10000);
  gaps = g.Gaps(t0 + 10000);
  bool found = false;
  for (const auto& x : gaps)
    if (x.source == "nws-alerts") {
      found = true;
      EXPECT_TRUE(x.status == om::FeedGap::Status::Failed);
      EXPECT_EQ(x.reason, std::string("HTTP 503"));
      EXPECT_EQ(x.alerts.size(), 1u);
      EXPECT_EQ(x.layers[0], std::string("weather"));
    }
  EXPECT_TRUE(found);
  // Stale by age even if the collector has not flagged it yet.
  g.IngestStatus(Status("opensky", "2026-10-29T12:00:05Z", 60, nullptr, false), t0 + 60000);
  auto all = g.All(t0 + 66000);
  for (const auto& x : all)
    if (x.source == "opensky") EXPECT_TRUE(x.status == om::FeedGap::Status::Stale);
  // A collector that stops sending heartbeats is missing after max(limit, 3 beats) + grace.
  g.IngestStatus(Status("opensky", "2026-10-29T12:01:00Z", 20, nullptr, false), t0 + 60000);
  for (const auto& x : g.All(t0 + 60000 + 35000))
    if (x.source == "opensky") EXPECT_TRUE(x.status != om::FeedGap::Status::Missing);
  for (const auto& x : g.All(t0 + 60000 + 35001))
    if (x.source == "opensky") EXPECT_TRUE(x.status == om::FeedGap::Status::Missing);
  auto layers = g.Layers(t0 + 60000 + 35001);
  for (const auto& l : layers) {
    if (l.layer == "air") EXPECT_EQ(l.status, std::string("stale"));
    if (l.layer == "weather") EXPECT_EQ(l.status, std::string("stale"));
  }
}

TEST(FeedGapsFromManifest) {
  om::FeedGaps g;
  om::Json m;
  om::Json::Parse(R"({"v":1,"layer":"air","freshness":{"status":"degraded","limit_s":60,"feeds":[
      {"source":"opensky","fetchedAt":1793275195000,"count":1234,"lastError":null,"stale":false},
      {"source":"adsb-lol","fetchedAt":1793275000000,"count":0,"lastError":"timeout","stale":true}]}})", m);
  g.IngestManifest("air", m, 1793275200000);
  auto gaps = g.Gaps(1793275200000);
  EXPECT_EQ(gaps.size(), 1u);
  EXPECT_EQ(gaps[0].source, std::string("adsb-lol"));
  EXPECT_TRUE(gaps[0].status == om::FeedGap::Status::Failed);
  EXPECT_EQ(gaps[0].layers[0], std::string("air"));
  auto layer_health = g.Layers(1793275200000);
  EXPECT_EQ(layer_health[0].status, std::string("degraded"));
}

// --- D3 ---------------------------------------------------------------------------------------

TEST(JcsMatchesPolicyService) {
  // Golden values printed by services/policy/internal/policy/jcs.go (omos PR #554) run with Go
  // 1.27.3 on the build machine (docs/evidence/M4/UE/jcs-golden.txt).
  EXPECT_EQ(om::JcsNumber(1e21), std::string("1e+21"));
  EXPECT_EQ(om::JcsNumber(1e-7), std::string("1e-7"));
  EXPECT_EQ(om::JcsNumber(0.1), std::string("0.1"));
  EXPECT_EQ(om::JcsNumber(123456789012345680000.0), std::string("123456789012345680000"));
  EXPECT_EQ(om::JcsNumber(-0.000001), std::string("-0.000001"));
  EXPECT_EQ(om::JcsNumber(5e-324), std::string("5e-324"));
  EXPECT_EQ(om::JcsNumber(1.7976931348623157e308), std::string("1.7976931348623157e+308"));
  EXPECT_EQ(om::JcsNumber(30.2690), std::string("30.269"));
  EXPECT_EQ(om::JcsNumber(100), std::string("100"));
  EXPECT_EQ(om::JcsNumber(1e20), std::string("100000000000000000000"));
  EXPECT_EQ(om::JcsNumber(0.0000001234), std::string("1.234e-7"));
  om::Json o;
  om::Json::Parse("{\"b\":\"\\u0001\\\"x\\n\",\"a\":[true,null,{\"\xc3\xa9\":1,\"z\":2}]}", o);
  EXPECT_EQ(om::JcsCanonical(o), std::string("{\"a\":[true,null,{\"z\":2,\"\xc3\xa9\":1}],\"b\":\"\\u0001\\\"x\\n\"}"));
  om::Json target = om::Json::MakeObject();
  target.set("entity_type", "drone");
  target.set("entity_id", "sim-x1");
  om::Json params;
  om::Json::Parse(R"({"lat":30.269,"lon":-97.742,"hae_m":160,"speed_mps":8})", params);
  EXPECT_EQ(om::ParamsHash("asset.goto", 1, target, params),
            std::string("sha256:c4ed3a928fc7e83d514db2fea28aeb3024cf88c3960485898aa246e4bdbe4db0"));
  om::TaskDraft d = om::GotoDraft("sim-x1", "drone", {30.26721234, -97.74314559, 0}, 120.54, 0, "");
  EXPECT_EQ(om::JcsCanonical(d.params), std::string("{\"hae_m\":120.5,\"lat\":30.2672123,\"lon\":-97.7431456}"));
  EXPECT_EQ(om::ParamsHash("asset.goto", 1, target, d.params),
            std::string("sha256:2df2f2a2c9c2cb8096858c53c6d3c2c8e127fc7779424c9616efdece383eada3"));
}

TEST(TaskingPreChecksAndRequest) {
  EXPECT_TRUE(om::IsAssetVerb("goto"));
  EXPECT_FALSE(om::IsAssetVerb("strike"));
  EXPECT_FALSE(om::IsAssetVerb("intercept"));
  EXPECT_EQ(om::AssetVerbLevel("takeoff"), std::string("A4"));
  om::TaskDraft d = om::GotoDraft("sim-x1", "drone", {30.27, -97.74, 0}, 160, 8, "look at the gate");
  EXPECT_EQ(om::AssetTasking::PreCheck(d), std::string());
  om::TaskDraft real = d;
  real.asset_id = "x1";
  EXPECT_TRUE(om::AssetTasking::PreCheck(real).rfind("not_simulated", 0) == 0);
  om::TaskDraft bad = d;
  bad.verb = "engage";
  EXPECT_TRUE(om::AssetTasking::PreCheck(bad).rfind("unknown_action", 0) == 0);
  om::TaskDraft wild = d;
  wild.asset_id = "sim-*";
  EXPECT_FALSE(om::AssetTasking::PreCheck(wild).empty());
  om::TaskDraft nohae = d;
  nohae.params = om::Json::MakeObject();
  nohae.params.set("lat", 1.0);
  nohae.params.set("lon", 1.0);
  EXPECT_FALSE(om::AssetTasking::PreCheck(nohae).empty());
  std::string u = om::NewUuid();
  EXPECT_EQ(u.size(), 36u);
  EXPECT_EQ(u[14], '4');
}

// --- C7 parsers -------------------------------------------------------------------------------

TEST(TakMissionTokensAndParsers) {
  EXPECT_EQ(om::TakMissionToken("patrol-1"), std::string("tak-patrol-1"));
  EXPECT_EQ(om::TakMissionToken("Patrol_A"), std::string("tak-patrol_a"));
  std::string t = om::TakMissionToken("North Gate.watch");
  EXPECT_EQ(t.size(), 30u);
  EXPECT_TRUE(t.rfind("tak-", 0) == 0);
  EXPECT_TRUE(t.find('.') == std::string::npos && t.find(' ') == std::string::npos);
  om::Json m;
  om::Json::Parse(R"({"name":"North Gate.watch","description":"d","creator_uid":"ANDROID-abc","create_time":1793275200000,
      "subscribed":true,"uids":[{"uid":"u1","type":"a-u-G","callsign":"Car","lat":30.1,"lon":-97.1,"creator_uid":"ANDROID-abc","timestamp":1}],
      "contents":[{"hash":"abc","filename":"plan.pdf","mime_type":"application/pdf","size":10,"submitter":"bob","timestamp":2}, {"filename":"nohash"}]})",
                  m);
  om::TakMissionView v;
  EXPECT_TRUE(om::TakMissionClient::ParseMission(m, v));
  EXPECT_EQ(v.summary.token, t);
  EXPECT_EQ(v.uids.size(), 1u);
  EXPECT_EQ(v.contents.size(), 1u);
  EXPECT_EQ(v.summary.uid_count, 1);
  EXPECT_NEAR(v.uids[0].point.lat, 30.1, 1e-12);
  EXPECT_FALSE(om::TakMissionClient::ParseMission(om::Json::MakeObject(), v));
}
