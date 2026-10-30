// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindGameMode.h"

#include "GlobeAwareDefaultPawn.h"
#include "OneMindEntityRenderer.h"
#include "OneMindGlobe.h"
#include "OneMindPlayerController.h"

AOneMindGameMode::AOneMindGameMode()
{
	DefaultPawnClass = AGlobeAwareDefaultPawn::StaticClass();
	PlayerControllerClass = AOneMindPlayerController::StaticClass();
}

void AOneMindGameMode::StartPlay()
{
	AOneMindGlobe::GetOrSpawn(this);
	AOneMindEntityRenderer::GetOrSpawn(this);
	Super::StartPlay();
}
