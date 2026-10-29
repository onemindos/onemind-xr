// On-screen overlay built in Slate code (no UMG assets): bus status, the layer list with
// toggle keys, renderer counters, the entity inspector (B6: sources, confidence breakdown,
// history, raw CoT) and the map attribution.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OneMindOverlaySubsystem.generated.h"

class SWidget;

UCLASS()
class ONEMINDENTITIES_API UOneMindOverlaySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** Selects "layer/entity_id" (from the renderer's pick) and loads the inspector. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Inspector")
	void SelectEntity(const FString& LayerAndId);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Inspector")
	void ClearSelection();

	FText GetStatusText() const;
	FText GetInspectorText() const;
	FText GetAttributionText() const;

private:
	TSharedPtr<SWidget> Overlay;
	FString SelectedId;
	FString InspectorText;
	FString RawCot;

	// Frame timing for the status line (B1/B4 read fps here: no `stat` console needed).
	// Sampled when the status text is evaluated, which Slate does once per painted frame.
	mutable double PerfWindowStart = 0.0;
	mutable double PerfWorstMs = 0.0;
	mutable double PerfShownWorstMs = 0.0;
	FString GetPerfLine() const;
};
