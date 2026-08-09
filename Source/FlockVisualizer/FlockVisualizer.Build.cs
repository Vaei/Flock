// Copyright (c) Jared Taylor. All Rights Reserved

using UnrealBuildTool;

public class FlockVisualizer : ModuleRules
{
	public FlockVisualizer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"Flock",

				// Reached through FlockSpeciesData.h, which carries the bake recipe.
				"AnimToTexture",
			}
		);
	}
}
