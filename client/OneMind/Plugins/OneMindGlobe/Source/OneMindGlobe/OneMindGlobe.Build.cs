// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMindGlobe : ModuleRules
{
	public OneMindGlobe(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "CesiumRuntime", "OneMindCore" });
	}
}
