// furcraeaHLSLEditorEditor.cpp

#include "Modules/ModuleManager.h"

// AssetTools
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"

// Your actions
#include "AssetTypeActions_CodeMaterialAsset.h"

class FfurcraeaHLSLEditorEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // AssetTools は Editor 起動後にロードされるので、LoadModuleChecked でOK
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        IAssetTools& AssetTools = AssetToolsModule.Get();

        CodeMaterialAssetActions = MakeShared<FAssetTypeActions_CodeMaterialAsset>();
        AssetTools.RegisterAssetTypeActions(CodeMaterialAssetActions.ToSharedRef());
    }

    virtual void ShutdownModule() override
    {
        if (FModuleManager::Get().IsModuleLoaded("AssetTools") && CodeMaterialAssetActions.IsValid())
        {
            FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
            IAssetTools& AssetTools = AssetToolsModule.Get();

            AssetTools.UnregisterAssetTypeActions(CodeMaterialAssetActions.ToSharedRef());
        }

        CodeMaterialAssetActions.Reset();
    }

private:
    TSharedPtr<IAssetTypeActions> CodeMaterialAssetActions;
};

IMPLEMENT_MODULE(FfurcraeaHLSLEditorEditorModule, furcraeaHLSLEditorEditor)
