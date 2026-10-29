// Drawing and tactical graphics shared as CoT (feature B11).
//
// Shapes use the CoT layouts of the TAK examples in atak-civ `takcot/examples/*.cot`
// (commit 9f6893dd), which ATAK itself wrote:
//   circle          u-d-c-c   centre in <point>; <shape><ellipse major minor angle="360"/>
//   free form       u-d-f     one <link point="lat,lon"/> per vertex; closed = first vertex repeated
//   rectangle       u-d-r     four <link point> corners (the example lists NW, NE, SE, SW)
//   R&B circle      u-r-b-c-c as circle (range rings: one per radius, sharing the centre)
//   R&B line        u-rb-a    <range value> (m), <bearing value> (deg), <inclination>,
//                             <rangeUnits value="1">, <bearingUnits value="0">, <northRef value="1">
//                             copied from the example; their unit meanings are unverified
//   route           b-m-r     existing BuildRoute (cot-projection.md §5.5)
// Styling: <strokeColor value>, <strokeWeight value>, <fillColor value> as signed 32-bit
// ARGB, <labels_on value>, <contact callsign> as the label (same example files).
// Unverified until the owner's ATAK check: whether `major`/`minor` are radii (assumed, the
// example's values cannot tell), and whether an open u-d-f (last vertex not repeated) shows
// as a line. Sector and buffer have no TAK type; they are sent as closed u-d-f polygons.
// Lasso selection is local only (no CoT).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/CoT.h"

#include <string>
#include <vector>

namespace om {

struct OM_CORE_API ShapeStyle {
  uint32_t stroke_argb = 0xffffffffu;
  double stroke_weight = 3.0;
  uint32_t fill_argb = 0x00000000u;  // 0 = no fill
  bool labels_on = true;
};

enum class ShapeKind { Unknown, Point, Line, Polygon, Rectangle, Circle, Sector, RangeRing, RangeBearing, Route };

/// A shape read back from CoT (for display of received shapes and round-trip checks).
struct OM_CORE_API Shape {
  ShapeKind kind = ShapeKind::Unknown;
  std::string uid, cot_type, label;
  GeoPoint center;
  std::vector<GeoPoint> points;  // vertices (polygon closed: first point repeated)
  double radius_m = 0;
  double range_m = 0, bearing_deg = 0;
  ShapeStyle style;
};

struct OM_CORE_API GeoMath {
  /// Point at `distance_m` along initial bearing `bearing_deg` (spherical earth, mean radius).
  static GeoPoint Destination(const GeoPoint& from, double bearing_deg, double distance_m);
  /// Initial great-circle bearing from a to b, degrees in [0, 360).
  static double Bearing(const GeoPoint& a, const GeoPoint& b);
  static double DistanceM(const GeoPoint& a, const GeoPoint& b);
  /// Ray-casting point-in-polygon on lat/lon (ring may be open or closed).
  static bool Contains(const std::vector<GeoPoint>& ring, double lat, double lon);
};

OM_CORE_API CotEvent BuildPoint(const CotBuild& b, const GeoPoint& p, TimeMs now);  // spot marker b-m-p-s-m
OM_CORE_API CotEvent BuildCircle(const CotBuild& b, const GeoPoint& c, double radius_m, const ShapeStyle& s, TimeMs now);
OM_CORE_API CotEvent BuildPolygon(const CotBuild& b, const std::vector<GeoPoint>& ring, const ShapeStyle& s, TimeMs now);
OM_CORE_API CotEvent BuildPolyline(const CotBuild& b, const std::vector<GeoPoint>& pts, const ShapeStyle& s, TimeMs now);
/// Axis-aligned rectangle from two opposite corners.
OM_CORE_API CotEvent BuildRectangle(const CotBuild& b, const GeoPoint& c1, const GeoPoint& c2, const ShapeStyle& s,
                                    TimeMs now);
/// Pie sector from bearing `from_deg` clockwise to `to_deg`, as a closed u-d-f.
OM_CORE_API CotEvent BuildSector(const CotBuild& b, const GeoPoint& c, double radius_m, double from_deg, double to_deg,
                                 const ShapeStyle& s, TimeMs now, int arc_points = 24);
/// One u-r-b-c-c per radius; uids "{uid}.r{n}".
OM_CORE_API std::vector<CotEvent> BuildRangeRings(const CotBuild& b, const GeoPoint& c, const std::vector<double>& radii_m,
                                                  const ShapeStyle& s, TimeMs now);
OM_CORE_API CotEvent BuildRangeBearing(const CotBuild& b, const GeoPoint& from, const GeoPoint& to, const ShapeStyle& s,
                                       TimeMs now);
/// Buffer around a point (circle polygon) or a polyline (round joins and caps), as a closed
/// ring. Computed in a local tangent plane around the first point: good to well under 1 %
/// for shapes a few tens of km across. Very sharp turns can self-intersect on the inner side.
OM_CORE_API std::vector<GeoPoint> BufferRing(const std::vector<GeoPoint>& pts, double distance_m, int arc_points = 16);
OM_CORE_API CotEvent BuildBuffer(const CotBuild& b, const std::vector<GeoPoint>& pts, double distance_m,
                                 const ShapeStyle& s, TimeMs now);

/// Reads a shape back from CoT XML. Returns false for non-shape types.
OM_CORE_API bool ParseShape(const std::string& xml, Shape& out);

/// Lasso selection: ids of the candidates inside the ring.
struct OM_CORE_API LassoCandidate {
  std::string id;
  double lat = 0, lon = 0;
};
OM_CORE_API std::vector<std::string> LassoSelect(const std::vector<GeoPoint>& ring,
                                                 const std::vector<LassoCandidate>& candidates);

}  // namespace om
