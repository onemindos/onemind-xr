// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindBusSubsystem.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OneMindBusSettings.h"
#include "OneMindUeWebSocketTransport.h"

DEFINE_LOG_CATEGORY(LogOneMindBus);

UOneMindBusSettings::UOneMindBusSettings()
{
	Urls.Add(TEXT("ws://127.0.0.1:9222"));
	ClientName = TEXT("onemind-unreal");
	ClientId = TEXT("unreal");
}

namespace OneMindBusPrivate
{
std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
}

void UOneMindBusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	using OneMindBusPrivate::ToStd;
	const UOneMindBusSettings* S = GetDefault<UOneMindBusSettings>();

	om::ClientConfig Cfg;
	Cfg.nats.urls.clear();
	for (const FString& U : S->Urls)
	{
		Cfg.nats.urls.push_back(ToStd(U));
	}
	// Command line override: -OneMindBusUrl=ws://192.168.1.20:9222 (the headset build uses it).
	FString CmdUrl;
	if (FParse::Value(FCommandLine::Get(), TEXT("OneMindBusUrl="), CmdUrl))
	{
		Cfg.nats.urls = {ToStd(CmdUrl)};
	}
	Cfg.nats.name = ToStd(S->ClientName);
	Cfg.nats.ping_interval_ms = S->PingIntervalMs;
	Cfg.nats.reconnect_wait_min_ms = S->ReconnectWaitMinMs;
	Cfg.nats.reconnect_wait_max_ms = S->ReconnectWaitMaxMs;
	// Credentials from the environment only (repo rule 4: never in Git or config files).
	const FString User = FPlatformMisc::GetEnvironmentVariable(TEXT("ONEMIND_NATS_USER"));
	const FString Pass = FPlatformMisc::GetEnvironmentVariable(TEXT("ONEMIND_NATS_PASSWORD"));
	if (!User.IsEmpty())
	{
		Cfg.nats.user = ToStd(User);
		Cfg.nats.password = ToStd(Pass);
	}
	Cfg.client_id = ToStd(S->ClientId);
	Cfg.max_msgs_per_tick = static_cast<size_t>(FMath::Max(100, S->MaxMessagesPerFrame));
	if (S->bWatchLayerManifests)
	{
		Cfg.kv_watches["world-state"] = "*.meta";
	}

	Core = std::make_unique<om::ClientCore>(std::make_shared<FOneMindUeWebSocketTransport>(), Cfg);
	Core->Start(om::WallClockMs());
	UE_LOG(LogOneMindBus, Log, TEXT("OneMind bus: connecting to %s as %s"), S->Urls.Num() ? *S->Urls[0] : TEXT("?"), *S->ClientName);
}

void UOneMindBusSubsystem::Deinitialize()
{
	if (Core)
	{
		Core->Stop();
		Core.reset();
	}
	Super::Deinitialize();
}

void UOneMindBusSubsystem::StartProbe()
{
	const UOneMindBusSettings* S = GetDefault<UOneMindBusSettings>();
	if (S->ProbeBucket.IsEmpty() || bProbeStarted) return;
	bProbeStarted = true;
	const std::string Filter = S->ProbeKeyFilter.IsEmpty() ? std::string(">") : OneMindBusPrivate::ToStd(S->ProbeKeyFilter);
	Core->WatchKv(OneMindBusPrivate::ToStd(S->ProbeBucket), Filter,
		[this](const std::string& Key, const std::string& Value, bool bDeleted)
		{
			const om::TimeMs Now = om::WallClockMs();
			if (bDeleted)
			{
				ProbeStore.Remove(Key, om::RemoveReason::Deleted, Now);
				return;
			}
			om::Json J;
			if (!om::Json::Parse(Value, J) || !J.is_object()) return;
			om::RenderEntity E;
			om::RenderEntity::FromJson(J, E);
			E.entity_id = Key;  // the KV key names the marker
			E.seq = 0;          // KV revisions replace the value; no entity revision rule here
			E.callsign = E.callsign.empty() ? Key : E.callsign;
			if (ProbeStore.Has(Key)) ProbeStore.Remove(Key, om::RemoveReason::LeftAoi, Now);
			ProbeStore.Upsert(E, Now);
			UE_LOG(LogOneMindBus, Log, TEXT("S5 probe: %s at %.6f, %.6f"), UTF8_TO_TCHAR(Key.c_str()), E.lat, E.lon);
		},
		om::WallClockMs());
}

void UOneMindBusSubsystem::Tick(float DeltaTime)
{
	if (!Core) return;
	Core->Tick(om::WallClockMs());
	const bool bConnected = Core->nats().connected();
	if (bConnected != bWasConnected)
	{
		bWasConnected = bConnected;
		if (bConnected)
		{
			UE_LOG(LogOneMindBus, Log, TEXT("OneMind bus connected (server %s, reconnects %llu)"),
				UTF8_TO_TCHAR(Core->nats().server_info().str("version").c_str()),
				static_cast<unsigned long long>(Core->nats().stats().reconnects));
			StartProbe();
		}
		else
		{
			UE_LOG(LogOneMindBus, Warning, TEXT("OneMind bus disconnected: %s"), UTF8_TO_TCHAR(Core->nats().last_error().c_str()));
		}
		OnConnectionChanged.Broadcast(bConnected);
	}
	if (Core->nats().stats().reconnects != LastReconnects)
	{
		LastReconnects = Core->nats().stats().reconnects;
		UE_LOG(LogOneMindBus, Log, TEXT("OneMind bus reconnected (%llu reconnects so far)"), static_cast<unsigned long long>(LastReconnects));
	}
}

TStatId UOneMindBusSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOneMindBusSubsystem, STATGROUP_Tickables);
}

bool UOneMindBusSubsystem::IsConnected() const
{
	return Core && Core->nats().connected();
}

FString UOneMindBusSubsystem::GetStatusText() const
{
	if (!Core) return TEXT("bus: not started");
	const om::NatsClient& N = Core->nats();
	const TCHAR* State = N.connected() ? TEXT("connected") : TEXT("connecting");
	return FString::Printf(TEXT("bus: %s | server %s | reconnects %llu | in %llu msgs | %s"), State,
		UTF8_TO_TCHAR(N.server_info().str("version", "-").c_str()),
		static_cast<unsigned long long>(N.stats().reconnects), static_cast<unsigned long long>(N.stats().msgs_in),
		UTF8_TO_TCHAR(N.last_error().c_str()));
}

void UOneMindBusSubsystem::Reconnect()
{
	if (!Core) return;
	Core->nats().Stop();
	Core->nats().Start(om::WallClockMs());
}
