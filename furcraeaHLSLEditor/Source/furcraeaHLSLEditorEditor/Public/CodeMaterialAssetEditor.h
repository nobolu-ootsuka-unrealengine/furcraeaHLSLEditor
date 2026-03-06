#pragma once

#include "CoreMinimal.h"

class UCodeMaterialAsset;
class IToolkitHost;

// š‚±‚±‚ğ forward éŒ¾icpp ‚Å include ‚·‚éj
class FCodeMaterialAssetEditorToolkit;

class FCodeMaterialAssetEditor
{
public:
	void InitCodeMaterialAssetEditor(
		EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		TObjectPtr<UCodeMaterialAsset> InAsset
	);

private:
	// šFAssetEditorToolkit ‚¶‚á‚È‚­‚Ä g©‘OToolkith ‚É‚·‚é
	TSharedPtr<FCodeMaterialAssetEditorToolkit> Toolkit;
};
