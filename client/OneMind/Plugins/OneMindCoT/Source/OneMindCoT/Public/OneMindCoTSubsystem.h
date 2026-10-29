// Sending CoT from the client (feature C2). Every send goes to `svc.cot.send`, served by
// the omos projector, which applies its gates and publishes tak.cot.inject (TAK Server)
// and tak.cot.mesh.tx (mesh). The client never publishes tak.cot.* itself
// (cot-projection.md §1 "Who may publish", §9). Request body: Docs/svc-cot-send.md.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OneMindCoTSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnOneMindCotSent, bool, bOk, const FString&, Uid, const FString&, Error);

UCLASS()
class ONEMINDCOT_API UOneMindCoTSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** A marker (CoT atom, e.g. a-u-G unknown ground point). Hostile atoms and tasking
	 *  types are refused locally (civilian scope; tasking goes through policy). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|CoT")
	FString SendMarker(double Lat, double Lon, const FString& Callsign, const FString& CotType = TEXT("a-u-G"), const FString& Remarks = TEXT(""));

	/** Sensor point of interest b-m-p-s-p-i (ephemeral, cot-projection.md §9). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|CoT")
	FString SendSpi(double Lat, double Lon, const FString& Callsign);

	/** Route b-m-r; points are (lat, lon). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|CoT")
	FString SendRoute(const TArray<FVector2D>& LatLonPoints, const FString& Callsign);

	/** Polygon u-d-f; Kind: aoi | keep_in | keep_out | hazard | control_area. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|CoT")
	FString SendGeofence(const TArray<FVector2D>& LatLonRing, const FString& Callsign, const FString& Kind = TEXT("aoi"));

	/** GeoChat to a room (default "All Chat Rooms"). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|CoT")
	FString SendChat(const FString& Text, const FString& Room = TEXT(""));

	UPROPERTY(BlueprintAssignable, Category = "OneMind|CoT")
	FOnOneMindCotSent OnSent;
};
