// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/furcraeaHLSLEditorModule.cpp

#include "Modules/ModuleManager.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h" // AddShaderSourceDirectoryMapping

class FfurcraeaHLSLEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogTemp, Warning, TEXT("furcraeaHLSLEditor StartupModule"));

        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("furcraeaHLSLEditor"));
        if (!Plugin.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("furcraeaHLSLEditor Plugin not found"));
            return;
        }

        const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/furcraeaHLSLEditor"), ShaderDir);
    }

    virtual void ShutdownModule() override
    {
    }
};

IMPLEMENT_MODULE(FfurcraeaHLSLEditorModule, furcraeaHLSLEditor)
