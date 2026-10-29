// Project settings for the client's own TAK identity (features C3, C8), in DefaultGame.ini under
// [/Script/OneMindCoT.OneMindTakSettings]. No credential is ever a setting: the TAK user's
// password is typed into the enrollment prompt and discarded; the key and certificate live in
// the macOS keychain (OneMindKeychain).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OneMindTakSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "OneMind TAK"))
class ONEMINDCOT_API UOneMindTakSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** TAK Server host on the tailnet (live: tak.tail717ba0.ts.net, omos CLAUDE.md "TAK Server"). */
	UPROPERTY(Config, EditAnywhere, Category = "Server") FString ServerHost = TEXT("tak.tail717ba0.ts.net");
	/** Streaming input with client certificates. */
	UPROPERTY(Config, EditAnywhere, Category = "Server") int32 StreamPort = 8089;
	/** HTTPS without client certificates, used for enrollment. */
	UPROPERTY(Config, EditAnywhere, Category = "Server") int32 EnrollPort = 8446;
	/** Also check the server certificate's host name (Common Commo checks the chain only). */
	UPROPERTY(Config, EditAnywhere, Category = "Server") bool bVerifyHostName = false;
	/** Connect on start when an enrolled identity exists. */
	UPROPERTY(Config, EditAnywhere, Category = "Server") bool bAutoConnect = false;
	/** "auto" accepts TAK Protocol v1 when the server offers it; "xml" never negotiates. */
	UPROPERTY(Config, EditAnywhere, Category = "Server") FString Protocol = TEXT("auto");
	/** Also send drawn shapes (B11) over the client's own TAK stream, bypassing the projector's
	 *  AOI, marking and rate gates (docs/contracts/cot-projection.md §1 "Who may publish").
	 *  Off until the owner ratifies proposal UE-P5; shapes always go through svc.cot.send. */
	UPROPERTY(Config, EditAnywhere, Category = "Server") bool bSendShapesOnOwnStream = false;

	/** Contact uid; empty = "ONEMIND-" + this machine's id. */
	UPROPERTY(Config, EditAnywhere, Category = "Identity") FString ClientUid;
	UPROPERTY(Config, EditAnywhere, Category = "Identity") FString Callsign = TEXT("ONEMIND");
	/** TAK team colour and role (<__group>). */
	UPROPERTY(Config, EditAnywhere, Category = "Identity") FString Team = TEXT("Cyan");
	UPROPERTY(Config, EditAnywhere, Category = "Identity") FString Role = TEXT("Team Member");
	/** Own SA period in seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Identity") int32 SaIntervalSeconds = 30;
};
