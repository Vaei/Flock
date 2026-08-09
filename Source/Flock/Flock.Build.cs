// Copyright (c) Jared Taylor. All Rights Reserved

using UnrealBuildTool;

public class Flock : ModuleRules
{
	public Flock(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
				"DeveloperSettings",
				"MassCore",
				"MassEntity",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"AnimToTexture",
				"Niagara",
				"NiagaraCore",
			}
			);
	}
}
