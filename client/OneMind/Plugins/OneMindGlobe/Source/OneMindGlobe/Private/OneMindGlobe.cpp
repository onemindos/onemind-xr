// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindGlobe.h"

#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumSunSky.h"
#include "CesiumUrlTemplateRasterOverlay.h"
#include "EngineUtils.h"
#include "OneMindGlobeSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogOneMindGlobe, Log, All);

AOneMindGlobe::AOneMindGlobe()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AOneMindGlobe* AOneMindGlobe::GetOrSpawn(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AOneMindGlobe> It(World); It; ++It)
	{
		return *It;
	}
	FActorSpawnParameters P;
	P.Name = TEXT("OneMindGlobe");
	return World->SpawnActor<AOneMindGlobe>(AOneMindGlobe::StaticClass(), FTransform::Identity, P);
}

void AOneMindGlobe::BeginPlay()
{
	Super::BeginPlay();
	BuildGlobe();
}

void AOneMindGlobe::BuildGlobe()
{
	const UOneMindGlobeSettings* S = GetDefault<UOneMindGlobeSettings>();
	UWorld* World = GetWorld();
	if (!World || Terrain) return;

	Georeference = ACesiumGeoreference::GetDefaultGeoreference(this);
	if (Georeference)
	{
		// Unreal origin at the start point: entity positions stay small and precise there.
		Georeference->SetOriginLongitudeLatitudeHeight(FVector(S->StartLongitudeLatitudeHeight.X, S->StartLongitudeLatitudeHeight.Y, 0.0));
	}

	// Light and atmosphere (the default map is empty).
	bool bHasSun = false;
	for (TActorIterator<ACesiumSunSky> It(World); It; ++It) bHasSun = true;
	if (!bHasSun) World->SpawnActor<ACesiumSunSky>(ACesiumSunSky::StaticClass(), FTransform::Identity);

	FActorSpawnParameters P;
	P.Name = TEXT("OneMindTerrain");
	Terrain = World->SpawnActor<ACesium3DTileset>(ACesium3DTileset::StaticClass(), FTransform::Identity, P);
	if (!Terrain)
	{
		UE_LOG(LogOneMindGlobe, Error, TEXT("Could not spawn the Cesium tileset"));
		return;
	}
	if (S->TerrainUrl.IsEmpty())
	{
		// Decision 0004 #4: no Cesium ion by default. The ellipsoid needs no network.
		Terrain->SetTilesetSource(ETilesetSource::FromEllipsoid);
		UE_LOG(LogOneMindGlobe, Log, TEXT("Terrain: WGS-84 ellipsoid (no terrain URL configured)"));
	}
	else
	{
		Terrain->SetTilesetSource(ETilesetSource::FromUrl);
		Terrain->SetUrl(S->TerrainUrl);
		UE_LOG(LogOneMindGlobe, Log, TEXT("Terrain: %s"), *S->TerrainUrl);
	}
	Terrain->SetMaximumScreenSpaceError(S->MaximumScreenSpaceError);

	if (!S->ImageryUrlTemplate.IsEmpty())
	{
		Imagery = NewObject<UCesiumUrlTemplateRasterOverlay>(Terrain, TEXT("OneMindImagery"));
		Imagery->TemplateUrl = S->ImageryUrlTemplate;
		Imagery->MaximumLevel = S->ImageryMaximumLevel;
		Imagery->RequestHeaders = S->ImageryRequestHeaders;
		Imagery->MaterialLayerKey = TEXT("Overlay0");
		Terrain->AddInstanceComponent(Imagery);
		Imagery->RegisterComponent();
		Imagery->Activate(true);
		UE_LOG(LogOneMindGlobe, Log, TEXT("Imagery: %s (%s)"), *S->ImageryUrlTemplate, *S->Attribution);
	}
}

FVector AOneMindGlobe::LonLatHeightToWorld(const FVector& LonLatHeight) const
{
	if (!Georeference) return FVector::ZeroVector;
	// The georeference frame is the world frame while the georeference actor sits at the
	// origin with an identity transform (as spawned here); otherwise apply its transform.
	const FVector Local = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(LonLatHeight);
	return Georeference->GetActorTransform().TransformPosition(Local);
}

FVector AOneMindGlobe::WorldToLonLatHeight(const FVector& World) const
{
	if (!Georeference) return FVector::ZeroVector;
	const FVector Local = Georeference->GetActorTransform().InverseTransformPosition(World);
	return Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(Local);
}

FString AOneMindGlobe::GetAttribution() const
{
	return GetDefault<UOneMindGlobeSettings>()->Attribution;
}
