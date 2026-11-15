// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/furcraeaHLSLEditorEditor.Build.cs
//
// Editor モジュール: アセットエディタ / ファクトリ / Python・BP 用関数ライブラリ。
using UnrealBuildTool;

public class furcraeaHLSLEditorEditor : ModuleRules
{
    public furcraeaHLSLEditorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "EditorFramework",           // IToolkitHost
            "furcraeaHLSLEditor",        // CodeMaterialAsset / CodeMaterialCompiler
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "InputCore",                 // SComboBox
            "ApplicationCore",           // クリップボード (FPlatformApplicationMisc)
            "AssetTools",
            "EditorScriptingUtilities",  // UEditorAssetLibrary
            "RenderCore",                // GetMaterialCompileErrors (ShaderCompiler)
            "RHI",
        });
    }
}
