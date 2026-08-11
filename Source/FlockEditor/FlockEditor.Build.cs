// Copyright (c) Jared Taylor

using UnrealBuildTool;

public class FlockEditor : ModuleRules
{
	public FlockEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DeveloperSettings",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"PlacementMode",
				"LevelEditor",
				"ToolMenus",
				"Projects",
				"PropertyEditor",
				"AssetRegistry",
				"Flock",
				"AnimToTexture",
				"AnimToTextureEditor",
			}
			);
	}
}
