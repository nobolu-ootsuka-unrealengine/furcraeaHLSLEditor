// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Public/CodeMaterialAssetFactory.h
#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "CodeMaterialAssetFactory.generated.h"

/** コンテンツブラウザの Create Asset メニュー (Misc) から UCodeMaterialAsset を作る */
UCLASS()
class FURCRAEAHLSLEDITOREDITOR_API UCodeMaterialAssetFactory : public UFactory
{
    GENERATED_BODY()

public:
    UCodeMaterialAssetFactory();

    virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
                                      UObject* Context, FFeedbackContext* Warn) override;
    virtual bool ShouldShowInNewMenu() const override { return true; }
    virtual uint32 GetMenuCategories() const override;
    virtual bool DoesSupportClass(UClass* Class) override;
};
