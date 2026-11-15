// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatHLSLSource.cpp

#include "CodeMatHLSLSource.h"

#if WITH_EDITOR

#include "Internationalization/Regex.h"

namespace CodeMatSource
{

FString SafeifyHLSL(const FString& Code)
{
    // FXC / DXC は VertexShader 等の Effects キーワードを関数名に使うと "unexpected token" で失敗する。
    static const TPair<const TCHAR*, const TCHAR*> Renames[] = {
        { TEXT("VertexShader"),   TEXT("WPO_Main") },
        { TEXT("PixelShader"),    TEXT("PS_Main")  },
        { TEXT("GeometryShader"), TEXT("GS_Main")  },
        { TEXT("HullShader"),     TEXT("HS_Main")  },
        { TEXT("DomainShader"),   TEXT("DS_Main")  },
        { TEXT("ComputeShader"),  TEXT("CS_Main")  },
    };
    FString Result = Code;
    for (const auto& KV : Renames)
        Result.ReplaceInline(KV.Key, KV.Value, ESearchCase::CaseSensitive);
    return Result;
}

FString StripLineComments(const FString& Code)
{
    const FRegexPattern Pat(TEXT("//[^\n]*"));
    FString Out;
    int32 LastEnd = 0;
    FRegexMatcher M(Pat, Code);
    while (M.FindNext())
    {
        Out += Code.Mid(LastEnd, M.GetMatchBeginning() - LastEnd);
        LastEnd = M.GetMatchEnding();
    }
    Out += Code.Mid(LastEnd);
    return Out;
}

FString InjectHLSLBuiltins(const FString& Body)
{
    if (FRegexMatcher(FRegexPattern(TEXT("\\bTime\\b")), Body).FindNext() &&
        !FRegexMatcher(FRegexPattern(TEXT("\\bfloat\\s+Time\\b")), Body).FindNext())
    {
        return TEXT("float Time = View.GameTime;\n") + Body;
    }
    return Body;
}

FString DetectFunctionName(const FString& Code)
{
    FRegexMatcher M(FRegexPattern(TEXT("float[34]\\s+(\\w+)\\s*\\(")), StripLineComments(Code));
    TArray<FString> Names;
    while (M.FindNext())
        Names.Add(M.GetCaptureGroup(1).TrimStartAndEnd());
    if (Names.Num() == 0) return FString();

    static const TCHAR* KnownEntries[] = {
        TEXT("FragmentShader"), TEXT("MainHLSL"), TEXT("PS_Main"), TEXT("WPO_Main"), TEXT("MainWPO"),
    };
    for (const TCHAR* Known : KnownEntries)
        if (Names.Contains(Known)) return Known;
    return Names.Last();
}

int32 DetectReturnComponents(const FString& Code)
{
    return FRegexMatcher(FRegexPattern(TEXT("float4\\s+\\w+\\s*\\(")), StripLineComments(Code)).FindNext() ? 4 : 3;
}

FString ExtractFunctionBody(const FString& Code, const FString& FuncName)
{
    FRegexMatcher M(FRegexPattern(FString::Printf(TEXT("\\b%s\\b[^{]*\\{"), *FuncName)), Code);
    if (!M.FindNext()) return FString();

    const int32 OpenIdx = M.GetMatchEnding() - 1; // '{'
    int32 Depth = 1;
    int32 Pos   = OpenIdx + 1;
    while (Pos < Code.Len() && Depth > 0)
    {
        const TCHAR Ch = Code[Pos];
        if      (Ch == TEXT('{')) ++Depth;
        else if (Ch == TEXT('}')) --Depth;
        if (Depth > 0) ++Pos;
    }
    if (Depth != 0) return FString();
    return Code.Mid(OpenIdx + 1, Pos - OpenIdx - 1);
}

FString ExtractHelperFunctions(const FString& Code, const FString& MainFuncName)
{
    FRegexMatcher MOrig(FRegexPattern(FString::Printf(TEXT("\\bfloat[34]\\s+%s\\s*\\("), *MainFuncName)), Code);
    if (!MOrig.FindNext()) return FString();

    const FString Before = Code.Left(MOrig.GetMatchBeginning()).TrimEnd();
    if (Before.IsEmpty()) return FString();

    // コメント (// @param 等) だけなら空扱い
    FRegexMatcher FuncM(FRegexPattern(TEXT("[a-zA-Z_]\\w*\\s+[a-zA-Z_]\\w*\\s*\\(")), StripLineComments(Before));
    return FuncM.FindNext() ? Before : FString();
}

TArray<TPair<FString, FString>> ExtractFuncParams(const FString& Code, const FString& FuncName)
{
    TArray<TPair<FString, FString>> Result;
    // コメント内のシグネチャ例にマッチしないようにコメントを除去してから検索する
    FRegexMatcher M(FRegexPattern(FString::Printf(TEXT("\\b%s\\s*\\(([^)]*)\\)"), *FuncName)), StripLineComments(Code));
    if (!M.FindNext()) return Result;

    const FString ParamList = M.GetCaptureGroup(1).TrimStartAndEnd();
    if (ParamList.IsEmpty()) return Result;

    TArray<FString> Parts;
    ParamList.ParseIntoArray(Parts, TEXT(","));
    for (FString& Part : Parts)
    {
        TArray<FString> Tokens;
        Part.TrimStartAndEnd().ParseIntoArrayWS(Tokens);
        if (Tokens.Num() >= 2)
            Result.Add(TPair<FString, FString>(Tokens[0], Tokens.Last()));
    }
    return Result;
}

TArray<FParamDecl> ParseParamDecls(const FString& Code)
{
    TArray<FParamDecl> Params;

    // float / float3 / float4
    {
        FRegexMatcher M(FRegexPattern(TEXT("//\\s*@param\\s+(float[34]?)\\s+(\\w+)\\s*=\\s*([^\\n]+)")), Code);
        while (M.FindNext())
        {
            const FString TypeStr  = M.GetCaptureGroup(1).TrimStartAndEnd();
            const FString ValueStr = M.GetCaptureGroup(3).TrimStartAndEnd();

            FParamDecl P;
            P.Name = M.GetCaptureGroup(2).TrimStartAndEnd();
            if (TypeStr == TEXT("float"))
            {
                P.Type          = FParamDecl::EType::Scalar;
                P.DefaultScalar = FCString::Atof(*ValueStr);
            }
            else
            {
                P.Type = (TypeStr == TEXT("float4")) ? FParamDecl::EType::Vector4 : FParamDecl::EType::Vector3;

                TArray<FString> Parts;
                ValueStr.ParseIntoArray(Parts, TEXT(","));
                auto Comp = [&](int32 i) { return Parts.IsValidIndex(i) ? FCString::Atof(*Parts[i].TrimStartAndEnd()) : 0.f; };
                P.DefaultColor = FLinearColor(Comp(0), Comp(1), Comp(2), Parts.Num() >= 4 ? Comp(3) : 1.f);
            }
            Params.Add(P);
        }
    }

    // Texture2D
    {
        FRegexMatcher M(FRegexPattern(TEXT("//\\s*@param\\s+Texture2D\\s+(\\w+)(?:\\s*=\\s*([^\\n]+))?")), Code);
        while (M.FindNext())
        {
            FParamDecl P;
            P.Type        = FParamDecl::EType::Texture2D;
            P.Name        = M.GetCaptureGroup(1).TrimStartAndEnd();
            P.TexturePath = M.GetCaptureGroup(2).TrimStartAndEnd();
            Params.Add(P);
        }
    }
    return Params;
}

} // namespace CodeMatSource

#endif // WITH_EDITOR
