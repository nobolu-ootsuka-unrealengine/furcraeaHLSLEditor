// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMaterialCompiler.cpp

#include "CodeMaterialCompiler.h"
#include "CodeMaterialAsset.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "EditorAssetLibrary.h"
#include "ObjectTools.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "Internationalization/Regex.h"

// ============================================================================
// WriteShaderFile
// ファイル名: CodeMat_<AssetName>.ush (Fragment + Vertex を 1 ファイルに統合)
// ============================================================================

static bool WriteShaderFile(
    const UCodeMaterialAsset* Asset,
    const FString& FragCode,
    const FString& VertCode,
    FString& OutVirtualIncludePath,
    FString& OutErr)
{
    OutErr.Empty();
    if (!Asset) { OutErr = TEXT("Asset is null"); return false; }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("furcraeaHLSLEditor"));
    if (!Plugin.IsValid()) { OutErr = TEXT("Plugin not found: furcraeaHLSLEditor"); return false; }

    FString SafeAssetName = ObjectTools::SanitizeObjectName(Asset->GetName());
    const FString FileName = FString::Printf(TEXT("CodeMat_%s.ush"), *SafeAssetName);
    const FString DiskPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders/Private"), FileName);
    OutVirtualIncludePath  = FString::Printf(TEXT("/Plugin/furcraeaHLSLEditor/Private/%s"), *FileName);

    // Fragment + Vertex を 1 ファイルに統合
    FString TextToWrite = TEXT("#pragma once\n\n");
    TextToWrite += TEXT("// ===== Fragment Shader =====\n");
    TextToWrite += FragCode;
    if (!VertCode.TrimStartAndEnd().IsEmpty())
    {
        TextToWrite += TEXT("\n\n// ===== Vertex Shader =====\n");
        TextToWrite += VertCode;
    }

    TextToWrite.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    TextToWrite.ReplaceInline(TEXT("\r"),   TEXT("\n"));

    if (!FFileHelper::SaveStringToFile(TextToWrite, *DiskPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutErr = FString::Printf(TEXT("Failed to write: %s"), *DiskPath);
        return false;
    }
    return true;
}

// ============================================================================
// MakeMaterialPackagePath / CreateOrLoadMaterial
// ============================================================================

static FString MakeMaterialPackagePath(const UCodeMaterialAsset* Asset)
{
    FString AssetName = Asset ? Asset->GetName() : TEXT("Unknown");
    AssetName = ObjectTools::SanitizeObjectName(AssetName);

    // Asset と同じフォルダに生成する
    const FString AssetFolder = Asset
        ? FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName())
        : TEXT("/Game");

    return FString::Printf(TEXT("%s/M_%s"), *AssetFolder, *AssetName);
}

static UMaterial* CreateOrLoadMaterial(UCodeMaterialAsset* Asset)
{
    if (!Asset) return nullptr;

    // Asset と同じフォルダにマテリアルを生成する
    UEditorAssetLibrary::MakeDirectory(FPackageName::GetLongPackagePath(MakeMaterialPackagePath(Asset)));

    if (Asset->OutputMaterial)
        return Asset->OutputMaterial;

    const FString PackagePath = MakeMaterialPackagePath(Asset);
    const FString AssetName   = FPackageName::GetLongPackageAssetName(PackagePath);

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) return nullptr;

    if (UMaterial* Existing = FindObject<UMaterial>(Package, *AssetName))
    {
        Asset->OutputMaterial = Existing;
        Asset->MarkPackageDirty();
        return Existing;
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UObject* NewObj = Factory->FactoryCreateNew(
        UMaterial::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone, nullptr, GWarn);

    UMaterial* Mat = Cast<UMaterial>(NewObj);
    if (!Mat) return nullptr;

    FAssetRegistryModule::AssetCreated(Mat);
    Mat->MarkPackageDirty();
    Asset->OutputMaterial = Mat;
    Asset->MarkPackageDirty();

    return Mat;
}

// ============================================================================
// ============================================================================
// ユーザーの HLSL コードから関数ボディを抽出する
//
// 例: "float3 WPO_Main(float3 N, float W) { return N * W; }"
//     → "return N * W; "
//
// Custom expression の Code フィールドは生成ラッパー関数の内部に展開されるため、
// インクルードファイルを使わず関数ボディを直接 Code に埋め込む方式を採用。
// これにより ShaderCompileWorker が Plugin の仮想インクルードパスを
// 解決できない問題（UE5.7 クラッシュの根本原因）を回避する。
// ============================================================================

static FString ExtractShaderFunctionBody(const FString& Code, const FString& FuncName)
{
    // FuncName の開き波括弧を探す
    const FString Pattern = FString::Printf(TEXT("\\b%s\\b[^{]*\\{"), *FuncName);
    FRegexMatcher M(FRegexPattern(*Pattern), Code);
    if (!M.FindNext()) return FString();

    int32 OpenIdx = M.GetMatchEnding() - 1; // '{' の位置

    // ネスト対応で対応する '}' を探す
    int32 Depth = 1;
    int32 Pos   = OpenIdx + 1;
    while (Pos < Code.Len() && Depth > 0)
    {
        TCHAR Ch = Code[Pos];
        if      (Ch == TEXT('{')) ++Depth;
        else if (Ch == TEXT('}')) --Depth;
        if (Depth > 0) ++Pos;
    }
    if (Depth != 0) return FString(); // 対応する } が見つからない

    // 波括弧の内側を返す
    return Code.Mid(OpenIdx + 1, Pos - OpenIdx - 1);
}

// HLSL 予約語の関数名を安全な名前にリネームする
//
// FXC / DXC では VertexShader・PixelShader 等の Effects キーワードが
// 関数名として使われると "error: unexpected token" でコンパイル失敗する。
// これが FinalizeShaderCode() 未呼出し → OptionalDataSize == -1 クラッシュの原因。
// ============================================================================

static FString SafeifyHLSL(const FString& Code)
{
    static const TArray<TPair<FString, FString>> Renames = {
        { TEXT("VertexShader"),   TEXT("WPO_Main")    },
        { TEXT("PixelShader"),    TEXT("PS_Main")     },
        { TEXT("GeometryShader"), TEXT("GS_Main")     },
        { TEXT("HullShader"),     TEXT("HS_Main")     },
        { TEXT("DomainShader"),   TEXT("DS_Main")     },
        { TEXT("ComputeShader"),  TEXT("CS_Main")     },
    };
    FString Result = Code;
    for (const auto& KV : Renames)
        Result.ReplaceInline(*KV.Key, *KV.Value, ESearchCase::CaseSensitive);
    return Result;
}

// コメント行を除去する（// ... 形式）
// DetectFunctionName/ExtractFuncParams がコメント内の仮シグネチャにマッチするのを防ぐ
static FString StripLineComments(const FString& Code)
{
    const FRegexPattern Pat(TEXT("//[^\n]*"));
    FString Out;
    int32 LastEnd = 0;
    FRegexMatcher M(Pat, Code);
    while (M.FindNext())
    {
        Out += Code.Mid(LastEnd, M.GetMatchBeginning() - LastEnd);
        LastEnd = M.GetMatchEnding();
    }
    Out += Code.Mid(LastEnd);
    return Out;
}

// HLSL ビルトイン変数の自動注入: Custom Expression のボディ内で Time を裸で使用してもコンパイルエラーにならないように View.GameTime を注入する。
static FString InjectHLSLBuiltins(const FString& Body)
{
    FString Prefix;
    // Time: ボディ内で使用されているが float 宣言もパラメータもない場合、View.GameTime を注入
    if (FRegexMatcher(FRegexPattern(TEXT("\\bTime\\b")), Body).FindNext() &&
        !FRegexMatcher(FRegexPattern(TEXT("\\bfloat\\s+Time\\b")), Body).FindNext())
    {
        Prefix += TEXT("float Time = View.GameTime;\n");
    }
    return Prefix.IsEmpty() ? Body : (Prefix + Body);
}

// HLSL ソース内の最初の float3 関数定義の関数名を返す（コメント内は除外）
static FString DetectFunctionName(const FString& Code)
{
    const FString Stripped = StripLineComments(Code);
    const FRegexPattern Pat(TEXT("float3\\s+(\\w+)\\s*\\("));
    FRegexMatcher M(Pat, Stripped);
    if (M.FindNext())
        return M.GetCaptureGroup(1).TrimStartAndEnd();
    return FString();
}

// メイン関数の前に定義されているヘルパー関数群を抽出して返す。
// ヘルパーが存在しない（// @param コメントのみ等）場合は空文字列を返す。
static FString ExtractHelperFunctions(const FString& Code, const FString& MainFuncName)
{
    const FString Pattern = FString::Printf(TEXT("\\bfloat3\\s+%s\\s*\\("), *MainFuncName);
    FRegexMatcher MOrig(FRegexPattern(*Pattern), Code);
    if (!MOrig.FindNext()) return FString();

    const FString Before = Code.Left(MOrig.GetMatchBeginning()).TrimEnd();
    if (Before.IsEmpty()) return FString();

    // コメント除去後に関数定義が 1 つ以上あるか確認
    const FRegexPattern FuncDefPat(TEXT("[a-zA-Z_]\\w*\\s+[a-zA-Z_]\\w*\\s*\\("));
    FRegexMatcher FuncM(FuncDefPat, StripLineComments(Before));
    if (!FuncM.FindNext()) return FString();

    return Before;
}

// ============================================================================
// @param parsing
//
// Syntax (inside HLSL comments — valid HLSL, parsed by the compiler):
//   // @param float   Name = 0.5
//   // @param float3  Name = 0.0,0.0,0.0
//   // @param float4  Name = 0.0,0.0,0.0,1.0
//
// Entry functions:
//   float3 MainWPO  (typed params...)  -> World Position Offset node (optional)
//   float3 MainHLSL (float2 uv, ...)   -> Emissive Color node (required)
//
// Parameter name auto-wiring rules (by parameter name):
//   "uv" / "UV"          -> TextureCoordinate
//   contains "Normal"    -> VertexNormalWS
//   matches a @param     -> ScalarParameter / VectorParameter
// ============================================================================

struct FCodeMatParam
{
    enum class EType { Scalar, Vector3, Vector4 } Type;
    FString      Name;
    float        DefaultScalar = 0.f;
    FLinearColor DefaultColor  = FLinearColor(0, 0, 0, 1);
};

static TArray<FCodeMatParam> ParseCodeMatParams(const FString& Code)
{
    TArray<FCodeMatParam> Params;
    const FRegexPattern Pat(TEXT("//\\s*@param\\s+(float[34]?)\\s+(\\w+)\\s*=\\s*([^\\n]+)"));
    FRegexMatcher M(Pat, Code);
    while (M.FindNext())
    {
        const FString TypeStr  = M.GetCaptureGroup(1).TrimStartAndEnd();
        const FString NameStr  = M.GetCaptureGroup(2).TrimStartAndEnd();
        const FString ValueStr = M.GetCaptureGroup(3).TrimStartAndEnd();

        FCodeMatParam P;
        P.Name = NameStr;

        if (TypeStr == TEXT("float"))
        {
            P.Type          = FCodeMatParam::EType::Scalar;
            P.DefaultScalar = FCString::Atof(*ValueStr);
        }
        else
        {
            P.Type = (TypeStr == TEXT("float4"))
                   ? FCodeMatParam::EType::Vector4
                   : FCodeMatParam::EType::Vector3;

            TArray<FString> Parts;
            ValueStr.ParseIntoArray(Parts, TEXT(","));
            auto Comp = [&](int32 i)
            {
                return Parts.IsValidIndex(i) ? FCString::Atof(*Parts[i].TrimStartAndEnd()) : 0.f;
            };
            P.DefaultColor = FLinearColor(Comp(0), Comp(1), Comp(2),
                                          Parts.Num() >= 4 ? Comp(3) : 1.f);
        }
        Params.Add(P);
    }
    return Params;
}

// Extract typed parameter list from a function signature.
// e.g. "float3 MainHLSL(float2 uv, float3 OutlineColor)"
//   -> [("float2","uv"), ("float3","OutlineColor")]
// コメント内のシグネチャ(e.g. "// Entry: float3 Foo(float2 uv, ...)")にマッチしないよう
// コメントを除去してから検索する。
static TArray<TPair<FString, FString>> ExtractFuncParams(const FString& Code, const FString& FuncName)
{
    TArray<TPair<FString, FString>> Result;
    const FString Stripped = StripLineComments(Code);
    const FString Pattern = FString::Printf(TEXT("\\b%s\\s*\\(([^)]*)\\)"), *FuncName);
    FRegexMatcher M(FRegexPattern(*Pattern), Stripped);
    if (!M.FindNext()) return Result;

    const FString ParamList = M.GetCaptureGroup(1).TrimStartAndEnd();
    if (ParamList.IsEmpty()) return Result;

    TArray<FString> Parts;
    ParamList.ParseIntoArray(Parts, TEXT(","));
    for (FString& Part : Parts)
    {
        Part = Part.TrimStartAndEnd();
        TArray<FString> Tokens;
        Part.ParseIntoArrayWS(Tokens);
        if (Tokens.Num() >= 2)
            Result.Add(TPair<FString, FString>(Tokens[0], Tokens.Last()));
    }
    return Result;
}

// ============================================================================
// BuildMaterialGraph
// ============================================================================

static void BuildMaterialGraph(
    UMaterial*     Material,
    const FString& FragmentCode,
    const FString& VertexCode)
{
    if (!Material) return;

    // 既存のコンパイルを全部終わらせてから変更する
    if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
        GShaderCompilingManager->FinishAllCompilation();

    Material->PreEditChange(nullptr);

    // ---- Expression を直接操作してクリア（UMaterialEditingLibrary 不使用） ----
    // UMaterialEditingLibrary::CreateMaterialExpression / DeleteAllMaterialExpressions は
    // 内部で Material->PostEditChange() を毎回呼ぶため、中途半端なグラフ状態で
    // 何度もシェーダーコンパイルが走り FShaderPipelineCompileJob が失敗、
    // FinalizeShaderCode 未呼び出し → OptionalDataSize == -1 アサートが発生する。
    // NewObject で直接生成し PostEditChange は末尾の1回だけにする。
    UMaterialEditorOnlyData* ED = Material->GetEditorOnlyData();
    ED->ExpressionCollection.Empty();

    // マテリアルピン接続をリセット
    ED->EmissiveColor.Expression       = nullptr;
    ED->WorldPositionOffset.Expression = nullptr;
    ED->OpacityMask.Expression         = nullptr;

    // Expression を生成して ExpressionCollection に登録するヘルパー
    auto NewExpr = [&](UClass* ExprClass) -> UMaterialExpression*
    {
        UMaterialExpression* E = NewObject<UMaterialExpression>(
            Material, ExprClass, NAME_None, RF_Transactional);
        if (E)
        {
            E->Material = Material;  // CreateMaterialExpressionEx と同じバックリファレンス設定
            ED->ExpressionCollection.AddExpression(E);
        }
        return E;
    };

    // ---- @param expressions ----
    const FString CombinedCode = FragmentCode + TEXT("\n\n") + VertexCode;
    const TArray<FCodeMatParam> Params = ParseCodeMatParams(CombinedCode);
    TMap<FString, UMaterialExpression*> ParamExprMap;
    {
        int32 Y = 0;
        for (const FCodeMatParam& P : Params)
        {
            UMaterialExpression* Expr = nullptr;
            if (P.Type == FCodeMatParam::EType::Scalar)
            {
                auto* E = Cast<UMaterialExpressionScalarParameter>(
                    NewExpr(UMaterialExpressionScalarParameter::StaticClass()));
                if (E) { E->ParameterName = FName(*P.Name); E->DefaultValue = P.DefaultScalar; Expr = E; }
            }
            else
            {
                auto* E = Cast<UMaterialExpressionVectorParameter>(
                    NewExpr(UMaterialExpressionVectorParameter::StaticClass()));
                if (E) { E->ParameterName = FName(*P.Name); E->DefaultValue = P.DefaultColor; Expr = E; }
            }
            if (Expr)
            {
                Expr->MaterialExpressionEditorX = -700;
                Expr->MaterialExpressionEditorY = Y;
                ParamExprMap.Add(P.Name, Expr);
                Y += 80;
            }
        }
    }

    // ---- Standard source expressions ----
    auto* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(
        NewExpr(UMaterialExpressionTextureCoordinate::StaticClass()));
    if (TexCoord)
    {
        TexCoord->CoordinateIndex           = 0;
        TexCoord->MaterialExpressionEditorX = -400;
        TexCoord->MaterialExpressionEditorY =    0;
    }

    // WorldPosition: スケール依存座標に使用 (float3 WorldPos という名前で自動配線)
    auto* WorldPosExpr = Cast<UMaterialExpressionWorldPosition>(
        NewExpr(UMaterialExpressionWorldPosition::StaticClass()));
    if (WorldPosExpr)
    {
        WorldPosExpr->MaterialExpressionEditorX = -400;
        WorldPosExpr->MaterialExpressionEditorY =  200;
    }

    // ObjectRadius: WPO のスケール依存変位に使用 (float ObjectRadius という名前で自動配線)
    auto* ObjectRadiusExpr = Cast<UMaterialExpressionObjectRadius>(
        NewExpr(UMaterialExpressionObjectRadius::StaticClass()));
    if (ObjectRadiusExpr)
    {
        ObjectRadiusExpr->MaterialExpressionEditorX = -400;
        ObjectRadiusExpr->MaterialExpressionEditorY =  400;
    }

    // Time ノード (float Time パラメータで自動配線)
    auto* TimeExpr = Cast<UMaterialExpressionTime>(
        NewExpr(UMaterialExpressionTime::StaticClass()));
    if (TimeExpr)
    {
        TimeExpr->MaterialExpressionEditorX = -400;
        TimeExpr->MaterialExpressionEditorY =  600;
    }

    const bool bHasWPO = !VertexCode.TrimStartAndEnd().IsEmpty();
    UMaterialExpressionVertexNormalWS* VertNormal = nullptr;
    if (bHasWPO)
    {
        VertNormal = Cast<UMaterialExpressionVertexNormalWS>(
            NewExpr(UMaterialExpressionVertexNormalWS::StaticClass()));
        if (VertNormal)
        {
            VertNormal->MaterialExpressionEditorX = -400;
            VertNormal->MaterialExpressionEditorY =  300;
        }
    }

    // ---- 関数引数 → Expression 入力 の解決 ----
    // 自動配線ルール (パラメータ名による):
    //   "uv" / "UV"               → TextureCoordinate (UV 座標)
    //   "WorldPos" / "worldpos"   → WorldPosition (絶対ワールド座標 float3)
    //   "ObjectRadius"            → ObjectRadius (バウンディング球半径 float)
    //   contains "Normal"         → VertexNormalWS
    //   matches a @param          → ScalarParameter / VectorParameter
    auto ResolveInput = [&](const FString& TypeStr, const FString& PName, FExpressionInput& Out)
    {
        if (UMaterialExpression** Found = ParamExprMap.Find(PName))
        {
            Out.Expression = *Found;
            const FCodeMatParam* P = Params.FindByPredicate(
                [&](const FCodeMatParam& X){ return X.Name == PName; });
            if (P && P->Type == FCodeMatParam::EType::Vector3)
            {
                Out.Mask = 1; Out.MaskR = 1; Out.MaskG = 1; Out.MaskB = 1; Out.MaskA = 0;
            }
            return;
        }
        if (PName.Equals(TEXT("uv"), ESearchCase::IgnoreCase) && TexCoord)
        {
            Out.Expression = TexCoord;
            return;
        }
        if (PName.Equals(TEXT("WorldPos"), ESearchCase::IgnoreCase) && WorldPosExpr)
        {
            Out.Expression = WorldPosExpr;
            return;
        }
        if (PName.Equals(TEXT("ObjectRadius"), ESearchCase::IgnoreCase) && ObjectRadiusExpr)
        {
            Out.Expression = ObjectRadiusExpr;
            return;
        }
        if (PName.Equals(TEXT("Time"), ESearchCase::IgnoreCase) && TimeExpr)
        {
            Out.Expression = TimeExpr;
            return;
        }
        if ((PName.Contains(TEXT("Normal")) || PName.Contains(TEXT("normal"))) && VertNormal)
        {
            Out.Expression = VertNormal;
            return;
        }
    };

    // ---- Custom ノードを生成して返すヘルパー（PostEditChange なし） ----
    // 注: ShaderCompileWorker がプロジェクトプラグインの仮想インクルードパスを
    //     解決できないため IncludeFilePaths を使わず関数ボディを直接 Code に埋め込む。
    //     CodeMat_<Name>.ush はユーザー編集用に書き出すが、コンパイルには使わない。
    auto BuildCustomNode = [&](const FString& FuncName,
                               const FString& SourceCode,
                               ECustomMaterialOutputType OutType,
                               int32 NodeY) -> UMaterialExpressionCustom*
    {
        if (SourceCode.IsEmpty()) return nullptr;

        auto* Custom = Cast<UMaterialExpressionCustom>(
            NewExpr(UMaterialExpressionCustom::StaticClass()));
        if (!Custom) return nullptr;

        Custom->OutputType                = OutType;
        Custom->MaterialExpressionEditorX =    0;
        Custom->MaterialExpressionEditorY = NodeY;
        // ShaderCompileWorker はプロジェクトプラグインの仮想インクルードパスを
        // 解決できないため IncludeFilePaths は使用しない。
        // 関数ボディを Custom->Code に直接埋め込むことでインクルードを回避する。
        Custom->IncludeFilePaths.Reset();
        Custom->Inputs.Reset();

        const TArray<TPair<FString, FString>> FuncParams = ExtractFuncParams(SourceCode, FuncName);
        for (const TPair<FString, FString>& Fp : FuncParams)
        {
            FCustomInput& In = Custom->Inputs.AddDefaulted_GetRef();
            In.InputName     = FName(*Fp.Value);
            ResolveInput(Fp.Key, Fp.Value, In.Input);
        }

        // 関数ボディとヘルパー関数群を抽出する。
        const FString FuncBody    = InjectHLSLBuiltins(ExtractShaderFunctionBody(SourceCode, FuncName));
        const FString HelpersCode = ExtractHelperFunctions(SourceCode, FuncName);

        if (!HelpersCode.IsEmpty() && !FuncBody.IsEmpty())
        {
            // ── Escape trick ────────────────────────────────────────────────
            // UE は Custom->Code を以下のように展開する:
            //   MaterialFloat3 CustomExpressionN(...) { <Code> }
            //
            // ヘルパー関数は HLSL の関数ボディ内に定義できないため、
            // 「脱出トリック」でラッパー関数を早期クローズし、
            // ヘルパー群をグローバルスコープに注入する。
            //
            // 生成される HLSL 構造:
            //   MaterialFloat3 CustomExpressionN(...) {
            //     return <ImplName>(args);      // ← 本実装への転送
            //   }
            //   <ヘルパー定義群>
            //   float3 <ImplName>(params) { <FuncBody> }
            //   void _close_<ImplName>() {     // UE の自動クローズ } を吸収
            //   }  ← UE が自動生成する閉じ括弧
            // ────────────────────────────────────────────────────────────────

            const FString ImplName = FuncName + TEXT("_impl");

            // 引数転送リスト ("uv, Phase, ...") と実装関数シグネチャ ("float2 uv, float Phase, ...") を構築
            FString CallArgs;
            FString ImplParams;
            for (const TPair<FString,FString>& Fp : FuncParams)
            {
                if (!CallArgs.IsEmpty())   CallArgs   += TEXT(", ");
                if (!ImplParams.IsEmpty()) ImplParams += TEXT(", ");
                CallArgs   += Fp.Value;
                ImplParams += Fp.Key + TEXT(" ") + Fp.Value;
            }

            FString EscCode;
            // 1. UE のラッパー関数から実装関数を呼んで閉じる
            EscCode += FString::Printf(TEXT("return %s(%s);\n}\n\n"), *ImplName, *CallArgs);
            // 2. ヘルパー関数定義をグローバルスコープへ
            EscCode += HelpersCode;
            EscCode += TEXT("\n");
            // 3. 実装関数 (ヘルパーを呼べるスコープ)
            EscCode += FString::Printf(TEXT("float3 %s(%s)\n{\n"), *ImplName, *ImplParams);
            EscCode += FuncBody;
            EscCode += TEXT("\n}\n\n");
            // 4. UE が自動生成する閉じ括弧を吸収するダミー関数
            EscCode += FString::Printf(TEXT("void _close_%s()\n{"), *ImplName);

            Custom->Code = EscCode;
        }
        else
        {
            // ヘルパーなし: 関数ボディを直接埋め込む (従来の動作)
            Custom->Code = !FuncBody.IsEmpty()
                ? FuncBody
                : FString::Printf(TEXT("// ERROR: could not extract body of '%s'\nreturn 0;\n"), *FuncName);
        }
        return Custom;
    };

    // ---- ノードを生成してマテリアルピンに直接接続 ----
    // SafeifyHLSL 済みのコードから関数名を動的検出（ハードコードしない）
    if (bHasWPO)
    {
        const FString WPOFuncName = DetectFunctionName(VertexCode);
        if (!WPOFuncName.IsEmpty())
        {
            auto* WPOCustom = BuildCustomNode(
                WPOFuncName, VertexCode, CMOT_Float3, -200);
            if (WPOCustom)
            {
                ED->WorldPositionOffset.Expression  = WPOCustom;
                ED->WorldPositionOffset.OutputIndex = 0;
            }
        }
    }

    const FString FragFuncName = DetectFunctionName(FragmentCode);
    auto* FragCustom = !FragFuncName.IsEmpty()
        ? BuildCustomNode(FragFuncName, FragmentCode, CMOT_Float3, 0)
        : nullptr;
    if (FragCustom)
    {
        ED->EmissiveColor.Expression   = FragCustom;
        ED->EmissiveColor.OutputIndex  = 0;
    }

    if (bHasWPO)
    {
        // アウトライン（シェル法）: バック面だけを描画する
        Material->TwoSided  = 1;
        Material->BlendMode = BLEND_Masked;

        auto* TwoSidedSign = Cast<UMaterialExpressionTwoSidedSign>(
            NewExpr(UMaterialExpressionTwoSidedSign::StaticClass()));
        auto* Negate = Cast<UMaterialExpressionMultiply>(
            NewExpr(UMaterialExpressionMultiply::StaticClass()));

        if (TwoSidedSign && Negate)
        {
            TwoSidedSign->MaterialExpressionEditorX = 0;
            TwoSidedSign->MaterialExpressionEditorY = 250;
            Negate->A.Expression                    = TwoSidedSign;
            Negate->ConstB                          = -1.f;
            Negate->MaterialExpressionEditorX       = 200;
            Negate->MaterialExpressionEditorY       = 250;
            ED->OpacityMask.Expression        = Negate;
            ED->OpacityMask.OutputIndex       = 0;
        }
    }
    else
    {
        Material->TwoSided  = 0;
        Material->BlendMode = BLEND_Opaque;
    }

    Material->SetShadingModel(MSM_Unlit);

    // PostEditChange は必ずここの1回だけ — これが唯一のシェーダーコンパイルトリガー
    Material->PostEditChange();
    Material->MarkPackageDirty();
}

// ============================================================================
// CompileCodeAssetToMaterial (public entry point)
// ============================================================================

bool CodeMat::CompileCodeAssetToMaterial(
    UCodeMaterialAsset* Asset,
    UMaterial*& OutMaterial,
    FString& OutError)
{
    OutMaterial = nullptr;
    OutError.Empty();

    if (!Asset) { OutError = TEXT("Asset is null"); return false; }

    UMaterial* Mat = CreateOrLoadMaterial(Asset);
    if (!Mat) { OutError = TEXT("Failed to create/load material"); return false; }

#if WITH_EDITOR
    // Wait for any in-flight shader compilation to finish before writing new
    // shader files. Writing .ush files while the compile thread holds shader
    // code objects causes the OptionalDataSize == -1 assertion in
    // FShaderCode::GetFinalizedResource (ShaderCore.h:1414).
    if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
    {
        GShaderCompilingManager->FinishAllCompilation();
    }

    // HLSL 予約語の関数名（VertexShader 等）をコンパイル前に安全な名前へ置換する。
    // FXC が VertexShader を予約キーワードとして扱いコンパイル失敗させるのが
    // OptionalDataSize == -1 クラッシュの根本原因。
    // ユーザーの Asset プロパティは書き換えず、書き出す .ush / グラフ構築にのみ適用する。
    const FString SafeFragCode = SafeifyHLSL(Asset->FragmentShaderCode);
    const FString SafeVertCode = SafeifyHLSL(Asset->VertexShaderCode);

    // Fragment + Vertex を 1 ファイル CodeMat_<Name>.ush に書き出す
    FString CombinedVirtualPath;
    if (!WriteShaderFile(Asset, SafeFragCode, SafeVertCode, CombinedVirtualPath, OutError))
        return false;

    // インメモリのシェーダーソースファイルキャッシュをクリアする。
    FlushShaderFileCache();
#endif

    BuildMaterialGraph(Mat, SafeFragCode, SafeVertCode);

#if WITH_EDITOR
    // PostEditChange がキックしたシェーダーコンパイルが完全に終わるまで待機する。
    // 完了前に SaveLoadedAsset を呼ぶと、サムネイル生成がまだ FinalizeShaderCode()
    // されていないシェーダーに対して GetFinalizedCodeResource() を呼び、
    // check(OptionalDataSize == -1) クラッシュ (UE5.7 エンジンバグ) が発生する。
    if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
        GShaderCompilingManager->FinishAllCompilation();
#endif

    UEditorAssetLibrary::SaveLoadedAsset(Mat, /*bOnlyIfIsDirty*/ false);

    OutMaterial = Mat;

    UE_LOG(LogTemp, Warning, TEXT("CompileCodeAssetToMaterial: Asset=%s  Mat=%s  Path=%s"),
        *GetNameSafe(Asset), *GetNameSafe(Mat), *MakeMaterialPackagePath(Asset));

    return true;
}

#endif // WITH_EDITOR
