// Mouse and keyboard for the desktop client without input assets: left click selects
// (inspector), 1-9 toggle layers, M sends a marker at the cursor through svc.cot.send,
// Esc clears the selection. Flight is the Cesium globe-aware pawn's.
// M4 (Act): T proposes "go to the cursor" for the selected sim-* asset (D3) and opens the
// confirm dialog; Y confirms and N rejects, only while the dialog shows the parameters and
// hash. B cycles the drawing tools (B11); while a tool is on, left clicks add points, Enter
// sends the shape, Backspace cancels; the lasso selects instead of sending. G toggles the
// intelligence-gaps panel (B20). TAK (C3, C8) and missions (C7) are console commands.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OneMindPlayerController.generated.h"

UCLASS()
class ONEMIND_API AOneMindPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOneMindPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

	/** Console: OneMindMarker 30.27 -97.74 MY-MARK */
	UFUNCTION(Exec)
	void OneMindMarker(double Lat, double Lon, const FString& Callsign);

	/** Console: OneMindLayer fleet 1 */
	UFUNCTION(Exec)
	void OneMindLayer(const FString& LayerId, int32 On);

	/** Console: OneMindChat hello team */
	UFUNCTION(Exec)
	void OneMindChat(const FString& Text);

	/** Console: OneMindTask sim-x1 drone 30.27 -97.74 120 (same path as T). */
	UFUNCTION(Exec)
	void OneMindTask(const FString& AssetId, const FString& AssetClass, double Lat, double Lon, double HaeMeters);

	/** Console: OneMindTakEnroll <tak user>. The password is read from the environment
	 *  variable ONEMIND_TAK_ENROLL_PASSWORD (never typed into the console, whose history is logged). */
	UFUNCTION(Exec)
	void OneMindTakEnroll(const FString& TakUser);

	UFUNCTION(Exec)
	void OneMindTakConnect();

	/** Console: OneMindTakChat ANDROID-1234 on my way   (or "All" for All Chat Rooms) */
	UFUNCTION(Exec)
	void OneMindTakChat(const FString& Conversation, const FString& Text);

	/** Console: OneMindChannel Cyan   (no argument = all channels) */
	UFUNCTION(Exec)
	void OneMindChannel(const FString& Channel);

	UFUNCTION(Exec)
	void OneMindMissionList();

	/** Console: OneMindMissionOpen patrol-1 */
	UFUNCTION(Exec)
	void OneMindMissionOpen(const FString& Name);

	/** Console: OneMindMissionAdd patrol-1 Water-point  (marker at the cursor) */
	UFUNCTION(Exec)
	void OneMindMissionAdd(const FString& Name, const FString& Callsign);

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	bool CursorLonLat(double& OutLon, double& OutLat, FVector& OutWorld) const;
	void TickAct(float DeltaTime);
	void FinishShape();

	enum class EDrawTool : uint8 { None, Point, Line, Polygon, Rectangle, Circle, Sector, RangeRings, RangeBearing, Buffer, Lasso, Count };
	EDrawTool DrawTool = EDrawTool::None;
	TArray<FVector2D> DrawPoints;  // (lat, lon)
	bool bShowGaps = true;
	float SelfTimer = 0.f;
};
