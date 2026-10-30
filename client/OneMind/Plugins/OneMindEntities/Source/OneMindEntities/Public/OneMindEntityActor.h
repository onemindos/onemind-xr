// A near-range entity: mesh, label, and selection ring. Pooled by the renderer.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OneMindEntityActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class ONEMINDENTITIES_API AOneMindEntityActor : public AActor
{
	GENERATED_BODY()

public:
	AOneMindEntityActor();
	void Show(const FTransform& T, const FString& Label, bool bSelected, bool bStale);
	void HideEntity();

	UPROPERTY(VisibleAnywhere, Category = "OneMind")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "OneMind")
	TObjectPtr<UTextRenderComponent> Label;
};
