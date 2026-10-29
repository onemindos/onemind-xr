// Project settings for the OneMind bus (Edit > Project Settings > OneMind > Bus).
// No credentials are stored here: user and password come from the environment
// (ONEMIND_NATS_USER, ONEMIND_NATS_PASSWORD) at run time, never from Git (repo rule 4).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OneMindBusSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "OneMind Bus"))
class ONEMINDBUS_API UOneMindBusSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOneMindBusSettings();
	virtual FName GetCategoryName() const override { return TEXT("OneMind"); }

	/** NATS WebSocket URLs, tried in order. The Mac uses the local leaf's WebSocket
	 *  listener; the headset uses the Mac's LAN address (OWNER-STEPS.md). */
	UPROPERTY(config, EditAnywhere, Category = "Connection")
	TArray<FString> Urls;

	/** Client name shown in the server's connz. */
	UPROPERTY(config, EditAnywhere, Category = "Connection")
	FString ClientName;

	/** Instance id used in CoT uids and svc.cot.send requests. */
	UPROPERTY(config, EditAnywhere, Category = "Connection")
	FString ClientId;

	UPROPERTY(config, EditAnywhere, Category = "Connection", meta = (ClampMin = "1000"))
	int32 PingIntervalMs = 20000;

	UPROPERTY(config, EditAnywhere, Category = "Connection", meta = (ClampMin = "50"))
	int32 ReconnectWaitMinMs = 250;

	UPROPERTY(config, EditAnywhere, Category = "Connection", meta = (ClampMin = "100"))
	int32 ReconnectWaitMaxMs = 5000;

	/** Upper bound on bus messages processed per frame (frame budget). */
	UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ClampMin = "100"))
	int32 MaxMessagesPerFrame = 20000;

	/** Spike S5 probe: watch this KV bucket and draw any value that has lat/lon as a
	 *  marker (OWNER-STEPS.md, S5). Empty disables the probe. */
	UPROPERTY(config, EditAnywhere, Category = "S5 Probe")
	FString ProbeBucket;

	UPROPERTY(config, EditAnywhere, Category = "S5 Probe")
	FString ProbeKeyFilter;

	/** Layer manifests (KV world-state `{layer}.meta`, layer-snapshot.md §2). */
	UPROPERTY(config, EditAnywhere, Category = "Layers")
	bool bWatchLayerManifests = true;
};
