// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/CodeMaterialAssetEditor.cpp

#include "CodeMaterialAssetEditor.h"

#include "CodeMaterialCompiler.h"
#include "CodeMatCodeValidation.h"
#include "CodeSyntaxHighlighter.h"

#include "EditorAssetLibrary.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CodeMaterialAssetEditor"

namespace
{
const FName AppIdentifier(TEXT("CodeMaterialAssetEditorApp"));
const FName ShaderTabId(TEXT("CodeMaterialAssetEditor_ShaderTab"));

void ShowNotification(const FString& Message, bool bSuccess)
{
    FNotificationInfo Info(FText::FromString(Message));
    Info.ExpireDuration = 5.0f;
    Info.bUseLargeFont  = false;
    TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
    if (Item.IsValid()) Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
}

void ShowError(const FString& Title, const FString& Detail)
{
    UE_LOG(LogTemp, Warning, TEXT("[CodeMaterialAsset] %s: %s"), *Title, *Detail);
    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("%s\n\n%s"), *Title, *Detail)));
}

/** ノード変換の注意を表示する (Custom ノードへのフォールバックが起きたときはダイアログ) */
void ReportConversionLog(const FString& Log)
{
    if (Log.IsEmpty()) return;
    UE_LOG(LogTemp, Log, TEXT("[CodeMaterialAsset]\n%s"), *Log);
    if (Log.Contains(TEXT("fell back")))
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Some functions could not be converted to material nodes:\n\n%s"), *Log)));
    else
        ShowNotification(Log.TrimEnd(), true);
}
} // namespace

// ============================================================================
// FOptionList
// ============================================================================

void FCodeMaterialAssetEditorToolkit::FOptionList::Init(std::initializer_list<const TCHAR*> Labels, int32 InitialIndex)
{
    Options.Reset();
    for (const TCHAR* L : Labels) Options.Add(MakeShared<FString>(L));
    Selected = Options[Options.IsValidIndex(InitialIndex) ? InitialIndex : 0];
}

int32 FCodeMaterialAssetEditorToolkit::FOptionList::GetSelectedIndex() const
{
    const int32 Idx = Options.IndexOfByKey(Selected);
    return Idx == INDEX_NONE ? 0 : Idx;
}

// ============================================================================
// Toolkit
// ============================================================================

void FCodeMaterialAssetEditorToolkit::Init(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCodeMaterialAsset* InAsset)
{
    Asset = InAsset;
    check(Asset);

    // 並びは ECodeMatDomain / ECodeMatBlendableLocation / ECodeMatOutputMode の値と一致
    DomainOptions.Init({ TEXT("Surface"), TEXT("Post Process") }, (int32)Asset->Domain);
    BlendLocOptions.Init({ TEXT("After Tonemapping"), TEXT("Before Tonemapping"), TEXT("Before Translucency"), TEXT("Replacing Tonemapper") },
                         (int32)Asset->BlendableLocation);
    OutputModeOptions.Init({ TEXT("Custom Node (HLSL)"), TEXT("Material Nodes") }, (int32)Asset->OutputMode);

    const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("CodeMaterialAssetEditor_Layout_v3")
        ->AddArea
        (
            FTabManager::NewPrimaryArea()
            ->SetOrientation(Orient_Vertical)
            ->Split(FTabManager::NewStack()->AddTab(ShaderTabId, ETabState::OpenedTab)->SetHideTabWell(true))
        );

    InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, Layout,
                    /*bCreateDefaultStandaloneMenu*/ true, /*bCreateDefaultToolbar*/ true, Asset.Get());

    TSharedPtr<FExtender> Extender = MakeShared<FExtender>();
    Extender->AddToolBarExtension("Asset", EExtensionHook::After, GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::FillToolbar));
    AddToolbarExtender(Extender);
    RegenerateMenusAndToolbars();
}

FName FCodeMaterialAssetEditorToolkit::GetToolkitFName() const { return FName("CodeMaterialAssetEditor"); }
FText FCodeMaterialAssetEditorToolkit::GetBaseToolkitName() const { return LOCTEXT("AppLabel", "Code Material Asset"); }
FString FCodeMaterialAssetEditorToolkit::GetWorldCentricTabPrefix() const { return TEXT("CodeMaterialAsset"); }
FLinearColor FCodeMaterialAssetEditorToolkit::GetWorldCentricTabColorScale() const { return FLinearColor(0.2f, 0.6f, 1.0f, 1.0f); }

void FCodeMaterialAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
    InTabManager->RegisterTabSpawner(ShaderTabId, FOnSpawnTab::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::SpawnShaderTab))
        .SetDisplayName(LOCTEXT("ShaderTabTitle", "Shaders"))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FCodeMaterialAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    InTabManager->UnregisterTabSpawner(ShaderTabId);
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FCodeMaterialAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
    Collector.AddReferencedObject(Asset);
}

FString FCodeMaterialAssetEditorToolkit::GetReferencerName() const
{
    return TEXT("FCodeMaterialAssetEditorToolkit");
}

// ============================================================================
// UI
// ============================================================================

TSharedRef<SWidget> FCodeMaterialAssetEditorToolkit::MakeOptionCombo(
    const FText& Label, const FText& Tooltip, FOptionList& List, float Width, bool bFirst, TAttribute<EVisibility> Visibility)
{
    TSharedRef<SWidget> Combo =
        SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&List.Options)
        .InitiallySelectedItem(List.Selected)
        .OnSelectionChanged_Lambda([&List](TSharedPtr<FString> NewSelection, ESelectInfo::Type) { List.Selected = NewSelection; })
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
        [
            SNew(STextBlock).Text_Lambda([&List]() { return FText::FromString(List.Selected.IsValid() ? *List.Selected : FString()); })
        ];
    if (Width > 0.0f)
    {
        Combo = SNew(SBox).WidthOverride(Width)[Combo];
    }

    return SNew(SHorizontalBox)
        .Visibility(Visibility)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(bFirst ? 0.0f : 16.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(Label)
            .ToolTipText(Tooltip)
            .Font(FAppStyle::GetFontStyle("BoldFont"))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            Combo
        ];
}

TSharedRef<SWidget> FCodeMaterialAssetEditorToolkit::MakeCodePane(
    const FText& Label, const FString& InitialText, TSharedPtr<SMultiLineEditableTextBox>& OutTextBox)
{
    SAssignNew(OutTextBox, SMultiLineEditableTextBox)
        .Text(FText::FromString(InitialText))
        .Font(FAppStyle::GetFontStyle("MonoFont"))
        .Marshaller(FCodeSyntaxHighlighter::Create())
        .AlwaysShowScrollbars(true)
        .AutoWrapText(false)
        // SMultiLineEditableText は既定で '\t' を入力不可とし、その結果 Tab キーがフォーカス移動に使われる。
        // Tab を有効な文字にすると Tab の KeyDown がテキスト側で処理され、Tab 文字が入力される。
        .OnIsTypedCharValid_Lambda([](const TCHAR) { return true; });

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock).Text(Label).Font(FAppStyle::GetFontStyle("BoldFont"))
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            OutTextBox.ToSharedRef()
        ];
}

TSharedRef<SDockTab> FCodeMaterialAssetEditorToolkit::SpawnShaderTab(const FSpawnTabArgs& Args)
{
    check(Args.GetTabId() == ShaderTabId);
    check(Asset);

    const TAttribute<EVisibility> PostProcessOnly = TAttribute<EVisibility>::CreateLambda([this]()
    {
        return GetSelectedDomain() == ECodeMatDomain::PostProcess ? EVisibility::Visible : EVisibility::Collapsed;
    });

    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            [
                SNew(SVerticalBox)
                // ---- 設定行 ----
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        MakeOptionCombo(LOCTEXT("DomainLabel", "Domain:"), FText::GetEmpty(), DomainOptions, 0.0f, /*bFirst*/ true)
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        MakeOptionCombo(LOCTEXT("OutputModeLabel", "Output:"),
                            LOCTEXT("OutputModeTooltip",
                                "Custom Node: embed the HLSL as-is in one Custom node.\n"
                                "Material Nodes: convert the HLSL into real material nodes (Add, Multiply, Lerp, ...).\n"
                                "Functions that cannot be converted automatically fall back to a Custom node."),
                            OutputModeOptions, 170.0f, false)
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        MakeOptionCombo(LOCTEXT("BlendLocLabel", "Blendable Location:"), FText::GetEmpty(), BlendLocOptions, 200.0f, false, PostProcessOnly)
                    ]
                ]
                // ---- Fragment / Vertex ペイン ----
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SSplitter)
                    .Orientation(Orient_Horizontal)
                    + SSplitter::Slot().Value(0.5f)
                    [
                        MakeCodePane(LOCTEXT("FragmentLabel", "Fragment Shader"), Asset->FragmentShaderCode, FragmentTextBox)
                    ]
                    + SSplitter::Slot().Value(0.5f)
                    [
                        MakeCodePane(LOCTEXT("VertexLabel", "Vertex Shader (optional)"), Asset->VertexShaderCode, VertexTextBox)
                    ]
                ]
            ]
        ];
}

void FCodeMaterialAssetEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
    auto AddButton = [&](void (FCodeMaterialAssetEditorToolkit::*Handler)(), const FText& Label, const FText& Tooltip, const FName& Icon)
    {
        ToolbarBuilder.AddToolBarButton(FUIAction(FExecuteAction::CreateRaw(this, Handler)), NAME_None, Label, Tooltip,
                                        FSlateIcon(FAppStyle::GetAppStyleSetName(), Icon));
    };

    AddButton(&FCodeMaterialAssetEditorToolkit::OnClickedSave,
        LOCTEXT("SaveLabel", "Save"),
        LOCTEXT("SaveTooltip", "Write editor text back to the asset, generate M_<Asset> and save."),
        "Icons.Save");
    AddButton(&FCodeMaterialAssetEditorToolkit::OnClickedReset,
        LOCTEXT("ResetLabel", "Reset"),
        LOCTEXT("ResetTooltip", "Reset editor text to the default template (does not save automatically)."),
        "Icons.Refresh");

    ToolbarBuilder.AddSeparator();

    AddButton(&FCodeMaterialAssetEditorToolkit::OnClickedCopyPython,
        LOCTEXT("CopyPythonLabel", "Copy Python"),
        LOCTEXT("CopyPythonTooltip",
            "Copy an Unreal Python script that builds the same material graph (current text + Output mode).\n"
            "Paste it into Output Log > Python. Also saved to Saved/furcraeaHLSL/<Asset>.py"),
        "GenericCommands.Copy");
    AddButton(&FCodeMaterialAssetEditorToolkit::OnClickedCopyT3D,
        LOCTEXT("CopyT3DLabel", "Copy T3D"),
        LOCTEXT("CopyT3DTooltip",
            "Copy the generated nodes as T3D text (the Material Editor's copy format).\n"
            "Open any Material and press Ctrl+V. Nodes that feed a material pin are labelled '-> Emissive' etc."),
        "GenericCommands.Paste");
}

// ============================================================================
// 状態
// ============================================================================

ECodeMatDomain FCodeMaterialAssetEditorToolkit::GetSelectedDomain() const
{
    return (ECodeMatDomain)DomainOptions.GetSelectedIndex();
}

ECodeMatBlendableLocation FCodeMaterialAssetEditorToolkit::GetSelectedBlendLoc() const
{
    return (ECodeMatBlendableLocation)BlendLocOptions.GetSelectedIndex();
}

ECodeMatOutputMode FCodeMaterialAssetEditorToolkit::GetSelectedOutputMode() const
{
    return (ECodeMatOutputMode)OutputModeOptions.GetSelectedIndex();
}

FString FCodeMaterialAssetEditorToolkit::GetFragmentText() const
{
    return CodeMatValidation::NormalizeCode(FragmentTextBox.IsValid() ? FragmentTextBox->GetText().ToString() : Asset->FragmentShaderCode);
}

FString FCodeMaterialAssetEditorToolkit::GetVertexText() const
{
    return CodeMatValidation::NormalizeCode(VertexTextBox.IsValid() ? VertexTextBox->GetText().ToString() : Asset->VertexShaderCode);
}

FCodeMatBuildSettings FCodeMaterialAssetEditorToolkit::GatherCurrentSettings() const
{
    FCodeMatBuildSettings S = FCodeMatBuildSettings::FromAsset(Asset);
    S.FragmentCode = GetFragmentText();
    S.VertexCode   = GetVertexText();
    S.Domain       = GetSelectedDomain();
    S.BlendLoc     = GetSelectedBlendLoc();
    S.OutputMode   = GetSelectedOutputMode();
    return S;
}

// ============================================================================
// ツールバー操作
// ============================================================================

void FCodeMaterialAssetEditorToolkit::OnClickedSave()
{
    if (!Asset) return;

    const FCodeMatBuildSettings S = GatherCurrentSettings();

    FString Error;
    if (!CodeMatValidation::ValidateFragmentCode(S.FragmentCode, Error))
    {
        ShowError(TEXT("Fragment save blocked."), Error);
        return;
    }
    if (!CodeMatValidation::IsWhitespaceOnly(S.VertexCode) && !CodeMatValidation::ValidateVertexCode(S.VertexCode, Error))
    {
        ShowError(TEXT("Vertex save blocked."), Error);
        return;
    }

    const bool bChanged =
        Asset->FragmentShaderCode != S.FragmentCode || Asset->VertexShaderCode != S.VertexCode ||
        Asset->Domain != S.Domain || Asset->BlendableLocation != S.BlendLoc || Asset->OutputMode != S.OutputMode;
    if (bChanged)
    {
        Asset->Modify();
        Asset->FragmentShaderCode = S.FragmentCode;
        Asset->VertexShaderCode   = S.VertexCode;
        Asset->Domain             = S.Domain;
        Asset->BlendableLocation  = S.BlendLoc;
        Asset->OutputMode         = S.OutputMode;
        Asset->MarkPackageDirty();
    }

    // 変更が無くても毎回コンパイルする (手動で壊れたマテリアルの再生成用)
    UMaterial* Mat = nullptr;
    FString Err, ConvLog;
    if (!CodeMat::CompileCodeAssetToMaterial(Asset, Mat, Err, &ConvLog) || !Mat)
    {
        ShowError(TEXT("Compile failed."), Err);
        return;
    }
    ReportConversionLog(ConvLog);
    UEditorAssetLibrary::SaveLoadedAsset(Mat,   /*bOnlyIfIsDirty*/ false);
    UEditorAssetLibrary::SaveLoadedAsset(Asset, /*bOnlyIfIsDirty*/ false);
}

void FCodeMaterialAssetEditorToolkit::OnClickedReset()
{
    const EAppReturnType::Type Ret = FMessageDialog::Open(EAppMsgType::YesNo,
        LOCTEXT("ResetConfirm", "Reset both shaders to the default templates?\n\n(Unsaved edits in the editor will be lost.)"));
    if (Ret != EAppReturnType::Yes) return;

    if (FragmentTextBox.IsValid()) FragmentTextBox->SetText(FText::FromString(UCodeMaterialAsset::GetDefaultFragmentCode()));
    if (VertexTextBox.IsValid())   VertexTextBox->SetText(FText::FromString(UCodeMaterialAsset::GetDefaultVertexCode()));
}

void FCodeMaterialAssetEditorToolkit::OnClickedCopyPython()
{
    if (!Asset) return;
    const FCodeMatBuildSettings S = GatherCurrentSettings();
    FString Script, Err, Log;
    if (!CodeMat::ExportPythonScript(S, Script, Err, &Log))
    {
        ShowError(TEXT("Python export failed."), Err);
        return;
    }
    FPlatformApplicationMisc::ClipboardCopy(*Script);

    const FString PyPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("furcraeaHLSL"), S.AssetName + TEXT(".py"));
    FFileHelper::SaveStringToFile(Script, *PyPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    ReportConversionLog(Log);
    ShowNotification(FString::Printf(TEXT("Python script copied to clipboard\n%s"), *FPaths::ConvertRelativePathToFull(PyPath)), true);
}

void FCodeMaterialAssetEditorToolkit::OnClickedCopyT3D()
{
    if (!Asset) return;
    FString Text, Err, Log;
    if (!CodeMat::ExportT3D(GatherCurrentSettings(), Text, Err, &Log))
    {
        ShowError(TEXT("T3D export failed."), Err);
        return;
    }
    FPlatformApplicationMisc::ClipboardCopy(*Text);
    ReportConversionLog(Log);
    ShowNotification(TEXT("T3D copied to clipboard - open a Material and press Ctrl+V"), true);
}

#undef LOCTEXT_NAMESPACE
