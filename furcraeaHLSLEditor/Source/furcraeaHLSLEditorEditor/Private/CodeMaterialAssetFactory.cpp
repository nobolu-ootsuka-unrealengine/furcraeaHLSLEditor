// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/CodeMaterialAssetFactory.cpp

#include "CodeMaterialAssetFactory.h"

// Runtime側
#include "CodeMaterialAsset.h"

#include "AssetTypeCategories.h"

UCodeMaterialAssetFactory::UCodeMaterialAssetFactory()
{
    bCreateNew = true;
    bEditAfterNew = true;
    bEditorImport = false;

    SupportedClass = UCodeMaterialAsset::StaticClass();
}

UObject* UCodeMaterialAssetFactory::FactoryCreateNew(
    UClass* InClass,
    UObject* InParent,
    FName InName,
    EObjectFlags Flags,
    UObject* Context,
    FFeedbackContext* Warn
)
{
    // UE5.7: SupportedClass は TSubclassOf<UObject> なので Get() で UClass* にする
    UClass* SupportedUClass = SupportedClass.Get();

    // InClass が null のときは SupportedClass を使う
    UClass* ClassToCreate = InClass ? InClass : SupportedUClass;

    if (!ClassToCreate || !ClassToCreate->IsChildOf(UCodeMaterialAsset::StaticClass()))
    {
        return nullptr;
    }

    UCodeMaterialAsset* NewAsset = NewObject<UCodeMaterialAsset>(InParent, ClassToCreate, InName, Flags);

    return NewAsset;
}

uint32 UCodeMaterialAssetFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Misc;
}

bool UCodeMaterialAssetFactory::DoesSupportClass(UClass* Class)
{
    return (Class != nullptr) && Class->IsChildOf(UCodeMaterialAsset::StaticClass());
}
