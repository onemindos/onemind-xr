// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMindLayers : ModuleRules
{
	public OneMindLayers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "CesiumRuntime", "OneMindCore", "OneMindBus" });
	}
}
