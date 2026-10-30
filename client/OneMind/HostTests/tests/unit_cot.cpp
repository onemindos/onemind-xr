// Unit tests: XML, CoT parse/emit (VH_COTEvent port), drop rules, TAK entity ids,
// builders for svc.cot.send, MIL-STD-2525 helpers (VH_MilStd2525 port).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/CoT.h"
#include "OneMindCore/MilStd2525.h"
#include "OneMindCore/Xml.h"
#include "TestHarness.h"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace om;

namespace {
std::string ReadFixture(const char* name) {
  std::ifstream f(std::string(OM_FIXTURES_DIR) + "/" + name);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
}  // namespace

TEST(xml_parse_entities_and_limits) {
  std::string err;
  auto n = XmlParse("<?xml version=\"1.0\"?><!-- c --><a x='1' y=\"&lt;&amp;&#65;&#x42;\"><b/>t<![CDATA[<raw>]]></a>", &err);
  EXPECT_TRUE(n != nullptr);
  if (!n) return;
  EXPECT_EQ(n->AttrOr("y"), std::string("<&AB"));
  EXPECT_EQ(n->text, std::string("t<raw>"));
  EXPECT_TRUE(n->Child("b") != nullptr);
  EXPECT_TRUE(XmlParse("<a><b></a>", &err) == nullptr);
  EXPECT_TRUE(XmlParse("<a>&xxe;</a>", &err) == nullptr);  // no custom entities
  std::string deep;
  for (int i = 0; i < 40; ++i) deep += "<d>";
  EXPECT_TRUE(XmlParse(deep, &err) == nullptr);
  // DOCTYPE is skipped, never processed.
  auto d = XmlParse("<!DOCTYPE x [<!ENTITY e \"boom\">]><a>ok</a>", &err);
  EXPECT_TRUE(d == nullptr || d->text == "ok");
}

TEST(cot_time_roundtrip) {
  TimeMs t = 0;
  EXPECT_TRUE(ParseCotTime("2026-10-29T12:34:56.789Z", t));
  EXPECT_EQ(CotTime(t), std::string("2026-10-29T12:34:56.789Z"));
  EXPECT_TRUE(ParseCotTime("1970-01-01T00:00:00Z", t));
  EXPECT_EQ(t, 0);
  EXPECT_TRUE(ParseCotTime("2026-10-29T13:34:56.789+01:00", t));
  EXPECT_EQ(CotTime(t), std::string("2026-10-29T12:34:56.789Z"));
  EXPECT_FALSE(ParseCotTime("yesterday", t));
}

TEST(cot_event_vh_port_path_api) {
  CotEvent ev;
  EXPECT_EQ(ev.GetAttribute("how"), std::string("h-g-i-g-o"));  // VH default
  ev.SetAttribute("uid", "ANDROID-123");
  ev.SetAttribute("callsign", "CYAN-1");  // keyword alias -> detail/contact/callsign
  ev.SetPoint(GeoPoint{30.2672, -97.7431, 150});
  ev.AddNode("detail\tlink", {{"point", "1,2"}});
  ev.AddNode("detail\tlink", {{"point", "3,4"}});
  EXPECT_EQ(ev.GetNodes("link").size(), 2u);
  EXPECT_TRUE(ev.HasNode("detail\tcontact"));
  EXPECT_EQ(ev.GetAttribute("detail\tcontact\tcallsign"), std::string("CYAN-1"));
  EXPECT_NEAR(ev.GetPoint().lat, 30.2672, 1e-9);
  EXPECT_NEAR(ev.GetAttributeAsDouble("point\tce"), 9999999.0, 1e-6);
  ev.RemoveNodes("link");
  EXPECT_EQ(ev.GetNodes("link").size(), 0u);
  CotEvent copy = ev;
  copy.SetAttribute("uid", "other");
  EXPECT_EQ(ev.GetAttribute("uid"), std::string("ANDROID-123"));  // deep copy
  CotEvent back;
  EXPECT_TRUE(back.PopulateFromXML(ev.ToXML(true)));
  EXPECT_EQ(back.GetAttribute("callsign"), std::string("CYAN-1"));
}

TEST(cot_parse_eud_pli_fixture) {
  CotInfo c;
  std::string err;
  EXPECT_TRUE(ParseCot(ReadFixture("eud_pli.xml"), c, &err));
  EXPECT_EQ(c.uid, std::string("ANDROID-e2e-eud-01"));
  EXPECT_EQ(c.type, std::string("a-f-G-U-C"));
  EXPECT_EQ(c.callsign, std::string("E2E-EUD"));
  EXPECT_EQ(c.endpoint, std::string("127.0.0.1:4242:tcp"));
  EXPECT_EQ(c.group_name, std::string("Cyan"));
  EXPECT_TRUE(c.has_takv);
  EXPECT_EQ(c.takv_platform, std::string("ATAK-CIV"));
  EXPECT_EQ(c.battery, 88);
  EXPECT_NEAR(c.course, 90.0, 1e-9);
  EXPECT_NEAR(c.point.lat, 30.2672, 1e-9);
  EXPECT_TRUE(c.stale > c.time);
  EXPECT_EQ(CotDropReason(c), std::string());
  EXPECT_EQ(CotType::Disposition(c.type), std::string("friendly"));
  EXPECT_EQ(CotType::Environment(c.type), std::string("land"));
}

TEST(cot_drop_rules_and_disposition) {
  CotInfo c;
  c.uid = "OM.drone.sim-x1";
  c.type = "a-f-A-M-H-Q";
  EXPECT_EQ(CotDropReason(c), std::string("echo"));
  c.uid = "x";
  c.type = "t-x-takp-v";
  EXPECT_EQ(CotDropReason(c), std::string("control"));
  c.type = "t-x-m-c";
  EXPECT_EQ(CotDropReason(c), std::string("mission_sync"));
  c.type = "a-f-G";
  c.uid = "abc-ping";
  EXPECT_EQ(CotDropReason(c), std::string("control"));
  EXPECT_EQ(CotType::Disposition("a-h-A"), std::string("suspicious"));  // never hostile
  EXPECT_EQ(CotType::Disposition("a-n-S"), std::string("neutral"));
  EXPECT_EQ(CotType::Environment("a-u-S"), std::string("surface"));
}

TEST(tak_entity_id_matches_entity_service_rule) {
  EXPECT_EQ(TakEntityId("ANDROID-abc_1"), std::string("tak-android-abc_1"));
  // uid with '.' is hashed: tak-{base32(sha256(uid))[0:26]}
  std::string h = TakEntityId("GeoChat.uid.room");
  EXPECT_EQ(h.size(), 4u + 26u);
  EXPECT_EQ(h.rfind("tak-", 0), 0u);
  EXPECT_EQ(TakEntityId("GeoChat.uid.room"), h);
  EXPECT_TRUE(TakEntityId(std::string(61, 'a')).size() == 30u);  // too long for 64-char ids
}

TEST(cot_builders_validate_and_send_body) {
  CotBuild b;
  b.uid = "onemind-test-1";
  b.callsign = "OM-MARK";
  TimeMs now = 1793275200000;
  CotEvent m = BuildMarker(b, "a-u-G", GeoPoint{30.0, -97.0, 9999999.0}, now);
  EXPECT_EQ(ValidateOutgoingCot(m), std::string());
  EXPECT_EQ(m.GetAttribute("opex"), std::string("s"));
  EXPECT_EQ(m.GetAttribute("how"), std::string("h-e"));
  EXPECT_TRUE(m.GetNodeContent("detail\tremarks").find("SIMULATED") != std::string::npos);
  CotInfo info;
  EXPECT_TRUE(ParseCot(m.ToXML(), info));
  EXPECT_EQ(info.stale - info.time, 300000);

  CotEvent r = BuildRoute(b, {{30, -97, 0}, {30.1, -97.1, 0}, {30.2, -97.2, 0}}, now);
  EXPECT_EQ(r.GetNodes("link").size(), 3u);
  EXPECT_EQ(r.GetNodes("link")[1]->AttrOr("uid"), std::string("onemind-test-1.wp2"));
  CotEvent g = BuildGeofence(b, {{30, -97, 0}, {30, -96, 0}, {31, -96, 0}}, "keep_out", now);
  EXPECT_EQ(g.GetAttribute("type"), std::string("u-d-f"));
  EXPECT_EQ(g.GetNodes("link").size(), 4u);  // closed ring
  EXPECT_EQ(g.GetAttribute("strokeColor"), std::string("-65536"));  // red, as the projector writes it
  CotEvent chat = BuildChat(b, "onemind-client", "", "hello", now);
  CotInfo ci;
  EXPECT_TRUE(ParseCot(chat.ToXML(), ci));
  EXPECT_TRUE(ci.is_chat);
  EXPECT_EQ(ci.chat_room, std::string("All Chat Rooms"));

  CotBuild bad = b;
  bad.uid = "OM.drone.fake";
  EXPECT_FALSE(ValidateOutgoingCot(BuildMarker(bad, "a-u-G", GeoPoint{}, now)).empty());
  EXPECT_FALSE(ValidateOutgoingCot(BuildMarker(b, "a-h-G", GeoPoint{}, now)).empty());
  EXPECT_FALSE(ValidateOutgoingCot(BuildMarker(b, "t-k-d", GeoPoint{}, now)).empty());

  CotSendRequest req;
  req.kind = "marker";
  req.client_id = "test";
  req.client_ref = b.uid;
  req.cot_xml = m.ToXML();
  Json j;
  EXPECT_TRUE(Json::Parse(req.ToJson(), j));
  EXPECT_EQ(j.i64("v"), 1);
  EXPECT_EQ(j["targets"].size(), 2u);
  EXPECT_EQ(j["marking"].str("tlp"), std::string("CLEAR"));
  CotSendReply rep;
  EXPECT_TRUE(CotSendReply::Parse(R"({"v":1,"ok":true,"uid":"onemind-test-1","targets":["mesh"]})", rep));
  EXPECT_TRUE(rep.ok);
  EXPECT_TRUE(CotSendReply::Parse(R"({"v":1,"error":"forbidden","detail":"marking"})", rep));
  EXPECT_FALSE(rep.ok);
}

TEST(milstd2525_vh_port) {
  EXPECT_EQ(MilStd2525::CotTypeToId("a-f-A-M-F"), std::string("f-A-M-F"));
  // "s" + affiliation + status 'p' + function, padded with '-' (icon names as VH_MilStd2525 reads them)
  EXPECT_EQ(MilStd2525::IconFileToId("sfapmf----.png"), std::string("f-A-M-F"));
  EXPECT_EQ(MilStd2525::IconFileToId("shgpucih--h----.png"), std::string("h-G-U-C-I-H"));
  EXPECT_EQ(MilStd2525::ParentId("f-A-M-F"), std::string("f-A-M"));
  SymbolRegistry reg;
  reg.Add("f-A", "air_friend");
  reg.Add("f-A-M", "air_mil_friend");
  reg.Add("f-A-C", "air_civ_friend");
  EXPECT_EQ(reg.Lookup("a-f-A-M-F-Q"), std::string("air_mil_friend"));  // walks up to the parent
  EXPECT_EQ(reg.Lookup("a-f-A-X"), std::string("air_friend"));
  EXPECT_EQ(reg.Lookup("a-n-S"), std::string());
  EXPECT_EQ(reg.ChildrenOf("f-A").size(), 2u);
  EXPECT_TRUE(MilStd2525::ClassFor("a-f-A-M-H-Q", "drone") == SymbolClass::Air);
  EXPECT_TRUE(MilStd2525::ClassFor("a-f-G-U-C", "person") == SymbolClass::Person);
  EXPECT_TRUE(MilStd2525::ClassFor("u-d-f", "zone") == SymbolClass::Shape);
  EXPECT_TRUE(MilStd2525::ClassFor("", "vessel") == SymbolClass::Sea);
}
