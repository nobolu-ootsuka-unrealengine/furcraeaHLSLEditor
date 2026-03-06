// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Public/AssetTypeActions_CodeMaterialAsset.h
#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class IToolkitHost;

class FAssetTypeActions_CodeMaterialAsset : public FAssetTypeActions_Base
{
public:
	// FAssetTypeActions_Base
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	virtual void OpenAssetEditor(
		const TArray<UObject*>& InObjects,
		TSharedPtr<IToolkitHost> EditWithinLevelEditor
	) override;
};
