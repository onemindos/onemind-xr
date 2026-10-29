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
#include "OneMindMissionSubsystem.h"
#include "OneMindOverlaySubsystem.h"
#include "OneMindTakSubsystem.h"
#include "HAL/PlatformMisc.h"

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
	TickAct(DeltaTime);
	if (DrawTool != EDrawTool::None && WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		double Lon = 0, Lat = 0;
		FVector WorldHit;
		if (CursorLonLat(Lon, Lat, WorldHit)) DrawPoints.Add(FVector2D(Lat, Lon));
	}
	else if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
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

namespace OneMindPcPrivate
{
const TCHAR* ToolName(uint8 T)
{
	static const TCHAR* Names[] = {TEXT("off"), TEXT("point"), TEXT("line"), TEXT("polygon"), TEXT("rectangle"), TEXT("circle"),
		TEXT("sector"), TEXT("range rings"), TEXT("range & bearing"), TEXT("buffer"), TEXT("lasso")};
	return T < UE_ARRAY_COUNT(Names) ? Names[T] : TEXT("?");
}
}  // namespace OneMindPcPrivate

void AOneMindPlayerController::TickAct(float DeltaTime)
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GetWorld();
	if (!GI || !World) return;
	UOneMindMissionSubsystem* Mission = GI->GetSubsystem<UOneMindMissionSubsystem>();
	UOneMindTakSubsystem* Tak = GI->GetSubsystem<UOneMindTakSubsystem>();
	UOneMindOverlaySubsystem* Overlay = World->GetSubsystem<UOneMindOverlaySubsystem>();

	// D3: T proposes "go to the cursor" for the selected sim-* asset.
	if (WasInputKeyJustPressed(EKeys::T) && Mission && Overlay)
	{
		FString Layer, Id;
		if (!Overlay->GetSelectedId().Split(TEXT("/"), &Layer, &Id)) Id = Overlay->GetSelectedId();
		double Lon = 0, Lat = 0;
		FVector WorldHit;
		if (Id.StartsWith(TEXT("sim-")) && CursorLonLat(Lon, Lat, WorldHit)) OneMindTask(Id, TEXT("drone"), Lat, Lon, 120.0);
		else UE_LOG(LogTemp, Warning, TEXT("OneMind: select a sim-* asset, point at the target and press T"));
	}
	FString Act;
	if (Mission)
	{
		const FOneMindPendingTask Task = Mission->GetPendingTask();
		const bool bAwaiting = Task.Status == TEXT("awaiting_confirmation");
		Act += Mission->GetOverlayText();
		if (bAwaiting)
		{
			// The dialog text (exact parameters and hash) is on screen from this frame on.
			Mission->MarkPendingShown();
			if (WasInputKeyJustPressed(EKeys::Y)) Mission->ConfirmPending();
			else if (WasInputKeyJustPressed(EKeys::N)) Mission->RejectPending(TEXT("rejected in the OneMind client"));
		}
	}
	// B11: drawing tools.
	if (WasInputKeyJustPressed(EKeys::B))
	{
		DrawTool = static_cast<EDrawTool>((static_cast<uint8>(DrawTool) + 1) % static_cast<uint8>(EDrawTool::Count));
		DrawPoints.Reset();
	}
	if (DrawTool != EDrawTool::None)
	{
		if (WasInputKeyJustPressed(EKeys::Enter)) FinishShape();
		if (WasInputKeyJustPressed(EKeys::BackSpace)) DrawPoints.Reset();
		Act += FString::Printf(TEXT("DRAW %s: %d points (click to add, Enter sends, Backspace clears, B next tool)\n"),
			OneMindPcPrivate::ToolName(static_cast<uint8>(DrawTool)), DrawPoints.Num());
	}
	if (WasInputKeyJustPressed(EKeys::G)) bShowGaps = !bShowGaps;
	// C3/C8: TAK status, contacts and the latest chat lines; own SA follows the camera.
	if (Tak)
	{
		Act += Tak->GetStatusText() + TEXT("\n");
		const TArray<FOneMindTakContact> Contacts = Tak->GetContacts();
		for (int32 i = 0; i < Contacts.Num() && i < 6; ++i)
			Act += FString::Printf(TEXT("  %s [%s] %s\n"), *Contacts[i].Callsign, *Contacts[i].Team, *Contacts[i].Platform);
		const TArray<FOneMindChatLine> All = Tak->GetConversation(TEXT("All Chat Rooms"));
		for (int32 i = FMath::Max(0, All.Num() - 3); i < All.Num(); ++i) Act += TEXT("  > ") + All[i].Sender + TEXT(": ") + All[i].Text + TEXT("\n");
		SelfTimer += DeltaTime;
		if (SelfTimer > 5.f && PlayerCameraManager)
		{
			SelfTimer = 0.f;
			if (AOneMindGlobe* Globe = AOneMindGlobe::GetOrSpawn(this))
			{
				const FVector Llh = Globe->WorldToLonLatHeight(PlayerCameraManager->GetCameraLocation());
				Tak->SetSelfPosition(Llh.Y, Llh.X, Llh.Z);
			}
		}
	}
	if (Overlay)
	{
		Overlay->SetShowGaps(bShowGaps);  // the gaps panel itself comes from the layer subsystem
		Overlay->SetActText(Act);
	}
}

void AOneMindPlayerController::FinishShape()
{
	UOneMindCoTSubsystem* Cot = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindCoTSubsystem>() : nullptr;
	if (!Cot || DrawPoints.Num() == 0) return;
	const FVector2D A = DrawPoints[0];
	const FVector2D Bp = DrawPoints.Last();
	auto Meters = [](const FVector2D& P, const FVector2D& Q)
	{
		const double R = 6371008.8, D2R = PI / 180.0;
		const double DLat = (Q.X - P.X) * D2R, DLon = (Q.Y - P.Y) * D2R;
		const double H = FMath::Sin(DLat / 2) * FMath::Sin(DLat / 2) +
			FMath::Cos(P.X * D2R) * FMath::Cos(Q.X * D2R) * FMath::Sin(DLon / 2) * FMath::Sin(DLon / 2);
		return 2 * R * FMath::Asin(FMath::Sqrt(H));
	};
	switch (DrawTool)
	{
		case EDrawTool::Point: Cot->SendPoint(A.X, A.Y, TEXT("Point")); break;
		case EDrawTool::Line: if (DrawPoints.Num() >= 2) Cot->SendLine(DrawPoints, TEXT("Line")); break;
		case EDrawTool::Polygon: if (DrawPoints.Num() >= 3) Cot->SendPolygon(DrawPoints, TEXT("Polygon")); break;
		case EDrawTool::Rectangle: if (DrawPoints.Num() >= 2) Cot->SendRectangle(A, Bp, TEXT("Rectangle")); break;
		case EDrawTool::Circle: if (DrawPoints.Num() >= 2) Cot->SendCircle(A.X, A.Y, Meters(A, Bp), TEXT("Circle")); break;
		case EDrawTool::Sector:
			if (DrawPoints.Num() >= 3)
			{
				auto Bearing = [](const FVector2D& P, const FVector2D& Q)
				{
					const double D2R = PI / 180.0;
					const double Y = FMath::Sin((Q.Y - P.Y) * D2R) * FMath::Cos(Q.X * D2R);
					const double X = FMath::Cos(P.X * D2R) * FMath::Sin(Q.X * D2R) - FMath::Sin(P.X * D2R) * FMath::Cos(Q.X * D2R) * FMath::Cos((Q.Y - P.Y) * D2R);
					return FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Y, X)) + 360.0, 360.0);
				};
				Cot->SendSector(A.X, A.Y, Meters(A, DrawPoints[1]), Bearing(A, DrawPoints[1]), Bearing(A, DrawPoints[2]), TEXT("Sector"));
			}
			break;
		case EDrawTool::RangeRings:
			if (DrawPoints.Num() >= 2)
			{
				const double R = Meters(A, Bp);
				Cot->SendRangeRings(A.X, A.Y, {R / 3, 2 * R / 3, R}, TEXT("Rings"));
			}
			break;
		case EDrawTool::RangeBearing: if (DrawPoints.Num() >= 2) Cot->SendRangeBearing(A, Bp, TEXT("R&B")); break;
		case EDrawTool::Buffer: Cot->SendBuffer(DrawPoints, 100.0, TEXT("Buffer 100 m")); break;
		case EDrawTool::Lasso:
			// Selection only: the renderer's entities inside the ring (nothing is sent).
			UE_LOG(LogTemp, Log, TEXT("OneMind lasso with %d vertices (selection is local)"), DrawPoints.Num());
			break;
		default: break;
	}
	DrawPoints.Reset();
}

void AOneMindPlayerController::OneMindTask(const FString& AssetId, const FString& AssetClass, double Lat, double Lon, double HaeMeters)
{
	if (UOneMindMissionSubsystem* M = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindMissionSubsystem>() : nullptr)
	{
		M->ProposeGoto(AssetId, AssetClass, Lat, Lon, HaeMeters);
	}
}

void AOneMindPlayerController::OneMindTakEnroll(const FString& TakUser)
{
	UOneMindTakSubsystem* Tak = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindTakSubsystem>() : nullptr;
	const FString Password = FPlatformMisc::GetEnvironmentVariable(TEXT("ONEMIND_TAK_ENROLL_PASSWORD"));
	if (!Tak || Password.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("OneMind: set ONEMIND_TAK_ENROLL_PASSWORD in the environment before OneMindTakEnroll"));
		return;
	}
	Tak->Enroll(TakUser, Password);
}

void AOneMindPlayerController::OneMindTakConnect()
{
	if (UOneMindTakSubsystem* Tak = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindTakSubsystem>() : nullptr) Tak->Connect();
}

void AOneMindPlayerController::OneMindTakChat(const FString& Conversation, const FString& Text)
{
	if (UOneMindTakSubsystem* Tak = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindTakSubsystem>() : nullptr)
	{
		Tak->SendChat(Conversation == TEXT("All") ? FString(TEXT("All Chat Rooms")) : Conversation, Text);
	}
}

void AOneMindPlayerController::OneMindChannel(const FString& Channel)
{
	if (UOneMindTakSubsystem* Tak = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindTakSubsystem>() : nullptr)
	{
		TArray<FString> Set;
		if (!Channel.IsEmpty()) Set.Add(Channel);
		Tak->SetActiveChannels(Set);
	}
}

void AOneMindPlayerController::OneMindMissionList()
{
	if (UOneMindMissionSubsystem* M = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindMissionSubsystem>() : nullptr) M->RefreshMissions();
}

void AOneMindPlayerController::OneMindMissionOpen(const FString& Name)
{
	if (UOneMindMissionSubsystem* M = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindMissionSubsystem>() : nullptr)
	{
		M->SubscribeMission(Name, true);
		M->OpenMission(Name);
	}
}

void AOneMindPlayerController::OneMindMissionAdd(const FString& Name, const FString& Callsign)
{
	UOneMindMissionSubsystem* M = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOneMindMissionSubsystem>() : nullptr;
	double Lon = 0, Lat = 0;
	FVector WorldHit;
	if (M && CursorLonLat(Lon, Lat, WorldHit)) M->AddMarkerToMission(Name, Lat, Lon, Callsign);
}
