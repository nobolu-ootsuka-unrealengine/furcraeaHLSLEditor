// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Public/CodeMaterialAssetFactory.h

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "CodeMaterialAssetFactory.generated.h"

class UCodeMaterialAsset;

UCLASS()
class FURCRAEAHLSLEDITOREDITOR_API UCodeMaterialAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UCodeMaterialAssetFactory();

	// UE5.7 でもこの形でOK
	virtual UObject* FactoryCreateNew(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn
	) override;

	// Create Asset メニューに出す
	virtual bool ShouldShowInNewMenu() const override { return true; }

	// どのクラスを作るFactoryか
	virtual uint32 GetMenuCategories() const override;

	// import系じゃないことを明示（念のため）
	virtual bool DoesSupportClass(UClass* Class) override;
};
