// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Geo.h"

#include <algorithm>
#include <cmath>

namespace om {

namespace geo_detail {
constexpr double kDeg = 3.14159265358979323846 / 180.0;
}

Vec3 Geo::LlhToEcef(double lat_deg, double lon_deg, double h_m) {
  double lat = lat_deg * geo_detail::kDeg, lon = lon_deg * geo_detail::kDeg;
  double s = std::sin(lat), c = std::cos(lat);
  double n = kA / std::sqrt(1.0 - kE2 * s * s);
  return Vec3{(n + h_m) * c * std::cos(lon), (n + h_m) * c * std::sin(lon), (n * (1.0 - kE2) + h_m) * s};
}

void Geo::EcefToLlh(const Vec3& p, double& lat_deg, double& lon_deg, double& h_m) {
  // Bowring's method with a few iterations (sub-millimetre for terrestrial points).
  double lon = std::atan2(p.y, p.x);
  double r = std::sqrt(p.x * p.x + p.y * p.y);
  double lat = std::atan2(p.z, r * (1.0 - kE2));
  double h = 0;
  for (int i = 0; i < 6; ++i) {
    double s = std::sin(lat);
    double n = kA / std::sqrt(1.0 - kE2 * s * s);
    h = r / std::cos(lat) - n;
    lat = std::atan2(p.z, r * (1.0 - kE2 * n / (n + h)));
  }
  lat_deg = lat / geo_detail::kDeg;
  lon_deg = lon / geo_detail::kDeg;
  h_m = h;
}

Vec3 Geo::EcefToEnu(const Vec3& p, double lat0_deg, double lon0_deg, double h0_m) {
  Vec3 o = LlhToEcef(lat0_deg, lon0_deg, h0_m);
  double dx = p.x - o.x, dy = p.y - o.y, dz = p.z - o.z;
  double lat = lat0_deg * geo_detail::kDeg, lon = lon0_deg * geo_detail::kDeg;
  double sl = std::sin(lat), cl = std::cos(lat), so = std::sin(lon), co = std::cos(lon);
  return Vec3{-so * dx + co * dy, -sl * co * dx - sl * so * dy + cl * dz, cl * co * dx + cl * so * dy + sl * dz};
}

double Geo::Distance(const Vec3& a, const Vec3& b) {
  double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double Geo::HaversineM(double lat1, double lon1, double lat2, double lon2) {
  double p1 = lat1 * geo_detail::kDeg, p2 = lat2 * geo_detail::kDeg;
  double dp = (lat2 - lat1) * geo_detail::kDeg, dl = (lon2 - lon1) * geo_detail::kDeg;
  double h = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  return 2 * kMeanRadius * std::asin((std::min)(1.0, std::sqrt(h)));
}

bool Geo::AboveHorizon(const Vec3& camera, const Vec3& target) {
  // Scale space: the ellipsoid becomes the unit sphere.
  Vec3 cv{camera.x / kA, camera.y / kA, camera.z / kB};
  Vec3 t{target.x / kA, target.y / kA, target.z / kB};
  double vh_mag2 = cv.x * cv.x + cv.y * cv.y + cv.z * cv.z - 1.0;
  if (vh_mag2 <= 0) return true;  // camera inside the ellipsoid: do not cull
  Vec3 vt{t.x - cv.x, t.y - cv.y, t.z - cv.z};
  double vt_dot_vc = -(vt.x * cv.x + vt.y * cv.y + vt.z * cv.z);
  bool occluded = vt_dot_vc > vh_mag2 &&
                  (vt_dot_vc * vt_dot_vc) / (vt.x * vt.x + vt.y * vt.y + vt.z * vt.z) > vh_mag2;
  return !occluded;
}

void Geo::LatLonToTile(double lat, double lon, int z, int& x, int& y) {
  lat = (std::max)(-85.05112878, (std::min)(85.05112878, lat));
  double n = std::ldexp(1.0, z);
  double lr = lat * geo_detail::kDeg;
  double fx = (lon + 180.0) / 360.0 * n;
  double fy = (1.0 - std::asinh(std::tan(lr)) / 3.14159265358979323846) / 2.0 * n;
  x = static_cast<int>((std::min)(n - 1, (std::max)(0.0, std::floor(fx))));
  y = static_cast<int>((std::min)(n - 1, (std::max)(0.0, std::floor(fy))));
}

int Geo::ZoomForAltitude(double altitude_m) {
  // At zoom z one 256 px tile spans 40075016.7 / 2^z metres at the equator. A camera
  // with a ~60 degree field of view sees a width of about 1.15 x altitude.
  double width = (std::max)(1.0, altitude_m * 1.15);
  double z = std::log2(40075016.7 / width);
  return static_cast<int>((std::max)(0.0, (std::min)(22.0, std::floor(z))));
}

}  // namespace om
