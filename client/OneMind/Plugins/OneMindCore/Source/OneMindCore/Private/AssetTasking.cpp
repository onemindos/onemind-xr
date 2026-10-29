// SPDX-License-Identifier: GPL-3.0-or-later
// Click-to-task through policy with a human confirm (feature D3); see AssetTasking.h.
#include "OneMindCore/AssetTasking.h"

#include "OneMindCore/Codec.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace om {

namespace {
// UTF-8 -> UTF-16 code units, for RFC 8785 §3.2.3 key order.
std::vector<uint16_t> Utf16(const std::string& s) {
  std::vector<uint16_t> out;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    uint32_t cp;
    size_t n;
    if (c < 0x80) { cp = c; n = 1; }
    else if ((c >> 5) == 6) { cp = c & 0x1f; n = 2; }
    else if ((c >> 4) == 14) { cp = c & 0x0f; n = 3; }
    else { cp = c & 0x07; n = 4; }
    for (size_t k = 1; k < n && i + k < s.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3f);
    i += n;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out.push_back(static_cast<uint16_t>(0xd800 + (cp >> 10)));
      out.push_back(static_cast<uint16_t>(0xdc00 + (cp & 0x3ff)));
    } else {
      out.push_back(static_cast<uint16_t>(cp));
    }
  }
  return out;
}

void JcsString(std::string& out, const std::string& s) {
  out += '"';
  for (char ch : s) {
    unsigned char c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += ch;
        }
    }
  }
  out += '"';
}

void JcsTo(std::string& out, const Json& v) {
  switch (v.type()) {
    case Json::Type::Null: out += "null"; break;
    case Json::Type::Bool: out += v.as_bool() ? "true" : "false"; break;
    case Json::Type::Number: out += JcsNumber(v.as_number()); break;
    case Json::Type::String: JcsString(out, v.as_string()); break;
    case Json::Type::Array: {
      out += '[';
      bool first = true;
      for (const auto& e : v.items()) {
        if (!first) out += ',';
        first = false;
        JcsTo(out, e);
      }
      out += ']';
      break;
    }
    case Json::Type::Object: {
      std::vector<std::string> keys;
      for (const auto& kv : v.fields()) keys.push_back(kv.first);
      std::sort(keys.begin(), keys.end(), [](const std::string& a, const std::string& b) { return Utf16(a) < Utf16(b); });
      out += '{';
      bool first = true;
      for (const auto& k : keys) {
        if (!first) out += ',';
        first = false;
        JcsString(out, k);
        out += ':';
        JcsTo(out, v[k]);
      }
      out += '}';
      break;
    }
  }
}

const char* const kVerbs[][2] = {{"goto", "A3"}, {"hold", "A3"}, {"rtl", "A3"}, {"land", "A3"},
                                 {"orbit", "A3"}, {"observe", "A3"}, {"estop", "A3"}, {"disarm", "A3"},
                                 {"arm", "A4"}, {"takeoff", "A4"}, {"estop_release", "A4"},
                                 {"geofence_override", "A4"}};
}  // namespace

std::string JcsNumber(double v) {
  if (!std::isfinite(v)) return "null";  // not valid JSON; never produced by the client
  if (v == 0) return "0";
  std::string sign;
  if (v < 0) {
    sign = "-";
    v = -v;
  }
  char buf[40];
  for (int prec = 0; prec <= 16; ++prec) {  // shortest round-trip scientific form
    std::snprintf(buf, sizeof(buf), "%.*e", prec, v);
    if (std::strtod(buf, nullptr) == v) break;
  }
  std::string e(buf);
  size_t ep = e.find('e');
  std::string mant = e.substr(0, ep);
  int exp = std::atoi(e.c_str() + ep + 1);
  std::string digits;
  for (char c : mant)
    if (c != '.') digits += c;
  while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
  int k = static_cast<int>(digits.size());
  int n = exp + 1;
  std::string out;
  if (k <= n && n <= 21) {
    out = digits + std::string(static_cast<size_t>(n - k), '0');
  } else if (0 < n && n <= 21) {
    out = digits.substr(0, static_cast<size_t>(n)) + "." + digits.substr(static_cast<size_t>(n));
  } else if (-6 < n && n <= 0) {
    out = "0." + std::string(static_cast<size_t>(-n), '0') + digits;
  } else {
    int ev = n - 1;
    std::string es = ev < 0 ? "-" : "+";
    out = digits.substr(0, 1) + (k > 1 ? "." + digits.substr(1) : std::string()) + "e" + es + std::to_string(std::abs(ev));
  }
  return sign + out;
}

std::string JcsCanonical(const Json& v) {
  std::string out;
  JcsTo(out, v);
  return out;
}

std::string ParamsHash(const std::string& action_type, int action_version, const Json& target, const Json& params) {
  Json doc = Json::MakeObject();
  doc.set("action_type", action_type);
  doc.set("action_version", action_version);
  doc.set("target", target);
  doc.set("params", params.is_null() ? Json::MakeObject() : params);
  std::string c = JcsCanonical(doc);
  auto h = Sha256(reinterpret_cast<const uint8_t*>(c.data()), c.size());
  return "sha256:" + HexLower(h.data(), h.size());
}

bool IsAssetVerb(const std::string& verb) { return !AssetVerbLevel(verb).empty(); }

std::string AssetVerbLevel(const std::string& verb) {
  for (const auto& v : kVerbs)
    if (verb == v[0]) return v[1];
  return std::string();
}

std::string NewUuid() {
  std::string seed = NewToken() + NewToken();
  auto h = Sha256(reinterpret_cast<const uint8_t*>(seed.data()), seed.size());
  h[6] = static_cast<uint8_t>((h[6] & 0x0f) | 0x40);
  h[8] = static_cast<uint8_t>((h[8] & 0x3f) | 0x80);
  std::string x = HexLower(h.data(), 16);
  return x.substr(0, 8) + "-" + x.substr(8, 4) + "-" + x.substr(12, 4) + "-" + x.substr(16, 4) + "-" + x.substr(20, 12);
}

TaskDraft GotoDraft(const std::string& asset_id, const std::string& asset_class, const GeoPoint& clicked, double hae_m,
                    double speed_mps, const std::string& reason) {
  TaskDraft d;
  d.asset_id = asset_id;
  d.asset_class = asset_class;
  d.verb = "goto";
  // Round to 1e-7 degrees (about 1 cm) so what the dialog shows is exactly what is hashed.
  d.params.set("lat", std::round(clicked.lat * 1e7) / 1e7);
  d.params.set("lon", std::round(clicked.lon * 1e7) / 1e7);
  d.params.set("hae_m", std::round(hae_m * 10) / 10);
  if (speed_mps > 0) d.params.set("speed_mps", std::round(speed_mps * 10) / 10);
  d.reason = reason;
  return d;
}

bool PendingAction::Final() const {
  return status == "denied" || status == "executed" || status == "failed" || status == "refused" ||
         status == "rejected" || status == "expired";
}

AssetTasking::AssetTasking(NatsClient& nc, std::string user_id, int timeout_ms)
    : nc_(nc), user_id_(std::move(user_id)), timeout_ms_(timeout_ms), alive_(std::make_shared<bool>(true)) {}

AssetTasking::~AssetTasking() {
  *alive_ = false;
  if (sid_) nc_.Unsubscribe(sid_);
}

std::string AssetTasking::PreCheck(const TaskDraft& d) {
  if (d.asset_id.rfind("sim-", 0) != 0) return "not_simulated: only sim-* assets can be tasked in this run";
  if (d.asset_class != "drone" && d.asset_class != "robot" && d.asset_class != "vehicle") return "unknown asset class";
  if (!IsAssetVerb(d.verb)) return "unknown_action: verb not in the closed list";
  for (char c : d.asset_id)
    if (c == '.' || c == '*' || c == '>' || c == ' ') return "invalid asset id";
  if (d.verb == "goto") {
    double lat = d.params.num("lat", 999), lon = d.params.num("lon", 999);
    if (std::fabs(lat) > 90 || std::fabs(lon) > 180 || !d.params["hae_m"].is_number()) return "schema_invalid: goto needs lat, lon, hae_m";
  }
  return std::string();
}

Json AssetTasking::BuildRequest(const TaskDraft& d, const std::string& request_id) const {
  Json r = Json::MakeObject();
  r.set("v", 1);
  r.set("request_id", request_id);
  r.set("action_type", "asset." + d.verb);
  r.set("action_version", 1);
  Json target = Json::MakeObject();
  target.set("entity_type", d.asset_class);
  target.set("entity_id", d.asset_id);
  r.set("target", target);
  r.set("params", d.params);
  Json by = Json::MakeObject();
  by.set("kind", "user");
  by.set("id", user_id_);
  r.set("requested_by", by);
  if (!d.reason.empty()) r.set("reason", d.reason.substr(0, 500));
  return r;
}

void AssetTasking::Watch() {
  if (sid_) return;
  std::weak_ptr<bool> alive = alive_;
  sid_ = nc_.Subscribe("evt.so.action.>", [this, alive](const NatsMsg& m) {
    if (!alive.expired()) OnEvent(m);
  });
}

void AssetTasking::Apply(PendingAction& a, const ActionEvent& e) {
  a.timeline.push_back(e);
  if (a.Final()) return;
  if (e.event == "approved" || e.event == "dispatched") a.status = e.event;
  else if (e.event == "acknowledged") a.status = "dispatched";
  else if (e.event == "denied" || e.event == "rejected" || e.event == "expired" || e.event == "executed" ||
           e.event == "failed" || e.event == "refused")
    a.status = e.event;
  if (!e.code.empty()) {
    a.denial_code = e.code;
    a.denial_message = e.message;
  }
}

void AssetTasking::OnEvent(const NatsMsg& m) {
  Json env;
  if (!Json::Parse(m.data, env)) return;
  const Json& data = env["data"];
  std::string id = data.str("action_id");
  if (id.empty()) return;
  ActionEvent e;
  e.event = m.subject.substr(m.subject.rfind('.') + 1);
  e.code = data.str("code");
  e.message = data.str("message");
  e.command_id = data.str("command_id");
  e.at = now_;
  auto it = actions_.find(id);
  if (it != actions_.end()) {
    Apply(it->second, e);
    return;
  }
  bool proposing = false;  // only keep events while one of our proposals awaits its reply
  for (const auto& kv : actions_) proposing = proposing || kv.second.status == "proposing";
  if (!proposing) return;
  early_.push_back({id, e});
  while (early_.size() > 256) early_.pop_front();
}

std::string AssetTasking::Propose(const TaskDraft& d, TimeMs now, Done done) {
  now_ = now;
  std::string why = PreCheck(d);
  PendingAction p;
  p.draft = d;
  p.action_type = "asset." + d.verb;
  if (!why.empty()) {
    p.status = "denied";
    p.denial_code = why.substr(0, why.find(':'));
    p.denial_message = why;
    if (done) done(p, why);
    return std::string();
  }
  p.request_id = NewUuid();
  p.status = "proposing";
  Json target = Json::MakeObject();
  target.set("entity_type", d.asset_class);
  target.set("entity_id", d.asset_id);
  p.local_hash = ParamsHash(p.action_type, 1, target, d.params);
  std::string key = "req:" + p.request_id;
  actions_[key] = p;
  std::weak_ptr<bool> alive = alive_;
  nc_.Request("svc.policy.command.propose", BuildRequest(d, p.request_id).Dump(),
              [this, alive, key, done](const NatsMsg* m, const std::string& err) {
                if (alive.expired()) return true;
                auto it = actions_.find(key);
                if (it == actions_.end()) return true;
                PendingAction a = it->second;
                actions_.erase(it);
                Json r;
                if (!m || !Json::Parse(m->data, r) || !r.is_object()) {
                  a.status = "failed";
                  a.denial_code = "policy_unavailable";
                  a.denial_message = m ? "bad reply" : err;
                  if (done) done(a, a.denial_message);
                  return true;
                }
                a.action_id = r.str("action_id");
                a.status = r.str("status");
                a.level = r.str("level");
                a.params_hash = r.str("params_hash");
                a.approvals_needed = static_cast<int>(r.i64("approvals_needed"));
                ParseCotTime(r.str("confirm_by"), a.confirm_by);
                if (r["denial"].is_object()) {
                  a.denial_code = r["denial"].str("code");
                  a.denial_message = r["denial"].str("message");
                }
                std::string e;
                if (a.status == "awaiting_confirmation" && a.params_hash != a.local_hash) {
                  // The policy service would dispatch something other than what the human sees.
                  a.status = "failed";
                  a.denial_code = "hash_mismatch";
                  e = "policy params_hash differs from the client's";
                }
                if (a.status == "denied") e = a.denial_code;
                if (!a.action_id.empty()) {
                  PendingAction& stored = actions_[a.action_id];
                  stored = a;
                  for (auto e2 = early_.begin(); e2 != early_.end();) {
                    if (e2->first == a.action_id) {
                      Apply(stored, e2->second);
                      e2 = early_.erase(e2);
                    } else {
                      ++e2;
                    }
                  }
                  a = stored;
                }
                if (done) done(a, e);
                return true;
              },
              timeout_ms_);
  return p.request_id;
}

void AssetTasking::MarkShown(const std::string& action_id, TimeMs now) {
  auto it = actions_.find(action_id);
  if (it == actions_.end() || it->second.shown) return;
  it->second.shown = true;
  it->second.shown_at = now;
}

void AssetTasking::Confirm(const std::string& action_id, bool approve, const std::string& reason, TimeMs now, Done done) {
  now_ = now;
  auto it = actions_.find(action_id);
  PendingAction none;
  if (it == actions_.end()) {
    if (done) done(none, "unknown action");
    return;
  }
  PendingAction& a = it->second;
  std::string why;
  if (a.status != "awaiting_confirmation") why = "not awaiting confirmation (" + a.status + ")";
  else if (!a.shown) why = "the parameters were not shown to the human";
  else if (approve && now - a.shown_at < kMinShownMs) why = "the parameters were shown for less than 500 ms";
  else if (a.confirm_by && now >= a.confirm_by) why = "expired";
  else if (a.params_hash.empty() || a.params_hash != a.local_hash) why = "params_hash mismatch";
  else if (!approve && reason.empty()) why = "a rejection needs a reason";
  if (!why.empty()) {
    if (done) done(a, why);
    return;
  }
  a.status = "confirming";
  Json body = Json::MakeObject();
  body.set("v", 1);
  body.set("action_id", a.action_id);
  body.set("params_hash", a.params_hash);
  body.set("decision", approve ? "approve" : "reject");
  if (!reason.empty()) body.set("reason", reason.substr(0, 500));
  std::weak_ptr<bool> alive = alive_;
  nc_.Request("svc.policy.command.confirm", body.Dump(),
              [this, alive, action_id, approve, done](const NatsMsg* m, const std::string& err) {
                if (alive.expired()) return true;
                auto it2 = actions_.find(action_id);
                if (it2 == actions_.end()) return true;
                PendingAction& p = it2->second;
                Json r;
                std::string e;
                if (!m || !Json::Parse(m->data, r) || !r.is_object()) {
                  e = m ? "bad reply" : err;
                  if (p.status == "confirming") p.status = "awaiting_confirmation";
                } else if (r["denial"].is_object()) {
                  p.denial_code = r["denial"].str("code");
                  p.denial_message = r["denial"].str("message");
                  e = p.denial_code;
                  if (p.status == "confirming") p.status = "awaiting_confirmation";
                } else if (p.status == "confirming") {
                  std::string st = r.str("status");
                  p.status = st.empty() ? (approve ? "approved" : "rejected") : st;
                }
                if (done) done(p, e);
                return true;
              },
              timeout_ms_);
}

void AssetTasking::Tick(TimeMs now) {
  now_ = now;
  for (auto& kv : actions_) {
    PendingAction& a = kv.second;
    if (a.status == "awaiting_confirmation" && a.confirm_by && now >= a.confirm_by) a.status = "expired";
  }
}

const PendingAction* AssetTasking::Find(const std::string& action_id) const {
  auto it = actions_.find(action_id);
  return it == actions_.end() ? nullptr : &it->second;
}

}  // namespace om
