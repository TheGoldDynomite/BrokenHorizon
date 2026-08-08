// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BrokenHorizon : ModuleRules
{
	public BrokenHorizon(ReadOnlyTargetRules Target) : base(Target)
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
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"DeveloperSettings",
			"UMG",
			"Slate",
			"SlateCore",
			"Niagara",
			"Json",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"BrokenHorizon",
			"BrokenHorizon/Variant_Horror",
			"BrokenHorizon/Variant_Horror/UI",
			"BrokenHorizon/Variant_Shooter",
			"BrokenHorizon/Variant_Shooter/AI",
			"BrokenHorizon/Variant_Shooter/UI",
			"BrokenHorizon/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
