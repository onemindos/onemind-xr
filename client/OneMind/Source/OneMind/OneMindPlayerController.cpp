// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "OneMindCoTSubsystem.h"
#include "OneMindEntityRenderer.h"
#include "OneMindGlobe.h"
#include "OneMindGlobeSettings.h"
#include "OneMindLayerSubsystem.h"
#include "OneMindOverlaySubsystem.h"

AOneMindPlayerController::AOneMindPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
}

void AOneMindPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AOneMindGlobe* Globe = AOneMindGlobe::GetOrSpawn(this);
	if (InPawn && Globe && Globe->GetGeoreference())
	{
		const FVector Start = GetDefault<UOneMindGlobeSettings>()->StartLongitudeLatitudeHeight;
		InPawn->SetActorLocation(Globe->LonLatHeightToWorld(Start));
		SetControlRotation(FRotator(-60.0, 0.0, 0.0));
	}
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

bool AOneMindPlayerController::CursorLonLat(double& OutLon, double& OutLat, FVector& OutWorld) const
{
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return false;
	AOneMindGlobe* Globe = AOneMindGlobe::GetOrSpawn(this);
	if (!Globe) return false;
	const FVector Llh = Globe->WorldToLonLatHeight(Hit.Location);
	OutLon = Llh.X;
	OutLat = Llh.Y;
	OutWorld = Hit.Location;
	return true;
}

void AOneMindPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World) return;
	UOneMindLayerSubsystem* Layers = World->GetSubsystem<UOneMindLayerSubsystem>();
	UOneMindOverlaySubsystem* Overlay = World->GetSubsystem<UOneMindOverlaySubsystem>();

	static const FKey NumberKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	if (Layers)
	{
		const TArray<FOneMindLayerStatus> List = Layers->GetLayers();
		for (int32 i = 0; i < 9 && i < List.Num(); ++i)
		{
			if (WasInputKeyJustPressed(NumberKeys[i])) Layers->ToggleLayer(List[i].Id);
		}
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		double Lon = 0, Lat = 0;
		FVector WorldHit;
		if (CursorLonLat(Lon, Lat, WorldHit))
		{
			const double Dist = PlayerCameraManager ? FVector::Dist(PlayerCameraManager->GetCameraLocation(), WorldHit) / 100.0 : 1000.0;
			AOneMindEntityRenderer* R = AOneMindEntityRenderer::GetOrSpawn(this);
			const FString Picked = R ? R->PickAtWorldLocation(WorldHit, FMath::Clamp(Dist * 0.02, 20.0, 50000.0)) : FString();
			if (Overlay)
			{
				if (Picked.IsEmpty()) Overlay->ClearSelection();
				else Overlay->SelectEntity(Picked);
			}
		}
	}
	if (WasInputKeyJustPressed(EKeys::Escape) && Overlay)
	{
		Overlay->ClearSelection();
		if (AOneMindEntityRenderer* R = AOneMindEntityRenderer::GetOrSpawn(this)) R->SetSelected(FString());
	}
	if (WasInputKeyJustPressed(EKeys::M))
	{
		double Lon = 0, Lat = 0;
		FVector WorldHit;
		if (CursorLonLat(Lon, Lat, WorldHit)) OneMindMarker(Lat, Lon, TEXT("OM-MARK"));
	}
}

void AOneMindPlayerController::OneMindMarker(double Lat, double Lon, const FString& Callsign)
{
	if (UOneMindCoTSubsystem* Cot = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindCoTSubsystem>() : nullptr)
	{
		const FString Uid = Cot->SendMarker(Lat, Lon, Callsign, TEXT("a-u-G"), TEXT("marker from the OneMind client"));
		UE_LOG(LogTemp, Log, TEXT("OneMind marker %s at %.6f, %.6f sent to svc.cot.send"), *Uid, Lat, Lon);
	}
}

void AOneMindPlayerController::OneMindLayer(const FString& LayerId, int32 On)
{
	if (UOneMindLayerSubsystem* Layers = GetWorld() ? GetWorld()->GetSubsystem<UOneMindLayerSubsystem>() : nullptr)
	{
		Layers->SetLayerEnabled(LayerId, On != 0);
	}
}

void AOneMindPlayerController::OneMindChat(const FString& Text)
{
	if (UOneMindCoTSubsystem* Cot = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindCoTSubsystem>() : nullptr)
	{
		Cot->SendChat(Text);
	}
}
