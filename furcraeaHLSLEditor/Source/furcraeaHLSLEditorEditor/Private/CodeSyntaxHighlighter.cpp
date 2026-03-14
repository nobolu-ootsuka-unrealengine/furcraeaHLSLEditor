// furcraeaHLSLEditorEditor/Private/CodeSyntaxHighlighter.cpp

#include "CodeSyntaxHighlighter.h"

#if WITH_FANCY_TEXT

#include "Framework/Text/SyntaxTokenizer.h"
#include "Framework/Text/SyntaxHighlighterTextLayoutMarshaller.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/IRun.h"            // FRunInfo

#include "Styling/AppStyle.h"
#include "Internationalization/Regex.h"

namespace CodeHL
{
	// ルール：Regex + 適用するスタイル名
	struct FRule
	{
		FRegexPattern Pattern;
		FName StyleName;

		FRule(const TCHAR* InPattern, FName InStyleName)
			: Pattern(InPattern)
			, StyleName(InStyleName)
		{
		}
	};

	// Tokenizer は「行構造だけ作る」（1行1トークン）
	class FRegexTokenizer : public ISyntaxTokenizer
	{
	public:
		TArray<FRule> Rules;

		virtual void Process(TArray<FTokenizedLine>& OutTokenizedLines, const FString& Input) override
		{
			OutTokenizedLines.Reset();

			TArray<FString> Lines;
			Input.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);

			int32 GlobalOffset = 0;

			for (const FString& Line : Lines)
			{
				FTokenizedLine& TL = OutTokenizedLines.AddDefaulted_GetRef();
				TL.Range = FTextRange(GlobalOffset, GlobalOffset + Line.Len());

				TL.Tokens.Add(ISyntaxTokenizer::FToken(
					ISyntaxTokenizer::ETokenType::Literal,
					TL.Range
				));

				GlobalOffset += Line.Len() + 1; // '\n'
			}
		}
	};

	// ---------------------------------------------------------------------

	class FRegexSyntaxHighlighterMarshaller : public FSyntaxHighlighterTextLayoutMarshaller
	{
	public:
		// ★ MakeShared で生成したいので ctor を public にする（ここが今回の肝）
		FRegexSyntaxHighlighterMarshaller(
			const TSharedRef<FRegexTokenizer>& InTokenizer,
			const TMap<FName, FTextBlockStyle>& InStyles
		)
			: FSyntaxHighlighterTextLayoutMarshaller(InTokenizer)
			, TokenizerStrong(InTokenizer)
			, Styles(InStyles)
		{
			EnableSyntaxHighlighting(true);
		}

		static TSharedRef<FRegexSyntaxHighlighterMarshaller> Create(
			const TSharedRef<FRegexTokenizer>& InTokenizer,
			const TMap<FName, FTextBlockStyle>& InStyles
		)
		{
			return MakeShared<FRegexSyntaxHighlighterMarshaller>(InTokenizer, InStyles);
		}

		virtual ~FRegexSyntaxHighlighterMarshaller() override = default;

	protected:
		virtual void ParseTokens(
			const FString& SourceString,
			FTextLayout& TargetTextLayout,
			TArray<ISyntaxTokenizer::FTokenizedLine> /*TokenizedLines*/
		) override
		{
			if (!IsSyntaxHighlightingEnabled())
			{
				FPlainTextLayoutMarshaller::SetText(SourceString, TargetTextLayout);
				return;
			}

			// Run を全部作り直す
			TargetTextLayout.ClearLines();

			TArray<FString> Lines;
			SourceString.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);

			const FTextBlockStyle* PlainStyle = Styles.Find("Plain");
			if (!PlainStyle)
			{
				static FTextBlockStyle Fallback = FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
				PlainStyle = &Fallback;
			}

			for (const FString& Line : Lines)
			{
				// Run はこの LineText を共有し、Range は「行内オフセット」で持つ
				TSharedRef<FString> LineText = MakeShared<FString>(Line);

				// 1行の中で「範囲→スタイル」を集める
				struct FSpan { int32 B; int32 E; FName Style; };
				TArray<FSpan> Spans;

				for (const FRule& R : TokenizerStrong->Rules)
				{
					FRegexMatcher M(R.Pattern, Line);
					while (M.FindNext())
					{
						const int32 B = M.GetMatchBeginning();
						const int32 E = M.GetMatchEnding();
						// Guard: ICU may return UTF-8 byte offsets on lines with
						// non-ASCII (CJK/Japanese) characters, producing
						// out-of-range FTextRange values that break AddLine.
						if (E > B && B >= 0 && E <= Line.Len())
						{
							Spans.Add({ B, E, R.StyleName });
						}
					}
				}

				// ソート：開始が早い順 / 同開始は長い方優先
				Spans.Sort([](const FSpan& A, const FSpan& B)
					{
						if (A.B != B.B) return A.B < B.B;
						return (A.E - A.B) > (B.E - B.B);
					});

				TArray<TSharedRef<IRun>> Runs;

				auto AddRun = [&](int32 B, int32 E, const FTextBlockStyle& Style)
					{
						if (E <= B) return;

						const FRunInfo Info(TEXT("CodeHL"));

						Runs.Add(
							FSlateTextRun::Create(
								Info,
								LineText,
								Style,
								FTextRange(B, E)
							)
						);
					};

				int32 Cursor = 0;

				for (const FSpan& S : Spans)
				{
					if (S.B < Cursor) continue;

					if (S.B > Cursor)
					{
						AddRun(Cursor, S.B, *PlainStyle);
					}

					const FTextBlockStyle* Style = Styles.Find(S.Style);
					AddRun(S.B, S.E, Style ? *Style : *PlainStyle);

					Cursor = S.E;
				}

				if (Cursor < Line.Len())
				{
					AddRun(Cursor, Line.Len(), *PlainStyle);
				}

				// 空行 (Enter直後等) でも Run が必要。空の場合は長さゼロのランを追加。
				if (Runs.IsEmpty())
				{
					const FRunInfo EmptyInfo(TEXT("CodeHL"));
					Runs.Add(FSlateTextRun::Create(EmptyInfo, LineText, *PlainStyle, FTextRange(0, 0)));
				}

				FTextLayout::FNewLineData NewLine(LineText, MoveTemp(Runs));
				TargetTextLayout.AddLine(NewLine);
			}
		}

	private:
		TSharedRef<FRegexTokenizer> TokenizerStrong;
		TMap<FName, FTextBlockStyle> Styles;
	};

} // namespace CodeHL

#endif // WITH_FANCY_TEXT

// ---------------------------------------------------------------------

TSharedRef<ITextLayoutMarshaller> FCodeSyntaxHighlighter::Create()
{
#if WITH_FANCY_TEXT
	// Base
	FTextBlockStyle Base = FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
	Base.SetFont(FAppStyle::GetFontStyle("MonoFont"));

	// VSCode Dark+ の “Foreground” っぽい（#D4D4D4）
	Base.SetColorAndOpacity(FSlateColor(FLinearColor(0.83f, 0.83f, 0.83f, 1.0f)));

	TMap<FName, FTextBlockStyle> Styles;
	Styles.Add("Plain", Base);

	auto MakeStyle = [&](float r, float g, float b)
		{
			FTextBlockStyle S = Base;
			S.SetColorAndOpacity(FSlateColor(FLinearColor(r, g, b, 1.0f)));
			return S;
		};

	// VSCode Dark+ 目安
	Styles.Add("Keyword", MakeStyle(0.10f, 0.33f, 0.10f)); // #C586C0
	Styles.Add("Type", MakeStyle(0.10f, 0.10f, 0.49f)); // #4EC9B0
	Styles.Add("Number", MakeStyle(0.51f, 0.10f, 0.46f)); // #B5CEA8
	Styles.Add("Comment", MakeStyle(0.32f, 0.10f, 0.23f)); // #6A9955
	Styles.Add("Function", MakeStyle(0.10f, 0.74f, 0.29f)); // #DCDCAA (関数名っぽい)
	Styles.Add("Parameter", MakeStyle(0.41f, 0.57f, 0.10f)); // #9CDCFE (uv みたいな識別子)

	// Tokenizer
	TSharedRef<CodeHL::FRegexTokenizer> Tokenizer = MakeShared<CodeHL::FRegexTokenizer>();

	// 1) コメント（最優先）
	Tokenizer->Rules.Add({ TEXT("//.*$"), "Comment" });
	Tokenizer->Rules.Add({ TEXT("/\\*[\\s\\S]*?\\*/"), "Comment" }); // ブロックコメントも一応

	// 2) 文字列（必要なら。今はHLSLで""使うなら）
	Tokenizer->Rules.Add({ TEXT("\"([^\"\\\\]|\\\\.)*\""), "Number" }); // 仮：文字列色は好み。あとで "String" 追加推奨

	// 3) 制御語（Keyword）
	Tokenizer->Rules.Add({ TEXT("\\b(return|if|else|for|while|do|break|continue|discard)\\b"), "Keyword" });

	// 4) 型（Type）
	Tokenizer->Rules.Add({ TEXT("\\b(void|bool|int|uint|half|float|double|float2|float3|float4|half2|half3|half4|int2|int3|int4)\\b"), "Type" });

	// 5) 数字（Number）
	Tokenizer->Rules.Add({ TEXT("\\b[0-9]+(\\.[0-9]+)?([eE][\\+\\-]?[0-9]+)?\\b"), "Number" });

	// 6) 関数っぽいもの（識別子の直後に '(' が来る）
	Tokenizer->Rules.Add({ TEXT("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()"), "Function" });

	// 7) 引数名っぽいもの（Main(float2 uv) の uv を狙い撃ち）
	Tokenizer->Rules.Add({ TEXT("\\bMain\\s*\\(\\s*float2\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"), "Parameter" });
	// ↑このルールは “マッチ全体” が色付けされる実装だと uv だけに当たらない可能性あり。
	// もしそうなら Parameter はいったん外してOK（Function/Type/Keyword/Comment/NumberだけでVSCode感は出る）

	return CodeHL::FRegexSyntaxHighlighterMarshaller::Create(Tokenizer, Styles);
#else
	return FPlainTextLayoutMarshaller::Create();
#endif
}

