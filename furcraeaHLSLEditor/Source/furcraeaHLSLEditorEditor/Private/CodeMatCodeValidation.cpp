// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/CodeMatCodeValidation.cpp

#include "CodeMatCodeValidation.h"
#include "Internationalization/Regex.h"

namespace
{

FString RemoveAllMatches(const FString& In, const TCHAR* Pattern)
{
    const FRegexPattern Pat(Pattern);
    FString Out;
    int32 LastEnd = 0;
    FRegexMatcher M(Pat, In);
    while (M.FindNext())
    {
        Out += In.Mid(LastEnd, M.GetMatchBeginning() - LastEnd);
        LastEnd = M.GetMatchEnding();
    }
    return Out + In.Mid(LastEnd);
}

FString StripComments(const FString& Code)
{
    return RemoveAllMatches(RemoveAllMatches(Code, TEXT("//[^\n]*")), TEXT("/\\*[\\s\\S]*?\\*/"));
}

/** HeadPat に一致するシグネチャ直後の { } 内側を取り出す */
bool ExtractBody(const FString& Stripped, const FRegexPattern& HeadPat, FString& OutBody, FString& OutError)
{
    FRegexMatcher HeadM(HeadPat, Stripped);
    if (!HeadM.FindNext())
    {
        OutError = TEXT("Entry function signature not found.");
        return false;
    }

    const int32 OpenBracePos = Stripped.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HeadM.GetMatchEnding());
    if (OpenBracePos == INDEX_NONE)
    {
        OutError = TEXT("Function body '{' not found.");
        return false;
    }

    int32 Depth = 0;
    for (int32 i = OpenBracePos; i < Stripped.Len(); ++i)
    {
        if (Stripped[i] == TEXT('{'))
        {
            ++Depth;
        }
        else if (Stripped[i] == TEXT('}') && --Depth == 0)
        {
            OutBody = Stripped.Mid(OpenBracePos + 1, i - OpenBracePos - 1);
            return true;
        }
    }
    OutError = TEXT("Function body '}' not found (brace mismatch).");
    return false;
}

bool ValidateEntry(const FString& Stripped, const TCHAR* HeadPattern, const TCHAR* SignatureHint, const TCHAR* Kind, FString& OutError)
{
    const FRegexPattern HeadPat(HeadPattern);
    if (!FRegexMatcher(HeadPat, Stripped).FindNext())
    {
        OutError = SignatureHint;
        return false;
    }
    FString Body;
    if (!ExtractBody(Stripped, HeadPat, Body, OutError)) return false;
    if (!Body.Contains(TEXT("return")))
    {
        OutError = FString::Printf(TEXT("%s shader must contain a return statement."), Kind);
        return false;
    }
    return true;
}

} // namespace

namespace CodeMatValidation
{

FString NormalizeCode(const FString& In)
{
    FString Out = In;
    Out.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    Out.ReplaceInline(TEXT("\r"), TEXT("\n"));

    TArray<FString> Lines;
    Out.ParseIntoArray(Lines, TEXT("\n"), false);
    for (FString& Line : Lines) Line = Line.TrimEnd();

    Out = FString::Join(Lines, TEXT("\n"));
    while (Out.EndsWith(TEXT("\n"))) Out.LeftChopInline(1);
    return Out + TEXT("\n");
}

bool IsWhitespaceOnly(const FString& S)
{
    for (TCHAR C : S)
        if (!FChar::IsWhitespace(C)) return false;
    return true;
}

bool ValidateFragmentCode(const FString& Code, FString& OutError)
{
    if (IsWhitespaceOnly(Code))
    {
        OutError = TEXT("Fragment shader code is empty.");
        return false;
    }
    return ValidateEntry(StripComments(Code),
        TEXT("\\bfloat[34]\\s+\\w+\\s*\\(\\s*float[23]\\s+[A-Za-z_][A-Za-z0-9_]*"),
        TEXT("Entry signature must be: float3/float4 <FuncName>(float2 uv, ...) or float3/float4 <FuncName>(float3 WorldPos, ...)"),
        TEXT("Fragment"), OutError);
}

bool ValidateVertexCode(const FString& Code, FString& OutError)
{
    const FString Stripped = StripComments(Code);
    if (IsWhitespaceOnly(Stripped)) return true; // 全てコメント → WPO 無効

    // 関数名は任意 (VertexShader は FXC 予約語なので WPO_Main 等を推奨)
    return ValidateEntry(Stripped,
        TEXT("\\bfloat3\\s+\\w+\\s*\\(\\s*float3\\s+[A-Za-z_][A-Za-z0-9_]*"),
        TEXT("Vertex entry signature must be: float3 <FuncName>(float3 VertexNormal, ...)"),
        TEXT("Vertex"), OutError);
}

} // namespace CodeMatValidation
