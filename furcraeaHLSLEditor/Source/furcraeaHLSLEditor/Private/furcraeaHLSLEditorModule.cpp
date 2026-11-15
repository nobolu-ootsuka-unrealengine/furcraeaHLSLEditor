// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/furcraeaHLSLEditorModule.cpp

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h" // AddShaderSourceDirectoryMapping
#include "CodeMatLog.h"

DEFINE_LOG_CATEGORY(LogCodeMat);

class FfurcraeaHLSLEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // /Plugin/furcraeaHLSLEditor → <Plugin>/Shaders (CodeMat_*.ush の参照用)
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("furcraeaHLSLEditor"));
        if (!Plugin.IsValid())
        {
            UE_LOG(LogCodeMat, Warning, TEXT("furcraeaHLSLEditor plugin not found; shader directory mapping skipped"));
            return;
        }
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/furcraeaHLSLEditor"), FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
    }
};

IMPLEMENT_MODULE(FfurcraeaHLSLEditorModule, furcraeaHLSLEditor)
