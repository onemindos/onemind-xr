// SPDX-License-Identifier: GPL-3.0-or-later
// CotEvent: ported from VR-TAK VH_COTEvent.cpp (GPL-3.0); see CoT.h and THIRD_PARTY.md.
#include "OneMindCore/CoT.h"

#include "OneMindCore/Codec.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace om {

namespace cot_detail {
// VH_COTEvent.cpp MakeXmlKeywords(), ported verbatim (paths relative to <event>).
const std::map<std::string, std::string>& Keywords() {
  static const std::map<std::string, std::string> k = {
      {"callsign", "detail\tcontact\tcallsign"},
      {"endpoint", "detail\tcontact\tendpoint"},
      {"groupName", "detail\t__group\tname"},
      {"groupRole", "detail\t__group\trole"},
      {"archive", "detail\tarchive"},
      {"latitude", "point\tlat"},
      {"longitude", "point\tlon"},
      {"hae", "point\thae"},
      {"link", "detail\tlink"},
      {"fillColor", "detail\tfillColor\tvalue"},
      {"strokeColor", "detail\tstrokeColor\tvalue"},
  };
  return k;
}

std::vector<std::string> SplitTab(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t t = s.find('\t', start);
    std::string part = s.substr(start, t == std::string::npos ? std::string::npos : t - start);
    if (!part.empty()) out.push_back(part);
    if (t == std::string::npos) break;
    start = t + 1;
  }
  return out;
}

std::string FormatNum(double v) {
  char buf[40];
  if (std::fabs(v - std::round(v)) < 1e-9 && std::fabs(v) < 1e15) {
    std::snprintf(buf, sizeof(buf), "%.1f", v);
  } else {
    std::snprintf(buf, sizeof(buf), "%.7f", v);
    // trim trailing zeros
    std::string s(buf);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s += '0';
    return s;
  }
  return buf;
}

int64_t DaysFromCivil(int64_t y, unsigned m, unsigned d) {
  // Howard Hinnant's algorithm (public domain).
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void CivilFromDays(int64_t z, int& y, unsigned& m, unsigned& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yy = static_cast<int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y = static_cast<int>(yy + (m <= 2));
}

bool ParseLinkPoint(const std::string& s, GeoPoint& p) {
  double v[3] = {0, 0, 9999999.0};
  int n = 0;
  size_t start = 0;
  while (n < 3 && start <= s.size()) {
    size_t c = s.find(',', start);
    std::string part = s.substr(start, c == std::string::npos ? std::string::npos : c - start);
    char* end = nullptr;
    v[n++] = std::strtod(part.c_str(), &end);
    if (end == part.c_str()) return false;
    if (c == std::string::npos) break;
    start = c + 1;
  }
  if (n < 2) return false;
  p.lat = v[0];
  p.lon = v[1];
  p.hae = v[2];
  return true;
}

std::string PointStr(const GeoPoint& p) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%.7f,%.7f", p.lat, p.lon);
  return buf;
}
}  // namespace cot_detail

std::string CotTime(TimeMs ms) {
  int64_t secs = ms >= 0 ? ms / 1000 : (ms - 999) / 1000;
  int millis = static_cast<int>(ms - secs * 1000);
  int64_t days = secs >= 0 ? secs / 86400 : (secs - 86399) / 86400;
  int64_t rem = secs - days * 86400;
  int y;
  unsigned mo, d;
  cot_detail::CivilFromDays(days, y, mo, d);
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02d:%02d:%02d.%03dZ", y, mo, d, static_cast<int>(rem / 3600),
                static_cast<int>((rem % 3600) / 60), static_cast<int>(rem % 60), millis);
  return buf;
}

bool ParseCotTime(const std::string& s, TimeMs& out) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  double sec = 0;
  if (s.size() < 19) return false;
  if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%lf", &y, &mo, &d, &h, &mi, &sec) != 6) return false;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec >= 61) return false;
  int64_t tz_off_s = 0;
  size_t tpos = s.find_first_of("Z+-", 19);
  if (tpos != std::string::npos && s[tpos] != 'Z') {
    int oh = 0, om = 0;
    if (std::sscanf(s.c_str() + tpos + 1, "%2d:%2d", &oh, &om) >= 1) {
      tz_off_s = (oh * 3600 + om * 60) * (s[tpos] == '-' ? -1 : 1);
    }
  }
  int64_t days = cot_detail::DaysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d));
  double total = static_cast<double>(days) * 86400.0 + h * 3600.0 + mi * 60.0 + sec - static_cast<double>(tz_off_s);
  out = static_cast<TimeMs>(std::llround(total * 1000.0));
  return true;
}

// ---------------------------------------------------------------------------------------------
// CotEvent (VH_COTEvent port)

CotEvent::CotEvent() {
  root_.reset(new XmlNode());
  root_->tag = "event";
  root_->SetAttr("version", "2.0");
  root_->SetAttr("how", "h-g-i-g-o");  // VH_COTEvent::Initialize default
}

CotEvent::CotEvent(const CotEvent& o) : root_(o.root_->Clone()) {}

CotEvent& CotEvent::operator=(const CotEvent& o) {
  if (this != &o) root_ = o.root_->Clone();
  return *this;
}

bool CotEvent::PopulateFromXML(const std::string& xml, std::string* err) {
  auto r = XmlParse(xml, err);
  if (!r) return false;
  if (r->tag != "event") {
    if (err) *err = "root is not <event>";
    return false;
  }
  root_ = std::move(r);
  return true;
}

std::string CotEvent::ToXML(bool declaration) const {
  std::string s = declaration ? "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>" : "";
  return s + root_->ToString();
}

std::vector<std::string> CotEvent::Resolve(const std::string& path) {
  const auto& k = cot_detail::Keywords();
  auto it = k.find(path);
  return cot_detail::SplitTab(it == k.end() ? path : it->second);
}

bool CotEvent::TryGetAttribute(const std::string& path, std::string& value) const {
  auto parts = Resolve(path);
  if (parts.empty()) return false;
  const XmlNode* node = root_.get();
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    node = node->Child(parts[i]);
    if (!node) return false;
  }
  const std::string* v = node->Attr(parts.back());
  if (!v) return false;
  value = *v;
  return true;
}

std::string CotEvent::GetAttribute(const std::string& path) const {
  std::string v;
  TryGetAttribute(path, v);
  return v;
}

double CotEvent::GetAttributeAsDouble(const std::string& path, double def) const {
  std::string v;
  if (!TryGetAttribute(path, v) || v.empty()) return def;
  char* end = nullptr;
  double d = std::strtod(v.c_str(), &end);
  return end == v.c_str() ? def : d;
}

void CotEvent::SetAttribute(const std::string& path, const std::string& value) {
  auto parts = Resolve(path);
  if (parts.empty()) return;
  XmlNode* node = root_.get();
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    XmlNode* c = node->Child(parts[i]);
    node = c ? c : node->AddChild(parts[i]);
  }
  node->SetAttr(parts.back(), value);
}

void CotEvent::SetAttribute(const std::string& path, double value) { SetAttribute(path, cot_detail::FormatNum(value)); }

std::string CotEvent::GetNodeContent(const std::string& path) const {
  auto parts = Resolve(path);
  const XmlNode* node = root_.get();
  for (const auto& p : parts) {
    node = node->Child(p);
    if (!node) return std::string();
  }
  return node == root_.get() ? std::string() : node->text;
}

bool CotEvent::HasNode(const std::string& path) const {
  auto parts = Resolve(path);
  if (parts.empty()) return false;
  const XmlNode* node = root_.get();
  for (const auto& p : parts) {
    node = node->Child(p);
    if (!node) return false;
  }
  return true;
}

void CotEvent::RemoveNodes(const std::string& path) {
  auto parts = Resolve(path);
  if (parts.empty()) return;
  XmlNode* node = root_.get();
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    node = node->Child(parts[i]);
    if (!node) return;
  }
  node->RemoveChildren(parts.back());
}

XmlNode* CotEvent::AddNode(const std::string& path, const std::vector<std::pair<std::string, std::string>>& attrs,
                           bool replace, const std::string& content) {
  auto parts = Resolve(path);
  if (parts.empty()) return nullptr;
  XmlNode* node = root_.get();
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    XmlNode* c = node->Child(parts[i]);
    node = c ? c : node->AddChild(parts[i]);
  }
  if (replace) {
    if (XmlNode* existing = node->Child(parts.back())) {
      existing->text = content;
      existing->attrs = attrs;
      return existing;
    }
  }
  XmlNode* n = node->AddChild(parts.back());
  n->attrs = attrs;
  n->text = content;
  return n;
}

std::vector<XmlNode*> CotEvent::GetNodes(const std::string& path) const {
  auto parts = Resolve(path);
  if (parts.empty()) return {};
  const XmlNode* node = root_.get();
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    node = node->Child(parts[i]);
    if (!node) return {};
  }
  return node->Children(parts.back());
}

GeoPoint CotEvent::GetPoint() const {
  GeoPoint p;
  p.lat = GetAttributeAsDouble("latitude");
  p.lon = GetAttributeAsDouble("longitude");
  p.hae = GetAttributeAsDouble("hae", 9999999.0);
  return p;
}

void CotEvent::SetPoint(const GeoPoint& p, double ce, double le) {
  SetAttribute("latitude", p.lat);
  SetAttribute("longitude", p.lon);
  SetAttribute("hae", p.hae);
  SetAttribute("point\tce", ce);
  SetAttribute("point\tle", le);
}

void CotEvent::PopulateTime(TimeMs now, int stale_s) {
  SetAttribute("time", CotTime(now));
  SetAttribute("start", CotTime(now));
  SetAttribute("stale", CotTime(now + static_cast<TimeMs>(stale_s) * 1000));
}

// ---------------------------------------------------------------------------------------------
// Typed parse

bool ParseCot(const std::string& xml, CotInfo& c, std::string* err) {
  CotEvent ev;
  if (!ev.PopulateFromXML(xml, err)) return false;
  c = CotInfo();
  c.uid = ev.GetAttribute("uid");
  c.type = ev.GetAttribute("type");
  if (c.uid.empty() || c.type.empty()) {
    if (err) *err = "event missing uid or type";
    return false;
  }
  c.how = ev.GetAttribute("how");
  c.access = ev.GetAttribute("access");
  c.opex = ev.GetAttribute("opex");
  ParseCotTime(ev.GetAttribute("time"), c.time);
  ParseCotTime(ev.GetAttribute("start"), c.start);
  ParseCotTime(ev.GetAttribute("stale"), c.stale);
  c.point = ev.GetPoint();
  c.ce = ev.GetAttributeAsDouble("point\tce", 9999999.0);
  c.le = ev.GetAttributeAsDouble("point\tle", 9999999.0);
  c.callsign = ev.GetAttribute("callsign");
  c.endpoint = ev.GetAttribute("endpoint");
  c.group_name = ev.GetAttribute("groupName");
  c.group_role = ev.GetAttribute("groupRole");
  c.remarks = ev.GetNodeContent("detail\tremarks");
  c.course = ev.GetAttributeAsDouble("detail\ttrack\tcourse", -1);
  c.speed = ev.GetAttributeAsDouble("detail\ttrack\tspeed", -1);
  c.battery = static_cast<int>(ev.GetAttributeAsDouble("detail\tstatus\tbattery", -1));
  c.has_takv = ev.HasNode("detail\ttakv");
  c.takv_platform = ev.GetAttribute("detail\ttakv\tplatform");
  c.takv_device = ev.GetAttribute("detail\ttakv\tdevice");
  c.takv_version = ev.GetAttribute("detail\ttakv\tversion");
  for (XmlNode* l : ev.GetNodes("detail\tlink")) {
    GeoPoint p;
    if (const std::string* pt = l->Attr("point")) {
      if (cot_detail::ParseLinkPoint(*pt, p)) c.link_points.push_back(p);
    }
    if (const std::string* u = l->Attr("uid")) c.link_uids.push_back(*u);
  }
  c.is_chat = c.type == "b-t-f" || c.type.rfind("b-t-f-", 0) == 0;
  if (c.is_chat) {
    c.chat_room = ev.GetAttribute("detail\t__chat\tchatroom");
    c.chat_sender = ev.GetAttribute("detail\t__chat\tsenderCallsign");
  }
  c.flow_tag_projector = ev.HasNode("detail\t_flow-tags_") && !ev.GetAttribute("detail\t_flow-tags_\tomos-tak-projector").empty();
  return true;
}

char CotType::Affiliation(const std::string& type) {
  if (!IsAtom(type) || type.size() < 3) return 0;
  return type[2];
}

char CotType::Dimension(const std::string& type) {
  if (!IsAtom(type) || type.size() < 5 || type[3] != '-') return 0;
  return type[4];
}

std::string CotType::Disposition(const std::string& type) {
  switch (Affiliation(type)) {
    case 'f': return "friendly";
    case 'a': return "assumed_friendly";
    case 'n': return "neutral";
    case 'u': return "unknown";
    case 'p': return "pending";
    case 's':
    case 'h':
    case 'j':
    case 'k': return "suspicious";  // civilian scope: never hostile (BUS-DESIGN D9)
    default: return "unknown";
  }
}

std::string CotType::Environment(const std::string& type) {
  switch (Dimension(type)) {
    case 'P': return "space";
    case 'A': return "air";
    case 'G': return "land";
    case 'S': return "surface";
    case 'U': return "subsurface";
    default: return std::string();
  }
}

std::string CotDropReason(const CotInfo& c) {
  if (c.uid.rfind("OM.", 0) == 0 || c.uid.rfind("INTEL.", 0) == 0 || c.flow_tag_projector) return "echo";
  if (c.type.rfind("t-x-takp", 0) == 0 || c.type == "t-x-c-t" || c.type == "t-x-c-t-r") return "control";
  if (c.uid.size() > 5 && c.uid.compare(c.uid.size() - 5, 5, "-ping") == 0) return "control";
  if (c.type.rfind("t-x-m-c", 0) == 0) return "mission_sync";
  if (c.is_chat) return "chat";
  return std::string();
}

std::string TakEntityId(const std::string& raw_uid) {
  // Same rule as the entity service (omos services/entity/internal/identity MakeID):
  // trim, lowercase, keep if "tak-" + uid is a valid id (<= 64 chars), else hash the raw uid.
  size_t a = 0, e = raw_uid.size();
  while (a < e && std::isspace(static_cast<unsigned char>(raw_uid[a]))) ++a;
  while (e > a && std::isspace(static_cast<unsigned char>(raw_uid[e - 1]))) --e;
  const std::string uid = raw_uid.substr(a, e - a);
  std::string lower;
  bool fits = !uid.empty() && uid.size() <= 60;
  for (char ch : uid) {
    char l = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (!((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '_' || l == '-')) fits = false;
    lower += l;
  }
  if (fits) return "tak-" + lower;
  auto h = Sha256(reinterpret_cast<const uint8_t*>(raw_uid.data()), raw_uid.size());
  return "tak-" + Base32LowerNoPad(h.data(), h.size()).substr(0, 26);
}

// ---------------------------------------------------------------------------------------------
// Builders

namespace cot_detail {
CotEvent Base(const CotBuild& b, const std::string& type, const GeoPoint& p, TimeMs now) {
  CotEvent ev;
  ev.SetAttribute("uid", b.uid);
  ev.SetAttribute("type", type);
  ev.SetAttribute("how", "h-e");  // human estimated: placed by a person in the client
  ev.PopulateTime(now, b.stale_s);
  if (b.simulated) ev.SetAttribute("opex", "s");
  ev.SetPoint(p, 9999999.0, 9999999.0);
  ev.AddNode("detail");
  if (!b.callsign.empty()) ev.SetAttribute("callsign", b.callsign.substr(0, 64));
  return ev;
}
void Remarks(CotEvent& ev, const CotBuild& b) {
  std::string r = b.remarks;
  if (b.simulated) r += r.empty() ? "SIMULATED" : " · SIMULATED";
  if (!r.empty()) ev.AddNode("detail\tremarks", {}, true, r);
}
// Signed 32-bit ARGB, as the projector writes it (cot-projection.md §5.5).
std::string Argb(uint32_t argb) { return std::to_string(static_cast<int32_t>(argb)); }
}  // namespace cot_detail

CotEvent BuildMarker(const CotBuild& b, const std::string& cot_type, const GeoPoint& p, TimeMs now) {
  CotEvent ev = cot_detail::Base(b, cot_type, p, now);
  cot_detail::Remarks(ev, b);
  return ev;
}

CotEvent BuildSpi(const CotBuild& b, const GeoPoint& p, TimeMs now) {
  CotEvent ev = cot_detail::Base(b, "b-m-p-s-p-i", p, now);
  cot_detail::Remarks(ev, b);
  return ev;
}

CotEvent BuildRoute(const CotBuild& b, const std::vector<GeoPoint>& wps, TimeMs now) {
  GeoPoint first = wps.empty() ? GeoPoint{} : wps.front();
  CotEvent ev = cot_detail::Base(b, "b-m-r", first, now);
  for (size_t i = 0; i < wps.size(); ++i) {
    ev.AddNode("detail\tlink", {{"uid", b.uid + ".wp" + std::to_string(i + 1)},
                                {"type", "b-m-p-w"},
                                {"relation", "c"},
                                {"point", cot_detail::PointStr(wps[i])}});
  }
  ev.AddNode("detail\tstrokeColor", {{"value", cot_detail::Argb(0xFFFFFFFFu)}});
  cot_detail::Remarks(ev, b);
  return ev;
}

CotEvent BuildGeofence(const CotBuild& b, const std::vector<GeoPoint>& ring, const std::string& kind, TimeMs now) {
  // Centroid as the point (cot-projection.md §5.2 "For shapes").
  GeoPoint c;
  c.hae = 9999999.0;
  size_t n = ring.size();
  if (n > 1 && ring.front().lat == ring.back().lat && ring.front().lon == ring.back().lon) --n;
  for (size_t i = 0; i < n; ++i) {
    c.lat += ring[i].lat / static_cast<double>(n);
    c.lon += ring[i].lon / static_cast<double>(n);
  }
  CotEvent ev = cot_detail::Base(b, "u-d-f", c, now);
  for (size_t i = 0; i < n; ++i) ev.AddNode("detail\tlink", {{"point", cot_detail::PointStr(ring[i])}});
  if (n > 0) ev.AddNode("detail\tlink", {{"point", cot_detail::PointStr(ring[0])}});  // closed ring
  uint32_t stroke = 0xFFFFFF00u, fill = 0;  // aoi: yellow, no fill
  if (kind == "keep_in") stroke = 0xFF00FF00u;
  else if (kind == "keep_out") { stroke = 0xFFFF0000u; fill = 0x40FF0000u; }
  else if (kind == "hazard") { stroke = 0xFFFFA500u; fill = 0x40FFA500u; }
  else if (kind == "control_area") stroke = 0xFF0000FFu;
  ev.AddNode("detail\tstrokeColor", {{"value", cot_detail::Argb(stroke)}});
  ev.AddNode("detail\tfillColor", {{"value", cot_detail::Argb(fill)}});
  ev.AddNode("detail\tstrokeWeight", {{"value", "3.0"}});
  ev.AddNode("detail\tlabels_on", {{"value", "true"}});
  cot_detail::Remarks(ev, b);
  return ev;
}

CotEvent BuildChat(const CotBuild& b, const std::string& sender_uid, const std::string& room, const std::string& text,
                   TimeMs now) {
  std::string r = room.empty() ? "All Chat Rooms" : room;
  CotBuild bb = b;
  bb.simulated = false;  // the message text is the user's own; opex is set below
  CotEvent ev = cot_detail::Base(bb, "b-t-f", GeoPoint{0, 0, 9999999.0}, now);
  if (b.simulated) ev.SetAttribute("opex", "s");
  std::string msg_id = b.uid.substr(b.uid.rfind('.') == std::string::npos ? 0 : b.uid.rfind('.') + 1);
  ev.AddNode("detail\t__chat", {{"parent", "RootContactGroup"},
                                {"groupOwner", "false"},
                                {"messageId", msg_id},
                                {"chatroom", r},
                                {"id", r},
                                {"senderCallsign", b.callsign}});
  ev.AddNode("detail\t__chat\tchatgrp", {{"uid0", sender_uid}, {"uid1", r}, {"id", r}});
  ev.AddNode("detail\tlink", {{"uid", sender_uid}, {"type", "a-f-G-U-C"}, {"relation", "p-p"}});
  ev.AddNode("detail\tremarks", {{"source", "BAO.F.OneMind." + sender_uid}, {"to", r}, {"time", CotTime(now)}}, true,
             text);
  return ev;
}

std::string ValidateOutgoingCot(const CotEvent& ev) {
  std::string uid = ev.GetAttribute("uid"), type = ev.GetAttribute("type");
  if (uid.empty() || type.empty()) return "missing uid or type";
  if (uid.rfind("OM.", 0) == 0 || uid.rfind("INTEL.", 0) == 0) return "uid prefix reserved for the projector";
  if (uid.size() > 128) return "uid too long";
  char aff = CotType::Affiliation(type);
  if (aff == 'h' || aff == 'j' || aff == 'k') return "hostile affiliations are not sent (civilian scope)";
  if (type.rfind("t-k", 0) == 0 || type.rfind("t-e", 0) == 0) return "tasking types go through policy, not svc.cot.send";
  std::string xml = ev.ToXML();
  if (xml.size() > 64 * 1024) return "event too large";
  TimeMs t = 0, st = 0;
  if (!ParseCotTime(ev.GetAttribute("time"), t) || !ParseCotTime(ev.GetAttribute("stale"), st) || st <= t)
    return "bad time or stale";
  return std::string();
}

std::string CotSendRequest::ToJson() const {
  Json j = Json::MakeObject();
  j.set("v", 1);
  j.set("kind", kind);
  Json t = Json::MakeArray();
  for (const auto& s : targets) t.push_back(Json(s));
  j.set("targets", t);
  j.set("client_id", client_id);
  if (!client_ref.empty()) j.set("client_ref", client_ref);
  Json marking = Json::MakeObject();
  marking.set("tlp", tlp);
  j.set("marking", marking);
  if (!dest_uid.empty()) j.set("dest_uid", dest_uid);
  j.set("cot", cot_xml);
  return j.Dump();
}

bool CotSendReply::Parse(const std::string& body, CotSendReply& out) {
  Json j;
  if (!Json::Parse(body, j) || !j.is_object()) return false;
  out = CotSendReply();
  out.ok = j.boolean("ok", false);
  out.uid = j.str("uid");
  out.error = j.str("error");
  out.detail = j.str("detail");
  for (const auto& t : j["targets"].items()) out.targets.push_back(t.as_string());
  if (!out.error.empty()) out.ok = false;
  return true;
}

}  // namespace om
