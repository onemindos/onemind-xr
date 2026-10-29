// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMind : ModuleRules
{
	public OneMind(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore",
			"CesiumRuntime",
			"OneMindCore", "OneMindBus", "OneMindGlobe", "OneMindLayers", "OneMindEntities", "OneMindCoT", "OneMindMission"
		});
	}
}
