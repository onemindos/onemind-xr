// Act in the client: TAK Data Sync missions (C7) and click-to-task with a human confirm (D3).
//
// Missions: om::TakMissionClient over the bus (svc.tak.mission.*, proposed for omos tak-client,
// Docs/svc-tak-mission.md); the view of each open mission follows evt.mission.tak-*.changed.
// Tasking: om::AssetTasking. A click proposes asset.goto through svc.policy.command.propose;
// the confirm dialog shows the exact parameters and the params_hash; only ConfirmPending /
// RejectPending, wired to the dialog's buttons, send svc.policy.command.confirm. The client
// never publishes cmd.* (asset-command.md §2) and its NATS user cannot (CLIENT account).
// Not compiled on the build machine (no Unreal); the core is tested on the host
// (unit_act.cpp, it_act.cpp, scripts/e2e_d3_policy.py against the real policy service).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "OneMindCore/AssetTasking.h"
#include "OneMindCore/TakMissions.h"

#include <memory>

#include "OneMindMissionSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FOneMindPendingTask
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString ActionId;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString AssetId;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString Verb;
	/** The exact parameters as hashed (JCS JSON), shown verbatim in the dialog. */
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString ParamsJson;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString ParamsHash;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString Level;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString Status;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") FString Denial;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") int32 SecondsLeft = 0;
	/** Audit trail so far: proposed, approved, dispatched, acknowledged, executed... */
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Task") TArray<FString> Timeline;
};

USTRUCT(BlueprintType)
struct FOneMindMissionItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Mission") FString Uid;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Mission") FString Callsign;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Mission") FString CotType;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Mission") double Lat = 0;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Mission") double Lon = 0;
	UPROPERTY(BlueprintReadOnly, Category = "OneMind|Mission") FString Creator;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOneMindTaskChanged, const FOneMindPendingTask&, Task);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOneMindMissionResult, const FString&, Operation, const FString&, Error);

UCLASS()
class ONEMINDMISSION_API UOneMindMissionSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	// --- D3 ---------------------------------------------------------------------------------
	/** Proposes "go to this point" for a sim-* asset. Opens the confirm dialog on success. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Task")
	void ProposeGoto(const FString& AssetId, const FString& AssetClass, double Lat, double Lon, double HaeMeters);

	/** The dialog's current task (Status empty when there is none). */
	UFUNCTION(BlueprintPure, Category = "OneMind|Task")
	FOneMindPendingTask GetPendingTask() const;

	/** Called by the dialog when it has drawn the parameters and hash. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Task")
	void MarkPendingShown();

	/** The human's "Confirm" button. */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Task")
	void ConfirmPending();

	/** The human's "Reject" button (a reason is required by the contract). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Task")
	void RejectPending(const FString& Reason);

	UPROPERTY(BlueprintAssignable, Category = "OneMind|Task") FOnOneMindTaskChanged OnTaskChanged;

	// --- C7 ---------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "OneMind|Mission") void RefreshMissions();
	UFUNCTION(BlueprintPure, Category = "OneMind|Mission") TArray<FString> GetMissionNames() const;
	UFUNCTION(BlueprintCallable, Category = "OneMind|Mission") void OpenMission(const FString& Name);
	UFUNCTION(BlueprintCallable, Category = "OneMind|Mission") void SubscribeMission(const FString& Name, bool bSubscribe);
	UFUNCTION(BlueprintPure, Category = "OneMind|Mission") TArray<FOneMindMissionItem> GetMissionItems(const FString& Name) const;
	UFUNCTION(BlueprintPure, Category = "OneMind|Mission") TArray<FString> GetMissionLog(const FString& Name) const;
	/** Publishes a marker the user placed into the mission (as an ATAK user adds a feature). */
	UFUNCTION(BlueprintCallable, Category = "OneMind|Mission")
	void AddMarkerToMission(const FString& Name, double Lat, double Lon, const FString& Callsign, const FString& CotType = TEXT("a-u-G"));

	UPROPERTY(BlueprintAssignable, Category = "OneMind|Mission") FOnOneMindMissionResult OnMissionResult;

	/** Multi-line text for the overlay: the confirm dialog and the open mission. */
	FString GetOverlayText() const;

private:
	bool Ensure();
	FOneMindPendingTask ToUi(const om::PendingAction& A) const;

	std::unique_ptr<om::AssetTasking> Tasking;
	std::unique_ptr<om::TakMissionClient> Missions;
	FString PendingActionId;
	FString OpenMissionName;
	FOneMindPendingTask LastBroadcast;
};
