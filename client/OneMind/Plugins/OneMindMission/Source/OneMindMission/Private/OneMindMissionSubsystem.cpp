// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindMissionSubsystem.h"

#include "Engine/GameInstance.h"
#include "OneMindBusSubsystem.h"
#include "OneMindCore/ClientCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogOneMindMission, Log, All);

bool UOneMindMissionSubsystem::Ensure()
{
	if (Tasking && Missions) return true;
	UGameInstance* GI = GetGameInstance();
	UOneMindBusSubsystem* Bus = GI ? GI->GetSubsystem<UOneMindBusSubsystem>() : nullptr;
	om::ClientCore* Core = Bus ? Bus->GetCore() : nullptr;
	if (!Core) return false;
	// The user id is informational: the policy service takes the identity from the
	// authenticated NATS connection (asset-command.md §6.1 requested_by).
	const std::string User = "user:" + (Core->config().nats.user.empty() ? Core->config().client_id : Core->config().nats.user);
	Tasking = std::make_unique<om::AssetTasking>(Core->nats(), User);
	Tasking->Watch();
	Missions = std::make_unique<om::TakMissionClient>(Core->nats(), "ONEMIND-" + Core->config().client_id);
	Missions->Watch();
	return true;
}

void UOneMindMissionSubsystem::Deinitialize()
{
	Tasking.reset();
	Missions.reset();
	Super::Deinitialize();
}

TStatId UOneMindMissionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOneMindMissionSubsystem, STATGROUP_Tickables);
}

void UOneMindMissionSubsystem::Tick(float DeltaTime)
{
	if (!Tasking) return;
	Tasking->Tick(om::WallClockMs());
	if (const om::PendingAction* A = Tasking->Find(TCHAR_TO_UTF8(*PendingActionId)))
	{
		const FOneMindPendingTask T = ToUi(*A);
		if (T.Status != LastBroadcast.Status || T.Timeline.Num() != LastBroadcast.Timeline.Num())
		{
			LastBroadcast = T;
			OnTaskChanged.Broadcast(T);
		}
	}
}

FOneMindPendingTask UOneMindMissionSubsystem::ToUi(const om::PendingAction& A) const
{
	FOneMindPendingTask T;
	T.ActionId = UTF8_TO_TCHAR(A.action_id.c_str());
	T.AssetId = UTF8_TO_TCHAR(A.draft.asset_id.c_str());
	T.Verb = UTF8_TO_TCHAR(A.draft.verb.c_str());
	T.ParamsJson = UTF8_TO_TCHAR(om::JcsCanonical(A.draft.params).c_str());
	T.ParamsHash = UTF8_TO_TCHAR(A.params_hash.c_str());
	T.Level = UTF8_TO_TCHAR(A.level.c_str());
	T.Status = UTF8_TO_TCHAR(A.status.c_str());
	T.Denial = UTF8_TO_TCHAR((A.denial_code + (A.denial_message.empty() ? "" : ": " + A.denial_message)).c_str());
	T.SecondsLeft = A.confirm_by ? FMath::Max<int32>(0, static_cast<int32>((A.confirm_by - om::WallClockMs()) / 1000)) : 0;
	for (const om::ActionEvent& E : A.timeline) T.Timeline.Add(UTF8_TO_TCHAR(E.event.c_str()));
	return T;
}

void UOneMindMissionSubsystem::ProposeGoto(const FString& AssetId, const FString& AssetClass, double Lat, double Lon, double HaeMeters)
{
	if (!Ensure()) return;
	const om::TaskDraft D = om::GotoDraft(TCHAR_TO_UTF8(*AssetId), TCHAR_TO_UTF8(*AssetClass), om::GeoPoint{Lat, Lon, 0}, HaeMeters, 0,
		"clicked on the map in the OneMind client");
	TWeakObjectPtr<UOneMindMissionSubsystem> Weak(this);
	Tasking->Propose(D, om::WallClockMs(), [Weak](const om::PendingAction& A, const std::string& Err)
	{
		if (!Weak.IsValid()) return;
		UE_LOG(LogOneMindMission, Log, TEXT("asset.%s %s: %s %s"), UTF8_TO_TCHAR(A.draft.verb.c_str()), UTF8_TO_TCHAR(A.draft.asset_id.c_str()),
			UTF8_TO_TCHAR(A.status.c_str()), UTF8_TO_TCHAR(Err.c_str()));
		Weak->PendingActionId = UTF8_TO_TCHAR(A.action_id.c_str());
		Weak->LastBroadcast = Weak->ToUi(A);
		Weak->OnTaskChanged.Broadcast(Weak->LastBroadcast);
	});
}

FOneMindPendingTask UOneMindMissionSubsystem::GetPendingTask() const
{
	const om::PendingAction* A = Tasking ? Tasking->Find(TCHAR_TO_UTF8(*PendingActionId)) : nullptr;
	return A ? ToUi(*A) : LastBroadcast;
}

void UOneMindMissionSubsystem::MarkPendingShown()
{
	if (Tasking) Tasking->MarkShown(TCHAR_TO_UTF8(*PendingActionId), om::WallClockMs());  // first call starts the 500 ms clock
}

void UOneMindMissionSubsystem::ConfirmPending()
{
	if (!Tasking) return;
	Tasking->Confirm(TCHAR_TO_UTF8(*PendingActionId), true, std::string(), om::WallClockMs(),
		[](const om::PendingAction& A, const std::string& Err)
		{
			UE_LOG(LogOneMindMission, Log, TEXT("confirm %s: %s %s"), UTF8_TO_TCHAR(A.action_id.c_str()), UTF8_TO_TCHAR(A.status.c_str()),
				UTF8_TO_TCHAR(Err.c_str()));
		});
}

void UOneMindMissionSubsystem::RejectPending(const FString& Reason)
{
	if (!Tasking) return;
	Tasking->Confirm(TCHAR_TO_UTF8(*PendingActionId), false, TCHAR_TO_UTF8(*Reason), om::WallClockMs(),
		[](const om::PendingAction&, const std::string&) {});
}

void UOneMindMissionSubsystem::RefreshMissions()
{
	if (!Ensure()) return;
	TWeakObjectPtr<UOneMindMissionSubsystem> Weak(this);
	Missions->List([Weak](const std::string& Err)
	{
		if (Weak.IsValid()) Weak->OnMissionResult.Broadcast(TEXT("list"), UTF8_TO_TCHAR(Err.c_str()));
	});
}

TArray<FString> UOneMindMissionSubsystem::GetMissionNames() const
{
	TArray<FString> Out;
	if (Missions)
		for (const om::TakMissionSummary& M : Missions->Missions()) Out.Add(UTF8_TO_TCHAR(M.name.c_str()));
	return Out;
}

void UOneMindMissionSubsystem::OpenMission(const FString& Name)
{
	if (!Ensure()) return;
	if (!OpenMissionName.IsEmpty() && OpenMissionName != Name) Missions->Close(TCHAR_TO_UTF8(*OpenMissionName));
	OpenMissionName = Name;
	TWeakObjectPtr<UOneMindMissionSubsystem> Weak(this);
	Missions->Open(TCHAR_TO_UTF8(*Name), [Weak](const std::string& Err)
	{
		if (Weak.IsValid()) Weak->OnMissionResult.Broadcast(TEXT("open"), UTF8_TO_TCHAR(Err.c_str()));
	});
}

void UOneMindMissionSubsystem::SubscribeMission(const FString& Name, bool bSubscribe)
{
	if (!Ensure()) return;
	TWeakObjectPtr<UOneMindMissionSubsystem> Weak(this);
	auto Done = [Weak, bSubscribe](const std::string& Err)
	{
		if (Weak.IsValid()) Weak->OnMissionResult.Broadcast(bSubscribe ? TEXT("subscribe") : TEXT("unsubscribe"), UTF8_TO_TCHAR(Err.c_str()));
	};
	if (bSubscribe) Missions->Subscribe(TCHAR_TO_UTF8(*Name), Done);
	else Missions->Unsubscribe(TCHAR_TO_UTF8(*Name), Done);
}

TArray<FOneMindMissionItem> UOneMindMissionSubsystem::GetMissionItems(const FString& Name) const
{
	TArray<FOneMindMissionItem> Out;
	const om::TakMissionView* V = Missions ? Missions->View(TCHAR_TO_UTF8(*Name)) : nullptr;
	if (!V) return Out;
	for (const om::TakMissionUid& U : V->uids)
	{
		FOneMindMissionItem I;
		I.Uid = UTF8_TO_TCHAR(U.uid.c_str());
		I.Callsign = UTF8_TO_TCHAR(U.callsign.c_str());
		I.CotType = UTF8_TO_TCHAR(U.cot_type.c_str());
		I.Lat = U.point.lat;
		I.Lon = U.point.lon;
		I.Creator = UTF8_TO_TCHAR(U.creator_uid.c_str());
		Out.Add(I);
	}
	return Out;
}

TArray<FString> UOneMindMissionSubsystem::GetMissionLog(const FString& Name) const
{
	TArray<FString> Out;
	const om::TakMissionView* V = Missions ? Missions->View(TCHAR_TO_UTF8(*Name)) : nullptr;
	if (!V) return Out;
	for (const om::TakMissionLog& L : V->logs) Out.Add(UTF8_TO_TCHAR((L.creator_uid + ": " + L.content).c_str()));
	for (const om::TakMissionChange& C : V->changes)
		Out.Add(UTF8_TO_TCHAR((C.type + " " + (C.callsign.empty() ? C.uid : C.callsign) + " by " + C.creator_uid).c_str()));
	return Out;
}

void UOneMindMissionSubsystem::AddMarkerToMission(const FString& Name, double Lat, double Lon, const FString& Callsign, const FString& CotType)
{
	if (!Ensure()) return;
	UGameInstance* GI = GetGameInstance();
	UOneMindBusSubsystem* Bus = GI ? GI->GetSubsystem<UOneMindBusSubsystem>() : nullptr;
	if (!Bus || !Bus->GetCore()) return;
	om::CotBuild B;
	B.uid = Bus->GetCore()->NewCotUid();
	B.callsign = TCHAR_TO_UTF8(*Callsign);
	B.simulated = true;
	const om::CotEvent Ev = om::BuildMarker(B, TCHAR_TO_UTF8(*CotType), om::GeoPoint{Lat, Lon, 9999999.0}, om::WallClockMs());
	TWeakObjectPtr<UOneMindMissionSubsystem> Weak(this);
	Missions->PutFeature(TCHAR_TO_UTF8(*Name), Ev, [Weak](const std::string& Err)
	{
		if (Weak.IsValid()) Weak->OnMissionResult.Broadcast(TEXT("add"), UTF8_TO_TCHAR(Err.c_str()));
	});
}

FString UOneMindMissionSubsystem::GetOverlayText() const
{
	FString Out;
	const FOneMindPendingTask T = GetPendingTask();
	if (!T.Status.IsEmpty())
	{
		Out += FString::Printf(TEXT("TASK %s %s [%s] %s\n  params %s\n  hash   %s\n"), *T.Verb, *T.AssetId, *T.Level, *T.Status, *T.ParamsJson,
			*T.ParamsHash.Left(23));
		if (T.Status == TEXT("awaiting_confirmation"))
			Out += FString::Printf(TEXT("  Y = confirm, N = reject   (%d s left)\n"), T.SecondsLeft);
		if (!T.Denial.IsEmpty()) Out += TEXT("  ") + T.Denial + TEXT("\n");
		if (T.Timeline.Num()) Out += TEXT("  audit: ") + FString::Join(T.Timeline, TEXT(" > ")) + TEXT("\n");
	}
	if (!OpenMissionName.IsEmpty())
	{
		const TArray<FOneMindMissionItem> Items = GetMissionItems(OpenMissionName);
		Out += FString::Printf(TEXT("MISSION %s: %d items\n"), *OpenMissionName, Items.Num());
		for (int32 i = 0; i < Items.Num() && i < 8; ++i) Out += TEXT("  ") + Items[i].Callsign + TEXT(" (") + Items[i].CotType + TEXT(")\n");
	}
	return Out;
}
