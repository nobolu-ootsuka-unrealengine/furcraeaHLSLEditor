// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/furcraeaHLSLEditor.Build.cs
//
// Runtime モジュール: UCodeMaterialAsset と、エディタ時のみ使う HLSL → マテリアル変換 (WITH_EDITOR)。
using UnrealBuildTool;
using System.IO;

public class furcraeaHLSLEditor : ModuleRules
{
    public furcraeaHLSLEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",    // IPluginManager
            "RenderCore",  // AddShaderSourceDirectoryMapping / FlushShaderFileCache
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",                  // MaterialGraph / EdGraphUtilities / ObjectTools
                "MaterialEditor",
                "EditorScriptingUtilities",  // UEditorAssetLibrary
            });
        }
    }
}
