// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/CodeMatScriptLibrary.cpp

#include "CodeMatScriptLibrary.h"
#include "CodeMaterialCompiler.h"

#include "Materials/Material.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "RHIShaderPlatform.h"

bool UCodeMatScriptLibrary::PasteT3DIntoMaterial(UMaterial* Material, const FString& T3DText, bool bConnectOutputs, FString& Log)
{
    return CodeMat::PasteT3DIntoMaterial(Material, T3DText, bConnectOutputs, Log);
}

UMaterial* UCodeMatScriptLibrary::CompileToMaterial(UCodeMaterialAsset* Asset, ECodeMatOutputMode Mode, FString& Log)
{
    Log.Empty();
    if (!Asset) { Log = TEXT("Asset is null"); return nullptr; }

    if (Asset->OutputMode != Mode)
    {
        Asset->Modify();
        Asset->OutputMode = Mode;
        Asset->MarkPackageDirty();
    }

    UMaterial* Mat = nullptr;
    FString Err;
    if (!CodeMat::CompileCodeAssetToMaterial(Asset, Mat, Err, &Log))
    {
        Log += Err;
        return nullptr;
    }
    return Mat;
}

FString UCodeMatScriptLibrary::ExportPythonScript(UCodeMaterialAsset* Asset, ECodeMatOutputMode Mode, FString& Log)
{
    Log.Empty();
    if (!Asset) { Log = TEXT("Asset is null"); return FString(); }
    FCodeMatBuildSettings S = FCodeMatBuildSettings::FromAsset(Asset);
    S.OutputMode = Mode;
    FString Out, Err;
    if (!CodeMat::ExportPythonScript(S, Out, Err, &Log)) Log += Err;
    return Out;
}

FString UCodeMatScriptLibrary::ExportT3D(UCodeMaterialAsset* Asset, ECodeMatOutputMode Mode, FString& Log)
{
    Log.Empty();
    if (!Asset) { Log = TEXT("Asset is null"); return FString(); }
    FCodeMatBuildSettings S = FCodeMatBuildSettings::FromAsset(Asset);
    S.OutputMode = Mode;
    FString Out, Err;
    if (!CodeMat::ExportT3D(S, Out, Err, &Log)) Log += Err;
    return Out;
}

TArray<FString> UCodeMatScriptLibrary::GetMaterialCompileErrors(UMaterial* Material)
{
    TArray<FString> Errors;
    if (!Material) { Errors.Add(TEXT("Material is null")); return Errors; }

    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();

    if (FMaterialResource* Res = Material->GetMaterialResource(GMaxRHIShaderPlatform))
    {
        Res->FinishCompilation();
        Errors = Res->GetCompileErrors();
    }
    else
    {
        Errors.Add(TEXT("No material resource for the current shader platform"));
    }
    return Errors;
}
