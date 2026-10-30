// Batched-rendering decisions (feature B5), engine-independent so they are tested on the
// host: horizon culling, distance bands (icons far, instanced models mid, actors near),
// a cap on near actors, and screen-space clustering above a density threshold.
// The Unreal renderer (AOneMindEntityRenderer) applies the plan with one instanced
// component per symbol class and band (unreal-platform.md "10,000 entities at 60 fps").
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Entity.h"
#include "OneMindCore/Geo.h"

#include <vector>

namespace om {

enum class Band : uint8_t { Culled, Icon, Model, Actor, Clustered };

struct OM_CORE_API RenderPlanConfig {
  double actor_max_m = 2000;     // nearer than this: full actor (label, selection)
  double model_max_m = 60000;    // nearer than this: instanced 3D model
  size_t max_actors = 64;        // nearest N only; the rest drop to Model
  double cluster_cell_deg = 0;   // 0 = automatic from camera altitude
  size_t cluster_min = 8;        // a cell with at least this many icons becomes a cluster
  bool horizon_cull = true;
};

struct OM_CORE_API Cluster {
  double lat = 0, lon = 0;
  size_t count = 0;
};

struct OM_CORE_API RenderPlan {
  std::vector<Band> band;  // per store index
  std::vector<Cluster> clusters;
  size_t culled = 0, icons = 0, models = 0, actors = 0, clustered = 0;
};

/// Computes the plan for every entity in `store` from a camera at (lat, lon, alt).
OM_CORE_API RenderPlan PlanRender(const EntityStore& store, double cam_lat, double cam_lon, double cam_alt_m,
                                  const RenderPlanConfig& cfg = RenderPlanConfig());

/// Cluster cell size in degrees for a camera altitude (about 1/40 of the view width).
OM_CORE_API double AutoClusterCellDeg(double cam_alt_m);

}  // namespace om
