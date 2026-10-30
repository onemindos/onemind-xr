// Mouse and keyboard for the desktop client without input assets: left click selects
// (inspector), 1-9 toggle layers, M sends a marker at the cursor through svc.cot.send,
// Esc clears the selection. Flight is the Cesium globe-aware pawn's.
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

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	bool CursorLonLat(double& OutLon, double& OutLat, FVector& OutWorld) const;
};
