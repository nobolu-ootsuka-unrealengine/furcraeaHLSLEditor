// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatHLSLParser.cpp
//
// HLSL サブセットの字句解析・構文解析 (AST 生成)。ノード化は CodeMatHLSLToNodes.cpp が担当する。

#include "CodeMatHLSLParser.h"

#if WITH_EDITOR

namespace CodeMatHLSL
{
// ============================================================================
// Lexer
// ============================================================================

enum class ETokKind : uint8 { Ident, Number, Punct, End };

struct FTok
{
    ETokKind Kind = ETokKind::End;
    FString  S;
    double   Num  = 0.0;
    int32    Line = 1;
};

/** コメントと # 行を除去する。改行は残すので行番号は元コードと一致する。 */
static FString StripCommentsAndPreproc(const FString& In)
{
    FString Out;
    Out.Reserve(In.Len());
    const int32 L = In.Len();
    int32 i = 0;
    bool bLineStart = true;
    while (i < L)
    {
        const TCHAR c = In[i];
        if (c == TEXT('/') && i + 1 < L && In[i + 1] == TEXT('/'))
        {
            while (i < L && In[i] != TEXT('\n')) ++i;
            continue;
        }
        if (c == TEXT('/') && i + 1 < L && In[i + 1] == TEXT('*'))
        {
            i += 2;
            while (i + 1 < L && !(In[i] == TEXT('*') && In[i + 1] == TEXT('/')))
            {
                if (In[i] == TEXT('\n')) Out += TEXT('\n');
                ++i;
            }
            i += 2;
            continue;
        }
        if (c == TEXT('#') && bLineStart)
        {
            while (i < L && In[i] != TEXT('\n')) ++i;
            continue;
        }
        if (c == TEXT('\n'))                bLineStart = true;
        else if (!FChar::IsWhitespace(c))   bLineStart = false;
        Out += c;
        ++i;
    }
    return Out;
}

static bool Tokenize(const FString& Src, TArray<FTok>& Out, FString& OutErr)
{
    static const TCHAR* TwoCharOps[] = {
        TEXT("+="), TEXT("-="), TEXT("*="), TEXT("/="), TEXT("%="),
        TEXT("=="), TEXT("!="), TEXT("<="), TEXT(">="),
        TEXT("&&"), TEXT("||"), TEXT("++"), TEXT("--"), TEXT("::"),
    };
    static const TCHAR* OneCharOps = TEXT("+-*/%=<>!?:;,.(){}[]&|^~");

    const int32 L = Src.Len();
    int32 i = 0, Line = 1;
    while (i < L)
    {
        const TCHAR c = Src[i];
        if (c == TEXT('\n')) { ++Line; ++i; continue; }
        if (FChar::IsWhitespace(c)) { ++i; continue; }

        FTok T;
        T.Line = Line;

        if (FChar::IsAlpha(c) || c == TEXT('_'))
        {
            const int32 S = i;
            while (i < L && (FChar::IsAlnum(Src[i]) || Src[i] == TEXT('_'))) ++i;
            T.Kind = ETokKind::Ident;
            T.S    = Src.Mid(S, i - S);
            Out.Add(T);
            continue;
        }

        if (FChar::IsDigit(c) || (c == TEXT('.') && i + 1 < L && FChar::IsDigit(Src[i + 1])))
        {
            const int32 S = i;
            if (c == TEXT('0') && i + 1 < L && (Src[i + 1] == TEXT('x') || Src[i + 1] == TEXT('X')))
            {
                i += 2;
                const int32 HS = i;
                while (i < L && FChar::IsHexDigit(Src[i])) ++i;
                T.Num = (double)FParse::HexNumber(*Src.Mid(HS, i - HS));
            }
            else
            {
                while (i < L && (FChar::IsDigit(Src[i]) || Src[i] == TEXT('.'))) ++i;
                if (i < L && (Src[i] == TEXT('e') || Src[i] == TEXT('E')))
                {
                    ++i;
                    if (i < L && (Src[i] == TEXT('+') || Src[i] == TEXT('-'))) ++i;
                    while (i < L && FChar::IsDigit(Src[i])) ++i;
                }
                T.Num = FCString::Atod(*Src.Mid(S, i - S));
            }
            while (i < L && FCString::Strchr(TEXT("fFhHuUlL"), Src[i])) ++i;
            T.Kind = ETokKind::Number;
            T.S    = Src.Mid(S, i - S);
            Out.Add(T);
            continue;
        }

        if (i + 1 < L)
        {
            const FString Two = Src.Mid(i, 2);
            bool bTwo = false;
            for (const TCHAR* Op : TwoCharOps) { if (Two == Op) { bTwo = true; break; } }
            if (bTwo)
            {
                T.Kind = ETokKind::Punct;
                T.S    = Two;
                Out.Add(T);
                i += 2;
                continue;
            }
        }

        if (FCString::Strchr(OneCharOps, c))
        {
            T.Kind = ETokKind::Punct;
            T.S    = FString::Chr(c);
            Out.Add(T);
            ++i;
            continue;
        }

        OutErr = FString::Printf(TEXT("line %d: unexpected character '%c'"), Line, c);
        return false;
    }

    FTok End;
    End.Kind = ETokKind::End;
    End.Line = Line;
    Out.Add(End);
    return true;
}

// ============================================================================
// Types
// ============================================================================

/** "float3" → 3, "float" → 1, "float3x3" → -1 (行列は未対応), 型でなければ 0 */
int32 TypeComponents(const FString& S)
{
    static const TCHAR* Bases[] = {
        TEXT("MaterialFloat"), TEXT("MaterialHalf"),
        TEXT("min16float"), TEXT("min10float"), TEXT("min16int"), TEXT("min16uint"),
        TEXT("float"), TEXT("half"), TEXT("double"), TEXT("int"), TEXT("uint"), TEXT("bool"),
    };
    for (const TCHAR* B : Bases)
    {
        if (!S.StartsWith(B, ESearchCase::CaseSensitive)) continue;
        const FString Rest = S.RightChop(FCString::Strlen(B));
        if (Rest.IsEmpty()) return 1;
        if (Rest.Len() == 1 && Rest[0] >= TEXT('1') && Rest[0] <= TEXT('4')) return Rest[0] - TEXT('0');
        if (Rest.Len() == 3 && Rest[1] == TEXT('x')) return -1;
        return 0;
    }
    return 0;
}

bool IsTextureType(const FString& S)
{
    return S == TEXT("Texture2D") || S == TEXT("Texture2DArray") || S == TEXT("TextureCube") || S == TEXT("Texture3D");
}

bool IsSamplerType(const FString& S)
{
    return S == TEXT("SamplerState") || S == TEXT("SamplerComparisonState");
}

bool IsIntType(const FString& S)
{
    return S.StartsWith(TEXT("int")) || S.StartsWith(TEXT("uint")) || S.StartsWith(TEXT("min16int")) || S.StartsWith(TEXT("min16uint"));
}

bool IsTypeName(const FString& S)
{
    return TypeComponents(S) != 0 || IsTextureType(S) || IsSamplerType(S) || S == TEXT("void");
}

static bool IsModifier(const FString& S)
{
    static const TCHAR* Mods[] = {
        TEXT("static"), TEXT("const"), TEXT("inline"), TEXT("uniform"), TEXT("precise"),
        TEXT("in"), TEXT("out"), TEXT("inout"), TEXT("nointerpolation"), TEXT("linear"),
        TEXT("centroid"), TEXT("row_major"), TEXT("column_major"), TEXT("volatile"),
    };
    for (const TCHAR* M : Mods) { if (S == M) return true; }
    return false;
}

// ============================================================================
// Parser
// ============================================================================

class FParser
{
public:
    explicit FParser(const TArray<FTok>& InToks) : Toks(InToks) {}

    FString Err;

    bool ParseProgram(TArray<FFunc>& OutFuncs, TArray<FStmtPtr>& OutGlobals)
    {
        while (!Failed() && Peek().Kind != ETokKind::End)
        {
            if (Accept(TEXT(";"))) continue;

            while (Peek().Kind == ETokKind::Ident && IsModifier(Peek().S)) ++P;

            if (IsId(TEXT("struct")) || IsId(TEXT("cbuffer")))
            {
                Fail(TEXT("struct / cbuffer is not supported in node conversion"));
                return false;
            }

            const int32 Line = Peek().Line;
            FString Type;
            if (!ExpectTypeName(Type)) return false;
            FString Name;
            if (!ExpectIdent(Name)) return false;

            if (IsP(TEXT("(")))
            {
                FFunc F;
                F.RetType = Type;
                F.Name    = Name;
                if (!ParseParams(F.Params)) return false;
                // セマンティクス (: SV_Target) を読み飛ばす
                if (Accept(TEXT(":"))) { FString Dummy; ExpectIdent(Dummy); }
                if (Accept(TEXT(";"))) continue; // プロトタイプ宣言
                if (!Expect(TEXT("{"))) return false;
                if (!ParseBlockBody(F.Body)) return false;
                OutFuncs.Add(MoveTemp(F));
            }
            else
            {
                // グローバル定数: static const float TAU = 6.28;
                FStmtPtr S = MakeShared<FStmt>();
                S->Kind = FStmt::EKind::Decl;
                S->Line = Line;
                S->Type = Type;
                if (!ParseDeclList(*S, Name)) return false;
                if (!Expect(TEXT(";"))) return false;
                OutGlobals.Add(S);
            }
        }
        return !Failed();
    }

private:
    const TArray<FTok>& Toks;
    int32 P = 0;

    const FTok& Peek(int32 O = 0) const { return Toks[FMath::Min(P + O, Toks.Num() - 1)]; }
    bool IsP(const TCHAR* S, int32 O = 0) const { const FTok& T = Peek(O); return T.Kind == ETokKind::Punct && T.S == S; }
    bool IsId(const TCHAR* S, int32 O = 0) const { const FTok& T = Peek(O); return T.Kind == ETokKind::Ident && T.S == S; }
    bool Accept(const TCHAR* S) { if (IsP(S)) { ++P; return true; } return false; }
    bool Failed() const { return !Err.IsEmpty(); }

    void Fail(const FString& M)
    {
        if (Err.IsEmpty()) Err = FString::Printf(TEXT("line %d: %s"), Peek().Line, *M);
    }

    bool Expect(const TCHAR* S)
    {
        if (Accept(S)) return true;
        Fail(FString::Printf(TEXT("'%s' expected but found '%s'"), S, *Peek().S));
        return false;
    }

    bool ExpectIdent(FString& Out)
    {
        if (Peek().Kind != ETokKind::Ident)
        {
            Fail(FString::Printf(TEXT("identifier expected but found '%s'"), *Peek().S));
            return false;
        }
        Out = Peek().S;
        ++P;
        return true;
    }

    bool ExpectTypeName(FString& Out)
    {
        if (Peek().Kind != ETokKind::Ident || !IsTypeName(Peek().S))
        {
            Fail(FString::Printf(TEXT("type name expected but found '%s'"), *Peek().S));
            return false;
        }
        Out = Peek().S;
        ++P;
        // テンプレート引数付き Texture2D<float4> を読み飛ばす
        if (IsTextureType(Out) && Accept(TEXT("<")))
        {
            while (!Failed() && Peek().Kind != ETokKind::End && !Accept(TEXT(">"))) ++P;
        }
        return true;
    }

    bool ParseParams(TArray<FParam>& Out)
    {
        if (!Expect(TEXT("("))) return false;
        if (Accept(TEXT(")"))) return true;
        if (IsId(TEXT("void")) && IsP(TEXT(")"), 1)) { P += 2; return true; }
        do
        {
            FParam Prm;
            while (Peek().Kind == ETokKind::Ident && IsModifier(Peek().S))
            {
                if (Peek().S == TEXT("out") || Peek().S == TEXT("inout")) Prm.bOut = true;
                ++P;
            }
            if (!ExpectTypeName(Prm.Type)) return false;
            if (!ExpectIdent(Prm.Name)) return false;
            if (IsP(TEXT("[")))
            {
                Fail(TEXT("array parameters are not supported"));
                return false;
            }
            if (Accept(TEXT(":"))) { FString Dummy; if (!ExpectIdent(Dummy)) return false; }
            if (Accept(TEXT("="))) { ParseExpr(); } // デフォルト引数は無視
            Out.Add(Prm);
        }
        while (Accept(TEXT(",")));
        return Expect(TEXT(")"));
    }

    bool ParseBlockBody(TArray<FStmtPtr>& Out)
    {
        while (!Failed() && !IsP(TEXT("}")))
        {
            if (Peek().Kind == ETokKind::End)
            {
                Fail(TEXT("'}' expected before end of code"));
                return false;
            }
            FStmtPtr S = ParseStmt();
            if (!S) return false;
            Out.Add(S);
        }
        return Expect(TEXT("}"));
    }

    bool ParseDeclList(FStmt& S, const FString& FirstName)
    {
        FString Name = FirstName;
        while (true)
        {
            if (IsP(TEXT("[")))
            {
                Fail(TEXT("arrays are not supported in node conversion"));
                return false;
            }
            FAstPtr Init;
            if (Accept(TEXT("=")))
            {
                if (IsP(TEXT("{")))
                {
                    Fail(TEXT("initializer lists { } are not supported"));
                    return false;
                }
                Init = ParseExpr();
                if (!Init) return false;
            }
            S.Decls.Add(TPair<FString, FAstPtr>(Name, Init));
            if (!Accept(TEXT(","))) break;
            if (!ExpectIdent(Name)) return false;
        }
        return true;
    }

    FStmtPtr NewStmt(FStmt::EKind K)
    {
        FStmtPtr S = MakeShared<FStmt>();
        S->Kind = K;
        S->Line = Peek().Line;
        return S;
    }

    FStmtPtr ParseStmt()
    {
        // [unroll] / [branch] などの属性
        while (IsP(TEXT("[")))
        {
            while (!Failed() && Peek().Kind != ETokKind::End && !Accept(TEXT("]"))) ++P;
        }

        if (IsP(TEXT(";")))
        {
            FStmtPtr S = NewStmt(FStmt::EKind::Empty);
            ++P;
            return S;
        }
        if (IsP(TEXT("{")))
        {
            FStmtPtr S = NewStmt(FStmt::EKind::Block);
            ++P;
            if (!ParseBlockBody(S->Body)) return nullptr;
            return S;
        }
        if (IsId(TEXT("return")))
        {
            FStmtPtr S = NewStmt(FStmt::EKind::Return);
            ++P;
            if (!IsP(TEXT(";")))
            {
                S->Value = ParseExpr();
                if (!S->Value) return nullptr;
            }
            if (!Expect(TEXT(";"))) return nullptr;
            return S;
        }
        if (IsId(TEXT("if")))
        {
            FStmtPtr S = NewStmt(FStmt::EKind::If);
            ++P;
            if (!Expect(TEXT("("))) return nullptr;
            S->Value = ParseExpr();
            if (!S->Value || !Expect(TEXT(")"))) return nullptr;
            S->Then = ParseStmt();
            if (!S->Then) return nullptr;
            if (IsId(TEXT("else")))
            {
                ++P;
                S->Else = ParseStmt();
                if (!S->Else) return nullptr;
            }
            return S;
        }
        if (IsId(TEXT("for")))
        {
            FStmtPtr S = NewStmt(FStmt::EKind::For);
            ++P;
            if (!Expect(TEXT("("))) return nullptr;
            if (!IsP(TEXT(";")))
            {
                S->Then = ParseSimpleStmt();
                if (!S->Then) return nullptr;
            }
            if (!Expect(TEXT(";"))) return nullptr;
            S->Value = ParseExpr();
            if (!S->Value || !Expect(TEXT(";"))) return nullptr;
            if (!IsP(TEXT(")")))
            {
                S->Else = ParseSimpleStmt();
                if (!S->Else) return nullptr;
            }
            if (!Expect(TEXT(")"))) return nullptr;
            FStmtPtr Body = ParseStmt();
            if (!Body) return nullptr;
            S->Body.Add(Body);
            return S;
        }
        static const TCHAR* Unsupported[] = {
            TEXT("while"), TEXT("do"), TEXT("switch"), TEXT("break"), TEXT("continue"), TEXT("discard"), TEXT("clip"),
        };
        for (const TCHAR* U : Unsupported)
        {
            if (IsId(U))
            {
                Fail(FString::Printf(TEXT("'%s' is not supported in node conversion"), U));
                return nullptr;
            }
        }

        FStmtPtr S = ParseSimpleStmt();
        if (!S) return nullptr;
        if (!Expect(TEXT(";"))) return nullptr;
        return S;
    }

    FStmtPtr ParseSimpleStmt()
    {
        while (IsId(TEXT("const")) || IsId(TEXT("static"))) ++P;

        // 宣言
        if (Peek().Kind == ETokKind::Ident && IsTypeName(Peek().S) && Peek(1).Kind == ETokKind::Ident)
        {
            FStmtPtr S = NewStmt(FStmt::EKind::Decl);
            S->Type = Peek().S;
            ++P;
            FString Name;
            if (!ExpectIdent(Name)) return nullptr;
            if (!ParseDeclList(*S, Name)) return nullptr;
            return S;
        }

        // 前置 ++ / --
        if (IsP(TEXT("++")) || IsP(TEXT("--")))
        {
            FStmtPtr S = NewStmt(FStmt::EKind::Assign);
            S->Op = IsP(TEXT("++")) ? TEXT("+=") : TEXT("-=");
            ++P;
            S->Target = ParsePostfix();
            if (!S->Target) return nullptr;
            S->Value = MakeNum(1.0);
            return S;
        }

        const int32 Line = Peek().Line;
        FAstPtr E = ParseExpr();
        if (!E) return nullptr;

        static const TCHAR* AssignOps[] = { TEXT("="), TEXT("+="), TEXT("-="), TEXT("*="), TEXT("/="), TEXT("%=") };
        for (const TCHAR* Op : AssignOps)
        {
            if (IsP(Op))
            {
                FStmtPtr S = MakeShared<FStmt>();
                S->Kind   = FStmt::EKind::Assign;
                S->Line   = Line;
                S->Op     = Op;
                S->Target = E;
                ++P;
                S->Value = ParseExpr();
                if (!S->Value) return nullptr;
                return S;
            }
        }
        if (IsP(TEXT("++")) || IsP(TEXT("--")))
        {
            FStmtPtr S = MakeShared<FStmt>();
            S->Kind   = FStmt::EKind::Assign;
            S->Line   = Line;
            S->Op     = IsP(TEXT("++")) ? TEXT("+=") : TEXT("-=");
            S->Target = E;
            S->Value  = MakeNum(1.0);
            ++P;
            return S;
        }

        FStmtPtr S = MakeShared<FStmt>();
        S->Kind  = FStmt::EKind::ExprS;
        S->Line  = Line;
        S->Value = E;
        return S;
    }

    FAstPtr MakeNum(double V)
    {
        FAstPtr A = MakeShared<FAst>();
        A->Kind = FAst::EKind::Num;
        A->Num  = V;
        A->Line = Peek().Line;
        return A;
    }

    FAstPtr MakeNode(FAst::EKind K, const FString& S, int32 Line)
    {
        FAstPtr A = MakeShared<FAst>();
        A->Kind = K;
        A->S    = S;
        A->Line = Line;
        return A;
    }

    FAstPtr ParseExpr() { return ParseTernary(); }

    FAstPtr ParseTernary()
    {
        FAstPtr C = ParseBinary(0);
        if (!C) return nullptr;
        if (IsP(TEXT("?")))
        {
            const int32 Line = Peek().Line;
            ++P;
            FAstPtr T = ParseExpr();
            if (!T || !Expect(TEXT(":"))) return nullptr;
            FAstPtr F = ParseTernary();
            if (!F) return nullptr;
            FAstPtr N = MakeNode(FAst::EKind::Ternary, TEXT("?"), Line);
            N->A = { C, T, F };
            return N;
        }
        return C;
    }

    static const TArray<TArray<FString>>& Levels()
    {
        static const TArray<TArray<FString>> L = {
            { TEXT("||") },
            { TEXT("&&") },
            { TEXT("|") },
            { TEXT("^") },
            { TEXT("&") },
            { TEXT("=="), TEXT("!=") },
            { TEXT("<"), TEXT(">"), TEXT("<="), TEXT(">=") },
            { TEXT("+"), TEXT("-") },
            { TEXT("*"), TEXT("/"), TEXT("%") },
        };
        return L;
    }

    FAstPtr ParseBinary(int32 Level)
    {
        if (Level >= Levels().Num()) return ParseUnary();
        FAstPtr L = ParseBinary(Level + 1);
        if (!L) return nullptr;
        while (true)
        {
            const FTok& T = Peek();
            if (T.Kind != ETokKind::Punct || !Levels()[Level].Contains(T.S)) break;
            const FString Op   = T.S;
            const int32   Line = T.Line;
            ++P;
            FAstPtr R = ParseBinary(Level + 1);
            if (!R) return nullptr;
            FAstPtr N = MakeNode(FAst::EKind::Binary, Op, Line);
            N->A = { L, R };
            L = N;
        }
        return L;
    }

    FAstPtr ParseUnary()
    {
        const int32 Line = Peek().Line;
        if (IsP(TEXT("-")) || IsP(TEXT("!")) || IsP(TEXT("~")))
        {
            const FString Op = Peek().S;
            ++P;
            FAstPtr X = ParseUnary();
            if (!X) return nullptr;
            FAstPtr N = MakeNode(FAst::EKind::Unary, Op, Line);
            N->A = { X };
            return N;
        }
        if (Accept(TEXT("+"))) return ParseUnary();

        // キャスト (float)x / (float3)x
        if (IsP(TEXT("(")) && Peek(1).Kind == ETokKind::Ident && IsTypeName(Peek(1).S) && IsP(TEXT(")"), 2))
        {
            const FString Type = Peek(1).S;
            P += 3;
            FAstPtr X = ParseUnary();
            if (!X) return nullptr;
            FAstPtr N = MakeNode(FAst::EKind::Call, Type, Line);
            N->A = { X };
            return N;
        }
        return ParsePostfix();
    }

    bool ParseArgs(TArray<FAstPtr>& Out)
    {
        if (!Expect(TEXT("("))) return false;
        if (Accept(TEXT(")"))) return true;
        do
        {
            FAstPtr A = ParseExpr();
            if (!A) return false;
            Out.Add(A);
        }
        while (Accept(TEXT(",")));
        return Expect(TEXT(")"));
    }

    FAstPtr ParsePostfix()
    {
        FAstPtr E = ParsePrimary();
        if (!E) return nullptr;
        while (true)
        {
            const int32 Line = Peek().Line;
            if (Accept(TEXT(".")))
            {
                FString Name;
                if (!ExpectIdent(Name)) return nullptr;
                if (IsP(TEXT("(")))
                {
                    FAstPtr N = MakeNode(FAst::EKind::Call, Name, Line);
                    N->Obj = E;
                    if (!ParseArgs(N->A)) return nullptr;
                    E = N;
                }
                else
                {
                    FAstPtr N = MakeNode(FAst::EKind::Member, Name, Line);
                    N->Obj = E;
                    E = N;
                }
            }
            else if (Accept(TEXT("[")))
            {
                FAstPtr Idx = ParseExpr();
                if (!Idx || !Expect(TEXT("]"))) return nullptr;
                FAstPtr N = MakeNode(FAst::EKind::Index, TEXT("[]"), Line);
                N->Obj = E;
                N->A   = { Idx };
                E = N;
            }
            else
            {
                break;
            }
        }
        return E;
    }

    FAstPtr ParsePrimary()
    {
        const FTok& T = Peek();
        if (T.Kind == ETokKind::Number)
        {
            FAstPtr N = MakeNode(FAst::EKind::Num, T.S, T.Line);
            N->Num = T.Num;
            ++P;
            return N;
        }
        if (T.Kind == ETokKind::Ident)
        {
            const FString Name = T.S;
            const int32   Line = T.Line;
            ++P;
            if (IsP(TEXT("(")))
            {
                FAstPtr N = MakeNode(FAst::EKind::Call, Name, Line);
                if (!ParseArgs(N->A)) return nullptr;
                return N;
            }
            return MakeNode(FAst::EKind::Ident, Name, Line);
        }
        if (Accept(TEXT("(")))
        {
            FAstPtr E = ParseExpr();
            if (!E || !Expect(TEXT(")"))) return nullptr;
            return E;
        }
        Fail(FString::Printf(TEXT("unexpected '%s'"), *T.S));
        return nullptr;
    }
};

bool ParseProgram(const FString& Source, TArray<FFunc>& OutFuncs, TArray<FStmtPtr>& OutGlobals, FString& OutError)
{
    TArray<FTok> Toks;
    if (!Tokenize(StripCommentsAndPreproc(Source), Toks, OutError)) return false;
    FParser Parser(Toks);
    if (!Parser.ParseProgram(OutFuncs, OutGlobals))
    {
        OutError = Parser.Err;
        return false;
    }
    return true;
}

} // namespace CodeMatHLSL

#endif // WITH_EDITOR
