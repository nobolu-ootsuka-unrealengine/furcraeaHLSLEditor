// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Public/CodeMaterialAssetEditor.h
//
// UCodeMaterialAsset のエディタ: Fragment / Vertex の 2 ペイン + Domain / Output / BlendableLocation の選択。
// ツールバー: Save (保存して M_<Name> を生成) / Reset / Copy Python / Copy T3D

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"
#include "CodeMaterialAsset.h"

class SMultiLineEditableTextBox;
struct FCodeMatBuildSettings;

class FCodeMaterialAssetEditorToolkit : public FAssetEditorToolkit, public FGCObject
{
public:
    void Init(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCodeMaterialAsset* InAsset);

    // FAssetEditorToolkit
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual FString GetWorldCentricTabPrefix() const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

    // FGCObject
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override;

private:
    /** 文字列の選択肢を持つコンボボックスの状態。選択肢の並びは対応する enum の値と一致させる。 */
    struct FOptionList
    {
        TArray<TSharedPtr<FString>> Options;
        TSharedPtr<FString>         Selected;

        void  Init(std::initializer_list<const TCHAR*> Labels, int32 InitialIndex);
        int32 GetSelectedIndex() const;
    };

    TSharedRef<SDockTab> SpawnShaderTab(const FSpawnTabArgs& Args);
    TSharedRef<SWidget>  MakeOptionCombo(const FText& Label, const FText& Tooltip, FOptionList& List, float Width, bool bFirst,
                                         TAttribute<EVisibility> Visibility = EVisibility::Visible);
    TSharedRef<SWidget>  MakeCodePane(const FText& Label, const FString& InitialText, TSharedPtr<SMultiLineEditableTextBox>& OutTextBox);
    void FillToolbar(FToolBarBuilder& ToolbarBuilder);

    ECodeMatDomain            GetSelectedDomain() const;
    ECodeMatBlendableLocation GetSelectedBlendLoc() const;
    ECodeMatOutputMode        GetSelectedOutputMode() const;
    FString GetFragmentText() const;
    FString GetVertexText() const;

    /** エディタ上の (未保存の) テキストと選択状態からビルド設定を作る */
    FCodeMatBuildSettings GatherCurrentSettings() const;

    void OnClickedSave();
    void OnClickedReset();
    void OnClickedCopyPython();
    void OnClickedCopyT3D();

    TObjectPtr<UCodeMaterialAsset> Asset = nullptr;

    TSharedPtr<SMultiLineEditableTextBox> FragmentTextBox;
    TSharedPtr<SMultiLineEditableTextBox> VertexTextBox;

    FOptionList DomainOptions;
    FOptionList BlendLocOptions;
    FOptionList OutputModeOptions;
};
