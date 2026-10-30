// SPDX-License-Identifier: GPL-3.0-or-later
using UnrealBuildTool;
using System.Collections.Generic;

public class OneMindEditorTarget : TargetRules
{
	public OneMindEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("OneMind");
	}
}
