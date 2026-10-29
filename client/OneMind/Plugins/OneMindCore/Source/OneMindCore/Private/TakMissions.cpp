// SPDX-License-Identifier: GPL-3.0-or-later
// TAK Data Sync missions through tak-client (feature C7); see TakMissions.h.
#include "OneMindCore/TakMissions.h"

#include "OneMindCore/Codec.h"

#include <cctype>

namespace om {

std::string TakMissionToken(const std::string& name) {
  std::string lower;
  bool fits = !name.empty() && name.size() <= 40;
  for (char ch : name) {
    char l = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (!((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '_' || l == '-')) fits = false;
    lower += l;
  }
  if (fits) return "tak-" + lower;
  auto h = Sha256(reinterpret_cast<const uint8_t*>(name.data()), name.size());
  return "tak-" + Base32LowerNoPad(h.data(), h.size()).substr(0, 26);
}

TakMissionClient::TakMissionClient(NatsClient& nc, std::string client_uid, int timeout_ms)
    : nc_(nc), client_uid_(std::move(client_uid)), timeout_ms_(timeout_ms), alive_(std::make_shared<bool>(true)) {}

TakMissionClient::~TakMissionClient() {
  *alive_ = false;
  if (sid_) nc_.Unsubscribe(sid_);
}

bool TakMissionClient::ParseSummary(const Json& j, TakMissionSummary& o) {
  if (!j.is_object() || j.str("name").empty()) return false;
  o.name = j.str("name");
  o.token = j.str("token", TakMissionToken(o.name));
  o.description = j.str("description");
  o.creator_uid = j.str("creator_uid");
  o.create_time = j.i64("create_time");
  o.subscribed = j.boolean("subscribed");
  o.uid_count = static_cast<int>(j.i64("uid_count"));
  o.content_count = static_cast<int>(j.i64("content_count"));
  return true;
}

void TakMissionClient::ParseChange(const Json& c, TakMissionChange& ch) {
  ch.type = c.str("type");
  ch.uid = c.str("uid");
  ch.hash = c.str("hash");
  ch.callsign = c.str("callsign");
  ch.cot_type = c.str("cot_type");
  ch.creator_uid = c.str("creator_uid");
  ch.timestamp = c.i64("timestamp");
}

bool TakMissionClient::ParseMission(const Json& j, TakMissionView& o) {
  if (!ParseSummary(j, o.summary)) return false;
  o.uids.clear();
  o.contents.clear();
  for (const auto& u : j["uids"].items()) {
    TakMissionUid x;
    x.uid = u.str("uid");
    if (x.uid.empty()) continue;
    x.cot_type = u.str("type");
    x.callsign = u.str("callsign");
    x.creator_uid = u.str("creator_uid");
    x.point.lat = u.num("lat");
    x.point.lon = u.num("lon");
    x.timestamp = u.i64("timestamp");
    o.uids.push_back(x);
  }
  for (const auto& c : j["contents"].items()) {
    TakMissionContent x;
    x.hash = c.str("hash");
    if (x.hash.empty()) continue;
    x.filename = c.str("filename");
    x.mime_type = c.str("mime_type");
    x.submitter = c.str("submitter");
    x.size = c.i64("size");
    x.timestamp = c.i64("timestamp");
    o.contents.push_back(x);
  }
  o.summary.uid_count = static_cast<int>(o.uids.size());
  o.summary.content_count = static_cast<int>(o.contents.size());
  return true;
}

void TakMissionClient::Call(const std::string& endpoint, const Json& body,
                            std::function<void(const Json&, const std::string&)> cb) {
  std::weak_ptr<bool> alive = alive_;
  nc_.Request("svc.tak.mission." + endpoint, body.Dump(),
              [alive, cb](const NatsMsg* m, const std::string& err) {
                if (alive.expired()) return true;
                if (!m) {
                  cb(Json(), err.empty() ? "no reply" : err);
                  return true;
                }
                Json j;
                std::string perr;
                if (!Json::Parse(m->data, j, &perr) || !j.is_object()) {
                  cb(Json(), "bad reply: " + perr);
                  return true;
                }
                if (j.has("error") && !j["error"].is_null()) {
                  cb(j, j.str("error", "error"));
                  return true;
                }
                cb(j, std::string());
                return true;
              },
              timeout_ms_);
}

void TakMissionClient::Watch() {
  if (sid_) return;
  std::weak_ptr<bool> alive = alive_;
  sid_ = nc_.Subscribe("evt.mission.*.changed", [this, alive](const NatsMsg& m) {
    if (alive.expired()) return;
    // evt.mission.{tak-token}.changed; OneMind's own msn-/tsk- facts are not ours.
    size_t a = m.subject.find('.', 4);
    size_t b = m.subject.rfind('.');
    if (a == std::string::npos || b <= a) return;
    std::string token = m.subject.substr(a + 1, b - a - 1);
    if (token.rfind("tak-", 0) != 0) return;
    change_events_++;
    Json env;
    std::string name;
    if (Json::Parse(m.data, env)) name = env["data"].str("name");
    if (name.empty() && token_to_name_.count(token)) name = token_to_name_[token];
    if (name.empty() || TakMissionToken(name) != token) return;  // name must match its token
    auto it = views_.find(name);
    if (it == views_.end()) return;
    TakMissionChange ch;
    ParseChange(env["data"]["change"], ch);
    if (!ch.type.empty()) it->second.changes.push_back(ch);
    Refresh(name);
  });
}

void TakMissionClient::List(Done done) {
  Json b = Json::MakeObject();
  b.set("v", 1);
  std::weak_ptr<bool> alive = alive_;
  Call("list", b, [this, alive, done](const Json& j, const std::string& err) {
    if (alive.expired()) return;
    if (err.empty()) {
      missions_.clear();
      for (const auto& m : j["missions"].items()) {
        TakMissionSummary s;
        if (ParseSummary(m, s)) {
          token_to_name_[s.token] = s.name;
          missions_.push_back(s);
        }
      }
    }
    if (done) done(err);
  });
}

void TakMissionClient::Refresh(const std::string& name) {
  auto it = views_.find(name);
  if (it == views_.end()) return;
  if (it->second.loading) {
    it->second.dirty = true;  // one request in flight per mission; refresh again after it
    return;
  }
  it->second.loading = true;
  Json b = Json::MakeObject();
  b.set("v", 1);
  b.set("name", name);
  std::weak_ptr<bool> alive = alive_;
  Call("get", b, [this, alive, name](const Json& j, const std::string& err) {
    if (alive.expired()) return;
    auto v = views_.find(name);
    if (v == views_.end()) return;
    v->second.loading = false;
    if (err.empty()) {
      std::vector<TakMissionChange> keep = v->second.changes;
      std::vector<TakMissionLog> logs = v->second.logs;
      ParseMission(j["mission"], v->second);
      v->second.changes = keep;
      v->second.logs = logs;
      v->second.refreshed_at = WallClockMs();
    }
    if (v->second.dirty) {
      v->second.dirty = false;
      Refresh(name);
    }
  });
}

void TakMissionClient::Open(const std::string& name, Done done) {
  TakMissionView& v = views_[name];
  v.summary.name = name;
  v.summary.token = TakMissionToken(name);
  token_to_name_[v.summary.token] = name;
  Json b = Json::MakeObject();
  b.set("v", 1);
  b.set("name", name);
  std::weak_ptr<bool> alive = alive_;
  v.loading = true;
  Call("get", b, [this, alive, name, b, done](const Json& j, const std::string& err) {
    if (alive.expired()) return;
    auto it = views_.find(name);
    if (it == views_.end()) return;
    it->second.loading = false;
    if (!err.empty() || !ParseMission(j["mission"], it->second)) {
      if (done) done(err.empty() ? "bad mission record" : err);
      return;
    }
    it->second.refreshed_at = WallClockMs();
    Call("changes", b, [this, alive, name](const Json& cj, const std::string& cerr) {
      if (alive.expired() || !cerr.empty()) return;
      auto v2 = views_.find(name);
      if (v2 == views_.end()) return;
      v2->second.changes.clear();
      for (const auto& c : cj["changes"].items()) {
        TakMissionChange ch;
        ParseChange(c, ch);
        v2->second.changes.push_back(ch);
      }
    });
    Call("logs", b, [this, alive, name](const Json& lj, const std::string& lerr) {
      if (alive.expired() || !lerr.empty()) return;
      auto v2 = views_.find(name);
      if (v2 == views_.end()) return;
      v2->second.logs.clear();
      for (const auto& l : lj["logs"].items()) {
        TakMissionLog lg;
        lg.id = l.str("id");
        lg.content = l.str("content");
        lg.creator_uid = l.str("creator_uid");
        lg.timestamp = l.i64("timestamp");
        v2->second.logs.push_back(lg);
      }
    });
    if (done) done(std::string());
  });
}

void TakMissionClient::Close(const std::string& name) { views_.erase(name); }

void TakMissionClient::Subscribe(const std::string& name, Done done) {
  Json b = Json::MakeObject();
  b.set("v", 1);
  b.set("name", name);
  b.set("client_uid", client_uid_);
  std::weak_ptr<bool> alive = alive_;
  Call("subscribe", b, [this, alive, name, done](const Json&, const std::string& err) {
    if (alive.expired()) return;
    if (err.empty()) {
      for (auto& m : missions_)
        if (m.name == name) m.subscribed = true;
      auto it = views_.find(name);
      if (it != views_.end()) it->second.summary.subscribed = true;
    }
    if (done) done(err);
  });
}

void TakMissionClient::Unsubscribe(const std::string& name, Done done) {
  Json b = Json::MakeObject();
  b.set("v", 1);
  b.set("name", name);
  b.set("client_uid", client_uid_);
  std::weak_ptr<bool> alive = alive_;
  Call("unsubscribe", b, [this, alive, name, done](const Json&, const std::string& err) {
    if (alive.expired()) return;
    if (err.empty()) {
      for (auto& m : missions_)
        if (m.name == name) m.subscribed = false;
      auto it = views_.find(name);
      if (it != views_.end()) it->second.summary.subscribed = false;
    }
    if (done) done(err);
  });
}

void TakMissionClient::PutFeature(const std::string& name, const CotEvent& ev, Done done) {
  std::string why = ValidateOutgoingCot(ev);
  if (!why.empty()) {
    if (done) done(why);
    return;
  }
  Json b = Json::MakeObject();
  b.set("v", 1);
  b.set("name", name);
  b.set("client_uid", client_uid_);
  b.set("cot", ev.ToXML());
  Call("content.put", b, [done](const Json&, const std::string& err) {
    if (done) done(err);
  });
}

const TakMissionView* TakMissionClient::View(const std::string& name) const {
  auto it = views_.find(name);
  return it == views_.end() ? nullptr : &it->second;
}

}  // namespace om
