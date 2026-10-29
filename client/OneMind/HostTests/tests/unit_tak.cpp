// Unit tests: TAK Protocol v1 codec, streaming client state machine, directed sends,
// enrollment, contacts, chat and channels (features C3, C8).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Codec.h"
#include "OneMindCore/TakContacts.h"
#include "OneMindCore/TakEnroll.h"
#include "OneMindCore/TakProto.h"
#include "OneMindCore/TakStream.h"
#include "TestHarness.h"

#include <deque>

namespace {

const char* kSa =
    "<event version=\"2.0\" uid=\"ANDROID-abc\" type=\"a-f-G-U-C\" how=\"m-g\" time=\"2026-10-29T12:00:00.000Z\" "
    "start=\"2026-10-29T12:00:00.000Z\" stale=\"2026-10-29T12:02:00.000Z\"><point lat=\"30.25\" lon=\"-97.75\" hae=\"150.5\" "
    "ce=\"9.9\" le=\"9999999.0\"/><detail><contact endpoint=\"192.168.1.20:4242:tcp\" callsign=\"ALPHA\"/>"
    "<__group name=\"Cyan\" role=\"Team Member\"/><takv device=\"Pixel\" platform=\"ATAK-CIV\" os=\"34\" version=\"5.6.0\"/>"
    "<track speed=\"1.5\" course=\"90\"/><status battery=\"88\"/><remarks>hi &amp; bye</remarks></detail></event>";

// A transport the test drives by hand.
struct FakeTransport : public om::ITransport {
  Listener* l = nullptr;
  std::vector<std::string> sent;
  std::vector<std::string> opened;
  int closes = 0;
  bool fail_open = false;
  void SetListener(Listener* x) override { l = x; }
  bool Open(const std::string& url) override {
    opened.push_back(url);
    return !fail_open;
  }
  void Send(const std::string& b) override { sent.push_back(b); }
  void Close() override { ++closes; }
  std::string All() const {
    std::string s;
    for (const auto& x : sent) s += x;
    return s;
  }
};

std::string Control(const std::string& type, const std::string& inner) {
  return "<?xml version=\"1.0\"?>\n<event version=\"2.0\" uid=\"protouid\" type=\"" + type +
         "\" time=\"2026-10-29T12:00:00.000Z\" start=\"2026-10-29T12:00:00.000Z\" stale=\"2026-10-29T12:01:00.000Z\" "
         "how=\"m-g\"><point lat=\"0.0\" lon=\"0.0\" hae=\"0.0\" ce=\"999999\" le=\"999999\"/><detail>" +
         inner + "</detail></event>";
}

om::TakSelf Self() {
  om::TakSelf s;
  s.uid = "ONEMIND-test";
  s.callsign = "OM-TEST";
  s.point = om::GeoPoint{30.2, -97.7, 100};
  return s;
}

}  // namespace

TEST(TakProtoVarint) {
  std::string b;
  om::PutVarint(b, 278);
  EXPECT_EQ(om::HexLower(reinterpret_cast<const uint8_t*>(b.data()), b.size()), std::string("9602"));  // tak-protocols.md spike
  size_t pos = 0;
  uint64_t v = 0;
  EXPECT_TRUE(om::GetVarint(b.data(), b.size(), pos, v));
  EXPECT_EQ(v, 278u);
  std::string big(11, static_cast<char>(0xff));
  pos = 0;
  EXPECT_FALSE(om::GetVarint(big.data(), big.size(), pos, v));
}

TEST(TakProtoRoundTripTypedAndXml) {
  std::string payload, err;
  EXPECT_TRUE(om::CotXmlToTakPayload(kSa, payload, &err));
  om::TakDecoded d;
  EXPECT_TRUE(om::DecodeTakPayload(payload, d, &err));
  EXPECT_TRUE(d.has_event);
  om::CotInfo a, b;
  EXPECT_TRUE(om::ParseCot(kSa, a));
  EXPECT_TRUE(om::ParseCot(d.xml, b));
  EXPECT_EQ(b.uid, a.uid);
  EXPECT_EQ(b.type, a.type);
  EXPECT_EQ(b.time, a.time);
  EXPECT_EQ(b.stale, a.stale);
  EXPECT_NEAR(b.point.lat, 30.25, 1e-12);
  EXPECT_NEAR(b.point.hae, 150.5, 1e-12);
  EXPECT_NEAR(b.ce, 9.9, 1e-12);
  EXPECT_EQ(b.callsign, std::string("ALPHA"));
  EXPECT_EQ(b.endpoint, std::string("192.168.1.20:4242:tcp"));
  EXPECT_EQ(b.group_name, std::string("Cyan"));
  EXPECT_EQ(b.takv_platform, std::string("ATAK-CIV"));
  EXPECT_NEAR(b.speed, 1.5, 1e-12);
  EXPECT_EQ(b.battery, 88);
  EXPECT_EQ(b.remarks, std::string("hi & bye"));
  // Typed fields were used: the xmlDetail holds only <remarks>.
  EXPECT_TRUE(payload.find("<contact") == std::string::npos);
  EXPECT_TRUE(payload.find("<remarks>") != std::string::npos);
}

TEST(TakProtoWholeElementRule) {
  // Extra attribute on contact, two __group elements, empty required takv field: all stay XML.
  std::string xml =
      "<event version=\"2.0\" uid=\"x\" type=\"a-f-G\" how=\"h-e\" time=\"2026-10-29T12:00:00Z\" start=\"2026-10-29T12:00:00Z\" "
      "stale=\"2026-10-29T12:01:00Z\"><point lat=\"1\" lon=\"2\" hae=\"3\" ce=\"4\" le=\"5\"/><detail>"
      "<contact callsign=\"A\" phone=\"123\"/><__group name=\"Red\" role=\"A\"/><__group name=\"Blue\" role=\"B\"/>"
      "<takv device=\"\" platform=\"p\" os=\"o\" version=\"v\"/><track speed=\"x\" course=\"1\"/></detail></event>";
  std::string payload, err;
  EXPECT_TRUE(om::CotXmlToTakPayload(xml, payload, &err));
  EXPECT_TRUE(payload.find("phone=\"123\"") != std::string::npos);
  EXPECT_TRUE(payload.find("name=\"Blue\"") != std::string::npos);
  EXPECT_TRUE(payload.find("<takv") != std::string::npos);
  EXPECT_TRUE(payload.find("speed=\"x\"") != std::string::npos);
  om::TakDecoded d;
  EXPECT_TRUE(om::DecodeTakPayload(payload, d, &err));
  EXPECT_TRUE(d.xml.find("phone=\"123\"") != std::string::npos);
}

TEST(TakProtoXmlDetailWinsOverTyped) {
  // Receiver rule 5: data in xmlDetail is kept, the duplicate typed message ignored.
  std::string detail;
  std::string contact;
  om::PutVarint(contact, (2 << 3) | 2);
  om::PutVarint(contact, 5);
  contact += "TYPED";
  std::string xmld = "<contact callsign=\"XML\"/>";
  std::string det;
  om::PutVarint(det, (1 << 3) | 2);
  om::PutVarint(det, xmld.size());
  det += xmld;
  om::PutVarint(det, (2 << 3) | 2);
  om::PutVarint(det, contact.size());
  det += contact;
  std::string ev;
  om::PutVarint(ev, (5 << 3) | 2);
  om::PutVarint(ev, 1);
  ev += "u";
  om::PutVarint(ev, (15 << 3) | 2);
  om::PutVarint(ev, det.size());
  ev += det;
  std::string msg;
  om::PutVarint(msg, (2 << 3) | 2);
  om::PutVarint(msg, ev.size());
  msg += ev;
  om::TakDecoded d;
  std::string err;
  EXPECT_TRUE(om::DecodeTakPayload(msg, d, &err));
  EXPECT_TRUE(d.xml.find("callsign=\"XML\"") != std::string::npos);
  EXPECT_TRUE(d.xml.find("TYPED") == std::string::npos);
}

TEST(TakProtoControlAndFraming) {
  om::TakControlMsg c;
  c.min_version = 1;
  c.max_version = 1;
  c.contact_uid = "sim-x1";
  std::string p = om::TakControlPayload(c);
  // Same bytes as takproto's TakControl in the M0 spike (tak-protocols.md: bf01bf0a0c0801100118...).
  EXPECT_EQ(om::HexLower(reinterpret_cast<const uint8_t*>(p.data()), p.size()), std::string("0a0c080110011a0673696d2d7831"));
  om::TakDecoded d;
  EXPECT_TRUE(om::DecodeTakMeshMessage(om::FrameTakMesh(p), d));
  EXPECT_TRUE(d.has_control && !d.has_event);
  EXPECT_EQ(d.control.contact_uid, std::string("sim-x1"));
  std::string f = om::FrameTakStream(std::string(278, 'a'));
  EXPECT_EQ(om::HexLower(reinterpret_cast<const uint8_t*>(f.data()), 3), std::string("bf9602"));
  EXPECT_FALSE(om::DecodeTakMeshMessage(std::string("\xbf\x02\xbf", 3), d));  // version 2 refused
  EXPECT_FALSE(om::DecodeTakPayload(std::string("\x12\x50", 2), d));          // truncated
  EXPECT_TRUE(om::DecodeTakMeshMessage(kSa, d));                               // legacy XML
}

TEST(TakStreamXmlFramingPingsAndSelf) {
  auto t = std::make_shared<FakeTransport>();
  om::TakStreamOptions o;
  o.url = "tcp://127.0.0.1:1";
  o.sa_interval_ms = 0;
  om::TakStreamClient c(t, o, Self());
  std::vector<std::string> got;
  c.OnEvent([&](const std::string&, const om::CotInfo& i) { got.push_back(i.uid); });
  c.Start(1000);
  c.Poll(1000);
  EXPECT_EQ(t->opened.size(), 1u);
  t->l->OnTransportOpen();
  c.Poll(1001);
  EXPECT_TRUE(c.connected());
  // Own SA first, with declaration, no newline after </event>.
  EXPECT_TRUE(t->All().find("<?xml") == 0);
  EXPECT_TRUE(t->All().find("uid=\"ONEMIND-test\"") != std::string::npos);
  EXPECT_TRUE(t->All().find("endpoint=\"*:-1:stcp\"") != std::string::npos);
  EXPECT_TRUE(t->All().back() == '>');
  // Two events split across reads, with junk between them.
  std::string two = std::string("<?xml version=\"1.0\"?>\n") + kSa + "\n  " + kSa;
  t->l->OnTransportData(two.data(), 100);
  c.Poll(1002);
  EXPECT_EQ(got.size(), 0u);
  t->l->OnTransportData(two.data() + 100, two.size() - 100);
  c.Poll(1003);
  EXPECT_EQ(got.size(), 2u);
  // Ping after 15 s idle, repeated every 4.5 s, reset at 25 s.
  size_t before = t->sent.size();
  c.Poll(1003 + 15000);
  EXPECT_EQ(t->sent.size(), before + 1);
  EXPECT_TRUE(t->sent.back().find("t-x-c-t") != std::string::npos);
  EXPECT_TRUE(t->sent.back().find("ONEMIND-test-ping") != std::string::npos);
  c.Poll(1003 + 16000);
  EXPECT_EQ(t->sent.size(), before + 1);
  c.Poll(1003 + 19600);
  EXPECT_EQ(t->sent.size(), before + 2);
  EXPECT_EQ(c.stats().pings_out, 2u);
  c.Poll(1003 + 25001);
  EXPECT_FALSE(c.connected());
  EXPECT_EQ(t->closes, 1);
  // Stream control from the server is never delivered as an event.
  EXPECT_EQ(got.size(), 2u);
}

TEST(TakStreamNegotiatesV1) {
  auto t = std::make_shared<FakeTransport>();
  om::TakStreamOptions o;
  o.url = "tls://x:8089";
  o.sa_interval_ms = 0;
  om::TakStreamClient c(t, o, Self());
  std::vector<std::string> got;
  c.OnEvent([&](const std::string&, const om::CotInfo& i) { got.push_back(i.uid); });
  c.Start(0);
  c.Poll(0);
  t->l->OnTransportOpen();
  c.Poll(1);
  std::string offer = Control("t-x-takp-v",
                              "<TakControl><TakProtocolSupport version=\"1\"/><TakServerVersionInfo serverVersion=\"5.8-RELEASE-69\"/></TakControl>");
  t->l->OnTransportData(offer.data(), offer.size());
  c.Poll(2);
  EXPECT_EQ(static_cast<int>(c.mode()), static_cast<int>(om::TakStreamClient::Mode::Negotiating));
  EXPECT_TRUE(t->sent.back().find("t-x-takp-q") != std::string::npos);
  EXPECT_TRUE(t->sent.back().find("<TakRequest version=\"1\"/>") != std::string::npos);
  EXPECT_EQ(c.server_version(), std::string("5.8-RELEASE-69"));
  // While negotiating, sends are queued (README step 6: no XML from the client).
  om::CotBuild b;
  b.uid = "onemind-q1";
  b.callsign = "Q";
  size_t n = t->sent.size();
  EXPECT_EQ(c.Send(om::BuildMarker(b, "a-u-G", om::GeoPoint{1, 2, 3}, 2)), std::string());
  EXPECT_EQ(t->sent.size(), n);
  // The server's answer and the first v1 frame arrive in the same read.
  std::string resp = Control("t-x-takp-r", "<TakControl><TakResponse status=\"true\"/></TakControl>");
  std::string payload;
  om::CotXmlToTakPayload(kSa, payload);
  std::string both = resp + om::FrameTakStream(payload);
  t->l->OnTransportData(both.data(), both.size());
  c.Poll(3);
  EXPECT_EQ(static_cast<int>(c.mode()), static_cast<int>(om::TakStreamClient::Mode::Proto));
  EXPECT_EQ(got.size(), 1u);
  // The queued marker went out as a v1 frame.
  EXPECT_EQ(t->sent.size(), n + 1);
  EXPECT_EQ(static_cast<unsigned char>(t->sent.back()[0]), 0xbfu);
  om::TakDecoded d;
  size_t pos = 1;
  uint64_t len;
  om::GetVarint(t->sent.back().data(), t->sent.back().size(), pos, len);
  EXPECT_TRUE(om::DecodeTakPayload(t->sent.back().substr(pos), d));
  EXPECT_TRUE(d.xml.find("onemind-q1") != std::string::npos);
  EXPECT_EQ(c.stats().negotiated_v1, 1u);
}

TEST(TakStreamDeclineAndTimeout) {
  auto t = std::make_shared<FakeTransport>();
  om::TakStreamOptions o;
  o.url = "tls://x:8089";
  o.negotiation_timeout_ms = 1000;
  o.reconnect_min_ms = 10;
  o.reconnect_max_ms = 10;
  o.sa_interval_ms = 0;
  om::TakStreamClient c(t, o, Self());
  c.Start(0);
  c.Poll(0);
  t->l->OnTransportOpen();
  c.Poll(1);
  std::string offer = Control("t-x-takp-v", "<TakControl><TakProtocolSupport version=\"1\"/></TakControl>");
  std::string no = Control("t-x-takp-r", "<TakControl><TakResponse status=\"false\"/></TakControl>");
  t->l->OnTransportData(offer.data(), offer.size());
  c.Poll(2);
  t->l->OnTransportData(no.data(), no.size());
  c.Poll(3);
  EXPECT_EQ(static_cast<int>(c.mode()), static_cast<int>(om::TakStreamClient::Mode::Xml));
  EXPECT_EQ(c.stats().declined, 1u);
  // A second offer in the same session is not taken up after a decline.
  t->l->OnTransportData(offer.data(), offer.size());
  c.Poll(4);
  EXPECT_EQ(c.stats().negotiations, 1u);
  // New session: offer, no answer -> disconnect at the timeout, later sessions stay on XML.
  t->l->OnTransportClosed("drop");
  c.Poll(5);
  c.Poll(100);
  t->l->OnTransportOpen();
  c.Poll(101);
  t->l->OnTransportData(offer.data(), offer.size());
  c.Poll(102);
  EXPECT_EQ(c.stats().negotiations, 2u);
  c.Poll(102 + 999);
  EXPECT_TRUE(c.connected());
  t->l->OnTransportData(offer.data(), 10);  // keep-alive bytes do not stop the timer
  c.Poll(102 + 1001);
  EXPECT_FALSE(c.connected());
  c.Poll(2000);
  t->l->OnTransportOpen();
  c.Poll(2001);
  t->l->OnTransportData(offer.data(), offer.size());
  c.Poll(2002);
  EXPECT_EQ(c.stats().negotiations, 2u);
  EXPECT_EQ(static_cast<int>(c.mode()), static_cast<int>(om::TakStreamClient::Mode::Xml));
}

TEST(TakStreamQueueRulesAndRefusals) {
  auto t = std::make_shared<FakeTransport>();
  om::TakStreamOptions o;
  o.url = "tcp://x:1";
  o.max_queue = 2;
  o.max_queue_age_ms = 1000;
  o.sa_interval_ms = 0;
  om::TakStreamClient c(t, o, Self());
  c.Start(0);
  om::CotBuild b;
  b.callsign = "Q";
  for (int i = 0; i < 3; ++i) {
    b.uid = "onemind-q" + std::to_string(i);
    c.Send(om::BuildMarker(b, "a-u-G", om::GeoPoint{1, 2, 3}, 0));
  }
  EXPECT_EQ(c.stats().queue_dropped, 1u);  // bounded: oldest dropped
  b.uid = "onemind-h";
  EXPECT_FALSE(c.Send(om::BuildMarker(b, "a-h-G", om::GeoPoint{1, 2, 3}, 0)).empty());  // hostile refused
  c.Poll(5000);  // connects later than max_queue_age: queued items are dropped, not sent late
  t->l->OnTransportOpen();
  c.Poll(5001);
  EXPECT_EQ(c.stats().queue_dropped, 3u);
  EXPECT_TRUE(t->All().find("onemind-q") == std::string::npos);
}

TEST(TakStreamBadV1FrameAndOversize) {
  auto t = std::make_shared<FakeTransport>();
  om::TakStreamOptions o;
  o.url = "tcp://x:1";
  o.max_event_bytes = 512;
  o.sa_interval_ms = 0;
  om::TakStreamClient c(t, o, Self());
  c.Start(0);
  c.Poll(0);
  t->l->OnTransportOpen();
  c.Poll(1);
  std::string junk(600, 'x');
  t->l->OnTransportData(junk.data(), junk.size());
  c.Poll(2);
  EXPECT_FALSE(c.connected());
  EXPECT_EQ(c.stats().protocol_errors, 1u);
}

TEST(TakDirectEncodingAndUrls) {
  EXPECT_EQ(om::DirectedUrl("192.168.1.20:4242:tcp"), std::string("tcp://192.168.1.20:4242"));
  EXPECT_EQ(om::DirectedUrl("127.0.0.1:4242"), std::string("tcp://127.0.0.1:4242"));
  EXPECT_EQ(om::DirectedUrl("*:-1:stcp"), std::string());
  EXPECT_EQ(om::DirectedUrl("10.0.0.1:6969:udp"), std::string());
  std::string out;
  EXPECT_TRUE(om::EncodeDirected(kSa, true, out));
  EXPECT_EQ(om::HexLower(reinterpret_cast<const uint8_t*>(out.data()), 3), std::string("bf01bf"));
  EXPECT_TRUE(om::EncodeDirected(kSa, false, out));
  EXPECT_TRUE(out.find("<?xml") == 0);
  // A directed send opens, sends once and closes.
  auto t = std::make_shared<FakeTransport>();
  om::TakDirect d([t] { return t; });
  std::string result = "pending";
  om::CotBuild b;
  b.uid = "onemind-d1";
  b.callsign = "D";
  d.Send("127.0.0.1:4242:tcp", om::BuildMarker(b, "a-u-G", om::GeoPoint{1, 2, 3}, 0), true,
         [&](const std::string& e) { result = e; }, 0);
  EXPECT_EQ(t->opened.back(), std::string("tcp://127.0.0.1:4242"));
  t->l->OnTransportOpen();
  d.Poll(1);
  EXPECT_EQ(result, std::string());
  EXPECT_EQ(t->sent.size(), 1u);
  EXPECT_EQ(t->closes, 1);
  d.Send("*:-1:stcp", om::BuildMarker(b, "a-u-G", om::GeoPoint{1, 2, 3}, 0), true, [&](const std::string& e) { result = e; }, 0);
  EXPECT_FALSE(result.empty());
}

TEST(TakSelfSaIsAContactCard) {
  om::TakSelf s = Self();
  s.endpoint = "192.168.1.10:4242:tcp";
  s.battery = 77;
  om::CotEvent ev = om::BuildSelfSa(s, 1793275200000);
  om::CotInfo i;
  EXPECT_TRUE(om::ParseCot(ev.ToXML(), i));
  EXPECT_EQ(i.type, std::string("a-f-G-U-C"));
  EXPECT_EQ(i.callsign, std::string("OM-TEST"));
  EXPECT_EQ(i.endpoint, std::string("192.168.1.10:4242:tcp"));
  EXPECT_EQ(i.group_name, std::string("Cyan"));
  EXPECT_TRUE(i.has_takv);
  EXPECT_EQ(i.battery, 77);
  EXPECT_EQ(i.opex, std::string("s"));
  EXPECT_EQ(om::ValidateOutgoingCot(ev), std::string());
  EXPECT_EQ(ev.GetAttribute("detail\tuid\tDroid"), std::string("OM-TEST"));
}

// --- enrollment -------------------------------------------------------------------------------

namespace {
const char* kCertBody =
    "MIIBszCCAVmgAwIBAgIUXq0d1XgV8h7m0Y1t0d5Z7oQ2gXcwCgYIKoZIzj0EAwIwEjEQMA4GA1UEAwwHdGVzdC1jYTAeFw0yNjEwMjkx"
    "MjAwMDBaFw0yNzEwMjkxMjAwMDBaMBQxEjAQBgNVBAMMCW9uZW1pbmQtMTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABA==";

struct FakePlatform : public om::IEnrollPlatform {
  std::vector<std::string> urls, bodies;
  std::vector<std::vector<std::pair<std::string, std::string>>> hdrs, subjects;
  std::deque<om::HttpResponse> replies;
  bool csr_ok = true;
  void Https(const std::string& method, const std::string& url, const std::vector<std::pair<std::string, std::string>>& h,
             const std::string& body, HttpCallback cb) override {
    urls.push_back(method + " " + url);
    hdrs.push_back(h);
    bodies.push_back(body);
    om::HttpResponse r = replies.front();
    replies.pop_front();
    cb(r);
  }
  bool MakeKeyAndCsr(const std::vector<std::pair<std::string, std::string>>& subject, int, std::string& key_ref,
                     std::string& csr, std::string& err) override {
    subjects.push_back(subject);
    if (!csr_ok) {
      err = "no";
      return false;
    }
    key_ref = "k1";
    csr = "-----BEGIN CERTIFICATE REQUEST-----\nAAAA\nBBBB\n-----END CERTIFICATE REQUEST-----\n";
    return true;
  }
};
}  // namespace

TEST(TakEnrollParsers) {
  std::vector<std::pair<std::string, std::string>> e;
  std::string cfg =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><ns2:certificateConfig xmlns:ns2=\"com.bbn.marti.config\">"
      "<nameEntries><nameEntry name=\"O\" value=\"OneMind\"/><nameEntry name=\"OU\" value=\"TAK\"/></nameEntries>"
      "</ns2:certificateConfig>";
  EXPECT_TRUE(om::ParseCertificateConfig(cfg, e));
  EXPECT_EQ(e.size(), 2u);
  EXPECT_EQ(e[1].first, std::string("OU"));
  EXPECT_FALSE(om::ParseCertificateConfig("<other/>", e));
  EXPECT_EQ(om::CsrBodyFromPem("-----BEGIN CERTIFICATE REQUEST-----\nAAAA\r\nBBBB\n-----END CERTIFICATE REQUEST-----\n"),
            std::string("AAAABBBB"));
  om::EnrollResult r;
  std::string json = std::string("{\n  \"signedCert\" : \"") + kCertBody + "\",\n  \"ca0\" : \"" + kCertBody + "\",\n  \"ca1\" : \"" +
                     kCertBody + "\"\n}";
  EXPECT_TRUE(om::ParseSignClientV2(json, "application/json", r));
  EXPECT_EQ(r.ca_pems.size(), 2u);
  EXPECT_TRUE(r.cert_pem.find("-----BEGIN CERTIFICATE-----\n") == 0);
  std::string xml = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?><enrollment><signedCert>-----BEGIN CERTIFICATE-----\n") +
                    kCertBody + "\n-----END CERTIFICATE-----\n</signedCert><ca>" + kCertBody + "</ca></enrollment>";
  EXPECT_TRUE(om::ParseSignClientV2(xml, "application/xml", r));
  EXPECT_EQ(r.ca_pems.size(), 1u);
  EXPECT_FALSE(om::ParseSignClientV2("{\"ca0\":\"x\"}", "application/json", r));
  EXPECT_FALSE(om::ParseSignClientV2("{\"signedCert\":\"!!notbase64!!\"}", "", r));
  EXPECT_EQ(om::BasicAuthHeader("u", "p"), std::string("Basic dTpw"));  // RFC 7617 example form
  EXPECT_EQ(om::UrlEncode("OneMind 0.4.0&x"), std::string("OneMind%200.4.0%26x"));
}

TEST(TakEnrollFlow) {
  FakePlatform p;
  om::HttpResponse cfg;
  cfg.status = 200;
  cfg.body = "<certificateConfig><nameEntries><nameEntry name=\"O\" value=\"TAK\"/></nameEntries></certificateConfig>";
  om::HttpResponse signed_r;
  signed_r.status = 200;
  signed_r.content_type = "application/json";
  signed_r.body = std::string("{\"signedCert\":\"") + kCertBody + "\",\"ca0\":\"" + kCertBody + "\"}";
  p.replies = {cfg, signed_r};
  om::EnrollConfig c;
  c.host = "tak.example";
  c.user = "omclient";
  c.password = "pw-" + om::NewToken();  // random per run; never a literal credential
  c.client_uid = "ONEMIND-mac";
  om::TakEnrollment e(p, c);
  om::EnrollResult got;
  int calls = 0;
  e.Start([&](const om::EnrollResult& r) {
    got = r;
    ++calls;
  });
  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(got.ok);
  EXPECT_EQ(got.key_ref, std::string("k1"));
  EXPECT_EQ(p.urls[0], std::string("GET https://tak.example:8446/Marti/api/tls/config"));
  EXPECT_EQ(p.urls[1],
            std::string("POST https://tak.example:8446/Marti/api/tls/signClient/v2?clientUid=ONEMIND-mac&version=OneMind%200.4.0"));
  EXPECT_EQ(p.bodies[1], std::string("AAAABBBB"));  // CSR without banners, as Commo sends it
  EXPECT_EQ(p.subjects[0][0].first, std::string("CN"));
  EXPECT_EQ(p.subjects[0][0].second, std::string("omclient"));
  EXPECT_EQ(p.subjects[0][1].second, std::string("TAK"));
  EXPECT_EQ(p.hdrs[0][0].second, om::BasicAuthHeader("omclient", c.password));
  // Failures surface once, with the step named.
  FakePlatform p2;
  om::HttpResponse denied;
  denied.status = 401;
  p2.replies = {denied};
  om::TakEnrollment e2(p2, c);
  e2.Start([&](const om::EnrollResult& r) { got = r; });
  EXPECT_FALSE(got.ok);
  EXPECT_TRUE(got.error.find("rejected") != std::string::npos);
}

// --- contacts, chat, channels (C8) ------------------------------------------------------------

TEST(TakContactsAndChannels) {
  om::TakContacts k("ONEMIND-test", "OM-TEST");
  EXPECT_EQ(k.Ingest(kSa, 1000, "stream"), std::string("contact"));
  std::string bravo = kSa;
  bravo.replace(bravo.find("ANDROID-abc"), 11, "ANDROID-bbb");
  bravo.replace(bravo.find("ALPHA"), 5, "BRAVO");
  bravo.replace(bravo.find("Cyan"), 4, "Red!");
  EXPECT_EQ(k.Ingest(bravo, 1000, "mesh"), std::string("contact"));
  EXPECT_EQ(k.Visible().size(), 2u);
  EXPECT_EQ(k.Visible()[0]->callsign, std::string("ALPHA"));
  k.SetActiveChannels({"Red!"});
  EXPECT_EQ(k.Visible().size(), 1u);
  EXPECT_EQ(k.Visible()[0]->callsign, std::string("BRAVO"));
  EXPECT_EQ(k.Channels().size(), 2u);
  std::string body = k.ActiveGroupsRequestBody({"Cyan", "Red!"});
  EXPECT_TRUE(body.find("{\"active\":false,\"direction\":\"IN\",\"name\":\"Cyan\"}") != std::string::npos);
  k.SetActiveChannels({});
  // Own SA echoed back is ignored; stale contacts expire.
  om::TakSelf s = Self();
  EXPECT_EQ(k.Ingest(om::BuildSelfSa(s, 1000).ToXML(), 1000, "stream"), std::string());
  om::TimeMs stale = 0;
  om::ParseCotTime("2026-10-29T12:02:00.000Z", stale);
  EXPECT_EQ(k.Expire(stale - 1), 0u);
  EXPECT_EQ(k.Expire(stale), 2u);
}

TEST(TakChatBothWays) {
  om::TakContacts k("ONEMIND-test", "OM-TEST");
  k.Ingest(kSa, 1000, "stream");
  // Outgoing direct chat to ALPHA: room = callsign, id = uid, marti/dest for the server.
  om::CotEvent out = k.BuildChatTo("ANDROID-abc", "on my way", 2000, true, "m1");
  EXPECT_EQ(out.GetAttribute("detail\t__chat\tid"), std::string("ANDROID-abc"));
  EXPECT_EQ(out.GetAttribute("detail\t__chat\tchatroom"), std::string("ALPHA"));
  EXPECT_EQ(out.GetAttribute("detail\t__chat\tchatgrp\tuid0"), std::string("ONEMIND-test"));
  EXPECT_EQ(out.GetAttribute("detail\t__chat\tchatgrp\tuid1"), std::string("ANDROID-abc"));
  EXPECT_EQ(out.GetAttribute("detail\tmarti\tdest\tcallsign"), std::string("ALPHA"));
  EXPECT_EQ(out.GetAttribute("uid"), std::string("GeoChat.ONEMIND-test.m1"));
  EXPECT_EQ(om::ValidateOutgoingCot(out), std::string());
  // ALPHA answers directly: the conversation is ALPHA's uid.
  om::CotBuild b;
  b.uid = "GeoChat.ANDROID-abc.ONEMIND-test.r1";
  b.callsign = "ALPHA";
  b.simulated = false;
  om::CotEvent reply = om::BuildChat(b, "ANDROID-abc", "OM-TEST", "copy", 3000);
  reply.SetAttribute("detail\t__chat\tid", "ONEMIND-test");
  EXPECT_EQ(k.Ingest(reply.ToXML(), 3000, "stream"), std::string("chat"));
  const om::ChatConversation* cv = k.Conversation("ANDROID-abc");
  EXPECT_TRUE(cv != nullptr);
  if (cv) {
    EXPECT_EQ(cv->messages.size(), 2u);
    EXPECT_EQ(cv->messages[1].text, std::string("copy"));
    EXPECT_TRUE(cv->direct);
    EXPECT_EQ(cv->unread, 1u);
  }
  // The same message over a second path is not shown twice.
  EXPECT_EQ(k.Ingest(reply.ToXML(), 3001, "mesh"), std::string("chat"));
  EXPECT_EQ(k.Conversation("ANDROID-abc")->messages.size(), 2u);
  // All Chat Rooms.
  om::CotBuild g;
  g.uid = "GeoChat.ANDROID-abc.All Chat Rooms.g1";
  g.callsign = "ALPHA";
  EXPECT_EQ(k.Ingest(om::BuildChat(g, "ANDROID-abc", "", "all hands", 4000).ToXML(), 4000, "mesh"), std::string("chat"));
  EXPECT_EQ(k.Conversation(om::TakContacts::kAllChatRooms)->messages.size(), 1u);
  om::CotEvent all = k.BuildChatTo(om::TakContacts::kAllChatRooms, "hello all", 5000, true, "m2");
  EXPECT_FALSE(all.HasNode("detail\tmarti"));
  EXPECT_EQ(k.Conversation(om::TakContacts::kAllChatRooms)->messages.size(), 2u);
  k.MarkRead("ANDROID-abc");
  EXPECT_EQ(k.Conversation("ANDROID-abc")->unread, 0u);
}

TEST(HttpExchangeOverTransport) {
  om::HttpResponse r;
  EXPECT_TRUE(om::ParseHttpResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n"
                                    "4\r\n{\"a\"\r\n3\r\n:1}\r\n0\r\n\r\n",
                                    r));
  EXPECT_EQ(r.status, 200);
  EXPECT_EQ(r.body, std::string("{\"a\":1}"));
  EXPECT_EQ(r.content_type, std::string("application/json"));
  EXPECT_FALSE(om::ParseHttpResponse("garbage", r));
  auto t = std::make_shared<FakeTransport>();
  {
    om::HttpExchange ex(t);
    EXPECT_TRUE(ex.Start("POST", "https://tak.example:8446/Marti/api/tls/signClient/v2?clientUid=u", {{"Accept", "application/json"}},
                         "CSR", 0));
    EXPECT_EQ(t->opened.back(), std::string("tls://tak.example:8446"));
    om::HttpResponse out;
    EXPECT_FALSE(ex.Poll(1, out));
    t->l->OnTransportOpen();
    EXPECT_FALSE(ex.Poll(2, out));
    EXPECT_TRUE(t->All().find("POST /Marti/api/tls/signClient/v2?clientUid=u HTTP/1.1\r\nHost: tak.example:8446\r\n") == 0);
    EXPECT_TRUE(t->All().find("Content-Length: 3\r\n") != std::string::npos);
    EXPECT_TRUE(t->All().find("\r\n\r\nCSR") != std::string::npos);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhel";
    t->l->OnTransportData(resp.data(), resp.size());
    EXPECT_FALSE(ex.Poll(3, out));
    t->l->OnTransportData("lo", 2);
    EXPECT_TRUE(ex.Poll(4, out));  // complete by Content-Length, before the server closes
    EXPECT_EQ(out.body, std::string("hello"));
  }
  auto t2 = std::make_shared<FakeTransport>();
  om::HttpExchange ex2(t2);
  EXPECT_TRUE(ex2.Start("GET", "https://h:1/x", {}, "", 0, 100));
  om::HttpResponse out2;
  EXPECT_TRUE(ex2.Poll(101, out2));
  EXPECT_EQ(out2.error, std::string("timeout"));
  EXPECT_FALSE(ex2.Start("GET", "http://h:1/x", {}, "", 0));
}
