//D:\Sandbox\UE57CreatePlugin4\UE57HLSLEdit\UE57HLSLEdit\Plugins\furcraeaHLSLEditor\Source\furcraeaHLSLEditor\furcraeaHLSLEditor.Build.cs
using UnrealBuildTool;
using System.IO;
public class furcraeaHLSLEditor : ModuleRules
{
    public furcraeaHLSLEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Åö Ç±Ç±Ç…ì¸ÇÍÇÈ
        bUseUnity = false;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
		
		if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "MaterialEditor",
                "EditorScriptingUtilities",
                // ShaderCore ÇÕÇ‡Ç§ñ≥Ç¢ Å® RenderCore Ç…
                "RenderCore",
                "Projects", // Åöí«â¡ÅFIPluginManager óp
            });
        }
    }
}
