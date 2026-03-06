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
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

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
static const FName CodeTabId(TEXT("CodeMaterialAssetEditor_CodeTab"));

/**
 * ここは「Assetを開いたときに出る独自エディタ」本体。
 * - SMultiLineEditableTextBox で UCodeMaterialAsset::Code を編集
 * - Save でアセットに書き戻して Dirty 付ける
 * - ついでに GC に落ちないよう参照保持
 */
class FCodeMaterialAssetEditorToolkit : public FAssetEditorToolkit, public FGCObject
{
public:
	FCodeMaterialAssetEditorToolkit() = default;
	virtual ~FCodeMaterialAssetEditorToolkit() override = default;

	void Init(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, TObjectPtr<UCodeMaterialAsset> InAsset)
	{
		Asset = InAsset;
		check(Asset);

		// レイアウト
		const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
			FTabManager::NewLayout("CodeMaterialAssetEditor_Layout_v1")
			->AddArea
			(
				FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(CodeTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
			);

		// ツールキット初期化
		FAssetEditorToolkit::InitAssetEditor(
			Mode,
			InitToolkitHost,
			CodeMaterialAssetEditorAppIdentifier,
			StandaloneDefaultLayout,
			/*bCreateDefaultStandaloneMenu=*/true,
			/*bCreateDefaultToolbar=*/true,
			Asset.Get()
		);

		// ツールバー拡張を入れてから再生成
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
			CodeTabId,
			FOnSpawnTab::CreateRaw(this, &FCodeMaterialAssetEditorToolkit::SpawnCodeTab)
		)
			.SetDisplayName(LOCTEXT("CodeTabTitle", "Code"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	}

	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override
	{
		InTabManager->UnregisterTabSpawner(CodeTabId);
		FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	}

	// ------------------------------
	// FGCObject overrides
	// ------------------------------

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		// UE5.7 推奨：TObjectPtr をそのまま渡す
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
	TSharedRef<SDockTab> SpawnCodeTab(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == CodeTabId);
		check(Asset);

		const FText InitialText = FText::FromString(Asset->HlslCode);

		// Marshaller は TSharedRef のまま使う（TSharedPtrに落とす必要なし）
		TSharedRef<ITextLayoutMarshaller> HighlighterRef = FCodeSyntaxHighlighter::Create();

		SAssignNew(CodeTextBox, SMultiLineEditableTextBox)
			.Text(InitialText)
			.Font(FAppStyle::GetFontStyle("MonoFont"))
			.Marshaller(HighlighterRef)
			.AlwaysShowScrollbars(true)
			.AutoWrapText(false)
			.OnTextChanged(this, &FCodeMaterialAssetEditorToolkit::OnCodeTextChanged);
		// .ForegroundColor(...) 3つは一旦消す

		check(CodeTextBox.IsValid());

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(8.0f)
					[
						CodeTextBox.ToSharedRef()
					]
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

	void OnCodeTextChanged(const FText& NewText)
	{
		WorkingText = NewText.ToString();
	}

	bool CanSave() const
	{
		return Asset != nullptr;
	}

	static FString GetDefaultTemplateCode()
	{
		return TEXT(
			R"(// Minimal template
float3 MainHLSL(float2 uv)
{
	// uv: 0..1
	return float3(uv.x, uv.y, 0.0);
}
)");
	}


	static bool ValidateCodeForSave(const FString& Code, FString& OutError)
	{
		// ★ ここで宣言（スコープを関数全体に）
		FString Stripped = Code;

		// 1) empty
		if (Code.IsEmpty() || IsWhitespaceOnly(Code))
		{
			OutError = TEXT("Code is empty.");
			return false;
		}

		// 2) コメント除去（← あなたの既存コードをそのまま）
		{
			// 行コメント //
			FRegexPattern Pat(TEXT("//.*?$"));
			FRegexMatcher M(Pat, Stripped);
			while (M.FindNext())
			{
				const int32 Begin = M.GetMatchBeginning();
				const int32 End = M.GetMatchEnding();
				Stripped.RemoveAt(Begin, End - Begin);
				M = FRegexMatcher(Pat, Stripped);
			}
		}
		{
			// ブロックコメント /* */
			FRegexPattern Pat(TEXT("/\\*.*?\\*/"));
			FRegexMatcher M(Pat, Stripped);
			while (M.FindNext())
			{
				const int32 Begin = M.GetMatchBeginning();
				const int32 End = M.GetMatchEnding();
				Stripped.RemoveAt(Begin, End - Begin);
				M = FRegexMatcher(Pat, Stripped);
			}
		}

		// 3) Entry signature
		const FRegexPattern EntryPat(
			TEXT("\\bfloat3\\s+MainHLSL\\s*\\(\\s*float2\\s+[A-Za-z_][A-Za-z0-9_]*")
		);
		FRegexMatcher EntryM(EntryPat, Stripped);
		if (!EntryM.FindNext())
		{
			OutError = TEXT("Entry signature must be: float3 MainHLSL(float2 uv, ...");
			return false;
		}

		// 4) Main body 抽出
		FString MainBody;
		FString MainErr;
		if (!ExtractMainBody(Stripped, MainBody, MainErr))
		{
			OutError = MainErr;
			return false;
		}

		// 5) return チェック
		if (!MainBody.Contains(TEXT("return")))
		{
			OutError = TEXT("MainHLSL must contain a return statement.");
			return false;
		}

		return true;
	}


	static bool ExtractMainBody(const FString& Stripped, FString& OutBody, FString& OutError)
	{
		// Main( ... ) の後ろの '{' を探す
		const FRegexPattern MainHeadPat(TEXT("\\bfloat3\\s+MainHLSL\\s*\\(\\s*float2\\s+[A-Za-z_][A-Za-z0-9_]*"));
		FRegexMatcher HeadM(MainHeadPat, Stripped);
		if (!HeadM.FindNext())
		{
			OutError = TEXT("Entry signature must be: float3 MainHLSL(float2 uv, ...");
			return false;
		}

		const int32 HeadEnd = HeadM.GetMatchEnding();

		// '{' を探す（MainHLSL宣言の後）
		const int32 OpenBracePos = Stripped.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HeadEnd);
		if (OpenBracePos == INDEX_NONE)
		{
			OutError = TEXT("MainHLSL body '{' not found.");
			return false;
		}

		// 対応する '}' をネスト込みで探す
		int32 Depth = 0;
		for (int32 i = OpenBracePos; i < Stripped.Len(); ++i)
		{
			const TCHAR C = Stripped[i];
			if (C == '{') Depth++;
			else if (C == '}')
			{
				Depth--;
				if (Depth == 0)
				{
					// '{' と '}' の中身
					const int32 BodyBegin = OpenBracePos + 1;
					const int32 BodyLen = i - BodyBegin;
					OutBody = Stripped.Mid(BodyBegin, BodyLen);
					return true;
				}
			}
		}

		OutError = TEXT("MainHLSL body '}' not found (brace mismatch inside MainHLSL).");
		return false;
	}


	static FString NormalizeCodeForSave(const FString& In)
	{
		// 1) 改行コード統一（CRLF / CR → LF）
		FString Out = In;
		Out.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Out.ReplaceInline(TEXT("\r"), TEXT("\n"));

		// 2) 行ごとに末尾空白を削る
		TArray<FString> Lines;
		Out.ParseIntoArray(Lines, TEXT("\n"), false);

		for (FString& Line : Lines)
		{
			Line = Line.TrimEnd();
		}

		// 3) 再結合
		Out = FString::Join(Lines, TEXT("\n"));

		// 4) ファイル末尾に必ず改行を 1 つだけ付ける
		while (Out.EndsWith(TEXT("\n")))
		{
			Out.LeftChopInline(1);
		}
		Out += TEXT("\n");

		return Out;
	}



	bool CanReset() const
	{
		return true; // 常に押せてOK
	}

	void OnClickedReset()
	{
		const EAppReturnType::Type Ret = FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("ResetConfirm", "Reset the text to the default template?\n\n(Unsaved edits in the editor will be lost.)")
		);

		if (Ret != EAppReturnType::Yes)
		{
			return;
		}

		const FString Template = GetDefaultTemplateCode();

		WorkingText = Template;

		if (CodeTextBox.IsValid())
		{
			CodeTextBox->SetText(FText::FromString(Template));
		}
	}




	void OnClickedSave()
	{
		UE_LOG(LogTemp, Warning, TEXT("OnClickedSave() CALLED"));

		if (!Asset) return;

		FString NewCode = CodeTextBox.IsValid()
			? CodeTextBox->GetText().ToString()
			: WorkingText;

		NewCode = NormalizeCodeForSave(NewCode);

		FString Error;
		if (!ValidateCodeForSave(NewCode, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CodeMaterialAsset] Save blocked: %s"), *Error);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Save blocked.\n\n%s"), *Error)));
			return;
		}

		const bool bCodeChanged = (Asset->HlslCode != NewCode);

		if (bCodeChanged)
		{
			Asset->Modify();
			Asset->HlslCode = NewCode;

			if (UPackage* Pkg = Asset->GetOutermost())
			{
				Pkg->SetDirtyFlag(true);
				UE_LOG(LogTemp, Log, TEXT("[CodeMaterialAsset] Saved."));
			}
		}

		// ★ここが重要：コードが変わってなくても、OutputMaterial が無ければ生成する
		//const bool bNeedCompile = bCodeChanged || (Asset->OutputMaterial == nullptr);
		//一番雑で確実なのはこれ：
		const bool bNeedCompile = true;

		if (bNeedCompile)
		{
			UMaterial* Mat = nullptr;
			FString Err;

			UE_LOG(LogTemp, Warning, TEXT("Compile start: bCodeChanged=%d OutputMaterial=%s"),
				bCodeChanged ? 1 : 0, *GetNameSafe(Asset->OutputMaterial));

			if (CodeMat::CompileCodeAssetToMaterial(Asset, Mat, Err) && Mat)
			{
				UEditorAssetLibrary::SaveLoadedAsset(Mat, /*bOnlyIfIsDirty*/ false);
				UEditorAssetLibrary::SaveLoadedAsset(Asset, /*bOnlyIfIsDirty*/ false);
				UE_LOG(LogTemp, Warning, TEXT("Compile OK -> Saved Mat=%s Path=%s"),
					*GetNameSafe(Mat), *Mat->GetPathName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Compile failed: %s"), *Err);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Skip compile: no changes and OutputMaterial exists."));
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

	TSharedPtr<SMultiLineEditableTextBox> CodeTextBox;
	FString WorkingText;
};

// ============================================================================
// FCodeMaterialAssetEditor (外側ラッパ)
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
