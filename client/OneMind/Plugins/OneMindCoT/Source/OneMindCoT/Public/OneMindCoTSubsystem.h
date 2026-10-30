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

	// --- Drawing and tactical graphics (B11) -------------------------------------------
	// Each shape goes to svc.cot.send as kind "shape" (Docs/svc-cot-send.md), through the
	// projector's gates. A copy over the client's own TAK stream (C3) is sent only with
	// UOneMindTakSettings::bSendShapesOnOwnStream, off until proposal UE-P5. Layouts:
	// OneMindCore/Drawing.h. Colours are ARGB 0xAARRGGBB as integers (4294967040 = opaque yellow); FillArgb 0 = no fill.

	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendPoint(double Lat, double Lon, const FString& Label);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendLine(const TArray<FVector2D>& LatLonPoints, const FString& Label, int64 StrokeArgb = 4294967295);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendPolygon(const TArray<FVector2D>& LatLonRing, const FString& Label, int64 StrokeArgb = 4294967040, int64 FillArgb = 1090518784);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendRectangle(FVector2D CornerLatLon1, FVector2D CornerLatLon2, const FString& Label, int64 StrokeArgb = 4294967040, int64 FillArgb = 0);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendCircle(double Lat, double Lon, double RadiusMeters, const FString& Label, int64 StrokeArgb = 4294967040, int64 FillArgb = 0);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendSector(double Lat, double Lon, double RadiusMeters, double FromBearingDeg, double ToBearingDeg, const FString& Label);

	/** One ring per radius, sharing the centre. Returns the first uid. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendRangeRings(double Lat, double Lon, const TArray<double>& RadiiMeters, const FString& Label);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendRangeBearing(FVector2D FromLatLon, FVector2D ToLatLon, const FString& Label);

	/** Buffer around a point (one vertex) or a line, sent as a polygon. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	FString SendBuffer(const TArray<FVector2D>& LatLonPoints, double DistanceMeters, const FString& Label);

	/** Lasso: ids of the given candidates inside the ring (local, nothing is sent). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Draw")
	TArray<FString> LassoSelect(const TArray<FVector2D>& LatLonRing, const TArray<FString>& Ids, const TArray<FVector2D>& LatLons) const;

	UPROPERTY(BlueprintAssignable, Category = "OneMind|CoT")
	FOnOneMindCotSent OnSent;
};
