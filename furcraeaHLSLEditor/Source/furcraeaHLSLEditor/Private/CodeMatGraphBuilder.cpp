// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatGraphBuilder.cpp
//
// グラフ構築の流れ:
//   1. @param 注釈 → Scalar / Vector / TextureObject パラメータノード
//   2. 自動配線用の入力ノード (TexCoord / WorldPosition / ObjectRadius / Time / VertexNormalWS / SceneTexture)
//   3. Vertex (WPO) と Fragment (Emissive / Opacity) のエントリ関数を
//        - Material Nodes モード: CodeMatHLSLToNodes で本物のノードに展開 (失敗時は Custom ノード)
//        - Custom Node モード  : Custom ノード 1 個に埋め込む (CodeMatCustomNode)
//   4. ドメイン / ブレンドモード / 使用フラグを設定
//
// UMaterialEditingLibrary は呼ぶたびに PostEditChange するため使わない。ノードは NewObject で直接作り、
// PostEditChange は最後に 1 回だけ呼ぶ (中途半端なグラフでのコンパイルが UE5.7 の
// OptionalDataSize == -1 アサートの原因になるため)。

#include "CodeMatGraphBuilder.h"

#if WITH_EDITOR

#include "CodeMaterialCompiler.h"
#include "CodeMatCustomNode.h"
#include "CodeMatGraphUtils.h"
#include "CodeMatHLSLSource.h"
#include "CodeMatHLSLToNodes.h"
#include "CodeMatLog.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Engine/Texture.h"
#include "ShaderCompiler.h"

namespace
{
using CodeMatSource::FParamDecl;

int32 HLSLTypeComponents(const FString& TypeStr)
{
    const TCHAR Last = TypeStr.IsEmpty() ? 0 : TypeStr[TypeStr.Len() - 1];
    return (Last >= TEXT('2') && Last <= TEXT('4')) ? (Last - TEXT('0')) : 1;
}

void SetInputMaskRGB(FExpressionInput& In)
{
    In.Mask = 1; In.MaskR = 1; In.MaskG = 1; In.MaskB = 1; In.MaskA = 0;
}

void SetInputMaskA(FExpressionInput& In)
{
    In.Mask = 1; In.MaskR = 0; In.MaskG = 0; In.MaskB = 0; In.MaskA = 1;
}

/** パスに ".AssetName" が無ければ補完してテクスチャをロードする */
UTexture* LoadTextureFromParamPath(const FString& InPath)
{
    if (InPath.IsEmpty()) return nullptr;
    FString Path = InPath;
    int32 LastSlash;
    if (!Path.Contains(TEXT(".")) && Path.FindLastChar(TEXT('/'), LastSlash))
        Path += TEXT(".") + Path.RightChop(LastSlash + 1);
    return LoadObject<UTexture>(nullptr, *Path);
}

// ============================================================================
// FCodeMatGraphBuilder
// ============================================================================

class FCodeMatGraphBuilder
{
public:
    FCodeMatGraphBuilder(UMaterial* InMaterial, const FCodeMatBuildSettings& InSettings, FString& InLog)
        : Material(InMaterial)
        , ED(InMaterial->GetEditorOnlyData())
        , FragmentCode(CodeMatSource::SafeifyHLSL(InSettings.FragmentCode))
        , VertexCode(CodeMatSource::SafeifyHLSL(InSettings.VertexCode))
        , Settings(InSettings)
        , Log(InLog)
        , bPostProcess(InSettings.Domain == ECodeMatDomain::PostProcess)
        , bHasWPO(!bPostProcess && !VertexCode.TrimStartAndEnd().IsEmpty())
        , bNodeMode(InSettings.OutputMode == ECodeMatOutputMode::MaterialNodes)
    {
    }

    void Build()
    {
        ResetGraph();
        CreateParameterNodes();
        CreateStandardInputNodes();

        if (bNodeMode)
        {
            NodeBuilder = MakeUnique<FCodeMatNodeBuilder>(
                [this](UClass* Cls) { return NewExpr(Cls); },
                [this](const FString& Type, const FString& Name, FCodeMatValue& Out) { return ResolveNodeValue(Type, Name, Out); });
        }

        if (bHasWPO) BuildVertexOutput();
        BuildFragmentOutput();

        if (NodeBuilder)
        {
            for (const FString& Note : NodeBuilder->GetNotes())
                Log += FString::Printf(TEXT("[Material Nodes] %s\n"), *Note);
        }

        ApplyMaterialSettings();

        if (bNodeMode)
        {
            // 使われなかった自動配線ノードを掃除して整列する
            CodeMatGraph::RemoveUnreachableExpressions(ED);
            CodeMatGraph::LayoutNodeGraph(ED);
        }
    }

private:
    UMaterial*               Material;
    UMaterialEditorOnlyData* ED;
    const FString            FragmentCode;
    const FString            VertexCode;
    const FCodeMatBuildSettings& Settings;
    FString&                 Log;

    const bool bPostProcess;
    const bool bHasWPO;
    const bool bNodeMode;

    TArray<FParamDecl> Params;
    TMap<FString, UMaterialExpression*> ParamNodes;

    UMaterialExpressionTextureCoordinate* TexCoord       = nullptr;
    UMaterialExpressionWorldPosition*     WorldPos       = nullptr;
    UMaterialExpressionObjectRadius*      ObjectRadius   = nullptr;
    UMaterialExpressionTime*              Time           = nullptr;
    UMaterialExpressionVertexNormalWS*    VertexNormal   = nullptr;
    TMap<ESceneTextureId, UMaterialExpressionSceneTexture*> SceneTextures;

    TUniquePtr<FCodeMatNodeBuilder> NodeBuilder;

    // ------------------------------------------------------------------
    // ノード生成
    // ------------------------------------------------------------------

    UMaterialExpression* NewExpr(UClass* ExprClass, int32 X = 0, int32 Y = 0)
    {
        UMaterialExpression* E = NewObject<UMaterialExpression>(Material, ExprClass, NAME_None, RF_Transactional);
        if (E)
        {
            E->Material = Material;  // CreateMaterialExpressionEx と同じバックリファレンス
            E->MaterialExpressionEditorX = X;
            E->MaterialExpressionEditorY = Y;
            ED->ExpressionCollection.AddExpression(E);
        }
        return E;
    }

    template <typename T>
    T* NewExpr(int32 X, int32 Y)
    {
        return Cast<T>(NewExpr(T::StaticClass(), X, Y));
    }

    void ResetGraph()
    {
        ED->ExpressionCollection.Empty();
        ED->EmissiveColor.Expression       = nullptr;
        ED->WorldPositionOffset.Expression = nullptr;
        ED->OpacityMask.Expression         = nullptr;
        ED->Opacity.Expression             = nullptr;
    }

    void CreateParameterNodes()
    {
        Params = CodeMatSource::ParseParamDecls(FragmentCode + TEXT("\n\n") + VertexCode);

        int32 Y = 0;
        for (const FParamDecl& P : Params)
        {
            UMaterialExpression* Expr = nullptr;
            switch (P.Type)
            {
            case FParamDecl::EType::Scalar:
                if (auto* E = NewExpr<UMaterialExpressionScalarParameter>(-700, Y))
                {
                    E->ParameterName = FName(*P.Name);
                    E->DefaultValue  = P.DefaultScalar;
                    Expr = E;
                }
                break;
            case FParamDecl::EType::Texture2D:
                // MI で上書き可能なテクスチャパラメータ
                if (auto* E = NewExpr<UMaterialExpressionTextureObjectParameter>(-700, Y))
                {
                    E->ParameterName = FName(*P.Name);
                    if (UTexture* Tex = LoadTextureFromParamPath(P.TexturePath)) E->Texture = Tex;
                    Expr = E;
                }
                break;
            default:
                if (auto* E = NewExpr<UMaterialExpressionVectorParameter>(-700, Y))
                {
                    E->ParameterName = FName(*P.Name);
                    E->DefaultValue  = P.DefaultColor;
                    Expr = E;
                }
                break;
            }
            if (Expr)
            {
                ParamNodes.Add(P.Name, Expr);
                Y += 80;
            }
        }
    }

    void CreateStandardInputNodes()
    {
        TexCoord     = NewExpr<UMaterialExpressionTextureCoordinate>(-400, 0);
        WorldPos     = NewExpr<UMaterialExpressionWorldPosition>(-400, 200);
        ObjectRadius = NewExpr<UMaterialExpressionObjectRadius>(-400, 400);
        Time         = NewExpr<UMaterialExpressionTime>(-400, 600);
        if (TexCoord) TexCoord->CoordinateIndex = 0;
        if (bHasWPO)  VertexNormal = NewExpr<UMaterialExpressionVertexNormalWS>(-400, 300);
    }

    UMaterialExpressionSceneTexture* GetOrCreateSceneTexture(ESceneTextureId TexId, int32 NodeY)
    {
        if (UMaterialExpressionSceneTexture** Found = SceneTextures.Find(TexId)) return *Found;
        auto* ST = NewExpr<UMaterialExpressionSceneTexture>(-600, NodeY);
        if (ST)
        {
            ST->SceneTextureId = TexId;
            ST->bFiltered      = false;
            if (TexCoord) ST->Coordinates.Expression = TexCoord;  // スクリーン UV
            SceneTextures.Add(TexId, ST);
        }
        return ST;
    }

    // ------------------------------------------------------------------
    // 関数引数の自動配線 (引数名で決まる)
    //   @param 名               → パラメータノード
    //   uv                      → TextureCoordinate
    //   WorldPos                → WorldPosition (絶対ワールド座標)
    //   ObjectRadius            → ObjectRadius
    //   Time                    → Time
    //   *Normal* (WPO 有効時)   → VertexNormalWS
    //   SceneColor / SceneDepth / CustomDepth / CustomStencil (PP) → SceneTexture
    // ------------------------------------------------------------------

    void ResolveInput(const FString& PName, FExpressionInput& Out)
    {
        if (UMaterialExpression** Found = ParamNodes.Find(PName))
        {
            Out.Expression = *Found;
            const FParamDecl* P = Params.FindByPredicate([&](const FParamDecl& X) { return X.Name == PName; });
            if (P && P->Type == FParamDecl::EType::Vector3) SetInputMaskRGB(Out);
            return;
        }

        const TPair<const TCHAR*, UMaterialExpression*> NamedInputs[] = {
            { TEXT("uv"),           TexCoord },
            { TEXT("WorldPos"),     WorldPos },
            { TEXT("ObjectRadius"), ObjectRadius },
            { TEXT("Time"),         Time },
        };
        for (const auto& NI : NamedInputs)
        {
            if (NI.Value && PName.Equals(NI.Key, ESearchCase::IgnoreCase)) { Out.Expression = NI.Value; return; }
        }

        if (VertexNormal && PName.Contains(TEXT("Normal"), ESearchCase::IgnoreCase))
        {
            Out.Expression = VertexNormal;
            return;
        }

        if (bPostProcess)
        {
            // PP マテリアルで SceneColor は使えないため PostProcessInput0 を使う
            static const TPair<const TCHAR*, ESceneTextureId> PPWireRules[] = {
                { TEXT("SceneColor"),    PPI_PostProcessInput0 },
                { TEXT("SceneDepth"),    PPI_SceneDepth },
                { TEXT("CustomDepth"),   PPI_CustomDepth },
                { TEXT("CustomStencil"), PPI_CustomStencil },
            };
            for (int32 i = 0; i < UE_ARRAY_COUNT(PPWireRules); ++i)
            {
                if (PName.Equals(PPWireRules[i].Key, ESearchCase::IgnoreCase))
                {
                    Out.Expression = GetOrCreateSceneTexture(PPWireRules[i].Value, 800 + i * 120);
                    return;
                }
            }
        }
    }

    /** ノード変換器用: 自動配線ノードを (出力ピン, 成分数) の値にする */
    bool ResolveNodeValue(const FString& TypeStr, const FString& PName, FCodeMatValue& Out)
    {
        FExpressionInput Tmp;
        ResolveInput(PName, Tmp);
        UMaterialExpression* E = Tmp.Expression;
        if (!E) return false;

        if (TypeStr == TEXT("Texture2D"))
        {
            Out = FCodeMatValue::MakeExpr(E, 0, 1);
            Out.bTexture = true;
        }
        else if (E->IsA<UMaterialExpressionVectorParameter>())
            Out = HLSLTypeComponents(TypeStr) >= 4 ? FCodeMatValue::MakeExpr(E, 5, 4) : FCodeMatValue::MakeExpr(E, 0, 3); // RGBA / RGB
        else if (E->IsA<UMaterialExpressionTextureCoordinate>())
            Out = FCodeMatValue::MakeExpr(E, 0, 2);
        else if (E->IsA<UMaterialExpressionWorldPosition>() || E->IsA<UMaterialExpressionVertexNormalWS>())
            Out = FCodeMatValue::MakeExpr(E, 0, 3);
        else if (E->IsA<UMaterialExpressionSceneTexture>())
            Out = FCodeMatValue::MakeExpr(E, 0, 4);   // Color
        else
            Out = FCodeMatValue::MakeExpr(E, 0, 1);   // Scalar / Time / ObjectRadius
        return true;
    }

    // ------------------------------------------------------------------
    // Custom ノード (コード生成は CodeMatCustomNode、ここでは生成と自動配線だけ)
    // ------------------------------------------------------------------

    UMaterialExpressionCustom* BuildCustomNode(const FString& FuncName, const FString& SourceCode,
                                               ECustomMaterialOutputType OutType, CodeMatCustom::EStage Stage, int32 NodeY)
    {
        if (SourceCode.IsEmpty()) return nullptr;
        auto* Custom = NewExpr<UMaterialExpressionCustom>(0, NodeY);
        if (!Custom) return nullptr;

        const CodeMatCustom::FCustomNodeSpec Spec = CodeMatCustom::MakeSpec(SourceCode, FuncName, OutType, Stage);
        if (!Spec.Error.IsEmpty())
        {
            Log += FString::Printf(TEXT("[Custom Node] %s: %s\n"), *FuncName, *Spec.Error);
            UE_LOG(LogCodeMat, Warning, TEXT("Custom node for %s: %s"), *FuncName, *Spec.Error);
        }
        CodeMatCustom::ApplySpec(Custom, Spec);
        for (FCustomInput& In : Custom->Inputs)
            ResolveInput(In.InputName.ToString(), In.Input);
        return Custom;
    }

    // ------------------------------------------------------------------
    // エントリ関数 → 出力ピン
    // ------------------------------------------------------------------

    /** Material Nodes モードでノード化を試みる。失敗時はログを残して false (呼び出し側で Custom にする)。 */
    bool TryBuildNodes(const FString& FuncName, const FString& SourceCode, FCodeMatValue& OutV)
    {
        if (!NodeBuilder) return false;
        FString Err;
        if (NodeBuilder->BuildEntry(SourceCode, FuncName, OutV, Err)) return true;
        Log += FString::Printf(TEXT("[Material Nodes] %s: %s\n    -> this function fell back to a Custom node.\n"), *FuncName, *Err);
        UE_LOG(LogCodeMat, Warning, TEXT("Node conversion of %s failed: %s (fallback: Custom node)"), *FuncName, *Err);
        return false;
    }

    void BuildVertexOutput()
    {
        const FString FuncName = CodeMatSource::DetectFunctionName(VertexCode);
        if (FuncName.IsEmpty()) return;

        FCodeMatValue V;
        if (TryBuildNodes(FuncName, VertexCode, V))
        {
            NodeBuilder->Connect(ED->WorldPositionOffset, V);
        }
        else if (auto* Custom = BuildCustomNode(FuncName, VertexCode, CMOT_Float3, CodeMatCustom::EStage::Vertex, -200))
        {
            ED->WorldPositionOffset.Expression  = Custom;
            ED->WorldPositionOffset.OutputIndex = 0;
        }
    }

    void BuildFragmentOutput()
    {
        const FString FuncName = CodeMatSource::DetectFunctionName(FragmentCode);
        if (FuncName.IsEmpty()) return;

        FCodeMatValue V;
        if (TryBuildNodes(FuncName, FragmentCode, V))
        {
            if (V.N >= 4)
            {
                NodeBuilder->Connect(ED->EmissiveColor, NodeBuilder->Swizzle(V, TEXT("rgb")));
                if (!bPostProcess) NodeBuilder->Connect(ED->Opacity, NodeBuilder->Swizzle(V, TEXT("a")));
            }
            else
            {
                NodeBuilder->Connect(ED->EmissiveColor, V);
            }
            return;
        }

        const bool bFloat4 = CodeMatSource::DetectReturnComponents(FragmentCode) == 4;
        UMaterialExpressionCustom* Custom = BuildCustomNode(FuncName, FragmentCode, bFloat4 ? CMOT_Float4 : CMOT_Float3, CodeMatCustom::EStage::Pixel, 0);
        if (!Custom) return;

        ED->EmissiveColor.Expression  = Custom;
        ED->EmissiveColor.OutputIndex = 0;
        if (bFloat4)
        {
            // RGB → Emissive, A → Opacity (Surface のみ。PP は Opaque でシェーダー内合成)
            SetInputMaskRGB(ED->EmissiveColor);
            if (!bPostProcess)
            {
                ED->Opacity.Expression  = Custom;
                ED->Opacity.OutputIndex = 0;
                SetInputMaskA(ED->Opacity);
            }
        }
    }

    // ------------------------------------------------------------------
    // マテリアル設定
    // ------------------------------------------------------------------

    void ApplyMaterialSettings()
    {
        if (bPostProcess)
        {
            // 常に Opaque: SceneColor 合成はシェーダー内の lerp で行う
            Material->MaterialDomain = MD_PostProcess;
            Material->TwoSided       = 0;
            Material->BlendMode      = BLEND_Opaque;

            static const EBlendableLocation BlendLocMap[] = {
                BL_SceneColorAfterTonemapping, // AfterTonemapping
                BL_SceneColorAfterDOF,         // BeforeTonemapping
                BL_SceneColorBeforeDOF,        // BeforeTranslucency
                BL_ReplacingTonemapper,        // ReplacingTonemapper
            };
            const int32 Idx = FMath::Clamp((int32)Settings.BlendLoc, 0, (int32)UE_ARRAY_COUNT(BlendLocMap) - 1);
            Material->BlendableLocation = BlendLocMap[Idx];
        }
        else if (bHasWPO)
        {
            // Surface + WPO: アウトライン (シェル法) — 裏面だけを描画する
            Material->MaterialDomain = MD_Surface;
            Material->TwoSided       = 1;
            Material->BlendMode      = BLEND_Masked;

            auto* TwoSidedSign = NewExpr<UMaterialExpressionTwoSidedSign>(0, 250);
            auto* Negate       = NewExpr<UMaterialExpressionMultiply>(200, 250);
            if (TwoSidedSign && Negate)
            {
                Negate->A.Expression           = TwoSidedSign;
                Negate->ConstB                 = -1.f;
                ED->OpacityMask.Expression  = Negate;
                ED->OpacityMask.OutputIndex = 0;
            }
        }
        else
        {
            Material->MaterialDomain = MD_Surface;
            Material->TwoSided       = 0;
            Material->BlendMode      = BLEND_Opaque;
        }

        // 生成マテリアルはスケルタルメッシュ用。StaticMesh 使用をオフにすると
        // UE 5.7 PathTracing の LocalVF 順列で Material.ush の UB 宣言が重複する問題を避けられる。
        Material->SetUsageByFlag(MATUSAGE_SkeletalMesh, true);
        Material->SetUsageByFlag(MATUSAGE_StaticMesh, false);
        Material->bAutomaticallySetUsageInEditor = 0;
        Material->bCastRayTracedShadows          = 0;
        Material->SetShadingModel(MSM_Unlit);
    }
};

} // namespace

namespace CodeMatGraph
{

void BuildMaterialGraph(UMaterial* Material, const FCodeMatBuildSettings& Settings, bool bFinalize, FString& OutLog)
{
    if (!Material) return;

    if (bFinalize)
    {
        // 既存のコンパイルを全部終わらせてから変更する
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
            GShaderCompilingManager->FinishAllCompilation();
        Material->PreEditChange(nullptr);
    }

    FCodeMatGraphBuilder(Material, Settings, OutLog).Build();

    if (bFinalize)
    {
        // シェーダーコンパイルの唯一のトリガー
        Material->PostEditChange();
        Material->MarkPackageDirty();
    }
}

} // namespace CodeMatGraph

#endif // WITH_EDITOR
