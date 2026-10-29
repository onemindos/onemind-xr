// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindEntityActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AOneMindEntityActor::AOneMindEntityActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Cone.Succeeded()) Mesh->SetStaticMesh(Cone.Object);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Mesh);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetRelativeLocation(FVector(0, 0, 120));
	Label->SetWorldSize(60.f);
	SetActorHiddenInGame(true);
}

void AOneMindEntityActor::Show(const FTransform& T, const FString& Text, bool bSelected, bool bStale)
{
	SetActorTransform(T);
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(bSelected ? FColor::Yellow : (bStale ? FColor(160, 160, 160) : FColor::White));
	SetActorHiddenInGame(false);
}

void AOneMindEntityActor::HideEntity()
{
	SetActorHiddenInGame(true);
}
