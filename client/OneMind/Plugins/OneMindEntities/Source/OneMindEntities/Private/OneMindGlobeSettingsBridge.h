// Reads the attribution string from the globe settings without a module dependency
// cycle: the setting lives in DefaultGame.ini ([/Script/OneMindGlobe.OneMindGlobeSettings]).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

inline FString OneMindGlobeAttribution()
{
	FString A;
	if (GConfig)
	{
		GConfig->GetString(TEXT("/Script/OneMindGlobe.OneMindGlobeSettings"), TEXT("Attribution"), A, GGameIni);
	}
	return A.IsEmpty() ? FString(TEXT("Map data: see OneMind Globe settings")) : A;
}
