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
// WriteUserUshFile
// ============================================================================

static bool WriteUserUshFile(
    const UCodeMaterialAsset* Asset,
    const FString& UserCode,
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

    FString TextToWrite = UserCode;
    if (!TextToWrite.Contains(TEXT("#pragma once")))
        TextToWrite = TEXT("#pragma once\n\n") + TextToWrite;

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
static TArray<TPair<FString, FString>> ExtractFuncParams(const FString& Code, const FString& FuncName)
{
    TArray<TPair<FString, FString>> Result;
    const FString Pattern = FString::Printf(TEXT("\\b%s\\s*\\(([^)]*)\\)"), *FuncName);
    FRegexMatcher M(FRegexPattern(*Pattern), Code);
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
    const FString& VirtualIncludePath,
    const FString& HlslCode)
{
    if (!Material) return;

    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

    // --- Create @param expressions ---
    const TArray<FCodeMatParam> Params = ParseCodeMatParams(HlslCode);
    TMap<FString, UMaterialExpression*> ParamExprMap;
    {
        int32 Y = 0;
        for (const FCodeMatParam& P : Params)
        {
            UMaterialExpression* Expr = nullptr;
            if (P.Type == FCodeMatParam::EType::Scalar)
            {
                auto* E = Cast<UMaterialExpressionScalarParameter>(
                    UMaterialEditingLibrary::CreateMaterialExpression(
                        Material, UMaterialExpressionScalarParameter::StaticClass()));
                if (E) { E->ParameterName = FName(*P.Name); E->DefaultValue = P.DefaultScalar; Expr = E; }
            }
            else
            {
                auto* E = Cast<UMaterialExpressionVectorParameter>(
                    UMaterialEditingLibrary::CreateMaterialExpression(
                        Material, UMaterialExpressionVectorParameter::StaticClass()));
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

    // --- Standard source expressions ---
    auto* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material, UMaterialExpressionTextureCoordinate::StaticClass()));
    if (TexCoord)
    {
        TexCoord->CoordinateIndex          = 0;
        TexCoord->MaterialExpressionEditorX = -400;
        TexCoord->MaterialExpressionEditorY =    0;
    }

    // VertexNormalWS は MainWPO がある場合のみ生成する
    const bool bHasWPO = HlslCode.Contains(TEXT("MainWPO"));
    UMaterialExpressionVertexNormalWS* VertNormal = nullptr;
    if (bHasWPO)
    {
        VertNormal = Cast<UMaterialExpressionVertexNormalWS>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material, UMaterialExpressionVertexNormalWS::StaticClass()));
        if (VertNormal)
        {
            VertNormal->MaterialExpressionEditorX = -400;
            VertNormal->MaterialExpressionEditorY =  100;
        }
    }

    // --- Helper: resolve a function parameter to a material expression input ---
    auto ResolveInput = [&](const FString& TypeStr, const FString& PName, FExpressionInput& Out)
    {
        // @param match (case-sensitive name)
        if (UMaterialExpression** Found = ParamExprMap.Find(PName))
        {
            Out.Expression = *Found;
            // float3 VectorParameter outputs float4; mask to .rgb
            const FCodeMatParam* P = Params.FindByPredicate(
                [&](const FCodeMatParam& X){ return X.Name == PName; });
            if (P && P->Type == FCodeMatParam::EType::Vector3)
            {
                Out.Mask = 1; Out.MaskR = 1; Out.MaskG = 1; Out.MaskB = 1; Out.MaskA = 0;
            }
            return;
        }
        // uv / UV -> TexCoord
        if (PName.Equals(TEXT("uv"), ESearchCase::IgnoreCase) && TexCoord)
        {
            Out.Expression = TexCoord;
            return;
        }
        // *Normal* -> VertexNormalWS
        if ((PName.Contains(TEXT("Normal")) || PName.Contains(TEXT("normal"))) && VertNormal)
        {
            Out.Expression = VertNormal;
            return;
        }
        // unresolved: leave null (Custom node treats it as 0)
    };

    // --- Helper: build a Custom node for one entry function ---
    auto BuildCustomNode = [&](const FString& FuncName, EMaterialProperty TargetProp,
                               ECustomMaterialOutputType OutType, int32 NodeY)
    {
        // Only build if function appears in source
        if (!HlslCode.Contains(FuncName)) return;

        auto* Custom = Cast<UMaterialExpressionCustom>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material, UMaterialExpressionCustom::StaticClass()));
        if (!Custom) return;

        Custom->OutputType                = OutType;
        Custom->MaterialExpressionEditorX =    0;
        Custom->MaterialExpressionEditorY = NodeY;
        Custom->IncludeFilePaths.Reset();
        Custom->IncludeFilePaths.Add(VirtualIncludePath);
        Custom->Inputs.Reset();

        const TArray<TPair<FString, FString>> FuncParams = ExtractFuncParams(HlslCode, FuncName);
        TArray<FString> ArgNames;
        for (const TPair<FString, FString>& Fp : FuncParams)
        {
            ArgNames.Add(Fp.Value);
            FCustomInput& In = Custom->Inputs.AddDefaulted_GetRef();
            In.InputName     = FName(*Fp.Value);
            ResolveInput(Fp.Key, Fp.Value, In.Input);
        }

        const FString ArgList = FString::Join(ArgNames, TEXT(", "));
        Custom->Code = FString::Printf(TEXT("return %s(%s);\n"), *FuncName, *ArgList);

        UMaterialEditingLibrary::ConnectMaterialProperty(Custom, TEXT(""), TargetProp);
    };

    // Build vertex (WPO) node first, then pixel node
    BuildCustomNode(TEXT("MainWPO"),  MP_WorldPositionOffset, CMOT_Float3, -200);
    BuildCustomNode(TEXT("MainHLSL"), MP_EmissiveColor,       CMOT_Float3,    0);

    if (bHasWPO)
    {
        // アウトライン（シェル法）: バック面だけを描画する
        // TwoSidedSign は フロント面=1, バック面=-1 を返す
        // -1 を掛けると フロント面=-1(破棄), バック面=1(描画) になり
        // BLEND_Masked のデフォルト閾値(0.333)でフロント面が自動的に破棄される
        Material->TwoSided      = 1;
        Material->BlendMode     = BLEND_Masked;

        auto* TwoSidedSign = Cast<UMaterialExpressionTwoSidedSign>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material, UMaterialExpressionTwoSidedSign::StaticClass()));
        auto* Negate = Cast<UMaterialExpressionMultiply>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material, UMaterialExpressionMultiply::StaticClass()));

        if (TwoSidedSign && Negate)
        {
            TwoSidedSign->MaterialExpressionEditorX = 0;
            TwoSidedSign->MaterialExpressionEditorY = 250;
            Negate->A.Expression                    = TwoSidedSign;
            Negate->ConstB                          = -1.f;
            Negate->MaterialExpressionEditorX       = 200;
            Negate->MaterialExpressionEditorY       = 250;
            UMaterialEditingLibrary::ConnectMaterialProperty(Negate, TEXT(""), MP_OpacityMask);
        }
    }
    else
    {
        Material->TwoSided    = 0;
        Material->BlendMode   = BLEND_Opaque;
    }

    Material->SetShadingModel(MSM_Unlit);

    Material->PreEditChange(nullptr);
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
    FString VirtualIncludePath;
    if (!WriteUserUshFile(Asset, Asset->HlslCode, VirtualIncludePath, OutError))
        return false;
    // FlushShaderFileCache() は呼ばない。
    // シェーダーコンパイルスレッドとの競合で OptionalDataSize == -1 アサートが起きるため。
    // .ush はアセットごとに固有のファイル名なので、初回コンパイル時に必ずディスクから読まれる。
#endif

    BuildMaterialGraph(Mat, VirtualIncludePath, Asset->HlslCode);

    UEditorAssetLibrary::SaveLoadedAsset(Mat, /*bOnlyIfIsDirty*/ false);

    OutMaterial = Mat;

    UE_LOG(LogTemp, Warning, TEXT("CompileCodeAssetToMaterial: Asset=%s  Mat=%s  Path=%s"),
        *GetNameSafe(Asset), *GetNameSafe(Mat), *MakeMaterialPackagePath(Asset));

    return true;
}

#endif // WITH_EDITOR
