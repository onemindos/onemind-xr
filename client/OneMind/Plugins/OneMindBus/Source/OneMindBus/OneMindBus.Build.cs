// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMindBus : ModuleRules
{
	public OneMindBus(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "OneMindCore" });
		// The engine WebSockets module (libwebsockets on Mac, Linux, Android; WinHttp or lws on Windows).
		PrivateDependencyModuleNames.AddRange(new string[] { "WebSockets", "Json" });
	}
}
