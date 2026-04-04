// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMaterialAssetEditor.cpp

#include "CodeMaterialAssetEditor.h"

#include "CodeMaterialAsset.h"
#include "CodeMaterialCompiler.h"
#include "furcraeaHLSLEditorStyle.h"

#include "Toolkits/AssetEditorToolkit.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "UObject/GCObject.h"
#include "Misc/MessageDialog.h"
#include "Internationalization/Regex.h"
#include "CodeSyntaxHighlighter.h"


#include "Materials/Material.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"

#define LOCTEXT_NAMESPACE "CodeMaterialAssetEditor"

static const FName CodeMaterialAssetEditorAppIdentifier(TEXT("CodeMaterialAssetEditorApp"));
static const FName ShaderTabId(TEXT("CodeMaterialAssetEditor_ShaderTab"));

/** Custom asset editor toolkit for UCodeMaterialAsset */
class FCodeMaterialAssetEditorToolkit : public FAssetEditorToolkit, public FGCObject
{
public:
	FCodeMaterialAssetEditorToolkit() = default;
	virtual ~FCodeMaterialAssetEditorToolkit() override = default;

	void Init(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, TObjectPtr<UCodeMaterialAsset> InAsset)
	{
		Asset = InAsset;
		check(Asset);

		// Domain コンボボックスの選択肢を初期化
		DomainOptions = {
			MakeShared<FString>(TEXT("Surface")),
			MakeShared<FString>(TEXT("Post Process")),
		};
		SelectedDomain = (Asset->Domain == ECodeMatDomain::PostProcess)
			? DomainOptions[1] : DomainOptions[0];

		// BlendableLocation コンボボックスの選択肢を初期化
		BlendLocOptions = {
			MakeShared<FString>(TEXT("After Tonemapping")),
			MakeShared<FString>(TEXT("Before Tonemapping")),
			MakeShared<FString>(TEXT("Before Translucency")),
			MakeShared<FString>(TEXT("Replacing Tonemapper")),
		};
		SelectedBlendLoc = BlendLocOptions[(int32)Asset->BlendableLocation < BlendLocOptions.Num()
			? (int32)Asset->BlendableLocation : 0];

		// レイアウト
		const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
			FTabManager::NewLayout("CodeMaterialAssetEditor_Layout_v3")
			->AddArea
			(
				FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(ShaderTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
			);

		//
		FAssetEditorToolkit::InitAssetEditor(
			Mode,
			InitToolkitHost,
			CodeMaterialAssetEditorAppIdentifier,
			StandaloneDefaultLayout,
			/*bCreateDefaultStandaloneMenu=*/true,
			/*bCreateDefaultToolbar=*/true,
			Asset.Get()
		);

		//
		ExtendToolbar();
		RegenerateMenusAndToolbars();
	}

	// ------------------------------
	// FAssetEditorToolkit overrides
	// ------------------------------

	virtual FName GetToolkitFName() const override { return FName("CodeMaterialAssetEditor"); }
	virtual FText GetBaseToolkitName() const override { return LOCTEXT("AppLabel", "Code Material Asset"); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("CodeMaterialAsset"); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.2f, 0.6f, 1.0f, 1.0f); }

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override
	{
		FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

		InTabManager->RegisterTabSpawner(
			ShaderTabId,
			FOnSpawnTab::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::SpawnShaderTab)
		)
			.SetDisplayName(LOCTEXT("ShaderTabTitle", "Shaders"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	}

	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override
	{
		InTabManager->UnregisterTabSpawner(ShaderTabId);
		FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	}

	// ------------------------------
	// FGCObject overrides
	// ------------------------------

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		// UE5.7 FTObjectPtr
		Collector.AddReferencedObject(Asset);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FCodeMaterialAssetEditorToolkit");
	}

	// ------------------------------
	// Toolbar
	// ------------------------------

	void ExtendToolbar()
	{
		TSharedPtr<FExtender> Extender = MakeShared<FExtender>();
		Extender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			GetToolkitCommands(),
			FToolBarExtensionDelegate::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::FillToolbar)
		);
		AddToolbarExtender(Extender);
	}

private:
	TSharedRef<SDockTab> SpawnShaderTab(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == ShaderTabId);
		check(Asset);

		// Fragment pane
		TSharedRef<ITextLayoutMarshaller> FragHighlighter = FCodeSyntaxHighlighter::Create();
		SAssignNew(FragmentTextBox, SMultiLineEditableTextBox)
			.Text(FText::FromString(Asset->FragmentShaderCode))
			.Font(FAppStyle::GetFontStyle("MonoFont"))
			.Marshaller(FragHighlighter)
			.AlwaysShowScrollbars(true)
			.AutoWrapText(false)
			.OnTextChanged(this, &FCodeMaterialAssetEditorToolkit::OnFragmentTextChanged);

		// Vertex pane
		TSharedRef<ITextLayoutMarshaller> VertHighlighter = FCodeSyntaxHighlighter::Create();
		SAssignNew(VertexTextBox, SMultiLineEditableTextBox)
			.Text(FText::FromString(Asset->VertexShaderCode))
			.Font(FAppStyle::GetFontStyle("MonoFont"))
			.Marshaller(VertHighlighter)
			.AlwaysShowScrollbars(true)
			.AutoWrapText(false)
			.OnTextChanged(this, &FCodeMaterialAssetEditorToolkit::OnVertexTextChanged);

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(8.0f)
					[
						SNew(SVerticalBox)
						// ---- Domain 選択行 ----
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 6.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("DomainLabel", "Domain:"))
									.Font(FAppStyle::GetFontStyle("BoldFont"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SComboBox<TSharedPtr<FString>>)
									.OptionsSource(&DomainOptions)
									.InitiallySelectedItem(SelectedDomain)
									.OnSelectionChanged(this, &FCodeMaterialAssetEditorToolkit::OnDomainSelectionChanged)
									.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
									{
										return SNew(STextBlock).Text(FText::FromString(*Item));
									})
									[
										SNew(STextBlock)
											.Text_Lambda([this]()
											{
												return FText::FromString(SelectedDomain.IsValid() ? *SelectedDomain : TEXT("Surface"));
											})
									]
							]
							// ---- Blendable Location (PP 時のみ表示) ----
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(16.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("BlendLocLabel", "Blendable Location:"))
									.Font(FAppStyle::GetFontStyle("BoldFont"))
									.Visibility_Lambda([this]()
									{
										return (SelectedDomain.IsValid() && *SelectedDomain == TEXT("Post Process"))
											? EVisibility::Visible : EVisibility::Collapsed;
									})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBox).WidthOverride(200.0f)
								[
									SNew(SComboBox<TSharedPtr<FString>>)
										.OptionsSource(&BlendLocOptions)
										.InitiallySelectedItem(SelectedBlendLoc)
										.OnSelectionChanged(this, &FCodeMaterialAssetEditorToolkit::OnBlendLocSelectionChanged)
										.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
										{
											return SNew(STextBlock).Text(FText::FromString(*Item));
										})
										.Visibility_Lambda([this]()
										{
											return (SelectedDomain.IsValid() && *SelectedDomain == TEXT("Post Process"))
												? EVisibility::Visible : EVisibility::Collapsed;
										})
										[
											SNew(STextBlock)
												.Text_Lambda([this]()
												{
													return FText::FromString(SelectedBlendLoc.IsValid() ? *SelectedBlendLoc : TEXT("After Tonemapping"));
												})
										]
								]
							]
						]
						// ---- Fragment / Vertex ペイン ----
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
						SNew(SSplitter)
						.Orientation(Orient_Horizontal)
						+ SSplitter::Slot()
						.Value(0.5f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 4.0f)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("FragmentLabel", "Fragment Shader"))
									.Font(FAppStyle::GetFontStyle("BoldFont"))
							]
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								FragmentTextBox.ToSharedRef()
							]
						]
						+ SSplitter::Slot()
						.Value(0.5f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 4.0f)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("VertexLabel", "Vertex Shader (optional)"))
									.Font(FAppStyle::GetFontStyle("BoldFont"))
							]
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								VertexTextBox.ToSharedRef()
							]
						] // SSplitter::Slot (Vertex)
						] // SVerticalBox::Slot (FillHeight)
					] // SBorder content (SVerticalBox)
				];
	}


	void FillToolbar(FToolBarBuilder& ToolbarBuilder)
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::OnClickedSave),
				FCanExecuteAction::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::CanSave)
			),
			NAME_None,
			LOCTEXT("SaveLabel", "Save"),
			LOCTEXT("SaveTooltip", "Write editor text back to the asset and mark dirty."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save")
		);
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::OnClickedReset),
				FCanExecuteAction::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::CanReset)
			),
			NAME_None,
			LOCTEXT("ResetLabel", "Reset"),
			LOCTEXT("ResetTooltip", "Reset editor text to the default template (does not save automatically)."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")
		);

	}

	void OnDomainSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type)
	{
		SelectedDomain = NewSelection;
	}

	void OnBlendLocSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type)
	{
		SelectedBlendLoc = NewSelection;
	}

	void OnFragmentTextChanged(const FText& NewText)
	{
		WorkingFragment = NewText.ToString();
	}

	void OnVertexTextChanged(const FText& NewText)
	{
		WorkingVertex = NewText.ToString();
	}

	bool CanSave() const
	{
		return Asset != nullptr;
	}

	static FString GetDefaultFragmentCode()
	{
		// Sample: Character outline (cel shader) — solid outline color
		// OutlineColor is declared as a function parameter so the compiler
		// auto-wires it to the generated VectorParameter node.
		return
			TEXT("// Fragment shader — Character Outline\n")
			TEXT("// Entry: float3 FragmentShader(float2 uv, ...) -> EmissiveColor\n")
			TEXT("//\n")
			TEXT("// @param float3 OutlineColor = 0.0,0.0,0.0\n")
			TEXT("\n")
			TEXT("float3 FragmentShader(float2 uv, float3 OutlineColor)\n")
			TEXT("{\n")
			TEXT("    return OutlineColor;\n")
			TEXT("}\n");
	}

	static FString GetDefaultVertexCode()
	{
		// Sample: Shell extrusion outline
		// VertexNormal is auto-wired to VertexNormalWS (name contains "Normal").
		// OutlineWidth is auto-wired to the generated ScalarParameter node.
		// The compiler sets TwoSided=true, BlendMode=Masked, and wires
		// TwoSidedSign * -1 to OpacityMask automatically when WPO is active.
		return
			TEXT("// Vertex shader — Character Outline (Shell Extrusion)\n")
			TEXT("// Entry: float3 WPO_Main(float3 VertexNormal, ...) -> WorldPositionOffset\n")
			TEXT("// Leave this pane empty (or all commented out) to disable WPO.\n")
			TEXT("//\n")
			TEXT("// @param float OutlineWidth = 0.3\n")
			TEXT("\n")
			TEXT("float3 WPO_Main(float3 VertexNormal, float OutlineWidth)\n")
			TEXT("{\n")
			TEXT("    return normalize(VertexNormal) * OutlineWidth;\n")
			TEXT("}\n");
	}


	// Strip // and /* */ comments for validation purposes
	static FString StripComments(const FString& Code)
	{
		FString Out = Code;
		// Single-line comments
		{
			FRegexPattern Pat(TEXT("//[^\n]*"));
			FRegexMatcher M(Pat, Out);
			while (M.FindNext())
			{
				const int32 Begin = M.GetMatchBeginning();
				const int32 End   = M.GetMatchEnding();
				Out.RemoveAt(Begin, End - Begin);
				M = FRegexMatcher(Pat, Out);
			}
		}
		// Block comments
		{
			FRegexPattern Pat(TEXT("/\\*.*?\\*/"));
			FRegexMatcher M(Pat, Out);
			while (M.FindNext())
			{
				const int32 Begin = M.GetMatchBeginning();
				const int32 End   = M.GetMatchEnding();
				Out.RemoveAt(Begin, End - Begin);
				M = FRegexMatcher(Pat, Out);
			}
		}
		return Out;
	}

	// Extract the body of a function whose signature matches HeadPat
	static bool ExtractFunctionBody(const FString& Stripped, const FRegexPattern& HeadPat, FString& OutBody, FString& OutError)
	{
		FRegexMatcher HeadM(HeadPat, Stripped);
		if (!HeadM.FindNext())
		{
			OutError = TEXT("Entry function signature not found.");
			return false;
		}

		const int32 HeadEnd = HeadM.GetMatchEnding();
		const int32 OpenBracePos = Stripped.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HeadEnd);
		if (OpenBracePos == INDEX_NONE)
		{
			OutError = TEXT("Function body '{' not found.");
			return false;
		}

		int32 Depth = 0;
		for (int32 i = OpenBracePos; i < Stripped.Len(); ++i)
		{
			const TCHAR C = Stripped[i];
			if      (C == '{') { ++Depth; }
			else if (C == '}')
			{
				--Depth;
				if (Depth == 0)
				{
					OutBody = Stripped.Mid(OpenBracePos + 1, i - OpenBracePos - 1);
					return true;
				}
			}
		}

		OutError = TEXT("Function body '}' not found (brace mismatch).");
		return false;
	}

	// Validate fragment shader code (required)
	// Entry: float3 <AnyName>(float2 uv, ...)        UV 座標ベース
	//     or float3 <AnyName>(float3 WorldPos, ...)  ワールド座標ベース
	static bool ValidateFragmentCode(const FString& Code, FString& OutError)
	{
		if (Code.IsEmpty() || IsWhitespaceOnly(Code))
		{
			OutError = TEXT("Fragment shader code is empty.");
			return false;
		}

		const FString Stripped = StripComments(Code);

		// 最初の引数が float2 または float3 であれば OK
		// (uv ベース / WorldPos ベース どちらも許可)
		const FRegexPattern EntryPat(TEXT("\\bfloat3\\s+\\w+\\s*\\(\\s*float[23]\\s+[A-Za-z_][A-Za-z0-9_]*"));
		FRegexMatcher EntryM(EntryPat, Stripped);
		if (!EntryM.FindNext())
		{
			OutError = TEXT("Entry signature must be: float3 <FuncName>(float2 uv, ...) or float3 <FuncName>(float3 WorldPos, ...)");
			return false;
		}

		FString Body, BodyErr;
		if (!ExtractFunctionBody(Stripped, EntryPat, Body, BodyErr))
		{
			OutError = BodyErr;
			return false;
		}

		if (!Body.Contains(TEXT("return")))
		{
			OutError = TEXT("Fragment shader must contain a return statement.");
			return false;
		}

		return true;
	}

	// Validate vertex shader code (optional)
	// Entry: float3 <AnyName>(float3 VertexNormal, ...)  -> WorldPositionOffset
	// Returns true if all active code is commented out (treated as disabled)
	static bool ValidateVertexCode(const FString& Code, FString& OutError)
	{
		const FString Stripped = StripComments(Code);

		if (IsWhitespaceOnly(Stripped))
		{
			return true; // All commented out — WPO disabled
		}

		// 関数名は任意（VertexShader は FXC 予約語なので WPO_Main 等も許可）
		const FRegexPattern EntryPat(TEXT("\\bfloat3\\s+\\w+\\s*\\(\\s*float3\\s+[A-Za-z_][A-Za-z0-9_]*"));
		FRegexMatcher EntryM(EntryPat, Stripped);
		if (!EntryM.FindNext())
		{
			OutError = TEXT("Vertex entry signature must be: float3 <FuncName>(float3 VertexNormal, ...)");
			return false;
		}

		FString Body, BodyErr;
		if (!ExtractFunctionBody(Stripped, EntryPat, Body, BodyErr))
		{
			OutError = BodyErr;
			return false;
		}

		if (!Body.Contains(TEXT("return")))
		{
			OutError = TEXT("Vertex shader must contain a return statement.");
			return false;
		}

		return true;
	}


	static FString NormalizeCodeForSave(const FString& In)
	{
		// Normalize line endings to \n
		FString Out = In;
		Out.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Out.ReplaceInline(TEXT("\r"),   TEXT("\n"));

		// Trim trailing whitespace from each line
		TArray<FString> Lines;
		Out.ParseIntoArray(Lines, TEXT("\n"), false);
		for (FString& Line : Lines)
		{
			Line = Line.TrimEnd();
		}

		// Rejoin and ensure single trailing newline
		Out = FString::Join(Lines, TEXT("\n"));
		while (Out.EndsWith(TEXT("\n")))
		{
			Out.LeftChopInline(1);
		}
		Out += TEXT("\n");

		return Out;
	}



	bool CanReset() const
	{
		return true;
	}

	void OnClickedReset()
	{
		const EAppReturnType::Type Ret = FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("ResetConfirm", "Reset both shaders to the default templates?\n\n(Unsaved edits in the editor will be lost.)")
		);

		if (Ret != EAppReturnType::Yes)
		{
			return;
		}

		const FString FragTemplate = GetDefaultFragmentCode();
		const FString VertTemplate = GetDefaultVertexCode();

		WorkingFragment = FragTemplate;
		WorkingVertex   = VertTemplate;

		if (FragmentTextBox.IsValid()) { FragmentTextBox->SetText(FText::FromString(FragTemplate)); }
		if (VertexTextBox.IsValid())   { VertexTextBox->SetText(FText::FromString(VertTemplate));   }
	}




	void OnClickedSave()
	{
		UE_LOG(LogTemp, Warning, TEXT("OnClickedSave() CALLED"));

		if (!Asset) return;

		FString NewFragment = FragmentTextBox.IsValid()
			? FragmentTextBox->GetText().ToString()
			: WorkingFragment;
		FString NewVertex = VertexTextBox.IsValid()
			? VertexTextBox->GetText().ToString()
			: WorkingVertex;

		NewFragment = NormalizeCodeForSave(NewFragment);
		NewVertex   = NormalizeCodeForSave(NewVertex);

		// Validate fragment (required)
		FString Error;
		if (!ValidateFragmentCode(NewFragment, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CodeMaterialAsset] Fragment save blocked: %s"), *Error);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Fragment save blocked.\n\n%s"), *Error)));
			return;
		}

		// Validate vertex (optional — skip if all whitespace after normalization)
		if (!IsWhitespaceOnly(NewVertex))
		{
			if (!ValidateVertexCode(NewVertex, Error))
			{
				UE_LOG(LogTemp, Warning, TEXT("[CodeMaterialAsset] Vertex save blocked: %s"), *Error);
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Vertex save blocked.\n\n%s"), *Error)));
				return;
			}
		}

		// Domain をドロップダウン選択から Asset に反映
		const ECodeMatDomain NewDomain = (SelectedDomain.IsValid() && *SelectedDomain == TEXT("Post Process"))
			? ECodeMatDomain::PostProcess : ECodeMatDomain::Surface;

		// BlendableLocation をドロップダウン選択から Asset に反映
		static const ECodeMatBlendableLocation BlendLocFromString[] = {
			ECodeMatBlendableLocation::AfterTonemapping,
			ECodeMatBlendableLocation::BeforeTonemapping,
			ECodeMatBlendableLocation::BeforeTranslucency,
			ECodeMatBlendableLocation::ReplacingTonemapper,
		};
		int32 BlendIdx = 0;
		if (SelectedBlendLoc.IsValid())
		{
			for (int32 i = 0; i < BlendLocOptions.Num(); ++i)
			{
				if (*BlendLocOptions[i] == *SelectedBlendLoc) { BlendIdx = i; break; }
			}
		}
		const ECodeMatBlendableLocation NewBlendLoc = BlendLocFromString[BlendIdx];

		const bool bFragChanged      = (Asset->FragmentShaderCode != NewFragment);
		const bool bVertChanged      = (Asset->VertexShaderCode   != NewVertex);
		const bool bDomainChanged    = (Asset->Domain             != NewDomain);
		const bool bBlendLocChanged  = (Asset->BlendableLocation  != NewBlendLoc);

		if (bFragChanged || bVertChanged || bDomainChanged || bBlendLocChanged)
		{
			Asset->Modify();
			Asset->FragmentShaderCode = NewFragment;
			Asset->VertexShaderCode   = NewVertex;
			Asset->Domain             = NewDomain;
			Asset->BlendableLocation  = NewBlendLoc;

			if (UPackage* Pkg = Asset->GetOutermost())
			{
				Pkg->SetDirtyFlag(true);
			}
		}

		// Always compile
		UMaterial* Mat = nullptr;
		FString Err;

		UE_LOG(LogTemp, Warning, TEXT("Compile start: FragChanged=%d VertChanged=%d DomainChanged=%d OutputMaterial=%s"),
			bFragChanged ? 1 : 0, bVertChanged ? 1 : 0, bDomainChanged ? 1 : 0, *GetNameSafe(Asset->OutputMaterial));

		if (CodeMat::CompileCodeAssetToMaterial(Asset, Mat, Err) && Mat)
		{
			UEditorAssetLibrary::SaveLoadedAsset(Mat,   /*bOnlyIfIsDirty*/ false);
			UEditorAssetLibrary::SaveLoadedAsset(Asset, /*bOnlyIfIsDirty*/ false);
			UE_LOG(LogTemp, Log, TEXT("[CodeMaterialAsset] Compile OK -> %s (%s)"),
				*GetNameSafe(Mat), *Mat->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CodeMaterialAsset] Compile failed: %s"), *Err);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Compile failed.\n\n%s"), *Err)));
		}
	}




	static bool IsWhitespaceOnly(const FString& S)
	{
		for (TCHAR C : S)
		{
			if (!FChar::IsWhitespace(C))
			{
				return false;
			}
		}
		return true;
	}





private:
	TObjectPtr<UCodeMaterialAsset> Asset = nullptr;

	TSharedPtr<SMultiLineEditableTextBox> FragmentTextBox;
	TSharedPtr<SMultiLineEditableTextBox> VertexTextBox;
	FString WorkingFragment;
	FString WorkingVertex;

	// Domain ドロップダウン
	TArray<TSharedPtr<FString>> DomainOptions;
	TSharedPtr<FString>         SelectedDomain;

	// BlendableLocation ドロップダウン
	TArray<TSharedPtr<FString>> BlendLocOptions;
	TSharedPtr<FString>         SelectedBlendLoc;
};

// ============================================================================
// FCodeMaterialAssetEditor
// ============================================================================

void FCodeMaterialAssetEditor::InitCodeMaterialAssetEditor(
	EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	TObjectPtr<UCodeMaterialAsset> InAsset
)
{
	check(InAsset);


	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShared<FCodeMaterialAssetEditorToolkit>();
	}

	Toolkit->Init(Mode, InitToolkitHost, InAsset);
}

#undef LOCTEXT_NAMESPACE
