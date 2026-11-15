// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/CodeMaterialAssetFactory.cpp

#include "CodeMaterialAssetFactory.h"

#include "AssetTypeCategories.h"
#include "CodeMaterialAsset.h"

UCodeMaterialAssetFactory::UCodeMaterialAssetFactory()
{
    bCreateNew     = true;
    bEditAfterNew  = true;
    bEditorImport  = false;
    SupportedClass = UCodeMaterialAsset::StaticClass();
}

UObject* UCodeMaterialAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
                                                     UObject* /*Context*/, FFeedbackContext* /*Warn*/)
{
    UClass* ClassToCreate = InClass ? InClass : SupportedClass.Get();
    if (!DoesSupportClass(ClassToCreate)) return nullptr;
    return NewObject<UCodeMaterialAsset>(InParent, ClassToCreate, InName, Flags);
}

uint32 UCodeMaterialAssetFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Misc;
}

bool UCodeMaterialAssetFactory::DoesSupportClass(UClass* Class)
{
    return Class && Class->IsChildOf(UCodeMaterialAsset::StaticClass());
}
