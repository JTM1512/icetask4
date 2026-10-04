// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IceTask4 : ModuleRules
{
	public IceTask4(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"IceTask4",
			"IceTask4/Variant_Platforming",
			"IceTask4/Variant_Platforming/Animation",
			"IceTask4/Variant_Combat",
			"IceTask4/Variant_Combat/AI",
			"IceTask4/Variant_Combat/Animation",
			"IceTask4/Variant_Combat/Gameplay",
			"IceTask4/Variant_Combat/Interfaces",
			"IceTask4/Variant_Combat/UI",
			"IceTask4/Variant_SideScrolling",
			"IceTask4/Variant_SideScrolling/AI",
			"IceTask4/Variant_SideScrolling/Gameplay",
			"IceTask4/Variant_SideScrolling/Interfaces",
			"IceTask4/Variant_SideScrolling/UI",
			"IceTask4/Flock"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
