using UnrealBuildTool;

public class Parabellum : ModuleRules
{
	public Parabellum(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Модуль плоский, с подпапками по областям (Character, Player, Game, ...).
		// Без этой строки UBT не кладёт корень модуля в include-пути и
		// #include "Character/PBLCharacter.h" не разрешается.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"DeveloperSettings",
			"Slate",
			"SlateCore"
		});
	}
}
