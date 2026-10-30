// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMindEntities : ModuleRules
{
	public OneMindEntities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "DeveloperSettings",
			"CesiumRuntime", "OneMindCore", "OneMindBus", "OneMindLayers"
		});
	}
}
