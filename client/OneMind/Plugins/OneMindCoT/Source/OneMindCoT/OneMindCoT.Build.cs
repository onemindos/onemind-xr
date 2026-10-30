// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMindCoT : ModuleRules
{
	public OneMindCoT(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "OneMindCore", "OneMindBus" });
	}
}
