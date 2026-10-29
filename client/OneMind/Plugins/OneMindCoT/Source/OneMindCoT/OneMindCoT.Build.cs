// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

public class OneMindCoT : ModuleRules
{
	public OneMindCoT(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "OneMindCore", "OneMindBus" });
		// C3: the TAK streaming connection (TLS over FSocket) and enrollment.
		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking" });
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private"));
		// The engine's bundled OpenSSL, as the engine's own SSL module uses it. Unverified for
		// UE 5.8 on Mac until the owner's build (docs/evidence/M4/UE/OWNER-STEPS.md, step UE-1).
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// Keychain storage for the TAK client key and certificate (OneMindKeychain.cpp).
			PublicFrameworks.Add("Security");
		}
	}
}
