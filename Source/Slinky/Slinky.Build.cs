// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Slinky : ModuleRules
{
	public Slinky(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Slinky",
			"Slinky/Variant_Platforming",
			"Slinky/Variant_Platforming/Animation",
			"Slinky/Variant_Combat",
			"Slinky/Variant_Combat/AI",
			"Slinky/Variant_Combat/Animation",
			"Slinky/Variant_Combat/Gameplay",
			"Slinky/Variant_Combat/Interfaces",
			"Slinky/Variant_Combat/UI",
			"Slinky/Variant_SideScrolling",
			"Slinky/Variant_SideScrolling/AI",
			"Slinky/Variant_SideScrolling/Gameplay",
			"Slinky/Variant_SideScrolling/Interfaces",
			"Slinky/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
