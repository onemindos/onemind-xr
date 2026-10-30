// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindEntityRenderer.h"

#include "Camera/PlayerCameraManager.h"
#include "CesiumGeoreference.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OneMindBusSubsystem.h"
#include "OneMindCore/Geo.h"
#include "OneMindEntityActor.h"
#include "OneMindLayerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogOneMindEntities, Log, All);

namespace OneMindRender
{
FLinearColor ColorFor(om::SymbolClass C)
{
	switch (C)
	{
	case om::SymbolClass::Air: return FLinearColor(0.2f, 0.7f, 1.0f);
	case om::SymbolClass::Ground: return FLinearColor(0.3f, 0.9f, 0.3f);
	case om::SymbolClass::Sea: return FLinearColor(0.1f, 0.4f, 0.9f);
	case om::SymbolClass::Subsurface: return FLinearColor(0.1f, 0.2f, 0.6f);
	case om::SymbolClass::Space: return FLinearColor(0.8f, 0.6f, 1.0f);
	case om::SymbolClass::Person: return FLinearColor(0.0f, 1.0f, 1.0f);
	case om::SymbolClass::Shape: return FLinearColor(1.0f, 1.0f, 0.2f);
	case om::SymbolClass::Point: return FLinearColor(1.0f, 0.6f, 0.1f);
	case om::SymbolClass::Event: return FLinearColor(1.0f, 0.3f, 0.2f);
	default: return FLinearColor(0.8f, 0.8f, 0.8f);
	}
}
constexpr int32 KeyFor(om::Band B, om::SymbolClass C) { return static_cast<int32>(B) * 16 + static_cast<int32>(C); }
constexpr int32 ClusterKey = 999;
constexpr int32 SelectionKey = 998;
}  // namespace OneMindRender

AOneMindEntityRenderer::AOneMindEntityRenderer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AOneMindEntityRenderer* AOneMindEntityRenderer::GetOrSpawn(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AOneMindEntityRenderer> It(World); It; ++It) return *It;
	return World->SpawnActor<AOneMindEntityRenderer>(AOneMindEntityRenderer::StaticClass(), FTransform::Identity);
}

void AOneMindEntityRenderer::BeginPlay()
{
	Super::BeginPlay();
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Georeference = ACesiumGeoreference::GetDefaultGeoreference(this);
	for (int32 i = 0; i < MaxActors; ++i)
	{
		ActorPool.Add(GetWorld()->SpawnActor<AOneMindEntityActor>(AOneMindEntityActor::StaticClass(), FTransform::Identity));
	}
}

UInstancedStaticMeshComponent* AOneMindEntityRenderer::GetIsm(int32 Key, UStaticMesh* Mesh, const FLinearColor& Color)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Isms.Find(Key)) return *Found;
	UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this);
	Ism->SetStaticMesh(Mesh);
	Ism->SetMobility(EComponentMobility::Movable);
	Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ism->SetCastShadow(false);
	if (BaseMaterial)
	{
		// BasicShapeMaterial exposes a "Color" vector parameter (unverified for 5.8; if it
		// does not, instances render in the material's default colour).
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
		Ism->SetMaterial(0, Mid);
	}
	Ism->SetupAttachment(RootComponent);
	Ism->RegisterComponent();
	Isms.Add(Key, Ism);
	return Ism;
}

void AOneMindEntityRenderer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Accum += DeltaSeconds;
	if (Accum < 1.f / FMath::Max(1.f, RefreshHz)) return;
	Accum = 0.f;
	Rebuild();
}

void AOneMindEntityRenderer::Rebuild()
{
	const double T0 = FPlatformTime::Seconds();
	UWorld* World = GetWorld();
	ACesiumGeoreference* Geo = Georeference.Get();
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, 0);
	UOneMindLayerSubsystem* LayersSys = World ? World->GetSubsystem<UOneMindLayerSubsystem>() : nullptr;
	if (!Geo || !Cam || !LayersSys) return;
	om::LayerManager* Manager = LayersSys->GetManager();
	UOneMindBusSubsystem* Bus = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UOneMindBusSubsystem>() : nullptr;

	const FTransform GeoXf = Geo->GetActorTransform();
	const FVector CamWorld = Cam->GetCameraLocation();
	const FVector CamLlh = Geo->TransformUnrealPositionToLongitudeLatitudeHeight(GeoXf.InverseTransformPosition(CamWorld));

	om::RenderPlanConfig Cfg;
	Cfg.actor_max_m = ActorMaxMeters;
	Cfg.model_max_m = ModelMaxMeters;
	Cfg.max_actors = static_cast<size_t>(MaxActors);
	Cfg.cluster_min = static_cast<size_t>(ClusterMin);

	TMap<int32, TArray<FTransform>> Batches;
	struct FNear { FTransform T; FString Label; bool bSel; bool bStale; };
	TArray<FNear> Near;
	const om::TimeMs Now = om::WallClockMs();
	int32 Total = 0, Icons = 0, Models = 0, Clusters = 0, Culled = 0;

	auto ToWorld = [&](double Lon, double Lat, double H) {
		return GeoXf.TransformPosition(Geo->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(Lon, Lat, H)));
	};
	auto Heading = [&](const FVector& Pos, double CourseDeg) {
		const FMatrix Esu = Geo->ComputeEastSouthUpToUnrealTransformation(GeoXf.InverseTransformPosition(Pos));
		const FQuat Frame = GeoXf.GetRotation() * FQuat(Esu.Rotator());
		const double Yaw = CourseDeg >= 0 ? CourseDeg - 90.0 : 0.0;  // ESU: +X east, +Y south
		return Frame * FRotator(-90.0, Yaw, 0.0).Quaternion();       // cone apex along the course
	};

	// The selection ("layer/entity_id") is resolved to one index per layer per rebuild, so
	// the per-entity loop builds no strings (10,000 entities at 15 Hz).
	std::string SelLayer, SelEntity;
	{
		FString L, E;
		if (Selected.Split(TEXT("/"), &L, &E))
		{
			SelLayer = TCHAR_TO_UTF8(*L);
			SelEntity = TCHAR_TO_UTF8(*E);
		}
	}

	auto Draw = [&](const std::string& LayerId, const om::EntityStore& Store) {
		const om::RenderPlan Plan = om::PlanRender(Store, CamLlh.Y, CamLlh.X, CamLlh.Z, Cfg);
		const int64 SelIndex = (!SelEntity.empty() && LayerId == SelLayer) ? static_cast<int64>(Store.IndexOf(SelEntity)) : -1;
		const auto& Lats = Store.lats();
		const auto& Lons = Store.lons();
		const auto& Haes = Store.haes();
		const auto& Courses = Store.courses();
		const auto& Classes = Store.classes();
		for (size_t i = 0; i < Store.size(); ++i)
		{
			++Total;
			const om::Band B = Plan.band[i];
			if (B == om::Band::Culled) { ++Culled; continue; }
			if (B == om::Band::Clustered) continue;
			const double H = (Haes[i] > -1000.0 && Haes[i] < 1.0e6 && Haes[i] != -1.0) ? Haes[i] : 0.0;
			const FVector P = ToWorld(Lons[i], Lats[i], H);
			const double DistM = FVector::Dist(P, CamWorld) / 100.0;
			const bool bSelected = static_cast<int64>(i) == SelIndex;
			if (B == om::Band::Icon)
			{
				// Far band: instanced spheres scaled with distance (constant screen size).
				// FEATURES B5 names Niagara icons; this is the interim (proposal B-P8 in
				// docs/evidence/M2/B/EVIDENCE.md), Niagara sprites are the follow-up.
				++Icons;
				const double D = FMath::Max(1.0, DistM * IconAngularSize);
				Batches.FindOrAdd(OneMindRender::KeyFor(B, Classes[i])).Add(FTransform(FQuat::Identity, P, FVector(D)));
			}
			else if (B == om::Band::Model)
			{
				++Models;
				Batches.FindOrAdd(OneMindRender::KeyFor(B, Classes[i]))
					.Add(FTransform(Heading(P, Courses[i]), P, FVector(ModelSizeMeters)));
			}
			else
			{
				const std::string& Cs = Store.callsigns()[i];
				Near.Add({FTransform(Heading(P, Courses[i]), P, FVector(4.0)),
					Cs.empty() ? FString(UTF8_TO_TCHAR(Store.ids()[i].c_str())) : FString(UTF8_TO_TCHAR(Cs.c_str())),
					bSelected, Store.IsStale(i, Now)});
			}
			if (bSelected)
			{
				const double D = FMath::Max(2.0, DistM * IconAngularSize * 2.5);
				Batches.FindOrAdd(OneMindRender::SelectionKey).Add(FTransform(FQuat::Identity, P, FVector(D, D, D * 0.1)));
			}
		}
		for (const om::Cluster& C : Plan.clusters)
		{
			++Clusters;
			const FVector P = ToWorld(C.lon, C.lat, 0.0);
			const double DistM = FVector::Dist(P, CamWorld) / 100.0;
			const double D = FMath::Max(1.0, DistM * IconAngularSize) * (1.0 + FMath::Log2(static_cast<double>(C.count)));
			Batches.FindOrAdd(OneMindRender::ClusterKey).Add(FTransform(FQuat::Identity, P, FVector(D)));
		}
	};

	if (Manager)
	{
		for (const std::string& Id : Manager->Ids())
		{
			if (om::LayerSession* L = Manager->Get(Id)) Draw(Id, L->store());
		}
	}
	if (Bus) Draw(std::string("probe"), Bus->GetProbeStore());

	// Apply: one bulk update per instanced component.
	TSet<int32> Used;
	for (auto& KV : Batches)
	{
		const int32 Key = KV.Key;
		UStaticMesh* Mesh = SphereMesh;
		FLinearColor Color = FLinearColor::White;
		if (Key == OneMindRender::ClusterKey) { Color = FLinearColor(1.f, 0.85f, 0.2f); }
		else if (Key == OneMindRender::SelectionKey) { Mesh = CubeMesh; Color = FLinearColor::Yellow; }
		else
		{
			const om::SymbolClass C = static_cast<om::SymbolClass>(Key % 16);
			Color = OneMindRender::ColorFor(C);
			if (Key / 16 == static_cast<int32>(om::Band::Model)) Mesh = ConeMesh;
		}
		UInstancedStaticMeshComponent* Ism = GetIsm(Key, Mesh, Color);
		if (!Ism) continue;
		Used.Add(Key);
		if (Ism->GetInstanceCount() == KV.Value.Num())
		{
			Ism->BatchUpdateInstancesTransforms(0, KV.Value, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
		}
		else
		{
			Ism->ClearInstances();
			Ism->AddInstances(KV.Value, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
		}
	}
	for (auto& KV : Isms)
	{
		if (!Used.Contains(KV.Key) && KV.Value && KV.Value->GetInstanceCount() > 0) KV.Value->ClearInstances();
	}
	for (int32 i = 0; i < ActorPool.Num(); ++i)
	{
		if (!ActorPool[i]) continue;
		if (i < Near.Num()) ActorPool[i]->Show(Near[i].T, Near[i].Label, Near[i].bSel, Near[i].bStale);
		else ActorPool[i]->HideEntity();
	}
	LastTotal = Total;
	LastIcons = Icons;
	LastModels = Models;
	LastActors = Near.Num();
	LastClusters = Clusters;
	LastCulled = Culled;
	LastRebuildMs = (FPlatformTime::Seconds() - T0) * 1000.0;
}

FString AOneMindEntityRenderer::PickAtWorldLocation(const FVector& WorldLocation, double RadiusMeters)
{
	ACesiumGeoreference* Geo = Georeference.Get();
	UWorld* World = GetWorld();
	UOneMindLayerSubsystem* LayersSys = World ? World->GetSubsystem<UOneMindLayerSubsystem>() : nullptr;
	if (!Geo || !LayersSys || !LayersSys->GetManager()) return FString();
	const FVector Llh = Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Geo->GetActorTransform().InverseTransformPosition(WorldLocation));
	om::LayerManager* M = LayersSys->GetManager();
	FString Best;
	double BestD = RadiusMeters;
	for (const std::string& Id : M->Ids())
	{
		om::LayerSession* L = M->Get(Id);
		if (!L) continue;
		const std::string Hit = L->store().Pick(Llh.Y, Llh.X, RadiusMeters);
		if (Hit.empty()) continue;
		const om::RenderEntity E = L->store().Get(static_cast<size_t>(L->store().IndexOf(Hit)));
		const double D = om::Geo::HaversineM(Llh.Y, Llh.X, E.lat, E.lon);
		if (D <= BestD)
		{
			BestD = D;
			Best = FString(UTF8_TO_TCHAR(Id.c_str())) + TEXT("/") + UTF8_TO_TCHAR(Hit.c_str());
		}
	}
	Selected = Best;
	return Best;
}

FString AOneMindEntityRenderer::GetStatsText() const
{
	return FString::Printf(TEXT("entities %d | icons %d | models %d | actors %d | clusters %d | culled %d | rebuild %.2f ms"),
		LastTotal, LastIcons, LastModels, LastActors, LastClusters, LastCulled, LastRebuildMs);
}
