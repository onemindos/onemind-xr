// Globe settings (Edit > Project Settings > OneMind > Globe). Decision 0004 #4: open data
// by default, Cesium ion optional and off, Google Photorealistic 3D Tiles off.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OneMindGlobeSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "OneMind Globe"))
class ONEMINDGLOBE_API UOneMindGlobeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("OneMind"); }

	/** Terrain: a quantized-mesh `layer.json` or 3D Tiles `tileset.json` URL (http(s)://
	 *  or file:///). Empty = the WGS-84 ellipsoid (Cesium "From Ellipsoid", no network,
	 *  no token). No keyless public terrain endpoint is assumed (OWNER-STEPS.md, S1). */
	UPROPERTY(config, EditAnywhere, Category = "Terrain")
	FString TerrainUrl;

	/** Raster imagery URL template with {x} {y} {z}. Default: OpenStreetMap standard tiles
	 *  (ODbL data, tile usage policy: identify the app, no bulk download, show the
	 *  attribution). For offline or heavier use, point this at the local caching tile
	 *  proxy or a TileServer GL / PMTiles raster endpoint (edge-and-ddil.md Rec. 3). */
	UPROPERTY(config, EditAnywhere, Category = "Imagery")
	FString ImageryUrlTemplate;

	UPROPERTY(config, EditAnywhere, Category = "Imagery")
	int32 ImageryMaximumLevel = 19;

	/** HTTP headers for imagery requests (the OSM policy asks for an identifying User-Agent). */
	UPROPERTY(config, EditAnywhere, Category = "Imagery")
	TMap<FString, FString> ImageryRequestHeaders;

	/** Shown on screen at all times (licence duty for OSM-derived imagery). */
	UPROPERTY(config, EditAnywhere, Category = "Imagery")
	FString Attribution;

	/** Cesium screen-space error for the terrain tileset (16 desktop; 256 headset). */
	UPROPERTY(config, EditAnywhere, Category = "Performance")
	double MaximumScreenSpaceError = 16.0;

	/** Camera start (lon, lat, height above ellipsoid in m). Default: Austin, TX at 8 km,
	 *  where the scripted sim-x1 flies (scripts/sim/mavlink_sim_drone.py). */
	UPROPERTY(config, EditAnywhere, Category = "Camera")
	FVector StartLongitudeLatitudeHeight = FVector(-97.7431, 30.2672, 8000.0);
};
