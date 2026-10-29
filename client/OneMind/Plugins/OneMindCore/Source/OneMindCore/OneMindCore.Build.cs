// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;

// Engine-independent C++17 core. The sources include no engine headers (except the
// module glue in OneMindCoreModule.cpp), so the same files build on the host with CMake
// (client/OneMind/HostTests) where they are unit- and integration-tested.
public class OneMindCore : ModuleRules
{
	public OneMindCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;   // keep engine headers out of the plain C++ files
		bUseUnity = false;                // files use file-local helper namespaces
		CppStandard = CppStandardVersion.Cpp20;
		bEnableExceptions = false;
		bUseRTTI = false;
		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
