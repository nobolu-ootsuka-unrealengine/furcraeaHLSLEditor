// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMaterialCompiler.cpp

#include "CodeMaterialCompiler.h"
#include "CodeMaterialAsset.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "EditorAssetLibrary.h" // ���ǉ�
#include "ObjectTools.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"


static bool WriteUserUshFile(
    const UCodeMaterialAsset* Asset,
    const FString& UserCode,
    FString& OutVirtualIncludePath,
    FString& OutErr
) 
{
    OutErr.Empty();
    if (!Asset)
    {
        OutErr = TEXT("Asset is null");
        return false;
    }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("furcraeaHLSLEditor"));
    if (!Plugin.IsValid())
    {
        OutErr = TEXT("Plugin not found: furcraeaHLSLEditor");
        return false;
    }


    FString SafeAssetName = ObjectTools::SanitizeObjectName(Asset->GetName());
    const FString FileName = FString::Printf(TEXT("CodeMat_%s.ush"), *SafeAssetName);

    const FString DiskPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders/Private"), FileName);

    // Material �� #include �Ō���̂͂���
    OutVirtualIncludePath = FString::Printf(TEXT("/Plugin/furcraeaHLSLEditor/Private/%s"), *FileName);


    // �擪�� #pragma once �������t�^�i���[�U�[�������Ă�OK�j
    FString TextToWrite = UserCode;
    if (!TextToWrite.Contains(TEXT("#pragma once")))
    {
        TextToWrite = FString(TEXT("#pragma once\n\n")) + TextToWrite;
    }

    // ���s����Ȃǁi�C�Ӂj
    TextToWrite.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    TextToWrite.ReplaceInline(TEXT("\r"), TEXT("\n"));

    if (!FFileHelper::SaveStringToFile(TextToWrite, *DiskPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutErr = FString::Printf(TEXT("Failed to write: %s"), *DiskPath);
        return false;
    }

    return true;
}
static void ForceShaderRecompile()
{
    //FlushShaderFileCache();
    //RecompileChangedShaders(TEXT("CodeMatUser changed"));
}

static void BuildCustomNode_ToBaseColor(UMaterial* Material, const FString& HlslCode);


static FString MakeMaterialPackagePath(const UCodeMaterialAsset* Asset)
{
    FString AssetName = Asset ? Asset->GetName() : TEXT("Unknown");
    AssetName = ObjectTools::SanitizeObjectName(AssetName); // �����S��
    return FString::Printf(TEXT("/Game/Generated/M_%s"), *AssetName);
}

static UMaterial* CreateOrLoadMaterial(UCodeMaterialAsset* Asset)
{
    if (!Asset) return nullptr;

    // �� /Game/Generated �����iContent Browser��̃t�H���_�j
    UEditorAssetLibrary::MakeDirectory(TEXT("/Game/Generated"));

    if (Asset->OutputMaterial)
    {
        return Asset->OutputMaterial;
    }

    const FString PackagePath = MakeMaterialPackagePath(Asset); // /Game/Generated/M_xxx
    const FString PackageName = PackagePath;                    // CreatePackage�̓t���p�XOK
    const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) return nullptr;

    // ���ɓ���������Ȃ炻����E��
    if (UMaterial* Existing = FindObject<UMaterial>(Package, *AssetName))
    {
        Asset->OutputMaterial = Existing;
        Asset->MarkPackageDirty();
        return Existing;
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UObject* NewObj = Factory->FactoryCreateNew(
        UMaterial::StaticClass(),
        Package,
        FName(*AssetName),
        RF_Public | RF_Standalone,
        nullptr,
        GWarn
    );

    UMaterial* Mat = Cast<UMaterial>(NewObj);
    if (!Mat) return nullptr;

    // Asset Registry �ɒʒm�iContent Browser�ɏo���j
    FAssetRegistryModule::AssetCreated(Mat);

    // �p�b�P�[�WDirty
    Mat->MarkPackageDirty();

    Asset->OutputMaterial = Mat;
    Asset->MarkPackageDirty();

    return Mat;
}
// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/CodeMaterialCompiler.cpp

bool CodeMat::CompileCodeAssetToMaterial(
    UCodeMaterialAsset* Asset,
    UMaterial*& OutMaterial,
    FString& OutError
)
{
    OutMaterial = nullptr;
    OutError.Empty();

    if (!Asset)
    {
        OutError = TEXT("Asset is null");
        return false;
    }

    UMaterial* Mat = CreateOrLoadMaterial(Asset);
    if (!Mat)
    {
        OutError = TEXT("Failed to create/load material");
        return false;
    }

#if WITH_EDITOR
    // �� �܂� .ush ���X�V
    FString VirtualIncludePath;
    if (!WriteUserUshFile(Asset, Asset->HlslCode, VirtualIncludePath, OutError))
    {
        return false;
    }

    // �� ���ꂪ�����ƕύX���E���ɂ������Ƃ�����
    FlushShaderFileCache();
#endif

    BuildCustomNode_ToBaseColor(Mat, VirtualIncludePath);

    // ���d�v�F�ۑ����Ȃ��� /Game/Generated �������Ȃ�/�o�Ȃ����Ƃ�����
    UEditorAssetLibrary::SaveLoadedAsset(Mat, /*bOnlyIfIsDirty*/ false);

    OutMaterial = Mat;

    UE_LOG(LogTemp, Warning, TEXT("CompileCodeAssetToMaterial called. Asset=%s"), *GetNameSafe(Asset));
    UE_LOG(LogTemp, Warning, TEXT("PackagePath=%s"), *MakeMaterialPackagePath(Asset));
    UE_LOG(LogTemp, Warning, TEXT("Mat=%s Outer=%s"), *GetNameSafe(Mat), *GetNameSafe(Mat ? Mat->GetOutermost() : nullptr));

    return true;
}


static void BuildCustomNode_ToBaseColor(UMaterial* Material, const FString& VirtualIncludePath)
{
    if (!Material) return;

    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

    // 1) TextureCoordinate �m�[�h�����
    auto* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            UMaterialExpressionTextureCoordinate::StaticClass()
        )
    );
    if (!TexCoord) return;
    TexCoord->CoordinateIndex = 0; // UV0

    // 2) Custom �m�[�h�����
    auto* Custom = Cast<UMaterialExpressionCustom>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            UMaterialExpressionCustom::StaticClass()
        )
    );
    if (!Custom) return;

    Custom->OutputType = CMOT_Float3;

    Custom->IncludeFilePaths.Reset();
    Custom->IncludeFilePaths.Add(VirtualIncludePath);

    // 3) Custom Input �gUV�h ��ǉ����� TexCoord ��ڑ�
    {
        FCustomInput& UVInput = Custom->Inputs.AddDefaulted_GetRef();
        UVInput.InputName = TEXT("UV");

        // TexCoord �̏o��(0)�� Custom ���͂�
        UVInput.Input.Expression = TexCoord;
        UVInput.Input.OutputIndex = 0;
    }

    // 4) Custom �̃R�[�h�� Parameters ��G�炸�A����UV���g��
    Custom->Code =
        TEXT("return MainHLSL(UV);\n");

    // BaseColor�֐ڑ�
    //UMaterialEditingLibrary::ConnectMaterialProperty(Custom, TEXT(""), MP_BaseColor);
    
    //Emissive �Ɍq����
    UMaterialEditingLibrary::ConnectMaterialProperty(Custom, TEXT(""), MP_EmissiveColor);
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_Unlit);



    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
}

#endif // WITH_EDITOR
