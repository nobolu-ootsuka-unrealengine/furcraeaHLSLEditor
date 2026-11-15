// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMaterialAsset.cpp

#include "CodeMaterialAsset.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Internationalization/Regex.h"
#endif

UCodeMaterialAsset::UCodeMaterialAsset()
{
    FragmentShaderCode = GetDefaultFragmentCode();
    VertexShaderCode   = GetDefaultVertexCode();
    OutputMaterial     = nullptr;
}

FString UCodeMaterialAsset::GetDefaultFragmentCode()
{
    // OutlineColor は関数引数として宣言すると、生成される VectorParameter に自動配線される
    return
        TEXT("// Fragment shader — Character Outline\n")
        TEXT("// Entry: float3 FragmentShader(float2 uv, ...) -> EmissiveColor\n")
        TEXT("//\n")
        TEXT("// @param float3 OutlineColor = 0.0,0.0,0.0\n")
        TEXT("\n")
        TEXT("float3 FragmentShader(float2 uv, float3 OutlineColor)\n")
        TEXT("{\n")
        TEXT("    return OutlineColor;\n")
        TEXT("}\n");
}

FString UCodeMaterialAsset::GetDefaultVertexCode()
{
    // VertexNormal → VertexNormalWS (名前に "Normal")、OutlineWidth → ScalarParameter に自動配線。
    // WPO が有効なとき、コンパイラは TwoSided / Masked / TwoSidedSign * -1 → OpacityMask を自動設定する。
    return
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
}

#if WITH_EDITOR
void UCodeMaterialAsset::PostLoad()
{
    Super::PostLoad();

    // 起動時クラッシュ対策 (UE5.7 OptionalDataSize == -1):
    // 保存済み OutputMaterial の Custom ノードに裸の Time 参照が残っていると
    // コンパイルエラー → 再起動のたびにクラッシュするため、View.GameTime の束縛を先頭へ注入する。
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

        // Time を入力ピンで受け取っているノードは対象外 (注入すると引数の再定義エラーになる)
        const bool bHasTimeInput = Custom->Inputs.ContainsByPredicate(
            [](const FCustomInput& In) { return In.InputName.ToString().Equals(TEXT("Time"), ESearchCase::IgnoreCase); });
        if (bHasTimeInput) continue;

        if (FRegexMatcher(TimePat, Custom->Code).FindNext() && !FRegexMatcher(FloatTimePat, Custom->Code).FindNext())
        {
            Custom->Code = TEXT("float Time = View.GameTime;\n") + Custom->Code;
            bPatched = true;
        }
    }
    if (bPatched) OutputMaterial->MarkPackageDirty();
}
#endif
