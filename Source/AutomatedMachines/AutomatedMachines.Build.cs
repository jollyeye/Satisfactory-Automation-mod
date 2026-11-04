using UnrealBuildTool;

public class AutomatedMachines : ModuleRules
{
	public AutomatedMachines(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"NetCore",
			"UMG",
			"Slate",
			"SlateCore",
			// Satisfactory + mod loader
			"FactoryGame",
			"SML",
		});
	}
}
