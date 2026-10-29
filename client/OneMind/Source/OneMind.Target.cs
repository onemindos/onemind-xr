// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;
using System.Collections.Generic;

public class OneMindTarget : TargetRules
{
	public OneMindTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("OneMind");
	}
}
