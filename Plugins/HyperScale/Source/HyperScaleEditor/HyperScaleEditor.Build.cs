// Copyright 2024 Metagravity. All Rights Reserved.

using UnrealBuildTool;

public class HyperScaleEditor : ModuleRules
{
	public HyperScaleEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "AssetTools" });
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Include editor dependencies
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"Blutility"
			});
		}

		// Include engine dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UMG",
			"ToolMenus",
			"InputCore",
			"EditorFramework",
			"Projects",
			"GameplayTags",
			"PropertyEditor",
			"SourceControl",
			"KismetCompiler",
			"BlueprintGraph",
			"GraphEditor",
			"Json",
			"BlueprintHeaderView",
			"UMGEditor"
		});

		// Include plugin dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HyperScaleRuntime",
			"QuarkAPI"
		});
	}
}