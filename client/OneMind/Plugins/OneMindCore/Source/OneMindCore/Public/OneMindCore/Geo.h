// WGS-84 geodesy used outside the engine: ECEF/ENU conversions, horizon culling,
// distances and Web-Mercator tile maths for AOI zoom (layer-snapshot.md §3.1 `zoom`).
// Inside Unreal, placement on the globe goes through ACesiumGeoreference; these helpers
// drive level-of-detail, culling and clustering decisions that are tested on the host.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"

namespace om {

struct Vec3 {
  double x = 0, y = 0, z = 0;
};

struct OM_CORE_API Geo {
  static constexpr double kA = 6378137.0;                 // WGS-84 semi-major axis (m)
  static constexpr double kF = 1.0 / 298.257223563;       // flattening
  static constexpr double kB = kA * (1.0 - kF);           // semi-minor axis
  static constexpr double kE2 = kF * (2.0 - kF);          // first eccentricity squared
  static constexpr double kMeanRadius = 6371008.8;

  static Vec3 LlhToEcef(double lat_deg, double lon_deg, double h_m);
  static void EcefToLlh(const Vec3& p, double& lat_deg, double& lon_deg, double& h_m);
  /// East-North-Up offset of `p` from the reference point (lat0, lon0, h0).
  static Vec3 EcefToEnu(const Vec3& p, double lat0_deg, double lon0_deg, double h0_m);
  static double Distance(const Vec3& a, const Vec3& b);
  /// Great-circle distance on the mean sphere (haversine), metres.
  static double HaversineM(double lat1, double lon1, double lat2, double lon2);
  /// True if `target` is visible from `camera` over the ellipsoid (scaled-space
  /// horizon test, as in Cesium's EllipsoidalOccluder). Points above the ellipsoid
  /// are tested against the ellipsoid only (terrain ignored).
  static bool AboveHorizon(const Vec3& camera_ecef, const Vec3& target_ecef);
  /// Web-Mercator XYZ tile containing (lat, lon) at zoom z.
  static void LatLonToTile(double lat, double lon, int z, int& x, int& y);
  /// Approximate XYZ zoom for a camera at `altitude_m` above the ground (one 256 px tile
  /// spanning roughly the visible width); clamped to 0..22.
  static int ZoomForAltitude(double altitude_m);
};

}  // namespace om
