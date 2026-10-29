// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindLayerSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "CesiumGeoreference.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OneMindBusSubsystem.h"
#include "OneMindCore/ClientCore.h"
#include "OneMindCore/Geo.h"
#include "OneMindLayersSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogOneMindLayers, Log, All);

void UOneMindLayerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		Bus = GI->GetSubsystem<UOneMindBusSubsystem>();
	}
	RegisterCatalogue();
}

om::LayerManager* UOneMindLayerSubsystem::GetManager() const
{
	return Bus.IsValid() && Bus->GetCore() ? &Bus->GetCore()->layers() : nullptr;
}

void UOneMindLayerSubsystem::RegisterCatalogue()
{
	om::LayerManager* M = GetManager();
	if (!M || bRegistered) return;
	const UOneMindLayersSettings* S = GetDefault<UOneMindLayersSettings>();
	std::vector<om::LayerSpec> Specs;
	std::string Err;
	if (!om::LayerManager::ParseSpecs(TCHAR_TO_UTF8(*S->LayersJson), Specs, &Err))
	{
		UE_LOG(LogOneMindLayers, Error, TEXT("LayersJson: %s"), UTF8_TO_TCHAR(Err.c_str()));
		return;
	}
	for (const om::LayerSpec& L : Specs)
	{
		M->Register(L);
		const FString Id = UTF8_TO_TCHAR(L.id.c_str());
		LayerIds.Add(Id);
		Titles.Add(Id, UTF8_TO_TCHAR(L.title.c_str()));
	}
	bRegistered = true;
	UE_LOG(LogOneMindLayers, Log, TEXT("Registered %d layers"), LayerIds.Num());
}

bool UOneMindLayerSubsystem::SetLayerEnabled(const FString& LayerId, bool bEnabled)
{
	om::LayerManager* M = GetManager();
	if (!M) return false;
	const bool bOk = M->Toggle(TCHAR_TO_UTF8(*LayerId), bEnabled, om::WallClockMs());
	UE_LOG(LogOneMindLayers, Log, TEXT("Layer %s %s"), *LayerId, bEnabled ? TEXT("on") : TEXT("off (unsubscribed)"));
	return bOk;
}

bool UOneMindLayerSubsystem::ToggleLayer(const FString& LayerId)
{
	om::LayerManager* M = GetManager();
	if (!M) return false;
	return SetLayerEnabled(LayerId, !M->IsOn(TCHAR_TO_UTF8(*LayerId)));
}

TArray<FOneMindLayerStatus> UOneMindLayerSubsystem::GetLayers() const
{
	TArray<FOneMindLayerStatus> Out;
	om::LayerManager* M = GetManager();
	for (const FString& Id : LayerIds)
	{
		FOneMindLayerStatus St;
		St.Id = Id;
		St.Title = Titles.FindRef(Id);
		const std::string K = TCHAR_TO_UTF8(*Id);
		if (M)
		{
			if (om::LayerSession* L = M->Get(K))
			{
				St.bOn = true;
				St.State = UTF8_TO_TCHAR(om::LayerStateName(L->state()));
				St.Path = UTF8_TO_TCHAR(om::LayerPathName(L->spec().path));
				St.Entities = static_cast<int32>(L->store().size());
				St.LastError = UTF8_TO_TCHAR(L->last_error().c_str());
			}
			const om::LayerManifest* Mf = M->Manifest(K);
			St.Freshness = !Mf ? TEXT("unknown") : (Mf->present ? UTF8_TO_TCHAR(Mf->status.c_str()) : TEXT("down"));
		}
		Out.Add(St);
	}
	return Out;
}

void UOneMindLayerSubsystem::SetAoi(double Lon0, double Lat0, double Lon1, double Lat1, int32 Zoom)
{
	bManualAoi = true;
	if (om::LayerManager* M = GetManager())
	{
		M->SetAoi(om::Aoi::Bbox(Lon0, Lat0, Lon1, Lat1), Zoom, om::WallClockMs());
	}
}

void UOneMindLayerSubsystem::UpdateAoiFromCamera()
{
	om::LayerManager* M = GetManager();
	UWorld* World = GetWorld();
	if (!M || !World || bManualAoi) return;
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, 0);
	ACesiumGeoreference* Geo = ACesiumGeoreference::GetDefaultGeoreference(World);
	if (!Cam || !Geo) return;
	const FVector Llh = Geo->TransformUnrealPositionToLongitudeLatitudeHeight(
		Geo->GetActorTransform().InverseTransformPosition(Cam->GetCameraLocation()));
	const double Alt = FMath::Max(10.0, Llh.Z);
	// Visible half-width ~ 0.6 x altitude for a ~60 degree view; 1 degree ~ 111 km.
	const double HalfDeg = FMath::Clamp(Alt * 0.6 / 111000.0, 0.01, 60.0);
	const double CosLat = FMath::Max(0.1, FMath::Cos(FMath::DegreesToRadians(Llh.Y)));
	M->SetAoi(om::Aoi::Bbox(Llh.X - HalfDeg / CosLat, FMath::Max(-85.0, Llh.Y - HalfDeg), Llh.X + HalfDeg / CosLat,
		FMath::Min(85.0, Llh.Y + HalfDeg)), om::Geo::ZoomForAltitude(Alt), om::WallClockMs());
}

void UOneMindLayerSubsystem::StartGaps()
{
	om::ClientCore* Core = Bus.IsValid() ? Bus->GetCore() : nullptr;
	if (!Core || Gaps) return;
	Gaps = std::make_unique<om::FeedGaps>();
	om::Json Cfg;
	if (om::Json::Parse(TCHAR_TO_UTF8(*GetDefault<UOneMindLayersSettings>()->GapsJson), Cfg) && Cfg.is_object())
	{
		for (const auto& L : Cfg["layers"].fields())
		{
			std::vector<std::string> Src;
			for (const om::Json& F : L.second.items()) Src.push_back(F.as_string_or(""));
			Gaps->SetLayerFeeds(L.first, Src);
		}
		for (const auto& A : Cfg["alerts"].fields())
		{
			std::vector<std::string> Src;
			for (const om::Json& F : A.second.items()) Src.push_back(F.as_string_or(""));
			Gaps->SetAlertFeeds(A.first, Src);
		}
		for (const auto& E : Cfg["expect"].fields()) Gaps->ExpectFeed(E.first, E.second.as_number(60));
	}
	om::FeedGaps* G = Gaps.get();
	// Freshness heartbeats (omos-ingest, PR #553) and layer manifests (layer service, PR #561).
	FeedStatusSid = Core->nats().Subscribe("tel.feed.*.status", [G](const om::NatsMsg& M) { G->IngestStatus(M.data, om::WallClockMs()); });
	ManifestWatch = Core->WatchKv("world-state", "*.meta", [G](const std::string& Key, const std::string& Value, bool bDeleted)
	{
		om::Json J;
		if (!bDeleted && om::Json::Parse(Value, J)) G->IngestManifest(Key.substr(0, Key.rfind('.')), J, om::WallClockMs());
	}, om::WallClockMs());
}

TArray<FOneMindFeedGap> UOneMindLayerSubsystem::GetGaps() const
{
	TArray<FOneMindFeedGap> Out;
	if (!Gaps) return Out;
	for (const om::FeedGap& G : Gaps->Gaps(om::WallClockMs()))
	{
		FOneMindFeedGap F;
		F.Source = UTF8_TO_TCHAR(G.source.c_str());
		F.Status = UTF8_TO_TCHAR(om::FeedGapStatusName(G.status));
		F.Reason = UTF8_TO_TCHAR(G.reason.c_str());
		F.AgeSeconds = static_cast<float>(G.age_s);
		for (const std::string& L : G.layers) F.Layers.Add(UTF8_TO_TCHAR(L.c_str()));
		for (const std::string& A : G.alerts) F.Alerts.Add(UTF8_TO_TCHAR(A.c_str()));
		Out.Add(F);
	}
	return Out;
}

FString UOneMindLayerSubsystem::GetGapsText() const
{
	const TArray<FOneMindFeedGap> List = GetGaps();
	if (List.Num() == 0) return FString();
	FString Out = FString::Printf(TEXT("INTELLIGENCE GAPS (%d)\n"), List.Num());
	for (const FOneMindFeedGap& G : List)
	{
		Out += FString::Printf(TEXT("  %s %s: %s"), *G.Status.ToUpper(), *G.Source, *G.Reason);
		if (G.Layers.Num()) Out += TEXT("  weakens ") + FString::Join(G.Layers, TEXT(", "));
		if (G.Alerts.Num()) Out += TEXT("  alerts ") + FString::Join(G.Alerts, TEXT(", "));
		Out += TEXT("\n");
	}
	return Out;
}

void UOneMindLayerSubsystem::Tick(float DeltaTime)
{
	if (!bRegistered) RegisterCatalogue();
	om::LayerManager* M = GetManager();
	if (!M) return;
	if (!Gaps && Bus.IsValid() && Bus->IsConnected()) StartGaps();
	if (!bDefaultsApplied && Bus.IsValid() && Bus->IsConnected())
	{
		bDefaultsApplied = true;
		UpdateAoiFromCamera();
		for (const FString& Id : GetDefault<UOneMindLayersSettings>()->DefaultOn)
		{
			SetLayerEnabled(Id, true);
		}
	}
	AoiTimer += DeltaTime;
	if (AoiTimer >= GetDefault<UOneMindLayersSettings>()->AoiUpdateSeconds)
	{
		AoiTimer = 0.f;
		UpdateAoiFromCamera();
	}
}

TStatId UOneMindLayerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOneMindLayerSubsystem, STATGROUP_Tickables);
}
