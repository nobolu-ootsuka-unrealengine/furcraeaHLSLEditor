// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/furcraeaHLSLEditorEditorModule.cpp

#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetTypeActions_CodeMaterialAsset.h"

class FfurcraeaHLSLEditorEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        CodeMaterialAssetActions = MakeShared<FAssetTypeActions_CodeMaterialAsset>();
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(CodeMaterialAssetActions.ToSharedRef());
    }

    virtual void ShutdownModule() override
    {
        if (CodeMaterialAssetActions.IsValid() && FModuleManager::Get().IsModuleLoaded("AssetTools"))
        {
            FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(CodeMaterialAssetActions.ToSharedRef());
        }
        CodeMaterialAssetActions.Reset();
    }

private:
    TSharedPtr<IAssetTypeActions> CodeMaterialAssetActions;
};

IMPLEMENT_MODULE(FfurcraeaHLSLEditorEditorModule, furcraeaHLSLEditorEditor)
