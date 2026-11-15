// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMaterialCompiler.cpp
//
// UCodeMaterialAsset → M_<Name> (同じフォルダ) の生成と保存。
// グラフ構築は CodeMatGraphBuilder、Python / T3D は CodeMatExport が担当する。

#include "CodeMaterialCompiler.h"

#if WITH_EDITOR

#include "CodeMatGraphBuilder.h"
#include "CodeMatHLSLSource.h"
#include "CodeMatLog.h"

#include "Materials/Material.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ShaderCompiler.h"

namespace
{

void FinishShaderCompilation()
{
    if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
        GShaderCompilingManager->FinishAllCompilation();
}

/**
 * Fragment + Vertex を Shaders/Private/CodeMat_<Asset>.ush に書き出す (ユーザー参照用)。
 * コンパイルには使わない (Custom ノードは関数ボディを直接埋め込む)。
 */
bool WriteShaderFile(const FString& AssetName, const FString& FragCode, const FString& VertCode, FString& OutErr)
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("furcraeaHLSLEditor"));
    if (!Plugin.IsValid()) { OutErr = TEXT("Plugin not found: furcraeaHLSLEditor"); return false; }

    const FString DiskPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders/Private"),
                                             FString::Printf(TEXT("CodeMat_%s.ush"), *AssetName));

    FString Text = TEXT("#pragma once\n\n// ===== Fragment Shader =====\n") + FragCode;
    if (!VertCode.TrimStartAndEnd().IsEmpty())
        Text += TEXT("\n\n// ===== Vertex Shader =====\n") + VertCode;
    Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    Text.ReplaceInline(TEXT("\r"), TEXT("\n"));

    if (!FFileHelper::SaveStringToFile(Text, *DiskPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutErr = FString::Printf(TEXT("Failed to write: %s"), *DiskPath);
        return false;
    }
    return true;
}

/** Asset と同じフォルダの M_<AssetName> */
FString MakeMaterialPackagePath(const UCodeMaterialAsset* Asset)
{
    const FString Folder = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    return FString::Printf(TEXT("%s/M_%s"), *Folder, *ObjectTools::SanitizeObjectName(Asset->GetName()));
}

UMaterial* CreateOrLoadMaterial(UCodeMaterialAsset* Asset)
{
    const FString PackagePath = MakeMaterialPackagePath(Asset);
    UEditorAssetLibrary::MakeDirectory(FPackageName::GetLongPackagePath(PackagePath));

    if (Asset->OutputMaterial) return Asset->OutputMaterial;

    const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) return nullptr;

    UMaterial* Mat = FindObject<UMaterial>(Package, *AssetName);
    if (!Mat)
    {
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        Mat = Cast<UMaterial>(Factory->FactoryCreateNew(
            UMaterial::StaticClass(), Package, FName(*AssetName), RF_Public | RF_Standalone, nullptr, GWarn));
        if (!Mat) return nullptr;
        FAssetRegistryModule::AssetCreated(Mat);
        Mat->MarkPackageDirty();
    }

    Asset->OutputMaterial = Mat;
    Asset->MarkPackageDirty();
    return Mat;
}

} // namespace

FCodeMatBuildSettings FCodeMatBuildSettings::FromAsset(const UCodeMaterialAsset* Asset)
{
    FCodeMatBuildSettings S;
    if (!Asset) return S;
    S.FragmentCode = Asset->FragmentShaderCode;
    S.VertexCode   = Asset->VertexShaderCode;
    S.Domain       = Asset->Domain;
    S.BlendLoc     = Asset->BlendableLocation;
    S.OutputMode   = Asset->OutputMode;
    S.AssetName    = ObjectTools::SanitizeObjectName(Asset->GetName());
    S.AssetFolder  = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    return S;
}

bool CodeMat::CompileCodeAssetToMaterial(UCodeMaterialAsset* Asset, UMaterial*& OutMaterial, FString& OutError, FString* OutLog)
{
    OutMaterial = nullptr;
    OutError.Empty();
    if (!Asset) { OutError = TEXT("Asset is null"); return false; }

    UMaterial* Mat = CreateOrLoadMaterial(Asset);
    if (!Mat) { OutError = TEXT("Failed to create/load material"); return false; }

    // コンパイル中に .ush を書き換えると FShaderCode::GetFinalizedResource の
    // OptionalDataSize == -1 アサートが起きるため、先に全コンパイルを終わらせる。
    FinishShaderCompilation();

    const FCodeMatBuildSettings Settings = FCodeMatBuildSettings::FromAsset(Asset);
    if (!WriteShaderFile(Settings.AssetName,
                         CodeMatSource::SafeifyHLSL(Settings.FragmentCode),
                         CodeMatSource::SafeifyHLSL(Settings.VertexCode), OutError))
    {
        return false;
    }
    FlushShaderFileCache();

    FString Log;
    CodeMatGraph::BuildMaterialGraph(Mat, Settings, /*bFinalize*/ true, Log);
    if (OutLog) *OutLog = Log;

    // 完了前に保存するとサムネイル生成が未 Finalize のシェーダーを触ってアサートする (UE5.7)
    FinishShaderCompilation();
    UEditorAssetLibrary::SaveLoadedAsset(Mat, /*bOnlyIfIsDirty*/ false);

    OutMaterial = Mat;
    UE_LOG(LogCodeMat, Log, TEXT("Compiled %s -> %s"), *GetNameSafe(Asset), *Mat->GetPathName());
    return true;
}

#endif // WITH_EDITOR
