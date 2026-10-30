// SPDX-License-Identifier: GPL-3.0-or-later
// Drawing and tactical graphics as CoT (feature B11); see Drawing.h for the layouts' sources.
#include "OneMindCore/Drawing.h"

#include "CotDetail.h"
#include "OneMindCore/Geo.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace om {

namespace {
constexpr double kPi = 3.14159265358979323846;
double Rad(double d) { return d * kPi / 180.0; }
double Deg(double r) { return r * 180.0 / kPi; }

void AddStyle(CotEvent& ev, const ShapeStyle& s, bool fill) {
  ev.AddNode("detail\tstrokeColor", {{"value", cot_detail::Argb(s.stroke_argb)}});
  ev.AddNode("detail\tstrokeWeight", {{"value", cot_detail::FormatNum(s.stroke_weight)}});
  if (fill) ev.AddNode("detail\tfillColor", {{"value", cot_detail::Argb(s.fill_argb)}});
  ev.AddNode("detail\tlabels_on", {{"value", s.labels_on ? "true" : "false"}});
}

CotEvent ShapeBase(const CotBuild& b, const std::string& type, const GeoPoint& anchor, TimeMs now) {
  GeoPoint p = anchor;
  p.hae = 9999999.0;
  CotEvent ev = cot_detail::Base(b, type, p, now);
  cot_detail::Remarks(ev, b);
  ev.AddNode("detail\tarchive");
  return ev;
}

void AddLinks(CotEvent& ev, const std::vector<GeoPoint>& pts) {
  for (const auto& p : pts) ev.AddNode("detail\tlink", {{"point", cot_detail::PointStr(p)}});
}

GeoPoint Centroid(const std::vector<GeoPoint>& pts) {
  GeoPoint c;
  if (pts.empty()) return c;
  size_t n = pts.size();
  if (n > 1 && pts.front().lat == pts.back().lat && pts.front().lon == pts.back().lon) --n;
  for (size_t i = 0; i < n; ++i) {
    c.lat += pts[i].lat;
    c.lon += pts[i].lon;
  }
  c.lat /= static_cast<double>(n);
  c.lon /= static_cast<double>(n);
  return c;
}

uint32_t ParseArgb(const std::string& s, uint32_t def) {
  if (s.empty()) return def;
  return static_cast<uint32_t>(static_cast<int32_t>(std::strtol(s.c_str(), nullptr, 10)));
}

// Local tangent plane (equirectangular about the origin), metres.
struct Plane {
  GeoPoint o;
  double kx, ky;
  explicit Plane(const GeoPoint& origin) : o(origin) {
    ky = Rad(1.0) * Geo::kMeanRadius;
    kx = ky * std::cos(Rad(origin.lat));
  }
  void To(const GeoPoint& p, double& x, double& y) const {
    x = (p.lon - o.lon) * kx;
    y = (p.lat - o.lat) * ky;
  }
  GeoPoint From(double x, double y) const {
    GeoPoint p;
    p.lat = o.lat + y / ky;
    p.lon = o.lon + x / kx;
    p.hae = 9999999.0;
    return p;
  }
};
}  // namespace

GeoPoint GeoMath::Destination(const GeoPoint& from, double bearing_deg, double distance_m) {
  double d = distance_m / Geo::kMeanRadius, br = Rad(bearing_deg);
  double la1 = Rad(from.lat), lo1 = Rad(from.lon);
  double la2 = std::asin(std::sin(la1) * std::cos(d) + std::cos(la1) * std::sin(d) * std::cos(br));
  double lo2 = lo1 + std::atan2(std::sin(br) * std::sin(d) * std::cos(la1), std::cos(d) - std::sin(la1) * std::sin(la2));
  GeoPoint p;
  p.lat = Deg(la2);
  p.lon = std::fmod(Deg(lo2) + 540.0, 360.0) - 180.0;
  p.hae = from.hae;
  return p;
}

double GeoMath::Bearing(const GeoPoint& a, const GeoPoint& b) {
  double la1 = Rad(a.lat), la2 = Rad(b.lat), dlo = Rad(b.lon - a.lon);
  double y = std::sin(dlo) * std::cos(la2);
  double x = std::cos(la1) * std::sin(la2) - std::sin(la1) * std::cos(la2) * std::cos(dlo);
  return std::fmod(Deg(std::atan2(y, x)) + 360.0, 360.0);
}

double GeoMath::DistanceM(const GeoPoint& a, const GeoPoint& b) { return Geo::HaversineM(a.lat, a.lon, b.lat, b.lon); }

bool GeoMath::Contains(const std::vector<GeoPoint>& ring, double lat, double lon) {
  bool in = false;
  size_t n = ring.size();
  for (size_t i = 0, j = n ? n - 1 : 0; i < n; j = i++) {
    const GeoPoint& a = ring[i];
    const GeoPoint& b = ring[j];
    if ((a.lat > lat) != (b.lat > lat) && lon < (b.lon - a.lon) * (lat - a.lat) / (b.lat - a.lat) + a.lon) in = !in;
  }
  return in;
}

CotEvent BuildPoint(const CotBuild& b, const GeoPoint& p, TimeMs now) {
  CotEvent ev = cot_detail::Base(b, "b-m-p-s-m", p, now);
  cot_detail::Remarks(ev, b);
  return ev;
}

CotEvent BuildCircle(const CotBuild& b, const GeoPoint& c, double radius_m, const ShapeStyle& s, TimeMs now) {
  CotEvent ev = ShapeBase(b, "u-d-c-c", c, now);
  std::string r = cot_detail::FormatNum(radius_m);
  ev.AddNode("detail\tshape\tellipse", {{"major", r}, {"minor", r}, {"angle", "360"}});
  AddStyle(ev, s, true);
  return ev;
}

CotEvent BuildPolygon(const CotBuild& b, const std::vector<GeoPoint>& ring_in, const ShapeStyle& s, TimeMs now) {
  std::vector<GeoPoint> ring = ring_in;
  if (ring.size() >= 3 && (ring.front().lat != ring.back().lat || ring.front().lon != ring.back().lon))
    ring.push_back(ring.front());
  CotEvent ev = ShapeBase(b, "u-d-f", Centroid(ring), now);
  AddLinks(ev, ring);
  AddStyle(ev, s, true);
  return ev;
}

CotEvent BuildPolyline(const CotBuild& b, const std::vector<GeoPoint>& pts, const ShapeStyle& s, TimeMs now) {
  CotEvent ev = ShapeBase(b, "u-d-f", pts.empty() ? GeoPoint{} : pts.front(), now);
  AddLinks(ev, pts);
  AddStyle(ev, s, false);
  return ev;
}

CotEvent BuildRectangle(const CotBuild& b, const GeoPoint& c1, const GeoPoint& c2, const ShapeStyle& s, TimeMs now) {
  double n = (std::max)(c1.lat, c2.lat), so = (std::min)(c1.lat, c2.lat);
  double e = (std::max)(c1.lon, c2.lon), w = (std::min)(c1.lon, c2.lon);
  std::vector<GeoPoint> corners{{n, w, 9999999.0}, {n, e, 9999999.0}, {so, e, 9999999.0}, {so, w, 9999999.0}};
  CotEvent ev = ShapeBase(b, "u-d-r", GeoPoint{(n + so) / 2, (e + w) / 2, 9999999.0}, now);
  AddLinks(ev, corners);
  AddStyle(ev, s, true);
  return ev;
}

CotEvent BuildSector(const CotBuild& b, const GeoPoint& c, double radius_m, double from_deg, double to_deg,
                     const ShapeStyle& s, TimeMs now, int arc_points) {
  double span = std::fmod(to_deg - from_deg + 720.0, 360.0);
  if (span == 0) span = 360.0;
  std::vector<GeoPoint> ring{c};
  int n = (std::max)(2, arc_points);
  for (int i = 0; i <= n; ++i) ring.push_back(GeoMath::Destination(c, from_deg + span * i / n, radius_m));
  ring.push_back(c);
  for (auto& p : ring) p.hae = 9999999.0;
  CotEvent ev = ShapeBase(b, "u-d-f", c, now);
  AddLinks(ev, ring);
  AddStyle(ev, s, true);
  return ev;
}

std::vector<CotEvent> BuildRangeRings(const CotBuild& b, const GeoPoint& c, const std::vector<double>& radii_m,
                                      const ShapeStyle& s, TimeMs now) {
  std::vector<CotEvent> out;
  for (size_t i = 0; i < radii_m.size(); ++i) {
    CotBuild bi = b;
    bi.uid = b.uid + ".r" + std::to_string(i + 1);
    if (!b.callsign.empty()) bi.callsign = b.callsign + " " + cot_detail::FormatNum(radii_m[i]) + " m";
    CotEvent ev = ShapeBase(bi, "u-r-b-c-c", c, now);
    std::string r = cot_detail::FormatNum(radii_m[i]);
    ev.AddNode("detail\tshape\tellipse", {{"major", r}, {"minor", r}, {"angle", "360"}});
    AddStyle(ev, s, true);
    out.push_back(ev);
  }
  return out;
}

CotEvent BuildRangeBearing(const CotBuild& b, const GeoPoint& from, const GeoPoint& to, const ShapeStyle& s, TimeMs now) {
  CotEvent ev = ShapeBase(b, "u-rb-a", from, now);
  ev.AddNode("detail\trange", {{"value", cot_detail::FormatNum(GeoMath::DistanceM(from, to))}});
  ev.AddNode("detail\tbearing", {{"value", cot_detail::FormatNum(GeoMath::Bearing(from, to))}});
  ev.AddNode("detail\tinclination", {{"value", "0.0"}});
  ev.AddNode("detail\trangeUnits", {{"value", "1"}});
  ev.AddNode("detail\tbearingUnits", {{"value", "0"}});
  ev.AddNode("detail\tnorthRef", {{"value", "1"}});
  AddStyle(ev, s, false);
  return ev;
}

std::vector<GeoPoint> BufferRing(const std::vector<GeoPoint>& pts, double r, int arc_points) {
  std::vector<GeoPoint> ring;
  if (pts.empty() || r <= 0) return ring;
  Plane pl(pts.front());
  std::vector<std::pair<double, double>> xy;
  for (const auto& p : pts) {
    double x, y;
    pl.To(p, x, y);
    if (xy.empty() || std::hypot(x - xy.back().first, y - xy.back().second) > 1e-6) xy.push_back({x, y});
  }
  int per_circle = (std::max)(8, arc_points * 4);
  double step = 2 * kPi / per_circle;
  std::vector<std::pair<double, double>> out;
  auto arc = [&](double cx, double cy, double a0, double a1) {  // clockwise from a0 down to a1
    while (a1 > a0) a1 -= 2 * kPi;
    int n = (std::max)(1, static_cast<int>(std::ceil((a0 - a1) / step)));
    for (int i = 0; i <= n; ++i) {
      double a = a0 + (a1 - a0) * i / n;
      out.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
  };
  if (xy.size() == 1) {
    arc(xy[0].first, xy[0].second, 0.0, -2 * kPi + step);
  } else {
    auto side = [&](const std::vector<std::pair<double, double>>& s) {
      size_t n = s.size();
      std::vector<double> th(n - 1);
      for (size_t i = 0; i + 1 < n; ++i) th[i] = std::atan2(s[i + 1].second - s[i].second, s[i + 1].first - s[i].first);
      out.push_back({s[0].first + r * std::cos(th[0] + kPi / 2), s[0].second + r * std::sin(th[0] + kPi / 2)});
      for (size_t j = 1; j + 1 < n; ++j) {
        double turn = std::remainder(th[j] - th[j - 1], 2 * kPi);
        if (turn < 0) {
          arc(s[j].first, s[j].second, th[j - 1] + kPi / 2, th[j] + kPi / 2);  // outer side: round join
        } else {
          out.push_back({s[j].first + r * std::cos(th[j - 1] + kPi / 2), s[j].second + r * std::sin(th[j - 1] + kPi / 2)});
          out.push_back({s[j].first + r * std::cos(th[j] + kPi / 2), s[j].second + r * std::sin(th[j] + kPi / 2)});
        }
      }
      double tl = th[n - 2];
      arc(s[n - 1].first, s[n - 1].second, tl + kPi / 2, tl - kPi / 2);  // round cap
    };
    side(xy);
    std::vector<std::pair<double, double>> rev(xy.rbegin(), xy.rend());
    side(rev);
  }
  for (const auto& q : out) ring.push_back(pl.From(q.first, q.second));
  if (!ring.empty()) ring.push_back(ring.front());
  return ring;
}

CotEvent BuildBuffer(const CotBuild& b, const std::vector<GeoPoint>& pts, double distance_m, const ShapeStyle& s,
                     TimeMs now) {
  return BuildPolygon(b, BufferRing(pts, distance_m), s, now);
}

bool ParseShape(const std::string& xml, Shape& out) {
  CotInfo c;
  if (!ParseCot(xml, c)) return false;
  CotEvent ev;
  if (!ev.PopulateFromXML(xml)) return false;
  out = Shape();
  out.uid = c.uid;
  out.cot_type = c.type;
  out.label = c.callsign;
  out.center = c.point;
  out.points = c.link_points;
  out.style.stroke_argb = ParseArgb(ev.GetAttribute("detail\tstrokeColor\tvalue"), out.style.stroke_argb);
  out.style.stroke_weight = ev.GetAttributeAsDouble("detail\tstrokeWeight\tvalue", out.style.stroke_weight);
  out.style.fill_argb = ParseArgb(ev.GetAttribute("detail\tfillColor\tvalue"), 0);
  out.style.labels_on = ev.GetAttribute("detail\tlabels_on\tvalue") != "false";
  if (c.type == "u-d-c-c" || c.type == "u-r-b-c-c") {
    out.kind = c.type == "u-d-c-c" ? ShapeKind::Circle : ShapeKind::RangeRing;
    out.radius_m = ev.GetAttributeAsDouble("detail\tshape\tellipse\tmajor", 0);
    return out.radius_m > 0;
  }
  if (c.type == "u-d-r") {
    out.kind = ShapeKind::Rectangle;
    return out.points.size() == 4;
  }
  if (c.type == "u-d-f") {
    const auto& p = out.points;
    bool closed = p.size() >= 4 && p.front().lat == p.back().lat && p.front().lon == p.back().lon;
    out.kind = closed ? ShapeKind::Polygon : ShapeKind::Line;
    return p.size() >= 2;
  }
  if (c.type == "u-rb-a") {
    out.kind = ShapeKind::RangeBearing;
    out.range_m = ev.GetAttributeAsDouble("detail\trange\tvalue", 0);
    out.bearing_deg = ev.GetAttributeAsDouble("detail\tbearing\tvalue", 0);
    return true;
  }
  if (c.type == "b-m-r") {
    out.kind = ShapeKind::Route;
    return !out.points.empty();
  }
  if (c.type == "b-m-p-s-m") {
    out.kind = ShapeKind::Point;
    return true;
  }
  return false;
}

std::vector<std::string> LassoSelect(const std::vector<GeoPoint>& ring, const std::vector<LassoCandidate>& candidates) {
  std::vector<std::string> out;
  if (ring.size() < 3) return out;
  for (const auto& c : candidates)
    if (GeoMath::Contains(ring, c.lat, c.lon)) out.push_back(c.id);
  return out;
}

}  // namespace om
