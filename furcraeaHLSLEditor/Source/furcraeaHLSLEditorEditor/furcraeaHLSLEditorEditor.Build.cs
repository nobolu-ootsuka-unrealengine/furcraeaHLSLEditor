// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/furcraeaHLSLEditorEditor.Build.cs

using UnrealBuildTool;

public class furcraeaHLSLEditorEditor : ModuleRules
{
    public furcraeaHLSLEditorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// ★ ここに入れる
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "UnrealEd",
            "AssetTools",
            "EditorFramework",
            "MaterialEditor",
            "EditorSubsystem",
            "EditorScriptingUtilities",
            "Projects",                // ★追加（IPluginManager）
            // ★Runtimeモジュールに依存（これが無いと CodeMaterialAsset.h が見えない）
            "furcraeaHLSLEditor",
            "InputCore",               // SComboBox が EKeys を要求する
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "AssetTools",
            "AssetRegistry",
            "Slate",
            "SlateCore",
            "EditorStyle",
            "AppFramework",

            

            // ★UMaterialEditingLibrary
            "EditorScriptingUtilities",

            // 任意（足して害なし）
            "MaterialEditor",
            "EditorSubsystem",
            "Projects",                // ★追加（IPluginManager）
        });
    }
}
