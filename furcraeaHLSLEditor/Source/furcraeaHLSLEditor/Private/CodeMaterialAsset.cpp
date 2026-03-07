// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMaterialAsset.cpp

#include "CodeMaterialAsset.h"
#include "CodeMaterialCompiler.h"

UCodeMaterialAsset::UCodeMaterialAsset()
{
    // Default template: character outline (cel shader — shell extrusion)
    // Demonstrates @param auto-wiring and WPO usage.

    FragmentShaderCode =
        TEXT("// Fragment shader — Character Outline\n")
        TEXT("// Entry: float3 FragmentShader(float2 uv, ...) -> EmissiveColor\n")
        TEXT("//\n")
        TEXT("// @param float3 OutlineColor = 0.0,0.0,0.0\n")
        TEXT("\n")
        TEXT("float3 FragmentShader(float2 uv, float3 OutlineColor)\n")
        TEXT("{\n")
        TEXT("    return OutlineColor;\n")
        TEXT("}\n");

    VertexShaderCode =
        TEXT("// Vertex shader — Character Outline (Shell Extrusion)\n")
        TEXT("// Entry: float3 WPO_Main(float3 VertexNormal, ...) -> WorldPositionOffset\n")
        TEXT("// Leave this pane empty (or all commented out) to disable WPO.\n")
        TEXT("//\n")
        TEXT("// @param float OutlineWidth = 0.3\n")
        TEXT("\n")
        TEXT("float3 WPO_Main(float3 VertexNormal, float OutlineWidth)\n")
        TEXT("{\n")
        TEXT("    return normalize(VertexNormal) * OutlineWidth;\n")
        TEXT("}\n");

    OutputMaterial = nullptr;
}

#if WITH_EDITOR
void UCodeMaterialAsset::PostEditChangeProperty(FPropertyChangedEvent& E)
{
    Super::PostEditChangeProperty(E);
    // Auto-compile on property change is intentionally disabled (Save button driven)
}
#endif
