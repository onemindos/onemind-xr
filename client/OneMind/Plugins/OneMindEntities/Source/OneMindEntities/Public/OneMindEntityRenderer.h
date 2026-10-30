// Batched entity rendering (feature B5) from the layer stores of the client core.
//  - The band of every entity comes from om::PlanRender (host-tested): horizon culling,
//    icons far, instanced models mid, actors near (nearest N), clusters for dense icons.
//  - One UInstancedStaticMeshComponent per (band, symbol class); instance transforms are
//    rebuilt in bulk at RefreshHz, not per message, so a burst of deltas or a layer load
//    never costs more than one rebuild (B5 "no frame drop when a layer loads").
//  - Engine basic shapes only (/Engine/BasicShapes), so the project needs no content
//    assets. A Niagara far-icon system (unreal-platform.md "10,000 entities") can replace
//    the icon ISMs later through IconSystem.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "OneMindCore/RenderPlan.h"

#include "OneMindEntityRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class AOneMindEntityActor;
class ACesiumGeoreference;

UCLASS()
class ONEMINDENTITIES_API AOneMindEntityRenderer : public AActor
{
	GENERATED_BODY()

public:
	AOneMindEntityRenderer();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "OneMind|Entities", meta = (WorldContext = "WorldContextObject"))
	static AOneMindEntityRenderer* GetOrSpawn(const UObject* WorldContextObject);

	/** Picks the entity nearest to a world position (e.g. a cursor hit on the globe).
	 *  Returns "layer/entity_id" or empty. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Entities")
	FString PickAtWorldLocation(const FVector& WorldLocation, double RadiusMeters);

	UFUNCTION(BlueprintCallable, Category = "OneMind|Entities")
	void SetSelected(const FString& LayerAndId) { Selected = LayerAndId; }

	UFUNCTION(BlueprintPure, Category = "OneMind|Entities")
	FString GetSelected() const { return Selected; }

	/** Counters for the HUD and the 10,000-entity check. */
	UFUNCTION(BlueprintPure, Category = "OneMind|Entities")
	FString GetStatsText() const;

	UPROPERTY(EditAnywhere, Category = "OneMind|Entities")
	float RefreshHz = 15.f;

	UPROPERTY(EditAnywhere, Category = "OneMind|Entities")
	double ActorMaxMeters = 2000.0;

	UPROPERTY(EditAnywhere, Category = "OneMind|Entities")
	double ModelMaxMeters = 60000.0;

	UPROPERTY(EditAnywhere, Category = "OneMind|Entities")
	int32 MaxActors = 64;

	/** Icon size as a fraction of camera distance (constant on-screen size). */
	UPROPERTY(EditAnywhere, Category = "OneMind|Entities")
	double IconAngularSize = 0.006;

	UPROPERTY(EditAnywhere, Category = "OneMind|Entities")
	double ModelSizeMeters = 40.0;

	UPROPERTY(EditAnywhere, Category = "OneMind|Entities")
	int32 ClusterMin = 12;

protected:
	virtual void BeginPlay() override;

private:
	UInstancedStaticMeshComponent* GetIsm(int32 Key, UStaticMesh* Mesh, const FLinearColor& Color);
	void Rebuild();

	UPROPERTY()
	TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> Isms;

	UPROPERTY()
	TArray<TObjectPtr<AOneMindEntityActor>> ActorPool;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	TWeakObjectPtr<ACesiumGeoreference> Georeference;
	FString Selected;
	float Accum = 0.f;
	double LastRebuildMs = 0.0;
	int32 LastTotal = 0, LastIcons = 0, LastModels = 0, LastActors = 0, LastClusters = 0, LastCulled = 0;
};
