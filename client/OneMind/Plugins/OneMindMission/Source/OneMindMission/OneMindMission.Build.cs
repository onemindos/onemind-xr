// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMindMission : ModuleRules
{
	public OneMindMission(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "OneMindCore", "OneMindBus" });
	}
}
