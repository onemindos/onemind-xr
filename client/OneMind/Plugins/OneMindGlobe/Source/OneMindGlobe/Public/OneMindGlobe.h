// The OneMind globe: a Cesium georeference, one terrain tileset (ellipsoid or URL) and one
// open raster overlay, built in code from UOneMindGlobeSettings so the project needs no
// level assets. Feature B2 (OneMindGlobe), decision 0004 #4.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OneMindGlobe.generated.h"

class ACesiumGeoreference;
class ACesium3DTileset;
class UCesiumUrlTemplateRasterOverlay;

UCLASS()
class ONEMINDGLOBE_API AOneMindGlobe : public AActor
{
	GENERATED_BODY()

public:
	AOneMindGlobe();

	/** Finds the globe in the world, spawning one if needed. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Globe", meta = (WorldContext = "WorldContextObject"))
	static AOneMindGlobe* GetOrSpawn(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "OneMind|Globe")
	ACesiumGeoreference* GetGeoreference() const { return Georeference; }

	/** WGS-84 lon/lat/height (degrees, degrees, metres) to an Unreal world position. */
	UFUNCTION(BlueprintPure, Category = "OneMind|Globe")
	FVector LonLatHeightToWorld(const FVector& LonLatHeight) const;

	/** Unreal world position to WGS-84 lon/lat/height. */
	UFUNCTION(BlueprintPure, Category = "OneMind|Globe")
	FVector WorldToLonLatHeight(const FVector& World) const;

	UFUNCTION(BlueprintPure, Category = "OneMind|Globe")
	FString GetAttribution() const;

protected:
	virtual void BeginPlay() override;

private:
	void BuildGlobe();

	UPROPERTY()
	TObjectPtr<ACesiumGeoreference> Georeference;

	UPROPERTY()
	TObjectPtr<ACesium3DTileset> Terrain;

	UPROPERTY()
	TObjectPtr<UCesiumUrlTemplateRasterOverlay> Imagery;
};
