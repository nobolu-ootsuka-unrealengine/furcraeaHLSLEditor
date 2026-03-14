// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMaterialAsset.cpp

#include "CodeMaterialAsset.h"
#include "CodeMaterialCompiler.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Internationalization/Regex.h"
#endif

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

void UCodeMaterialAsset::PostLoad()
{
    Super::PostLoad();

    // 起動時クラッシュ対策 (UE5.7 OptionalDataSize == -1 バグ回避):
    // 保存済みの OutputMaterial の Custom Expression コードに裸の Time 参照が
    // 残っている場合、View.GameTime バインドを先頭に注入してコンパイルエラーを防ぐ。
    // これにより、エディタで保存 → 再起動のたびにクラッシュするループを断ち切る。
    if (!OutputMaterial) return;

    UMaterialEditorOnlyData* ED = OutputMaterial->GetEditorOnlyData();
    if (!ED) return;

    const FRegexPattern TimePat(TEXT("\\bTime\\b"));
    const FRegexPattern FloatTimePat(TEXT("\\bfloat\\s+Time\\b"));
    bool bPatched = false;

    for (UMaterialExpression* Expr : ED->ExpressionCollection.Expressions)
    {
        UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
        if (!Custom) continue;

        if (FRegexMatcher(TimePat, Custom->Code).FindNext() &&
            !FRegexMatcher(FloatTimePat, Custom->Code).FindNext())
        {
            Custom->Code = TEXT("float Time = View.GameTime;\n") + Custom->Code;
            bPatched = true;
        }
    }

    if (bPatched)
    {
        OutputMaterial->MarkPackageDirty();
    }
}
#endif
