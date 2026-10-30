// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/RenderPlan.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace om {

double AutoClusterCellDeg(double cam_alt_m) {
  // View width ~ 1.15 x altitude (60 degree FOV); 1 degree ~ 111 km.
  double width_deg = (std::max)(1.0, cam_alt_m * 1.15) / 111000.0;
  return (std::max)(0.0005, width_deg / 40.0);
}

RenderPlan PlanRender(const EntityStore& store, double cam_lat, double cam_lon, double cam_alt_m,
                      const RenderPlanConfig& cfg) {
  RenderPlan plan;
  const size_t n = store.size();
  plan.band.assign(n, Band::Culled);
  Vec3 cam = Geo::LlhToEcef(cam_lat, cam_lon, cam_alt_m);
  const auto& lats = store.lats();
  const auto& lons = store.lons();
  const auto& haes = store.haes();
  std::vector<std::pair<double, size_t>> near;
  std::vector<size_t> icons;
  const auto& loc = store.locatables();
  for (size_t i = 0; i < n; ++i) {
    if (!loc[i]) {
      plan.culled++;
      continue;
    }
    double h = haes[i] > -1000 && haes[i] < 1e6 ? haes[i] : 0;
    Vec3 p = Geo::LlhToEcef(lats[i], lons[i], h);
    if (cfg.horizon_cull && !Geo::AboveHorizon(cam, p)) {
      plan.culled++;
      continue;
    }
    double d = Geo::Distance(cam, p);
    if (d < cfg.actor_max_m) {
      near.emplace_back(d, i);
    } else if (d < cfg.model_max_m) {
      plan.band[i] = Band::Model;
    } else {
      plan.band[i] = Band::Icon;
      icons.push_back(i);
    }
  }
  std::sort(near.begin(), near.end());
  for (size_t k = 0; k < near.size(); ++k) plan.band[near[k].second] = k < cfg.max_actors ? Band::Actor : Band::Model;

  // Cluster dense icon cells.
  double cell = cfg.cluster_cell_deg > 0 ? cfg.cluster_cell_deg : AutoClusterCellDeg(cam_alt_m);
  std::unordered_map<int64_t, std::vector<size_t>> cells;
  for (size_t i : icons) {
    int64_t a = static_cast<int64_t>(std::floor((lats[i] + 90.0) / cell));
    int64_t b = static_cast<int64_t>(std::floor((lons[i] + 180.0) / cell));
    cells[a * 1000003 + b].push_back(i);
  }
  for (auto& kv : cells) {
    if (kv.second.size() < cfg.cluster_min) continue;
    Cluster c;
    for (size_t i : kv.second) {
      c.lat += lats[i];
      c.lon += lons[i];
      plan.band[i] = Band::Clustered;
    }
    c.count = kv.second.size();
    c.lat /= static_cast<double>(c.count);
    c.lon /= static_cast<double>(c.count);
    plan.clusters.push_back(c);
  }
  for (Band b : plan.band) {
    switch (b) {
      case Band::Icon: plan.icons++; break;
      case Band::Model: plan.models++; break;
      case Band::Actor: plan.actors++; break;
      case Band::Clustered: plan.clustered++; break;
      default: break;
    }
  }
  return plan;
}

}  // namespace om
