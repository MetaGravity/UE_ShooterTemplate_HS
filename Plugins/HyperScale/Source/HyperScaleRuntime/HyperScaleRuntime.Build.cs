// Copyright 2024 Metagravity. All Rights Reserved.

using UnrealBuildTool;

public class HyperScaleRuntime : ModuleRules
{
	public HyperScaleRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd"
			});
		}

		// Include private engine dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"DeveloperSettings",
			"Networking",
			"GameplayDebugger",
			"NavigationSystem",
			"HTTP",
			"Json"
		});

		// Include public engine dependencies
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
			"NetCore",
			"Sockets",
			"OnlineSubsystemUtils",
			"GameplayTags",
			"StructUtils",
			"QuarkAPI"
		});

		// Include plugin dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"QuarkAPI"
		});
	}
}