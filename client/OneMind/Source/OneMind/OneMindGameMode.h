// Game mode: Cesium globe-aware pawn, the OneMind player controller, and the globe and
// entity renderer spawned in code, so the default map needs nothing but this game mode.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OneMindGameMode.generated.h"

UCLASS()
class ONEMIND_API AOneMindGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOneMindGameMode();
	virtual void StartPlay() override;
};
