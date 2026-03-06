#include "AssetTypeActions_CodeMaterialAsset.h"

#include "CodeMaterialAsset.h"
#include "CodeMaterialAssetEditor.h"

#include "Toolkits/IToolkitHost.h"      // ← 追加
#include "Toolkits/ToolkitManager.h"
#include "AssetTypeCategories.h"


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
	// まずは確実に Misc に出す
	return EAssetTypeCategories::Misc;
}

void FAssetTypeActions_CodeMaterialAsset::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor
)
{
    for (UObject* Obj : InObjects)
    {
        if (UCodeMaterialAsset* Asset = Cast<UCodeMaterialAsset>(Obj))
        {
            TSharedRef<FCodeMaterialAssetEditor> Editor = MakeShared<FCodeMaterialAssetEditor>();
            Editor->InitCodeMaterialAssetEditor(
                EToolkitMode::Standalone,
                TSharedPtr<IToolkitHost>(),  // ← ここを null に固定
                Asset
            );
        }
    }
}

#undef LOCTEXT_NAMESPACE
