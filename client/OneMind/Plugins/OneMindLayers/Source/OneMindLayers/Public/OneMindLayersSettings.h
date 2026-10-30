// Layer catalogue (Edit > Project Settings > OneMind > Layers). Layer ids and entity types
// follow layer-snapshot.md §1. `path` picks the read path: watch (svc.layer.*, the
// required path), direct (ENT ordered consumer, trusted site clients) or core (interim).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OneMindLayersSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "OneMind Layers"))
class ONEMINDLAYERS_API UOneMindLayersSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("OneMind"); }

	/** JSON: {"layers":[{"id","title","entity_types":[],"path","tier","zoom","max"}]} */
	UPROPERTY(config, EditAnywhere, Category = "Layers", meta = (MultiLine = true))
	FString LayersJson;

	/** Layers switched on at start. */
	UPROPERTY(config, EditAnywhere, Category = "Layers")
	TArray<FString> DefaultOn;

	/** Intelligence-gaps panel (B20): which feeds each layer and alert kind depends on, and the
	 *  feeds that must exist (with their freshness limit in seconds) even before any heartbeat.
	 *  JSON: {"layers":{"weather":["nws-alerts"]},"alerts":{"severe_weather":["nws-alerts"]},
	 *  "expect":{"nws-alerts":900}}. Layer manifests add their own feeds at run time. */
	UPROPERTY(config, EditAnywhere, Category = "Gaps", meta = (MultiLine = true))
	FString GapsJson;

	/** Seconds between AOI updates from the camera (watch-path and large layers only). */
	UPROPERTY(config, EditAnywhere, Category = "Layers", meta = (ClampMin = "0.5"))
	float AoiUpdateSeconds = 2.0f;
};
