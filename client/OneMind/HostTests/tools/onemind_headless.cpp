// onemind_headless: the OneMind client core without a renderer. It runs the same
// ClientCore the Unreal plugins run (bus, layers, entity stores, inspector, CoT send)
// over a host WebSocket transport and prints what the client would show. It is the
// build-machine stand-in for "shows in the client" until Unreal runs on the Mac.
//
//   onemind_headless --url ws://127.0.0.1:9222 --layer tak:person,track,place:direct \
//       --duration 30 --expect ent-id --inspect ent-id --send-marker 30.2672,-97.7431,OM-TEST
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/ClientCore.h"
#include "PosixWsTransport.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<std::string> Split(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    size_t p = s.find(sep, start);
    out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
    if (p == std::string::npos) break;
    start = p + 1;
  }
  return out;
}

void Usage() {
  std::printf(
      "usage: onemind_headless [--url ws://host:port] [--layer id:type1,type2:watch|direct|core]...\n"
      "                        [--aoi lon0,lat0,lon1,lat1] [--kv bucket:keyfilter] [--duration s]\n"
      "                        [--expect entity_id] [--expect-gone entity_id] [--inspect entity_id]\n"
      "                        [--send-marker lat,lon,callsign] [--send-chat text] [--print-every s]\n"
      "  NATS_USER / NATS_PASSWORD are read from the environment (never from Git).\n");
}

}  // namespace

int main(int argc, char** argv) {
  om::ClientConfig cfg;
  cfg.client_id = "headless";
  std::vector<std::string> layer_ids;
  double duration_s = 10, print_every = 2;
  std::string expect, expect_gone, inspect, marker, chat;
  om::Aoi aoi;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
    if (a == "--url") cfg.nats.urls = {next()};
    else if (a == "--layer") {
      auto parts = Split(next(), ':');
      if (parts.size() < 2) { Usage(); return 2; }
      om::LayerSpec s;
      s.id = parts[0];
      s.title = parts[0];
      s.entity_types = Split(parts[1], ',');
      if (parts.size() > 2) om::ParseLayerPath(parts[2], s.path);
      cfg.layers.push_back(s);
      layer_ids.push_back(s.id);
    } else if (a == "--aoi") {
      auto v = Split(next(), ',');
      if (v.size() == 4) aoi = om::Aoi::Bbox(std::atof(v[0].c_str()), std::atof(v[1].c_str()), std::atof(v[2].c_str()), std::atof(v[3].c_str()));
    } else if (a == "--kv") {
      auto parts = Split(next(), ':');
      if (parts.size() == 2) cfg.kv_watches[parts[0]] = parts[1];
    } else if (a == "--duration") duration_s = std::atof(next().c_str());
    else if (a == "--print-every") print_every = std::atof(next().c_str());
    else if (a == "--expect") expect = next();
    else if (a == "--expect-gone") expect_gone = next();
    else if (a == "--inspect") inspect = next();
    else if (a == "--send-marker") marker = next();
    else if (a == "--send-chat") chat = next();
    else { Usage(); return 2; }
  }
  if (const char* u = std::getenv("NATS_USER")) cfg.nats.user = u;
  if (const char* p = std::getenv("NATS_PASSWORD")) cfg.nats.password = p;
  for (auto& l : cfg.layers)
    if (aoi.has_bbox) l.aoi = aoi;

  auto transport = std::make_shared<omhost::PosixWsTransport>();
  om::ClientCore core(transport, cfg);
  om::TimeMs t0 = om::WallClockMs();
  core.Start(t0);
  bool layers_on = false, sent = false, seen = false, gone_ok = expect_gone.empty(), inspected = false;
  bool expect_gone_seen = false;
  int rc = 0;
  om::TimeMs next_print = t0;
  while (om::WallClockMs() - t0 < static_cast<om::TimeMs>(duration_s * 1000)) {
    om::TimeMs now = om::WallClockMs();
    core.Tick(now);
    if (core.nats().connected() && !layers_on) {
      layers_on = true;
      std::printf("[headless] connected to %s (server %s)\n", cfg.nats.urls[0].c_str(),
                  core.nats().server_info().str("version").c_str());
      for (const auto& id : layer_ids) core.layers().Toggle(id, true, now);
    }
    for (const auto& id : layer_ids) {
      om::LayerSession* s = core.layers().Get(id);
      if (!s) continue;
      if (!expect.empty() && !seen && s->store().Has(expect)) {
        seen = true;
        auto e = s->store().Get(static_cast<size_t>(s->store().IndexOf(expect)));
        std::printf("[headless] SEEN %s in layer %s at %.6f,%.6f callsign=%s type=%s seq=%lld after %lld ms\n",
                    expect.c_str(), id.c_str(), e.lat, e.lon, e.callsign.c_str(), e.cot_type.c_str(),
                    static_cast<long long>(e.seq), static_cast<long long>(now - t0));
      }
      if (!expect_gone.empty() && s->store().Has(expect_gone)) expect_gone_seen = true;
    }
    if (!expect_gone.empty() && expect_gone_seen && !gone_ok) {
      bool present = false;
      for (const auto& id : layer_ids)
        if (core.layers().Get(id) && core.layers().Get(id)->store().Has(expect_gone)) present = true;
      if (!present) {
        gone_ok = true;
        std::printf("[headless] GONE %s after %lld ms\n", expect_gone.c_str(), static_cast<long long>(now - t0));
      }
    }
    if (!inspect.empty() && !inspected && seen && expect == inspect) {
      inspected = true;
      core.FetchEntity(inspect, [&core, inspect](const om::EntityDetails* d, const std::string& err) {
        if (!d) {
          std::printf("[headless] inspector: no details for %s (%s)\n", inspect.c_str(), err.c_str());
          return;
        }
        std::printf("[headless] INSPECTOR %s%s\n", inspect.c_str(), err.empty() ? "" : (" (" + err + ")").c_str());
        for (const auto& r : om::BuildInspectorRows(*d, om::WallClockMs()))
          std::printf("    %-11s %-24s %s\n", r.section.c_str(), r.key.c_str(), r.value.c_str());
        core.FetchRawCot(inspect, [inspect](const std::string& xml, const std::string& e2) {
          if (e2.empty()) std::printf("[headless] RAW COT %s\n%s\n", inspect.c_str(), xml.c_str());
          else std::printf("[headless] raw CoT unavailable: %s\n", e2.c_str());
        });
      });
    }
    if (core.nats().connected() && !sent && (!marker.empty() || !chat.empty()) && now - t0 > 500) {
      sent = true;
      if (!marker.empty()) {
        auto v = Split(marker, ',');
        om::CotBuild b;
        b.uid = core.NewCotUid();
        b.callsign = v.size() > 2 ? v[2] : "OM-MARKER";
        b.remarks = "marker from the OneMind client";
        om::GeoPoint p{std::atof(v[0].c_str()), std::atof(v.size() > 1 ? v[1].c_str() : "0"), 9999999.0};
        om::CotEvent ev = om::BuildMarker(b, "a-u-G", p, now);
        std::printf("[headless] svc.cot.send marker uid=%s\n%s\n", b.uid.c_str(), ev.ToXML().c_str());
        core.SendCot("marker", ev, [&rc](const om::CotSendReply& r, const std::string& err) {
          std::printf("[headless] svc.cot.send reply ok=%d uid=%s targets=%zu error=%s\n", r.ok ? 1 : 0,
                      r.uid.c_str(), r.targets.size(), err.c_str());
          if (!r.ok) rc = 3;
        });
      }
      if (!chat.empty()) {
        om::CotBuild b;
        b.uid = "GeoChat.onemind-headless.All Chat Rooms." + core.NewCotUid();
        b.callsign = "OneMind";
        om::CotEvent ev = om::BuildChat(b, "onemind-headless", "All Chat Rooms", chat, now);
        core.SendCot("chat", ev, [&rc](const om::CotSendReply& r, const std::string& err) {
          std::printf("[headless] chat reply ok=%d error=%s\n", r.ok ? 1 : 0, err.c_str());
          if (!r.ok) rc = 3;
        });
      }
    }
    if (now >= next_print) {
      next_print = now + static_cast<om::TimeMs>(print_every * 1000);
      for (const auto& id : layer_ids) {
        om::LayerSession* s = core.layers().Get(id);
        if (!s) continue;
        std::printf("[headless] layer %s path=%s state=%s entities=%zu snapshots=%llu batches=%llu err=%s\n", id.c_str(),
                    om::LayerPathName(s->spec().path), om::LayerStateName(s->state()), s->store().size(),
                    static_cast<unsigned long long>(s->stats().snapshots),
                    static_cast<unsigned long long>(s->stats().batches), s->last_error().c_str());
      }
    }
    bool done = (!expect.empty() ? seen : false) && gone_ok && (marker.empty() && chat.empty() ? true : sent) &&
                (inspect.empty() || inspected);
    if (done && expect_gone.empty() && inspect.empty() && marker.empty() && chat.empty()) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const auto& id : layer_ids) {
    om::LayerSession* s = core.layers().Get(id);
    if (!s) continue;
    std::printf("[headless] final layer %s: %zu entities (added %llu, updated %llu, removed %llu, expired %llu, "
                "ignored older %llu), last snapshot %lld ms\n",
                id.c_str(), s->store().size(), static_cast<unsigned long long>(s->store().counters().added),
                static_cast<unsigned long long>(s->store().counters().updated),
                static_cast<unsigned long long>(s->store().counters().removed),
                static_cast<unsigned long long>(s->store().counters().expired),
                static_cast<unsigned long long>(s->store().counters().ignored_older),
                static_cast<long long>(s->stats().last_snapshot_ms));
    for (size_t i = 0; i < s->store().size() && i < 20; ++i) {
      auto e = s->store().Get(i);
      std::printf("[headless] final %s %s %s %.6f,%.6f seq=%lld\n", id.c_str(), e.entity_id.c_str(),
                  e.callsign.c_str(), e.lat, e.lon, static_cast<long long>(e.seq));
    }
  }
  core.Stop();
  if (!expect.empty() && !seen) {
    std::printf("[headless] NOT SEEN %s\n", expect.c_str());
    return 1;
  }
  if (!gone_ok) {
    std::printf("[headless] NOT GONE %s\n", expect_gone.c_str());
    return 1;
  }
  return rc;
}
