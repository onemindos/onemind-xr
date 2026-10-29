// Registers the layer catalogue with the client core and exposes layer toggles. Toggling a
// layer on starts its snapshot plus delta session; off stops it and drops every
// subscription, watch and consumer it held (B4 acceptance: "a layer off means no
// subscription"). Keeps each layer's AOI in step with the camera.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "OneMindCore/Layers.h"

#include "OneMindLayerSubsystem.generated.h"

class UOneMindBusSubsystem;

USTRUCT(BlueprintType)
struct FOneMindLayerStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") bool bOn = false;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") FString State;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") FString Path;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") int32 Entities = 0;
	/** Manifest freshness badge: fresh | degraded | stale | down | unknown (layer-snapshot.md §5). */
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") FString Freshness;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Layers") FString LastError;
};

UCLASS()
class ONEMINDLAYERS_API UOneMindLayerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	UFUNCTION(BlueprintCallable, Category = "OneMind|Layers")
	bool SetLayerEnabled(const FString& LayerId, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Layers")
	bool ToggleLayer(const FString& LayerId);

	UFUNCTION(BlueprintPure, Category = "OneMind|Layers")
	TArray<FOneMindLayerStatus> GetLayers() const;

	/** Sets the AOI explicitly (otherwise it follows the camera). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Layers")
	void SetAoi(double Lon0, double Lat0, double Lon1, double Lat1, int32 Zoom);

	/** Core access for renderers (C++ only). */
	om::LayerManager* GetManager() const;

private:
	void RegisterCatalogue();
	void UpdateAoiFromCamera();

	TWeakObjectPtr<UOneMindBusSubsystem> Bus;
	TArray<FString> LayerIds;
	TMap<FString, FString> Titles;
	bool bRegistered = false;
	bool bDefaultsApplied = false;
	bool bManualAoi = false;
	float AoiTimer = 0.f;
};
