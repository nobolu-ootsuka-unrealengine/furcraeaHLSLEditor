// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/AssetTypeActions_CodeMaterialAsset.cpp

#include "AssetTypeActions_CodeMaterialAsset.h"

#include "AssetTypeCategories.h"
#include "CodeMaterialAsset.h"
#include "CodeMaterialAssetEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_CodeMaterialAsset"

FText FAssetTypeActions_CodeMaterialAsset::GetName() const
{
    return LOCTEXT("CodeMaterialAssetName", "Code Material Asset");
}

FColor FAssetTypeActions_CodeMaterialAsset::GetTypeColor() const
{
    return FColor(80, 200, 255);
}

UClass* FAssetTypeActions_CodeMaterialAsset::GetSupportedClass() const
{
    return UCodeMaterialAsset::StaticClass();
}

uint32 FAssetTypeActions_CodeMaterialAsset::GetCategories()
{
    return EAssetTypeCategories::Misc;
}

void FAssetTypeActions_CodeMaterialAsset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> /*EditWithinLevelEditor*/)
{
    for (UObject* Obj : InObjects)
    {
        if (UCodeMaterialAsset* Asset = Cast<UCodeMaterialAsset>(Obj))
        {
            // 常にスタンドアロンで開く (ツールキットの寿命は FToolkitManager が管理する)
            MakeShared<FCodeMaterialAssetEditorToolkit>()->Init(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Asset);
        }
    }
}

#undef LOCTEXT_NAMESPACE
