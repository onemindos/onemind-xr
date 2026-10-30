// Headless driver for the client's TAK features (C3, C8, B11) on the host: the same core
// classes the Unreal plugins use, over the host TLS/TCP transport. Prints one JSON object per
// line so the end-to-end scripts can check behaviour.
//
//   onemind_tak stream --url tls://127.0.0.1:8089 --cert C --key K --ca CA --uid U --callsign CS
//                      [--protocol auto|xml] [--chat-to UID|"All Chat Rooms"] [--text T]
//                      [--shapes] [--channel NAME] [--seconds N] [--insecure]
//   onemind_tak direct --to HOST:PORT:tcp --uid U --callsign CS [--v1] [--listen PORT]
//                      [--chat-to UID] [--text T] [--seconds N]
//   onemind_tak enroll --host H --port P --user USER --uid U --ca CA --out DIR
//                      (password from $TAK_ENROLL_PASSWORD; never on the command line)
//   onemind_tak task   --ws ws://127.0.0.1:9222 --asset sim-x1 --lat L --lon L --hae H
//                      [--class drone] [--confirm] [--reject REASON] [--confirm-after-ms N]
//                      [--seconds N]   (NATS user/password from $OM_NATS_USER / $OM_NATS_PASSWORD)
//                      --confirm stands in for the human pressing "Confirm" in the dialog.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/AssetTasking.h"
#include "OneMindCore/Codec.h"
#include "OneMindCore/Drawing.h"
#include "OneMindCore/Json.h"
#include "OneMindCore/TakContacts.h"
#include "OneMindCore/TakEnroll.h"
#include "OneMindCore/TakProto.h"
#include "OneMindCore/TakStream.h"
#include "PosixTlsTransport.h"
#include "PosixWsTransport.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace {

std::map<std::string, std::string> Args(int argc, char** argv, int from) {
  std::map<std::string, std::string> a;
  for (int i = from; i < argc; ++i) {
    std::string k = argv[i];
    if (k.rfind("--", 0) != 0) continue;
    if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) a[k.substr(2)] = argv[++i];
    else a[k.substr(2)] = "1";
  }
  return a;
}

std::string ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

bool WriteFile(const std::string& path, const std::string& data, mode_t mode) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  f << data;
  f.close();
  return ::chmod(path.c_str(), mode) == 0;
}

void Out(om::Json j) {
  std::printf("%s\n", j.Dump().c_str());
  std::fflush(stdout);
}

om::Json Obj(const char* ev) {
  om::Json j = om::Json::MakeObject();
  j.set("ev", ev);
  return j;
}

void SleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// Every B11 shape, around a centre near the sim drone's home (Austin), for the round trip.
std::vector<om::CotEvent> AllShapes(const std::string& uid, om::TimeMs now) {
  om::GeoPoint c{30.2672, -97.7431, 9999999.0};
  om::ShapeStyle st;
  st.stroke_argb = 0xffffff00u;
  st.fill_argb = 0x40ffff00u;
  std::vector<om::CotEvent> v;
  auto B = [&](const std::string& s, const std::string& label) {
    om::CotBuild b;
    b.uid = uid + "-" + s;
    b.callsign = label;
    return b;
  };
  v.push_back(om::BuildPoint(B("point", "Point 1"), c, now));
  v.push_back(om::BuildPolyline(B("line", "Line 1"), {c, om::GeoMath::Destination(c, 45, 500), om::GeoMath::Destination(c, 90, 900)}, st, now));
  v.push_back(om::BuildPolygon(B("polygon", "Polygon 1"), {om::GeoMath::Destination(c, 0, 400), om::GeoMath::Destination(c, 120, 400), om::GeoMath::Destination(c, 240, 400)}, st, now));
  v.push_back(om::BuildRectangle(B("rect", "Rectangle 1"), om::GeoMath::Destination(c, 315, 300), om::GeoMath::Destination(c, 135, 300), st, now));
  v.push_back(om::BuildCircle(B("circle", "Circle 1"), c, 250, st, now));
  v.push_back(om::BuildSector(B("sector", "Sector 1"), c, 600, 30, 90, st, now));
  for (auto& r : om::BuildRangeRings(B("rings", "Rings"), c, {200, 400, 600}, st, now)) v.push_back(r);
  v.push_back(om::BuildRangeBearing(B("rb", "R&B 1"), c, om::GeoMath::Destination(c, 60, 1000), st, now));
  v.push_back(om::BuildBuffer(B("buffer", "Buffer 1"), {c, om::GeoMath::Destination(c, 90, 800)}, 100, st, now));
  return v;
}

om::Json ShapeJson(const om::Shape& s) {
  om::Json j = om::Json::MakeObject();
  j.set("uid", s.uid);
  j.set("type", s.cot_type);
  j.set("label", s.label);
  j.set("radius_m", s.radius_m);
  j.set("range_m", s.range_m);
  j.set("bearing_deg", s.bearing_deg);
  om::Json pts = om::Json::MakeArray();
  for (const auto& p : s.points) {
    om::Json q = om::Json::MakeArray();
    q.push_back(p.lat);
    q.push_back(p.lon);
    pts.push_back(q);
  }
  j.set("points", pts);
  om::Json c = om::Json::MakeArray();
  c.push_back(s.center.lat);
  c.push_back(s.center.lon);
  j.set("center", c);
  return j;
}

int Stream(std::map<std::string, std::string> a) {
  omtls::TlsConfig tls;
  if (a.count("cert")) tls.cert_pem = ReadFile(a["cert"]);
  if (a.count("key")) tls.key_pem = ReadFile(a["key"]);
  if (a.count("ca")) tls.ca_pem = ReadFile(a["ca"]);
  tls.verify_peer = !a.count("insecure");
  auto t = std::make_shared<omhost::PosixTlsTransport>(tls);
  om::TakStreamOptions o;
  o.url = a["url"];
  o.protocol = a.count("protocol") ? a["protocol"] : "auto";
  o.reconnect_min_ms = 200;
  o.reconnect_max_ms = 1000;
  if (a.count("ping-idle-ms")) o.ping_idle_ms = std::atoi(a["ping-idle-ms"].c_str());
  if (a.count("neg-timeout-ms")) o.negotiation_timeout_ms = std::atoi(a["neg-timeout-ms"].c_str());
  om::TakSelf self;
  self.uid = a["uid"];
  self.callsign = a.count("callsign") ? a["callsign"] : "ONEMIND";
  self.point = om::GeoPoint{30.2672, -97.7431, 150};
  om::TakStreamClient sc(t, o, self);
  om::TakContacts contacts(self.uid, self.callsign);
  if (a.count("channel")) contacts.SetActiveChannels({a["channel"]});
  sc.OnState([&](om::TakStreamClient::State s, const std::string& reason) {
    om::Json j = Obj("state");
    j.set("state", static_cast<int>(s));
    j.set("connected", s == om::TakStreamClient::State::Connected);
    j.set("reason", reason);
    j.set("peer_cn", t->peer_cn());
    Out(j);
  });
  sc.OnEvent([&](const std::string& xml, const om::CotInfo& info) {
    om::TimeMs now = om::WallClockMs();
    std::string kind = contacts.Ingest(xml, now, "stream");
    om::Json j = Obj("cot");
    j.set("uid", info.uid);
    j.set("type", info.type);
    j.set("kind", kind);
    j.set("mode", static_cast<int>(sc.mode()));
    if (kind == "chat") {
      j.set("text", info.remarks);
      j.set("from", info.chat_sender);
    }
    om::Shape sh;
    if (om::ParseShape(xml, sh)) j.set("shape", ShapeJson(sh));
    Out(j);
  });
  om::TimeMs start = om::WallClockMs();
  int seconds = a.count("seconds") ? std::atoi(a["seconds"].c_str()) : 10;
  bool chat_sent = false, shapes_sent = false;
  sc.Start(start);
  while (om::WallClockMs() - start < seconds * 1000) {
    om::TimeMs now = om::WallClockMs();
    sc.Poll(now);
    bool ready = sc.connected() && sc.mode() != om::TakStreamClient::Mode::Negotiating &&
                 now - start > (a.count("settle-ms") ? std::atoi(a["settle-ms"].c_str()) : 800);
    if (ready && a.count("chat-to") && !chat_sent) {
      std::string to = a["chat-to"];
      if (to == om::TakContacts::kAllChatRooms || contacts.Find(to)) {
        om::CotEvent ev = contacts.BuildChatTo(to, a.count("text") ? a["text"] : "hello from OneMind", now, true,
                                               om::NewToken());
        std::string err = sc.Send(ev);
        om::Json j = Obj("chat_sent");
        j.set("to", to);
        j.set("error", err);
        Out(j);
        chat_sent = true;
      }
    }
    if (ready && a.count("shapes") && !shapes_sent) {
      for (auto& ev : AllShapes(self.uid, now)) {
        std::string err = sc.Send(ev);
        om::Shape sh;
        om::ParseShape(ev.ToXML(), sh);
        om::Json j = Obj("shape_sent");
        j.set("error", err);
        j.set("shape", ShapeJson(sh));
        Out(j);
      }
      shapes_sent = true;
    }
    SleepMs(5);
  }
  om::Json fin = Obj("final");
  const auto& s = sc.stats();
  fin.set("mode", static_cast<int>(sc.mode()));
  fin.set("events_in", static_cast<int64_t>(s.events_in));
  fin.set("events_out", static_cast<int64_t>(s.events_out));
  fin.set("pings_out", static_cast<int64_t>(s.pings_out));
  fin.set("connects", static_cast<int64_t>(s.connects));
  fin.set("negotiated_v1", static_cast<int64_t>(s.negotiated_v1));
  fin.set("declined", static_cast<int64_t>(s.declined));
  fin.set("server_version", sc.server_version());
  fin.set("last_error", sc.last_error());
  om::Json vis = om::Json::MakeArray();
  for (const auto* c : contacts.Visible()) vis.push_back(c->callsign);
  fin.set("visible_contacts", vis);
  fin.set("all_contacts", static_cast<int64_t>(contacts.size()));
  om::Json chats = om::Json::MakeArray();
  for (const auto& cv : contacts.Conversations())
    for (const auto& m : cv.second.messages) {
      om::Json c = om::Json::MakeObject();
      c.set("conversation", cv.first);
      c.set("text", m.text);
      c.set("outgoing", m.outgoing);
      chats.push_back(c);
    }
  fin.set("chats", chats);
  Out(fin);
  sc.Stop();
  return 0;
}

// Accepts directed TCP messages to this client's own endpoint (what the gateway sends to
// "host:port:tcp" for a Tak-Dest-Uid message) on a background thread.
struct DirectListener {
  int fd = -1;
  std::thread th;
  std::mutex mu;
  std::vector<std::string> msgs;
  bool stop = false;
  bool Start(int port) {
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(fd, 8) != 0) return false;
    th = std::thread([this] {
      while (!stop) {
        pollfd p{fd, POLLIN, 0};
        if (::poll(&p, 1, 50) <= 0) continue;
        int c = ::accept(fd, nullptr, nullptr);
        if (c < 0) continue;
        std::string data;
        char buf[8192];
        ssize_t n;
        while ((n = ::recv(c, buf, sizeof(buf), 0)) > 0 && data.size() < (256u << 10)) data.append(buf, static_cast<size_t>(n));
        ::close(c);
        std::lock_guard<std::mutex> l(mu);
        msgs.push_back(data);
      }
    });
    return true;
  }
  std::vector<std::string> Take() {
    std::lock_guard<std::mutex> l(mu);
    std::vector<std::string> out;
    out.swap(msgs);
    return out;
  }
  ~DirectListener() {
    stop = true;
    if (th.joinable()) th.join();
    if (fd >= 0) ::close(fd);
  }
};

int Direct(std::map<std::string, std::string> a) {
  om::TakDirect direct([] { return std::make_shared<omhost::PosixTlsTransport>(); });
  om::TakSelf self;
  self.uid = a["uid"];
  self.callsign = a.count("callsign") ? a["callsign"] : "ONEMIND";
  self.point = om::GeoPoint{30.2672, -97.7431, 150};
  DirectListener listener;
  if (a.count("listen")) {
    if (!listener.Start(std::atoi(a["listen"].c_str()))) {
      Out(Obj("listen_failed"));
      return 2;
    }
    self.endpoint = "127.0.0.1:" + a["listen"] + ":tcp";
  } else {
    self.endpoint.clear();
  }
  om::TakContacts contacts(self.uid, self.callsign);
  bool v1 = a.count("v1") != 0;
  std::string to = a["to"];
  om::TimeMs start = om::WallClockMs();
  int pending = 0;
  auto send = [&](const om::CotEvent& ev, const char* what) {
    ++pending;
    std::string w = what;
    direct.Send(to, ev, v1, [&pending, w](const std::string& err) {
      --pending;
      om::Json j = Obj("direct_sent");
      j.set("what", w);
      j.set("error", err);
      Out(j);
    }, om::WallClockMs());
  };
  send(om::BuildSelfSa(self, start), "sa");
  if (a.count("chat-to")) {
    om::CotEvent chat = contacts.BuildChatTo(a["chat-to"], a.count("text") ? a["text"] : "hello over the mesh", start,
                                             false, om::NewToken());
    send(chat, "chat");
  }
  if (a.count("shapes"))
    for (auto& ev : AllShapes(self.uid, start)) send(ev, "shape");
  int seconds = a.count("seconds") ? std::atoi(a["seconds"].c_str()) : 3;
  while (om::WallClockMs() - start < seconds * 1000) {
    direct.Poll(om::WallClockMs());
    for (const auto& m : listener.Take()) {
      om::TakDecoded d;
      std::string err;
      om::Json j = Obj("direct_in");
      j.set("v1", om::IsTakProtocol(m));
      if (om::DecodeTakMeshMessage(m, d, &err) && d.has_event) {
        std::string kind = contacts.Ingest(d.xml, om::WallClockMs(), "mesh");
        om::CotInfo info;
        om::ParseCot(d.xml, info);
        j.set("kind", kind);
        j.set("uid", info.uid);
        j.set("type", info.type);
        j.set("text", info.remarks);
      } else {
        j.set("error", err);
      }
      Out(j);
    }
    SleepMs(5);
  }
  om::Json fin = Obj("final");
  fin.set("pending", pending);
  Out(fin);
  return 0;
}

int Enroll(std::map<std::string, std::string> a) {
  const char* pw = std::getenv("TAK_ENROLL_PASSWORD");
  omhost::HostEnrollPlatform platform(ReadFile(a["ca"]), !a.count("insecure"));
  om::EnrollConfig cfg;
  cfg.host = a["host"];
  cfg.port = std::atoi(a["port"].c_str());
  cfg.user = a["user"];
  cfg.password = pw ? pw : "";
  cfg.client_uid = a["uid"];
  om::TakEnrollment e(platform, cfg);
  int rc = 1;
  e.Start([&](const om::EnrollResult& r) {
    om::Json j = Obj("enroll");
    j.set("ok", r.ok);
    j.set("error", r.error);
    j.set("ca_count", static_cast<int64_t>(r.ca_pems.size()));
    if (r.ok) {
      std::string dir = a["out"];
      std::string chain;
      for (const auto& ca : r.ca_pems) chain += ca;
      // A CA chain received over an unverified connection (--insecure) is never written as a
      // trust anchor; with --ca it came over a connection verified against that CA.
      bool ok = WriteFile(dir + "/client.pem", r.cert_pem, 0644) && WriteFile(dir + "/client.key", platform.KeyPem(r.key_ref), 0600) &&
                (a.count("insecure") || WriteFile(dir + "/server-ca.pem", chain, 0644));
      j.set("written", ok);
      rc = ok ? 0 : 1;
    }
    Out(j);
  });
  return rc;
}


int Task(std::map<std::string, std::string> a) {
  auto t = std::make_shared<omhost::PosixWsTransport>();
  om::NatsOptions o;
  o.urls = {a["ws"]};
  o.name = "onemind-client-task";
  if (const char* u = std::getenv("OM_NATS_USER")) o.user = u;
  if (const char* p = std::getenv("OM_NATS_PASSWORD")) o.password = p;
  om::NatsClient nc(t, o);
  om::AssetTasking tasking(nc, "user:" + o.user);
  nc.Start(om::WallClockMs());
  om::TimeMs start = om::WallClockMs();
  while (!nc.connected() && om::WallClockMs() - start < 5000) {
    nc.Poll(om::WallClockMs());
    SleepMs(5);
  }
  if (!nc.connected()) {
    om::Json j = Obj("error");
    j.set("error", "not connected: " + nc.last_error());
    Out(j);
    return 1;
  }
  tasking.Watch();
  nc.Poll(om::WallClockMs());
  om::GeoPoint clicked{std::atof(a["lat"].c_str()), std::atof(a["lon"].c_str()), 0};
  om::TaskDraft d = om::GotoDraft(a["asset"], a.count("class") ? a["class"] : "drone", clicked,
                                  std::atof(a["hae"].c_str()), 0, "clicked on the map");
  std::string action_id, phase = "proposing";
  auto dump = [&](const char* ev, const om::PendingAction& p, const std::string& err) {
    om::Json j = Obj(ev);
    j.set("action_id", p.action_id);
    j.set("status", p.status);
    j.set("level", p.level);
    j.set("params_hash", p.params_hash);
    j.set("local_hash", p.local_hash);
    j.set("denial_code", p.denial_code);
    j.set("denial_message", p.denial_message);
    j.set("error", err);
    j.set("params", p.draft.params);
    Out(j);
  };
  tasking.Propose(d, om::WallClockMs(), [&](const om::PendingAction& p, const std::string& err) {
    action_id = p.action_id;
    dump("proposed", p, err);
    phase = p.status == "awaiting_confirmation" ? "dialog" : "done";
  });
  int confirm_after = a.count("confirm-after-ms") ? std::atoi(a["confirm-after-ms"].c_str()) : 1000;
  om::TimeMs dialog_at = 0;
  size_t seen = 0;
  int seconds = a.count("seconds") ? std::atoi(a["seconds"].c_str()) : 10;
  while (om::WallClockMs() - start < seconds * 1000) {
    om::TimeMs now = om::WallClockMs();
    nc.Poll(now);
    tasking.Tick(now);
    if (phase == "dialog") {
      if (!dialog_at) {
        dialog_at = now;
        tasking.MarkShown(action_id, now);  // the dialog shows params and hash to the human
        const om::PendingAction* p = tasking.Find(action_id);
        if (p) dump("dialog", *p, "");
      }
      if (now - dialog_at >= confirm_after && (a.count("confirm") || a.count("reject"))) {
        phase = "confirming";
        bool approve = a.count("confirm") != 0;
        tasking.Confirm(action_id, approve, approve ? "" : a["reject"], now,
                        [&](const om::PendingAction& p, const std::string& err) {
                          dump("confirmed", p, err);
                          phase = "following";
                        });
      }
    }
    if (const om::PendingAction* p = tasking.Find(action_id)) {
      for (; seen < p->timeline.size(); ++seen) {
        om::Json j = Obj("audit");
        j.set("event", p->timeline[seen].event);
        j.set("code", p->timeline[seen].code);
        j.set("command_id", p->timeline[seen].command_id);
        Out(j);
      }
      if (p->Final() && phase != "dialog") break;
    }
    if (phase == "done") break;
    SleepMs(5);
  }
  const om::PendingAction* p = tasking.Find(action_id);
  om::Json fin = Obj("final");
  fin.set("status", p ? p->status : std::string("none"));
  fin.set("action_id", action_id);
  Out(fin);
  nc.Stop();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: onemind_tak stream|direct|enroll ...\n");
    return 2;
  }
  std::string cmd = argv[1];
  auto a = Args(argc, argv, 2);
  if (cmd == "stream") return Stream(a);
  if (cmd == "direct") return Direct(a);
  if (cmd == "enroll") return Enroll(a);
  if (cmd == "task") return Task(a);
  std::fprintf(stderr, "unknown command %s\n", cmd.c_str());
  return 2;
}
