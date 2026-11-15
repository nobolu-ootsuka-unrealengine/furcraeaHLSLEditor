// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatHLSLToNodes.cpp
//
// HLSL (サブセット) → マテリアルノード変換器の実装。
//
//   1. CodeMatHLSLParser で関数 / 文 / 式の AST を作る
//   2. エントリ関数を「シンボリック実行」する:
//        変数 = ノード出力 (または定数) への参照、演算 = ノード生成
//      if は両分岐を実行して変化した変数を If ノードで合成、for は定数回で展開、
//      ヘルパー関数はインライン展開する。
//   3. 定数畳み込み + 同一ノード共有 (CSE) でノード数を抑える。

#include "CodeMatHLSLToNodes.h"

#if WITH_EDITOR

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionUtils.h"
#include "Engine/Texture.h"
#include "Internationalization/Regex.h"
#include "UObject/UObjectGlobals.h"

#include "CodeMatHLSLParser.h"

using namespace CodeMatHLSL;

namespace CodeMatNodesPrivate
{
// ============================================================================
// Helpers
// ============================================================================

static bool ParseSwizzleString(const FString& S, TArray<int32>& Out)
{
    Out.Reset();
    if (S.Len() < 1 || S.Len() > 4) return false;
    for (TCHAR c : S)
    {
        int32 Idx = INDEX_NONE;
        switch (c)
        {
        case TEXT('x'): case TEXT('r'): Idx = 0; break;
        case TEXT('y'): case TEXT('g'): Idx = 1; break;
        case TEXT('z'): case TEXT('b'): Idx = 2; break;
        case TEXT('w'): case TEXT('a'): Idx = 3; break;
        default: return false;
        }
        Out.Add(Idx);
    }
    return true;
}

static UClass* FindExpressionClass(const TCHAR* ShortName)
{
    static TMap<FString, UClass*> Cache;
    if (UClass** Found = Cache.Find(ShortName)) return *Found;
    UClass* Cls = FindObject<UClass>(FTopLevelAssetPath(FName(TEXT("/Script/Engine")), FName(ShortName)));
    Cache.Add(ShortName, Cls);
    return Cls;
}

static void SetNumberProperty(UObject* Obj, const TCHAR* Name, float V)
{
    FProperty* Prop = FindFProperty<FProperty>(Obj->GetClass(), Name);
    if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))       FP->SetPropertyValue_InContainer(Obj, V);
    else if (FDoubleProperty* DP = CastField<FDoubleProperty>(Prop)) DP->SetPropertyValue_InContainer(Obj, (double)V);
    else if (FIntProperty* IP = CastField<FIntProperty>(Prop))       IP->SetPropertyValue_InContainer(Obj, (int32)V);
}

static FExpressionInput* FindInputPin(UMaterialExpression* E, const TCHAR* Pin)
{
    FStructProperty* SP = FindFProperty<FStructProperty>(E->GetClass(), Pin);
    if (!SP || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) return nullptr;
    return SP->ContainerPtrToValuePtr<FExpressionInput>(E);
}

static FString FormatFloatHLSL(float V)
{
    if (FMath::IsFinite(V) && FMath::Abs(V - FMath::RoundToFloat(V)) < 1e-6f && FMath::Abs(V) < 1e7f)
        return FString::Printf(TEXT("%d.0"), (int32)FMath::RoundToFloat(V));
    return FString::Printf(TEXT("%.9g"), V);
}

static float Frac(float X) { return X - FMath::FloorToFloat(X); }

} // namespace CodeMatNodesPrivate

using namespace CodeMatNodesPrivate;

// ============================================================================
// FImpl: シンボリック実行 + ノード生成
// ============================================================================

struct FCodeMatNodeBuilder::FImpl
{
    FNewExprFn      NewExpr;
    FResolveParamFn Resolve;

    TArray<FString> Notes;
    FString Err;
    int32   CurLine = 0;
    int32   NodeCount = 0;
    int32   InlineDepth = 0;

    TMap<FString, FFunc> Funcs;
    TMap<FString, int32> ExternTypes;
    TMap<FString, UMaterialExpression*> Cache;     // CSE

    struct FAppendInfo { FCodeMatValue A; FCodeMatValue B; };
    TMap<UMaterialExpression*, FAppendInfo> AppendParts;

    struct FVar
    {
        FCodeMatValue V;
        int32 N = 1;
        bool  bInt = false;
    };
    using FEnv = TMap<FString, FVar>;
    FEnv GlobalEnv;

    enum class EFlow : uint8 { Normal, Return, Error };

    static constexpr int32 MaxNodes = 4000;
    static constexpr float TwoPi    = 6.28318530717958647692f;

    // ------------------------------------------------------------------
    // エラー
    // ------------------------------------------------------------------

    FCodeMatValue Fail(const FString& Msg)
    {
        if (Err.IsEmpty())
            Err = CurLine > 0 ? FString::Printf(TEXT("line %d: %s"), CurLine, *Msg) : Msg;
        return FCodeMatValue();
    }

    bool Failed() const { return !Err.IsEmpty(); }

    // ------------------------------------------------------------------
    // 基本ユーティリティ
    // ------------------------------------------------------------------

    static bool SameValue(const FCodeMatValue& A, const FCodeMatValue& B)
    {
        if (A.N != B.N) return false;
        if (A.bConst && B.bConst)
        {
            for (int32 i = 0; i < A.N; ++i) if (A.C[i] != B.C[i]) return false;
            return true;
        }
        if (A.Cmp.IsValid() || B.Cmp.IsValid()) return A.Cmp == B.Cmp;
        return A.Expr && A.Expr == B.Expr && A.OutIdx == B.OutIdx;
    }

    static bool SameSource(const FCodeMatValue& A, const FCodeMatValue& B)
    {
        return A.Expr && A.Expr == B.Expr && A.OutIdx == B.OutIdx;
    }

    static float CompOf(const FCodeMatValue& V, int32 i) { return V.C[V.N == 1 ? 0 : FMath::Clamp(i, 0, 3)]; }

    static FCodeMatValue ConstVec(int32 N, const float* C)
    {
        FCodeMatValue R;
        R.bConst = true;
        R.N = N;
        for (int32 i = 0; i < 4; ++i) R.C[i] = (i < N) ? C[i] : (N == 1 ? C[0] : 0.f);
        return R;
    }

    UMaterialExpression* CreateNode(UClass* Cls)
    {
        if (!Cls) { Fail(TEXT("material expression class not found")); return nullptr; }
        if (++NodeCount > MaxNodes)
        {
            Fail(FString::Printf(TEXT("too many nodes (> %d). Simplify the shader or use Custom Node mode."), MaxNodes));
            return nullptr;
        }
        UMaterialExpression* E = NewExpr(Cls);
        if (!E) Fail(FString::Printf(TEXT("failed to create %s"), *Cls->GetName()));
        return E;
    }

    static FString KeyOf(const FCodeMatValue& M)
    {
        return FString::Printf(TEXT("%p:%d"), M.Expr, M.OutIdx);
    }

    // ------------------------------------------------------------------
    // 値 → ノード出力 (定数・比較は遅延生成)
    // ------------------------------------------------------------------

    FCodeMatValue Materialize(const FCodeMatValue& V)
    {
        if (Failed()) return FCodeMatValue();
        if (V.Expr) return V;
        if (V.bConst) return ConstNode(V);
        if (V.Cmp.IsValid()) return SelectCmp(*V.Cmp, FCodeMatValue::MakeConst(1.f), FCodeMatValue::MakeConst(0.f));
        if (V.bOpaque) return Fail(FString::Printf(TEXT("unknown identifier '%s'"), *V.Text));
        return Fail(TEXT("invalid value"));
    }

    FCodeMatValue ConstNode(const FCodeMatValue& V)
    {
        const FString Key = FString::Printf(TEXT("Const%d|%.9g|%.9g|%.9g|%.9g"), V.N, V.C[0], V.C[1], V.C[2], V.C[3]);
        if (UMaterialExpression** Found = Cache.Find(Key)) return FCodeMatValue::MakeExpr(*Found, 0, V.N);

        UMaterialExpression* E = nullptr;
        switch (V.N)
        {
        case 1:
            if (auto* C = Cast<UMaterialExpressionConstant>(CreateNode(UMaterialExpressionConstant::StaticClass())))
            { C->R = V.C[0]; E = C; }
            break;
        case 2:
            if (auto* C = Cast<UMaterialExpressionConstant2Vector>(CreateNode(UMaterialExpressionConstant2Vector::StaticClass())))
            { C->R = V.C[0]; C->G = V.C[1]; E = C; }
            break;
        case 3:
            if (auto* C = Cast<UMaterialExpressionConstant3Vector>(CreateNode(UMaterialExpressionConstant3Vector::StaticClass())))
            { C->Constant = FLinearColor(V.C[0], V.C[1], V.C[2], 0.f); E = C; }
            break;
        default:
            if (auto* C = Cast<UMaterialExpressionConstant4Vector>(CreateNode(UMaterialExpressionConstant4Vector::StaticClass())))
            { C->Constant = FLinearColor(V.C[0], V.C[1], V.C[2], V.C[3]); E = C; }
            break;
        }
        if (!E) return FCodeMatValue();
        Cache.Add(Key, E);
        return FCodeMatValue::MakeExpr(E, 0, V.N);
    }

    bool SetPin(UMaterialExpression* E, const TCHAR* Pin, const FCodeMatValue& M)
    {
        FExpressionInput* In = FindInputPin(E, Pin);
        if (!In) { Fail(FString::Printf(TEXT("pin '%s' not found on %s"), Pin, *E->GetClass()->GetName())); return false; }
        In->Expression  = M.Expr;
        In->OutputIndex = M.OutIdx;
        In->Mask = 0; In->MaskR = 0; In->MaskG = 0; In->MaskB = 0; In->MaskA = 0;
        return true;
    }

    struct FPinArg   { const TCHAR* Pin; FCodeMatValue V; };
    struct FConstArg { const TCHAR* Prop; float V; };

    /** 汎用ノード生成 (CSE 付き)。ピン名・定数プロパティ名はリフレクションで解決する。 */
    FCodeMatValue Op(const TCHAR* ClassName, const TArray<FPinArg>& Pins, const TArray<FConstArg>& Consts, int32 OutN, int32 OutIdx = 0)
    {
        if (Failed()) return FCodeMatValue();
        UClass* Cls = FindExpressionClass(ClassName);
        if (!Cls) return Fail(FString::Printf(TEXT("expression class '%s' not found"), ClassName));

        TArray<FCodeMatValue> M;
        FString Key = ClassName;
        for (const FPinArg& Pa : Pins)
        {
            FCodeMatValue Mv = Materialize(Pa.V);
            if (!Mv.Expr) return FCodeMatValue();
            M.Add(Mv);
            Key += FString::Printf(TEXT("|%s=%s"), Pa.Pin, *KeyOf(Mv));
        }
        for (const FConstArg& Ca : Consts)
            Key += FString::Printf(TEXT("|%s=%.9g"), Ca.Prop, Ca.V);

        if (UMaterialExpression** Found = Cache.Find(Key)) return FCodeMatValue::MakeExpr(*Found, OutIdx, OutN);

        UMaterialExpression* E = CreateNode(Cls);
        if (!E) return FCodeMatValue();
        for (int32 i = 0; i < Pins.Num(); ++i)
            if (!SetPin(E, Pins[i].Pin, M[i])) return FCodeMatValue();
        for (const FConstArg& Ca : Consts)
            SetNumberProperty(E, Ca.Prop, Ca.V);

        Cache.Add(Key, E);
        return FCodeMatValue::MakeExpr(E, OutIdx, OutN);
    }

    // ------------------------------------------------------------------
    // 成分操作: スウィズル / Append / 型変換
    // ------------------------------------------------------------------

    /** Append ツリーをたどって i 番目の成分の「元」を探す */
    TPair<FCodeMatValue, int32> ResolveComponent(FCodeMatValue V, int32 i)
    {
        while (V.Expr)
        {
            if (V.N == 1) { i = 0; break; }
            const FAppendInfo* AI = AppendParts.Find(V.Expr);
            if (!AI) break;
            if (i < AI->A.N) V = AI->A;
            else { i -= AI->A.N; V = AI->B; }
        }
        if (V.N == 1) i = 0;
        return TPair<FCodeMatValue, int32>(V, i);
    }

    FCodeMatValue MaskRun(const FCodeMatValue& Base, const TArray<int32>& Run)
    {
        if (Base.N == 1) return Base;
        bool bIdentity = (Run.Num() == Base.N);
        for (int32 k = 0; bIdentity && k < Run.Num(); ++k) bIdentity = (Run[k] == k);
        if (bIdentity) return Base;

        // TextureSample / VectorParameter の RGBA 出力は専用ピン (RGB/R/G/B/A) を使う
        if (Base.OutIdx == 5 && Base.Expr &&
            (Base.Expr->IsA<UMaterialExpressionTextureSample>() || Base.Expr->IsA<UMaterialExpressionVectorParameter>()))
        {
            if (Run.Num() == 3 && Run[0] == 0 && Run[1] == 1 && Run[2] == 2) return FCodeMatValue::MakeExpr(Base.Expr, 0, 3);
            if (Run.Num() == 1) return FCodeMatValue::MakeExpr(Base.Expr, 1 + Run[0], 1);
        }

        uint32 Bits = 0;
        for (int32 k : Run) Bits |= (1u << k);
        const FString Key = FString::Printf(TEXT("Mask|%s|%u"), *KeyOf(Base), Bits);
        if (UMaterialExpression** Found = Cache.Find(Key)) return FCodeMatValue::MakeExpr(*Found, 0, Run.Num());

        auto* M = Cast<UMaterialExpressionComponentMask>(CreateNode(UMaterialExpressionComponentMask::StaticClass()));
        if (!M) return FCodeMatValue();
        M->Input.Expression  = Base.Expr;
        M->Input.OutputIndex = Base.OutIdx;
        M->R = (Bits & 1) ? 1 : 0;
        M->G = (Bits & 2) ? 1 : 0;
        M->B = (Bits & 4) ? 1 : 0;
        M->A = (Bits & 8) ? 1 : 0;
        Cache.Add(Key, M);
        return FCodeMatValue::MakeExpr(M, 0, Run.Num());
    }

    FCodeMatValue Append(const FCodeMatValue& A, const FCodeMatValue& B)
    {
        if (Failed()) return FCodeMatValue();
        const int32 N = A.N + B.N;
        if (N > 4) return Fail(TEXT("vector with more than 4 components"));
        if (A.bConst && B.bConst)
        {
            float C[4];
            for (int32 i = 0; i < A.N; ++i) C[i] = CompOf(A, i);
            for (int32 i = 0; i < B.N; ++i) C[A.N + i] = CompOf(B, i);
            return ConstVec(N, C);
        }
        FCodeMatValue R = Op(TEXT("MaterialExpressionAppendVector"), { { TEXT("A"), A }, { TEXT("B"), B } }, {}, N);
        if (R.Expr && !AppendParts.Contains(R.Expr))
        {
            FAppendInfo AI;
            AI.A = A; AI.A.Cmp.Reset(); if (A.Cmp.IsValid()) AI.A = Materialize(A);
            AI.B = B; AI.B.Cmp.Reset(); if (B.Cmp.IsValid()) AI.B = Materialize(B);
            AppendParts.Add(R.Expr, AI);
        }
        return R;
    }

    FCodeMatValue Compose(const TArray<TPair<FCodeMatValue, int32>>& Comps)
    {
        TArray<FCodeMatValue> Parts;
        int32 k = 0;
        while (k < Comps.Num())
        {
            const FCodeMatValue& Base = Comps[k].Key;
            if (Base.bConst)
            {
                float C[4] = { 0, 0, 0, 0 };
                int32 Cn = 0;
                while (k < Comps.Num() && Comps[k].Key.bConst && Cn < 4)
                {
                    C[Cn++] = CompOf(Comps[k].Key, Comps[k].Value);
                    ++k;
                }
                Parts.Add(ConstVec(Cn, C));
                continue;
            }
            TArray<int32> Run = { Comps[k].Value };
            int32 j = k + 1;
            while (j < Comps.Num() && SameSource(Comps[j].Key, Base) && Comps[j].Value > Run.Last())
            {
                Run.Add(Comps[j].Value);
                ++j;
            }
            Parts.Add(MaskRun(Base, Run));
            k = j;
        }
        if (Parts.Num() == 0) return Fail(TEXT("empty vector"));
        FCodeMatValue Acc = Parts[0];
        for (int32 i = 1; i < Parts.Num() && !Failed(); ++i) Acc = Append(Acc, Parts[i]);
        return Acc;
    }

    FCodeMatValue SwizzleIdx(const FCodeMatValue& InV, const TArray<int32>& Idx)
    {
        if (Failed()) return FCodeMatValue();
        if (InV.bTexture || InV.bOpaque) return Fail(TEXT("cannot swizzle a texture or unknown identifier"));
        for (int32 i : Idx)
        {
            if (i >= InV.N && !(InV.N == 1 && i == 0))
                return Fail(FString::Printf(TEXT("swizzle component %d out of range for float%d"), i, InV.N));
        }
        const int32 K = Idx.Num();
        if (InV.bConst)
        {
            float C[4];
            for (int32 k = 0; k < K; ++k) C[k] = CompOf(InV, Idx[k]);
            return ConstVec(K, C);
        }
        FCodeMatValue V = InV.Cmp.IsValid() ? Materialize(InV) : InV;
        if (!V.IsValid()) return FCodeMatValue();
        if (V.N == 1) return (K == 1) ? V : Broadcast(V, K);

        bool bIdentity = (K == V.N);
        for (int32 k = 0; bIdentity && k < K; ++k) bIdentity = (Idx[k] == k);
        if (bIdentity) return V;

        TArray<TPair<FCodeMatValue, int32>> Comps;
        for (int32 i : Idx) Comps.Add(ResolveComponent(V, i));
        return Compose(Comps);
    }

    FCodeMatValue Broadcast(const FCodeMatValue& V, int32 N)
    {
        if (V.N == N) return V;
        if (V.N != 1) return Fail(TEXT("internal: broadcast of a vector"));
        if (V.bConst) { float C[4] = { V.C[0], V.C[0], V.C[0], V.C[0] }; return ConstVec(N, C); }
        FCodeMatValue S = V.Cmp.IsValid() ? Materialize(V) : V;
        FCodeMatValue Acc = S;
        for (int32 i = 1; i < N && !Failed(); ++i) Acc = Append(Acc, S);
        return Acc;
    }

    /** 宣言型 / 引数型への暗黙変換 (スカラー拡張・切り詰め) */
    FCodeMatValue ConvertTo(const FCodeMatValue& V, int32 N)
    {
        if (Failed() || !V.IsValid()) return FCodeMatValue();
        if (V.bTexture || V.bOpaque) return V;
        if (V.N == N) return V;
        if (V.N == 1) return Broadcast(V, N);
        if (V.N > N)
        {
            TArray<int32> Idx;
            for (int32 i = 0; i < N; ++i) Idx.Add(i);
            return SwizzleIdx(V, Idx);
        }
        return Fail(FString::Printf(TEXT("cannot convert float%d to float%d"), V.N, N));
    }

    /** 2 つのベクトルの成分数を揃える (HLSL の暗黙切り詰めと同じ) */
    void Equalize(FCodeMatValue& A, FCodeMatValue& B)
    {
        if (A.N > 1 && B.N > 1 && A.N != B.N)
        {
            const int32 N = FMath::Min(A.N, B.N);
            A = ConvertTo(A, N);
            B = ConvertTo(B, N);
        }
    }

    // ------------------------------------------------------------------
    // 定数畳み込み
    // ------------------------------------------------------------------

    FCodeMatValue FoldN(const TArray<FCodeMatValue>& Args, TFunctionRef<float(const float*)> F)
    {
        int32 N = 1;
        int32 MinVec = 4;
        for (const FCodeMatValue& A : Args)
        {
            N = FMath::Max(N, A.N);
            if (A.N > 1) MinVec = FMath::Min(MinVec, A.N);
        }
        if (N > 1) N = MinVec;
        float Out[4];
        for (int32 i = 0; i < N; ++i)
        {
            float X[4] = { 0, 0, 0, 0 };
            for (int32 k = 0; k < Args.Num() && k < 4; ++k) X[k] = CompOf(Args[k], i);
            Out[i] = F(X);
        }
        return ConstVec(N, Out);
    }

    static bool AllConst(const TArray<FCodeMatValue>& Args)
    {
        for (const FCodeMatValue& A : Args) if (!A.bConst) return false;
        return true;
    }

    // ------------------------------------------------------------------
    // 演算
    // ------------------------------------------------------------------

    static bool IsCmpOp(const FString& Op)
    {
        return Op == TEXT("<") || Op == TEXT("<=") || Op == TEXT(">") || Op == TEXT(">=") || Op == TEXT("==") || Op == TEXT("!=");
    }

    static FString InvertCmp(const FString& Op)
    {
        if (Op == TEXT("<"))  return TEXT(">=");
        if (Op == TEXT("<=")) return TEXT(">");
        if (Op == TEXT(">"))  return TEXT("<=");
        if (Op == TEXT(">=")) return TEXT("<");
        if (Op == TEXT("==")) return TEXT("!=");
        return TEXT("==");
    }

    static bool EvalCmp(const FString& Op, float A, float B)
    {
        if (Op == TEXT("<"))  return A <  B;
        if (Op == TEXT("<=")) return A <= B;
        if (Op == TEXT(">"))  return A >  B;
        if (Op == TEXT(">=")) return A >= B;
        if (Op == TEXT("==")) return A == B;
        return A != B;
    }

    FCodeMatValue MakeCmp(const FString& Op, const FCodeMatValue& A, const FCodeMatValue& B)
    {
        if (A.bConst && B.bConst && A.N == 1 && B.N == 1)
            return FCodeMatValue::MakeConst(EvalCmp(Op, A.C[0], B.C[0]) ? 1.f : 0.f);
        if (A.N != 1 || B.N != 1)
            return Fail(TEXT("vector comparisons are not supported in node conversion"));
        FCodeMatValue R;
        R.N = 1;
        R.Cmp = MakeShared<FCodeMatCmp>();
        R.Cmp->Op = Op;
        R.Cmp->A  = A.Cmp.IsValid() ? Materialize(A) : A;
        R.Cmp->B  = B.Cmp.IsValid() ? Materialize(B) : B;
        return R;
    }

    /** 比較結果で T / F を選ぶ If ノード */
    FCodeMatValue SelectCmp(const FCodeMatCmp& Cmp, FCodeMatValue T, FCodeMatValue F)
    {
        const int32 N = FMath::Max(T.N, F.N);
        T = ConvertTo(T, N);
        F = ConvertTo(F, N);
        if (Failed()) return FCodeMatValue();
        if (SameValue(T, F)) return T;

        FCodeMatValue G, E, L;
        const FString& O = Cmp.Op;
        if      (O == TEXT("<"))  { L = T; E = F; G = F; }
        else if (O == TEXT("<=")) { L = T; E = T; G = F; }
        else if (O == TEXT(">"))  { G = T; E = F; L = F; }
        else if (O == TEXT(">=")) { G = T; E = T; L = F; }
        else if (O == TEXT("==")) { E = T; G = F; L = F; }
        else                      { E = F; G = T; L = T; }

        TArray<FPinArg> Pins = {
            { TEXT("A"), Cmp.A },
            { TEXT("AGreaterThanB"), G },
            { TEXT("AEqualsB"), E },
            { TEXT("ALessThanB"), L },
        };
        TArray<FConstArg> Consts;
        if (Cmp.B.bConst && Cmp.B.N == 1) Consts.Add({ TEXT("ConstB"), Cmp.B.C[0] });
        else                              Pins.Add({ TEXT("B"), Cmp.B });
        return Op(TEXT("MaterialExpressionIf"), Pins, Consts, N);
    }

    FCodeMatValue Select(const FCodeMatValue& Cond, const FCodeMatValue& T, const FCodeMatValue& F)
    {
        if (Failed() || !Cond.IsValid() || !T.IsValid() || !F.IsValid()) return FCodeMatValue();
        if (Cond.bConst) return Cond.C[0] != 0.f ? T : F;
        if (SameValue(T, F)) return T;
        if (Cond.Cmp.IsValid()) return SelectCmp(*Cond.Cmp, T, F);
        // 汎用: lerp(F, T, cond)  (cond は 0/1 を想定)
        FCodeMatValue C = Cond.N > 1 ? SwizzleIdx(Cond, { 0 }) : Cond;
        const int32 N = FMath::Max(T.N, F.N);
        return Lerp(ConvertTo(F, N), ConvertTo(T, N), C);
    }

    FCodeMatValue Lerp(const FCodeMatValue& A, const FCodeMatValue& B, const FCodeMatValue& T)
    {
        return CallIntrinsic(TEXT("lerp"), { A, B, T });
    }

    FCodeMatValue Bin(const FString& OpStr, FCodeMatValue A, FCodeMatValue B)
    {
        if (Failed() || !A.IsValid() || !B.IsValid()) return FCodeMatValue();
        if (A.bOpaque) return Fail(FString::Printf(TEXT("unknown identifier '%s'"), *A.Text));
        if (B.bOpaque) return Fail(FString::Printf(TEXT("unknown identifier '%s'"), *B.Text));
        if (A.bTexture || B.bTexture) return Fail(TEXT("arithmetic on a texture object"));

        if (IsCmpOp(OpStr)) return MakeCmp(OpStr, A, B);
        if (OpStr == TEXT("&&")) return Bin(TEXT("*"), A, B);
        if (OpStr == TEXT("||")) return CallIntrinsic(TEXT("max"), { A, B });
        if (OpStr == TEXT("&") || OpStr == TEXT("|") || OpStr == TEXT("^"))
            return Fail(FString::Printf(TEXT("bitwise operator '%s' is not supported"), *OpStr));

        Equalize(A, B);
        if (Failed()) return FCodeMatValue();
        const int32 N = FMath::Max(A.N, B.N);

        if (A.bConst && B.bConst)
        {
            if (OpStr == TEXT("+")) return FoldN({ A, B }, [](const float* X) { return X[0] + X[1]; });
            if (OpStr == TEXT("-")) return FoldN({ A, B }, [](const float* X) { return X[0] - X[1]; });
            if (OpStr == TEXT("*")) return FoldN({ A, B }, [](const float* X) { return X[0] * X[1]; });
            if (OpStr == TEXT("/")) return FoldN({ A, B }, [](const float* X) { return X[1] != 0.f ? X[0] / X[1] : 0.f; });
            if (OpStr == TEXT("%")) return FoldN({ A, B }, [](const float* X) { return X[1] != 0.f ? FMath::Fmod(X[0], X[1]) : 0.f; });
        }

        // 恒等変形 (x*1, x+0, x-0, x/1)
        auto IsScalarConst = [](const FCodeMatValue& V, float C) { return V.bConst && V.N == 1 && V.C[0] == C; };
        if ((OpStr == TEXT("*") || OpStr == TEXT("/")) && IsScalarConst(B, 1.f)) return A;
        if (OpStr == TEXT("*") && IsScalarConst(A, 1.f)) return B;
        if ((OpStr == TEXT("+") || OpStr == TEXT("-")) && IsScalarConst(B, 0.f)) return A;
        if (OpStr == TEXT("+") && IsScalarConst(A, 0.f)) return B;

        const TCHAR* Cls = nullptr;
        if      (OpStr == TEXT("+")) Cls = TEXT("MaterialExpressionAdd");
        else if (OpStr == TEXT("-")) Cls = TEXT("MaterialExpressionSubtract");
        else if (OpStr == TEXT("*")) Cls = TEXT("MaterialExpressionMultiply");
        else if (OpStr == TEXT("/")) Cls = TEXT("MaterialExpressionDivide");
        else if (OpStr == TEXT("%")) return Op(TEXT("MaterialExpressionFmod"), { { TEXT("A"), A }, { TEXT("B"), B } }, {}, N);
        else return Fail(FString::Printf(TEXT("operator '%s' is not supported"), *OpStr));

        if (B.bConst && B.N == 1) return Op(Cls, { { TEXT("A"), A } }, { { TEXT("ConstB"), B.C[0] } }, N);
        if (A.bConst && A.N == 1) return Op(Cls, { { TEXT("B"), B } }, { { TEXT("ConstA"), A.C[0] } }, N);
        return Op(Cls, { { TEXT("A"), A }, { TEXT("B"), B } }, {}, N);
    }

    // ------------------------------------------------------------------
    // 組み込み関数
    // ------------------------------------------------------------------

    struct FIntrinsic
    {
        const TCHAR* Cls;
        TArray<const TCHAR*> Pins;
        TArray<const TCHAR*> ConstProps;   // 対応する定数プロパティ (nullptr = なし)
        int32 OutN;                        // 0 = 引数の最大成分数
        float Period;                      // > 0 なら Period プロパティを設定
        float (*Fold)(const float*);
        int32 OutNFromArg;                 // >= 0 ならその引数の成分数を出力に使う
    };

    static const TMap<FString, FIntrinsic>& Intrinsics()
    {
        static TMap<FString, FIntrinsic> T;
        if (T.Num() == 0)
        {
            auto Unary = [](const TCHAR* Cls, float (*F)(const float*), const TCHAR* Pin = TEXT("Input"), float Period = 0.f) -> FIntrinsic
            {
                return FIntrinsic{ Cls, { Pin }, { nullptr }, 0, Period, F, -1 };
            };
            T.Add(TEXT("sin"),      Unary(TEXT("MaterialExpressionSine"),      [](const float* X) { return FMath::Sin(X[0]); }, TEXT("Input"), TwoPi));
            T.Add(TEXT("cos"),      Unary(TEXT("MaterialExpressionCosine"),    [](const float* X) { return FMath::Cos(X[0]); }, TEXT("Input"), TwoPi));
            T.Add(TEXT("tan"),      Unary(TEXT("MaterialExpressionTangent"),   [](const float* X) { return FMath::Tan(X[0]); }, TEXT("Input"), TwoPi));
            T.Add(TEXT("asin"),     Unary(TEXT("MaterialExpressionArcsine"),   [](const float* X) { return FMath::Asin(X[0]); }));
            T.Add(TEXT("acos"),     Unary(TEXT("MaterialExpressionArccosine"), [](const float* X) { return FMath::Acos(X[0]); }));
            T.Add(TEXT("atan"),     Unary(TEXT("MaterialExpressionArctangent"),[](const float* X) { return FMath::Atan(X[0]); }));
            T.Add(TEXT("abs"),      Unary(TEXT("MaterialExpressionAbs"),       [](const float* X) { return FMath::Abs(X[0]); }));
            T.Add(TEXT("frac"),     Unary(TEXT("MaterialExpressionFrac"),      [](const float* X) { return Frac(X[0]); }));
            T.Add(TEXT("floor"),    Unary(TEXT("MaterialExpressionFloor"),     [](const float* X) { return FMath::FloorToFloat(X[0]); }));
            T.Add(TEXT("ceil"),     Unary(TEXT("MaterialExpressionCeil"),      [](const float* X) { return FMath::CeilToFloat(X[0]); }));
            T.Add(TEXT("round"),    Unary(TEXT("MaterialExpressionRound"),     [](const float* X) { return FMath::RoundToFloat(X[0]); }));
            T.Add(TEXT("trunc"),    Unary(TEXT("MaterialExpressionTruncate"),  [](const float* X) { return FMath::TruncToFloat(X[0]); }));
            T.Add(TEXT("sign"),     Unary(TEXT("MaterialExpressionSign"),      [](const float* X) { return FMath::Sign(X[0]); }));
            T.Add(TEXT("sqrt"),     Unary(TEXT("MaterialExpressionSquareRoot"),[](const float* X) { return FMath::Sqrt(FMath::Max(X[0], 0.f)); }));
            T.Add(TEXT("saturate"), Unary(TEXT("MaterialExpressionSaturate"),  [](const float* X) { return FMath::Clamp(X[0], 0.f, 1.f); }));
            T.Add(TEXT("exp"),      Unary(TEXT("MaterialExpressionExponential"),  [](const float* X) { return FMath::Exp(X[0]); }));
            T.Add(TEXT("exp2"),     Unary(TEXT("MaterialExpressionExponential2"), [](const float* X) { return FMath::Pow(2.f, X[0]); }));
            T.Add(TEXT("log"),      Unary(TEXT("MaterialExpressionLogarithm"),    [](const float* X) { return FMath::Loge(X[0]); }));
            T.Add(TEXT("log2"),     Unary(TEXT("MaterialExpressionLogarithm2"),   [](const float* X) { return FMath::Log2(X[0]); }, TEXT("X")));
            T.Add(TEXT("log10"),    Unary(TEXT("MaterialExpressionLogarithm10"),  [](const float* X) { return FMath::LogX(10.f, X[0]); }, TEXT("X")));
            T.Add(TEXT("ddx"),      Unary(TEXT("MaterialExpressionDDX"),       nullptr, TEXT("Value")));
            T.Add(TEXT("ddy"),      Unary(TEXT("MaterialExpressionDDY"),       nullptr, TEXT("Value")));
            T.Add(TEXT("normalize"),Unary(TEXT("MaterialExpressionNormalize"), nullptr, TEXT("VectorInput")));

            T.Add(TEXT("length"),   FIntrinsic{ TEXT("MaterialExpressionLength"),       { TEXT("Input") },           { nullptr },          1, 0.f, nullptr, -1 });
            T.Add(TEXT("dot"),      FIntrinsic{ TEXT("MaterialExpressionDotProduct"),   { TEXT("A"), TEXT("B") },    { nullptr, nullptr }, 1, 0.f, nullptr, -1 });
            T.Add(TEXT("distance"), FIntrinsic{ TEXT("MaterialExpressionDistance"),     { TEXT("A"), TEXT("B") },    { nullptr, nullptr }, 1, 0.f, nullptr, -1 });
            T.Add(TEXT("cross"),    FIntrinsic{ TEXT("MaterialExpressionCrossProduct"), { TEXT("A"), TEXT("B") },    { nullptr, nullptr }, 3, 0.f, nullptr, -1 });
            T.Add(TEXT("fmod"),     FIntrinsic{ TEXT("MaterialExpressionFmod"),         { TEXT("A"), TEXT("B") },    { nullptr, nullptr }, 0, 0.f,
                [](const float* X) { return X[1] != 0.f ? FMath::Fmod(X[0], X[1]) : 0.f; }, -1 });
            T.Add(TEXT("atan2"),    FIntrinsic{ TEXT("MaterialExpressionArctangent2"),  { TEXT("Y"), TEXT("X") },    { nullptr, nullptr }, 0, 0.f,
                [](const float* X) { return FMath::Atan2(X[0], X[1]); }, -1 });
            T.Add(TEXT("pow"),      FIntrinsic{ TEXT("MaterialExpressionPower"),        { TEXT("Base"), TEXT("Exponent") }, { nullptr, TEXT("ConstExponent") }, 0, 0.f,
                [](const float* X) { return FMath::Pow(X[0], X[1]); }, 0 });
            T.Add(TEXT("min"),      FIntrinsic{ TEXT("MaterialExpressionMin"),          { TEXT("A"), TEXT("B") },    { TEXT("ConstA"), TEXT("ConstB") }, 0, 0.f,
                [](const float* X) { return FMath::Min(X[0], X[1]); }, -1 });
            T.Add(TEXT("max"),      FIntrinsic{ TEXT("MaterialExpressionMax"),          { TEXT("A"), TEXT("B") },    { TEXT("ConstA"), TEXT("ConstB") }, 0, 0.f,
                [](const float* X) { return FMath::Max(X[0], X[1]); }, -1 });
            T.Add(TEXT("step"),     FIntrinsic{ TEXT("MaterialExpressionStep"),         { TEXT("Y"), TEXT("X") },    { TEXT("ConstY"), TEXT("ConstX") }, 0, 0.f,
                [](const float* X) { return X[1] >= X[0] ? 1.f : 0.f; }, -1 });
            T.Add(TEXT("lerp"),     FIntrinsic{ TEXT("MaterialExpressionLinearInterpolate"), { TEXT("A"), TEXT("B"), TEXT("Alpha") }, { TEXT("ConstA"), TEXT("ConstB"), TEXT("ConstAlpha") }, 0, 0.f,
                [](const float* X) { return X[0] + (X[1] - X[0]) * X[2]; }, -1 });
            T.Add(TEXT("smoothstep"), FIntrinsic{ TEXT("MaterialExpressionSmoothStep"), { TEXT("Min"), TEXT("Max"), TEXT("Value") }, { TEXT("ConstMin"), TEXT("ConstMax"), TEXT("ConstValue") }, 0, 0.f,
                [](const float* X)
                {
                    const float D = X[1] - X[0];
                    const float t = D != 0.f ? FMath::Clamp((X[2] - X[0]) / D, 0.f, 1.f) : (X[2] >= X[1] ? 1.f : 0.f);
                    return t * t * (3.f - 2.f * t);
                }, -1 });
            T.Add(TEXT("clamp"),    FIntrinsic{ TEXT("MaterialExpressionClamp"),        { TEXT("Input"), TEXT("Min"), TEXT("Max") }, { nullptr, TEXT("MinDefault"), TEXT("MaxDefault") }, 0, 0.f,
                [](const float* X) { return FMath::Clamp(X[0], X[1], X[2]); }, 0 });
        }
        return T;
    }

    FCodeMatValue CallIntrinsic(const FString& Name, TArray<FCodeMatValue> Args)
    {
        if (Failed()) return FCodeMatValue();
        const FIntrinsic* In = Intrinsics().Find(Name);
        if (!In) return Fail(FString::Printf(TEXT("unknown intrinsic '%s'"), *Name));
        if (Args.Num() != In->Pins.Num())
            return Fail(FString::Printf(TEXT("%s() expects %d argument(s)"), *Name, In->Pins.Num()));
        for (const FCodeMatValue& A : Args)
        {
            if (!A.IsValid()) return FCodeMatValue();
            if (A.bOpaque)  return Fail(FString::Printf(TEXT("unknown identifier '%s'"), *A.Text));
            if (A.bTexture) return Fail(FString::Printf(TEXT("%s(): texture objects cannot be used here"), *Name));
        }

        // 成分数を揃える (スカラーはそのまま: UE 側でブロードキャストされる)
        int32 MinVec = 4, MaxN = 1;
        for (const FCodeMatValue& A : Args) { MaxN = FMath::Max(MaxN, A.N); if (A.N > 1) MinVec = FMath::Min(MinVec, A.N); }
        if (MaxN > 1 && In->OutN != 1 && In->OutN != 3)
        {
            for (FCodeMatValue& A : Args) if (A.N > 1 && A.N != MinVec) A = ConvertTo(A, MinVec);
            MaxN = MinVec;
        }

        if (In->Fold && AllConst(Args)) return FoldN(Args, In->Fold);

        int32 OutN = In->OutN > 0 ? In->OutN : (In->OutNFromArg >= 0 ? FMath::Max(Args[In->OutNFromArg].N, 1) : MaxN);
        if (In->OutNFromArg >= 0 && In->OutN == 0)
        {
            // pow(base, e) / clamp(x, a, b): 出力はベース引数の成分数 (他引数がベクトルならそれも考慮)
            OutN = MaxN;
        }

        TArray<FPinArg> Pins;
        TArray<FConstArg> Consts;
        for (int32 i = 0; i < Args.Num(); ++i)
        {
            const TCHAR* CP = In->ConstProps.IsValidIndex(i) ? In->ConstProps[i] : nullptr;
            if (CP && Args[i].bConst && Args[i].N == 1)
                Consts.Add({ CP, Args[i].C[0] });
            else
                Pins.Add({ In->Pins[i], Args[i] });
        }
        if (In->Period > 0.f) Consts.Add({ TEXT("Period"), In->Period });

        // Clamp / Normalize などは入力ピン必須。定数だけならノード化して接続する。
        if (Pins.Num() == 0)
        {
            Consts.Reset();
            for (int32 i = 0; i < Args.Num(); ++i) Pins.Add({ In->Pins[i], Args[i] });
            if (In->Period > 0.f) Consts.Add({ TEXT("Period"), In->Period });
        }
        return Op(In->Cls, Pins, Consts, OutN);
    }

    // ------------------------------------------------------------------
    // テクスチャ / View / Custom フォールバック
    // ------------------------------------------------------------------

    FCodeMatValue TimeNode(bool bIgnorePause)
    {
        const FString Key = FString::Printf(TEXT("Time|%d"), bIgnorePause ? 1 : 0);
        if (UMaterialExpression** Found = Cache.Find(Key)) return FCodeMatValue::MakeExpr(*Found, 0, 1);
        auto* T = Cast<UMaterialExpressionTime>(CreateNode(UMaterialExpressionTime::StaticClass()));
        if (!T) return FCodeMatValue();
        T->bIgnorePause = bIgnorePause ? 1 : 0;
        Cache.Add(Key, T);
        return FCodeMatValue::MakeExpr(T, 0, 1);
    }

    FCodeMatValue SampleTexture(const FCodeMatValue& Tex, FCodeMatValue UV, const FCodeMatValue* Mip)
    {
        if (Failed()) return FCodeMatValue();
        if (!Tex.bTexture || !Tex.Expr) return Fail(TEXT("first argument of texture sampling must be a Texture2D parameter"));
        UV = ConvertTo(UV, 2);
        FCodeMatValue UVm = Materialize(UV);
        if (!UVm.Expr) return FCodeMatValue();
        FCodeMatValue MipM;
        if (Mip) { MipM = Materialize(ConvertTo(*Mip, 1)); if (!MipM.Expr) return FCodeMatValue(); }

        const FString Key = FString::Printf(TEXT("TexSample|%s|%s|%s"), *KeyOf(Tex), *KeyOf(UVm), Mip ? *KeyOf(MipM) : TEXT("-"));
        if (UMaterialExpression** Found = Cache.Find(Key)) return FCodeMatValue::MakeExpr(*Found, 5, 4);

        auto* S = Cast<UMaterialExpressionTextureSample>(CreateNode(UMaterialExpressionTextureSample::StaticClass()));
        if (!S) return FCodeMatValue();
        S->TextureObject.Expression  = Tex.Expr;
        S->TextureObject.OutputIndex = Tex.OutIdx;
        S->Coordinates.Expression    = UVm.Expr;
        S->Coordinates.OutputIndex   = UVm.OutIdx;
        if (Mip)
        {
            S->MipValueMode           = TMVM_MipLevel;
            S->MipValue.Expression    = MipM.Expr;
            S->MipValue.OutputIndex   = MipM.OutIdx;
        }
        if (auto* TP = Cast<UMaterialExpressionTextureBase>(Tex.Expr))
        {
            if (TP->Texture)
            {
                const EMaterialSamplerType ST = MaterialExpressionUtils::GetSamplerTypeForTexture(TP->Texture);
                TP->SamplerType = ST;
                S->SamplerType  = ST;
            }
            else
            {
                S->SamplerType = TP->SamplerType;
            }
        }
        Cache.Add(Key, S);
        return FCodeMatValue::MakeExpr(S, 5, 4);
    }

    FString ArgText(const FCodeMatValue& V, int32 Index, TArray<TPair<FString, FCodeMatValue>>& OutInputs)
    {
        if (V.bOpaque) return V.Text;
        if (V.bConst)
        {
            if (V.N == 1) return FormatFloatHLSL(V.C[0]);
            FString S = FString::Printf(TEXT("float%d("), V.N);
            for (int32 i = 0; i < V.N; ++i) S += (i ? TEXT(", ") : TEXT("")) + FormatFloatHLSL(V.C[i]);
            return S + TEXT(")");
        }
        const FString Name = FString::Printf(TEXT("a%d"), Index);
        OutInputs.Add(TPair<FString, FCodeMatValue>(Name, V));
        return Name;
    }

    /** 型が分かっている外部関数 / View メンバを、その部分だけの Custom ノードにする */
    FCodeMatValue CustomCall(const FString& CodeExpr, const TArray<TPair<FString, FCodeMatValue>>& Inputs, int32 N, const FString& What)
    {
        if (Failed()) return FCodeMatValue();
        if (N < 1 || N > 4) return Fail(FString::Printf(TEXT("%s: unsupported return type"), *What));

        TArray<FCodeMatValue> M;
        FString Key = FString::Printf(TEXT("Custom|%d|%s"), N, *CodeExpr);
        for (const auto& In : Inputs)
        {
            if (In.Value.bTexture) return Fail(FString::Printf(TEXT("%s: texture arguments are not supported"), *What));
            FCodeMatValue Mv = Materialize(In.Value);
            if (!Mv.Expr) return FCodeMatValue();
            M.Add(Mv);
            Key += FString::Printf(TEXT("|%s=%s"), *In.Key, *KeyOf(Mv));
        }
        if (UMaterialExpression** Found = Cache.Find(Key)) return FCodeMatValue::MakeExpr(*Found, 0, N);

        auto* C = Cast<UMaterialExpressionCustom>(CreateNode(UMaterialExpressionCustom::StaticClass()));
        if (!C) return FCodeMatValue();
        static const ECustomMaterialOutputType OutTypes[] = { CMOT_Float1, CMOT_Float2, CMOT_Float3, CMOT_Float4 };
        C->OutputType  = OutTypes[N - 1];
        C->Code        = FString::Printf(TEXT("return %s;"), *CodeExpr);
        C->Description = What;
        C->Inputs.Reset();
        for (int32 i = 0; i < Inputs.Num(); ++i)
        {
            FCustomInput& CI = C->Inputs.AddDefaulted_GetRef();
            CI.InputName = FName(*Inputs[i].Key);
            CI.Input.Expression  = M[i].Expr;
            CI.Input.OutputIndex = M[i].OutIdx;
        }
        Cache.Add(Key, C);
        Notes.AddUnique(FString::Printf(TEXT("'%s' was converted to a small Custom node (no node equivalent)."), *What));
        return FCodeMatValue::MakeExpr(C, 0, N);
    }

    FCodeMatValue ViewMember(const FString& Obj, const FString& Member)
    {
        if (Member == TEXT("GameTime")) return TimeNode(false);
        if (Member == TEXT("RealTime")) return TimeNode(true);

        static const TMap<FString, int32> Known = {
            { TEXT("BufferSizeAndInvSize"), 4 },
            { TEXT("ViewSizeAndInvSize"), 4 },
            { TEXT("ViewRectMin"), 4 },
            { TEXT("DeltaTime"), 1 },
            { TEXT("RealTimeDeltaTime"), 1 },
            { TEXT("FrameNumber"), 1 },
            { TEXT("TemporalAAParams"), 4 },
            { TEXT("ScreenPositionScaleBias"), 4 },
            { TEXT("ViewForward"), 3 },
            { TEXT("ViewRight"), 3 },
            { TEXT("ViewUp"), 3 },
        };
        if (const int32* N = Known.Find(Member))
            return CustomCall(FString::Printf(TEXT("%s.%s"), *Obj, *Member), {}, *N, FString::Printf(TEXT("%s.%s"), *Obj, *Member));
        return Fail(FString::Printf(TEXT("'%s.%s' is not supported in node conversion"), *Obj, *Member));
    }

    // ------------------------------------------------------------------
    // 式の評価
    // ------------------------------------------------------------------

    FCodeMatValue Eval(const FAstPtr& A, FEnv& Env)
    {
        if (Failed()) return FCodeMatValue();
        if (!A) return Fail(TEXT("missing expression"));
        if (A->Line > 0) CurLine = A->Line;

        switch (A->Kind)
        {
        case FAst::EKind::Num:
            return FCodeMatValue::MakeConst((float)A->Num);

        case FAst::EKind::Ident:
            return EvalIdent(A->S, Env);

        case FAst::EKind::Unary:
        {
            FCodeMatValue V = Eval(A->A[0], Env);
            if (!V.IsValid()) return FCodeMatValue();
            if (A->S == TEXT("-"))
            {
                if (V.bConst) { FCodeMatValue R = V; for (float& c : R.C) c = -c; return R; }
                return Bin(TEXT("*"), V, FCodeMatValue::MakeConst(-1.f));
            }
            if (A->S == TEXT("!"))
            {
                if (V.bConst) return FCodeMatValue::MakeConst(V.C[0] == 0.f ? 1.f : 0.f);
                if (V.Cmp.IsValid())
                {
                    FCodeMatValue R = V;
                    R.Cmp = MakeShared<FCodeMatCmp>(*V.Cmp);
                    R.Cmp->Op = InvertCmp(V.Cmp->Op);
                    return R;
                }
                return Op(TEXT("MaterialExpressionOneMinus"), { { TEXT("Input"), V } }, {}, V.N);
            }
            return Fail(FString::Printf(TEXT("operator '%s' is not supported"), *A->S));
        }

        case FAst::EKind::Binary:
        {
            FCodeMatValue L = Eval(A->A[0], Env);
            FCodeMatValue R = Eval(A->A[1], Env);
            CurLine = A->Line;
            return Bin(A->S, L, R);
        }

        case FAst::EKind::Ternary:
        {
            FCodeMatValue C = Eval(A->A[0], Env);
            FCodeMatValue T = Eval(A->A[1], Env);
            FCodeMatValue F = Eval(A->A[2], Env);
            CurLine = A->Line;
            return Select(C, T, F);
        }

        case FAst::EKind::Member:
        {
            if (A->Obj && A->Obj->Kind == FAst::EKind::Ident &&
                (A->Obj->S == TEXT("View") || A->Obj->S == TEXT("ResolvedView") || A->Obj->S == TEXT("PrimaryView")) &&
                !Env.Contains(A->Obj->S))
            {
                return ViewMember(A->Obj->S, A->S);
            }
            FCodeMatValue V = Eval(A->Obj, Env);
            if (!V.IsValid()) return FCodeMatValue();
            if (V.bOpaque) return Fail(FString::Printf(TEXT("'%s.%s' is not supported in node conversion"), *V.Text, *A->S));
            TArray<int32> Idx;
            if (!ParseSwizzleString(A->S, Idx)) return Fail(FString::Printf(TEXT("invalid swizzle '.%s'"), *A->S));
            return SwizzleIdx(V, Idx);
        }

        case FAst::EKind::Index:
            return Fail(TEXT("array / matrix indexing is not supported in node conversion"));

        case FAst::EKind::Call:
            return EvalCall(*A, Env);
        }
        return Fail(TEXT("unsupported expression"));
    }

    FCodeMatValue EvalIdent(const FString& Name, FEnv& Env)
    {
        if (const FVar* V = Env.Find(Name)) return V->V;
        if (const FVar* G = GlobalEnv.Find(Name)) return G->V;
        if (Name == TEXT("true"))  return FCodeMatValue::MakeConst(1.f);
        if (Name == TEXT("false")) return FCodeMatValue::MakeConst(0.f);
        if (Name == TEXT("Time"))  return TimeNode(false);
        if (Name == TEXT("PI"))    return FCodeMatValue::MakeConst(3.14159265358979f);
        FCodeMatValue O;
        O.bOpaque = true;
        O.Text    = Name;
        return O;
    }

    FCodeMatValue EvalCall(const FAst& A, FEnv& Env)
    {
        TArray<FCodeMatValue> Args;
        for (const FAstPtr& X : A.A)
        {
            Args.Add(Eval(X, Env));
            if (Failed()) return FCodeMatValue();
        }
        CurLine = A.Line;
        const FString& Name = A.S;

        // ---- メソッド呼び出し: Tex.Sample(Sampler, uv) ----
        if (A.Obj)
        {
            FCodeMatValue Obj = Eval(A.Obj, Env);
            if (Failed()) return FCodeMatValue();
            if (Obj.bTexture)
            {
                if (Name == TEXT("Sample") && Args.Num() >= 2)      return SampleTexture(Obj, Args[1], nullptr);
                if (Name == TEXT("SampleLevel") && Args.Num() >= 3) return SampleTexture(Obj, Args[1], &Args[2]);
            }
            return Fail(FString::Printf(TEXT("method '.%s()' is not supported in node conversion"), *Name));
        }

        // ---- ユーザー定義関数 (インライン展開) ----
        if (const FFunc* F = Funcs.Find(Name)) return Inline(*F, Args);

        // ---- 型コンストラクタ / キャスト ----
        const int32 TN = TypeComponents(Name);
        if (TN < 0) return Fail(FString::Printf(TEXT("matrix type '%s' is not supported"), *Name));
        if (TN > 0)
        {
            if (TN == 1)
            {
                if (Args.Num() != 1) return Fail(FString::Printf(TEXT("%s() expects 1 argument"), *Name));
                FCodeMatValue V = Args[0].N > 1 ? SwizzleIdx(Args[0], { 0 }) : Args[0];
                if (IsIntType(Name))
                    return V.bConst ? FCodeMatValue::MakeConst(FMath::TruncToFloat(V.C[0]))
                                    : Op(TEXT("MaterialExpressionTruncate"), { { TEXT("Input"), V } }, {}, 1);
                if (Name == TEXT("bool")) return MakeCmp(TEXT("!="), V, FCodeMatValue::MakeConst(0.f));
                return V;
            }
            return Construct(TN, Args);
        }

        // ---- 合成で表せる組み込み関数 ----
        if (Name == TEXT("rsqrt") && Args.Num() == 1) return Bin(TEXT("/"), FCodeMatValue::MakeConst(1.f), CallIntrinsic(TEXT("sqrt"), Args));
        if (Name == TEXT("rcp") && Args.Num() == 1)   return Bin(TEXT("/"), FCodeMatValue::MakeConst(1.f), Args[0]);
        if (Name == TEXT("mad") && Args.Num() == 3)   return Bin(TEXT("+"), Bin(TEXT("*"), Args[0], Args[1]), Args[2]);
        if (Name == TEXT("radians") && Args.Num() == 1) return Bin(TEXT("*"), Args[0], FCodeMatValue::MakeConst(0.0174532925f));
        if (Name == TEXT("degrees") && Args.Num() == 1) return Bin(TEXT("*"), Args[0], FCodeMatValue::MakeConst(57.2957795f));
        if (Name == TEXT("fwidth") && Args.Num() == 1)
            return Bin(TEXT("+"), CallIntrinsic(TEXT("abs"), { CallIntrinsic(TEXT("ddx"), Args) }), CallIntrinsic(TEXT("abs"), { CallIntrinsic(TEXT("ddy"), Args) }));
        if (Name == TEXT("reflect") && Args.Num() == 2)
        {
            // i - 2 * dot(i, n) * n
            FCodeMatValue D = CallIntrinsic(TEXT("dot"), { Args[0], Args[1] });
            return Bin(TEXT("-"), Args[0], Bin(TEXT("*"), Bin(TEXT("*"), D, FCodeMatValue::MakeConst(2.f)), Args[1]));
        }

        if (Intrinsics().Contains(Name)) return CallIntrinsic(Name, Args);

        // ---- テクスチャサンプリング ----
        if ((Name == TEXT("Texture2DSample") || Name == TEXT("Texture2DSampleBias")) && Args.Num() >= 3)
            return SampleTexture(Args[0], Args[2], nullptr);
        if (Name == TEXT("Texture2DSampleLevel") && Args.Num() >= 4)
            return SampleTexture(Args[0], Args[2], &Args[3]);
        if (Name == TEXT("tex2D") && Args.Num() == 2)
            return SampleTexture(Args[0], Args[1], nullptr);

        // ---- 型が分かっている外部関数 → Custom ノード ----
        if (const int32* N = ExternTypes.Find(Name))
        {
            TArray<TPair<FString, FCodeMatValue>> Inputs;
            FString Code = Name + TEXT("(");
            for (int32 i = 0; i < Args.Num(); ++i)
            {
                FCodeMatValue V = Args[i];
                if (V.Cmp.IsValid()) V = Materialize(V);
                if (V.bTexture) return Fail(FString::Printf(TEXT("%s(): texture arguments are not supported"), *Name));
                Code += (i ? TEXT(", ") : TEXT("")) + ArgText(V, Inputs.Num(), Inputs);
            }
            Code += TEXT(")");
            return CustomCall(Code, Inputs, *N, Name + TEXT("()"));
        }

        return Fail(FString::Printf(
            TEXT("function '%s' is not supported in node conversion (add  // @extern float4 %s  to call it through a small Custom node)"),
            *Name, *Name));
    }

    FCodeMatValue Construct(int32 N, const TArray<FCodeMatValue>& Args)
    {
        if (Args.Num() == 0) return Fail(TEXT("empty constructor"));
        for (const FCodeMatValue& A : Args)
        {
            if (!A.IsValid()) return FCodeMatValue();
            if (A.bOpaque)  return Fail(FString::Printf(TEXT("unknown identifier '%s'"), *A.Text));
            if (A.bTexture) return Fail(TEXT("texture object inside a vector constructor"));
        }
        if (Args.Num() == 1) return ConvertTo(Args[0], N);

        int32 Total = 0;
        for (const FCodeMatValue& A : Args) Total += A.N;
        if (Total != N) return Fail(FString::Printf(TEXT("float%d(...) got %d components"), N, Total));

        TArray<TPair<FCodeMatValue, int32>> Comps;
        for (const FCodeMatValue& A0 : Args)
        {
            FCodeMatValue A = A0.Cmp.IsValid() ? Materialize(A0) : A0;
            for (int32 i = 0; i < A.N; ++i) Comps.Add(ResolveComponent(A, i));
        }
        return Compose(Comps);
    }

    FCodeMatValue Inline(const FFunc& F, const TArray<FCodeMatValue>& Args)
    {
        if (InlineDepth > 16) return Fail(FString::Printf(TEXT("recursion / nesting too deep while inlining '%s'"), *F.Name));
        if (Args.Num() != F.Params.Num())
            return Fail(FString::Printf(TEXT("%s() expects %d argument(s) but got %d"), *F.Name, F.Params.Num(), Args.Num()));

        FEnv Local;
        for (int32 i = 0; i < F.Params.Num(); ++i)
        {
            const FParam& P = F.Params[i];
            if (P.bOut) return Fail(FString::Printf(TEXT("%s(): out / inout parameters are not supported"), *F.Name));
            FVar Var;
            Var.bInt = IsIntType(P.Type);
            if (IsTextureType(P.Type) || IsSamplerType(P.Type))
            {
                Var.V = Args[i];
            }
            else
            {
                Var.N = TypeComponents(P.Type);
                if (Var.N <= 0) return Fail(FString::Printf(TEXT("%s(): parameter type '%s' is not supported"), *F.Name, *P.Type));
                Var.V = ConvertTo(Args[i], Var.N);
            }
            Local.Add(P.Name, Var);
        }

        ++InlineDepth;
        const int32 SavedLine = CurLine;
        FCodeMatValue Ret;
        const EFlow Flow = ExecList(F.Body, 0, Local, Ret, true);
        CurLine = SavedLine;
        --InlineDepth;

        if (Flow == EFlow::Error || Failed()) return FCodeMatValue();
        if (F.RetType == TEXT("void")) return FCodeMatValue::MakeConst(0.f);
        if (Flow != EFlow::Return || !Ret.IsValid()) return Fail(FString::Printf(TEXT("%s() does not return a value"), *F.Name));
        const int32 RN = TypeComponents(F.RetType);
        return RN > 0 ? ConvertTo(Ret, RN) : Ret;
    }

    // ------------------------------------------------------------------
    // 文の実行
    // ------------------------------------------------------------------

    void MergeEnv(FEnv& Env, const FCodeMatValue& Cond, FEnv& EnvT, FEnv& EnvE)
    {
        for (auto& KV : Env)
        {
            const FVar* T = EnvT.Find(KV.Key);
            const FVar* E = EnvE.Find(KV.Key);
            if (!T || !E) continue;
            if (SameValue(T->V, E->V)) KV.Value.V = T->V;
            else                       KV.Value.V = Select(Cond, T->V, E->V);
            if (Failed()) return;
        }
    }

    EFlow ExecIf(const FStmt& S, FEnv& Env, FCodeMatValue& Ret, const TArray<FStmtPtr>* List, int32 Index, bool bTopLevel)
    {
        FCodeMatValue Cond = Eval(S.Value, Env);
        if (Failed()) return EFlow::Error;

        if (Cond.bConst)
        {
            const FStmtPtr& B = (Cond.C[0] != 0.f) ? S.Then : S.Else;
            if (!B) return EFlow::Normal;
            return ExecStmt(*B, Env, Ret);
        }

        FEnv EnvT = Env;
        FEnv EnvE = Env;
        FCodeMatValue RetT, RetE;
        EFlow FT = S.Then ? ExecStmt(*S.Then, EnvT, RetT) : EFlow::Normal;
        if (FT == EFlow::Error) return EFlow::Error;
        EFlow FE = S.Else ? ExecStmt(*S.Else, EnvE, RetE) : EFlow::Normal;
        if (FE == EFlow::Error) return EFlow::Error;

        if (FT == EFlow::Normal && FE == EFlow::Normal)
        {
            MergeEnv(Env, Cond, EnvT, EnvE);
            return Failed() ? EFlow::Error : EFlow::Normal;
        }

        // どちらかの分岐が return した: 残りの文を返さなかった側で続けて実行し、最後に選択する
        if (!bTopLevel || !List)
        {
            CurLine = S.Line;
            Fail(TEXT("'return' inside a conditional is only supported at the top level of a function"));
            return EFlow::Error;
        }
        if (FT == EFlow::Normal) FT = ExecList(*List, Index + 1, EnvT, RetT, true);
        if (FT == EFlow::Error) return EFlow::Error;
        if (FE == EFlow::Normal) FE = ExecList(*List, Index + 1, EnvE, RetE, true);
        if (FE == EFlow::Error) return EFlow::Error;
        if (FT != EFlow::Return || FE != EFlow::Return)
        {
            CurLine = S.Line;
            Fail(TEXT("not all code paths return a value"));
            return EFlow::Error;
        }
        CurLine = S.Line;
        Ret = Select(Cond, RetT, RetE);
        return Failed() ? EFlow::Error : EFlow::Return;
    }

    EFlow ExecList(const TArray<FStmtPtr>& List, int32 Start, FEnv& Env, FCodeMatValue& Ret, bool bTopLevel)
    {
        for (int32 i = Start; i < List.Num(); ++i)
        {
            const FStmt& S = *List[i];
            EFlow F;
            if (S.Kind == FStmt::EKind::If)
            {
                F = ExecIf(S, Env, Ret, &List, i, bTopLevel);
                // 早期 return 経路では残りの文は ExecIf 内で処理済み
                if (F == EFlow::Return) return F;
            }
            else
            {
                F = ExecStmt(S, Env, Ret);
            }
            if (F != EFlow::Normal) return F;
        }
        return EFlow::Normal;
    }

    EFlow ExecStmt(const FStmt& S, FEnv& Env, FCodeMatValue& Ret)
    {
        if (Failed()) return EFlow::Error;
        if (S.Line > 0) CurLine = S.Line;

        switch (S.Kind)
        {
        case FStmt::EKind::Empty:
            return EFlow::Normal;

        case FStmt::EKind::Block:
            return ExecList(S.Body, 0, Env, Ret, false);

        case FStmt::EKind::Decl:
        {
            const bool bTex = IsTextureType(S.Type) || IsSamplerType(S.Type);
            const int32 N = bTex ? 1 : TypeComponents(S.Type);
            if (N <= 0) { Fail(FString::Printf(TEXT("type '%s' is not supported"), *S.Type)); return EFlow::Error; }
            for (const auto& D : S.Decls)
            {
                FVar Var;
                Var.N    = N;
                Var.bInt = IsIntType(S.Type);
                if (D.Value)
                {
                    FCodeMatValue V = Eval(D.Value, Env);
                    if (Failed()) return EFlow::Error;
                    Var.V = bTex ? V : ConvertTo(V, N);
                }
                else
                {
                    Var.V = ConvertTo(FCodeMatValue::MakeConst(0.f), N);
                }
                if (Var.bInt && Var.V.bConst) for (float& c : Var.V.C) c = FMath::TruncToFloat(c);
                if (Failed()) return EFlow::Error;
                Env.Add(D.Key, Var);
            }
            return EFlow::Normal;
        }

        case FStmt::EKind::Assign:
            return ExecAssign(S, Env) ? EFlow::Normal : EFlow::Error;

        case FStmt::EKind::Return:
            Ret = S.Value ? Eval(S.Value, Env) : FCodeMatValue::MakeConst(0.f);
            return Failed() ? EFlow::Error : EFlow::Return;

        case FStmt::EKind::ExprS:
            Eval(S.Value, Env);
            return Failed() ? EFlow::Error : EFlow::Normal;

        case FStmt::EKind::If:
            return ExecIf(S, Env, Ret, nullptr, 0, false);

        case FStmt::EKind::For:
        {
            if (S.Then && ExecStmt(*S.Then, Env, Ret) == EFlow::Error) return EFlow::Error;
            for (int32 Iter = 0; ; ++Iter)
            {
                if (Iter >= 256)
                {
                    CurLine = S.Line;
                    Fail(TEXT("for loop exceeds 256 iterations (loops are unrolled into nodes)"));
                    return EFlow::Error;
                }
                FCodeMatValue C = Eval(S.Value, Env);
                if (Failed()) return EFlow::Error;
                if (!C.bConst)
                {
                    CurLine = S.Line;
                    Fail(TEXT("for loop condition must be a compile-time constant (loops are unrolled into nodes)"));
                    return EFlow::Error;
                }
                if (C.C[0] == 0.f) break;
                const EFlow F = ExecStmt(*S.Body[0], Env, Ret);
                if (F != EFlow::Normal) return F;
                if (S.Else && ExecStmt(*S.Else, Env, Ret) == EFlow::Error) return EFlow::Error;
            }
            return EFlow::Normal;
        }
        }
        return EFlow::Normal;
    }

    bool ExecAssign(const FStmt& S, FEnv& Env)
    {
        FCodeMatValue Rhs = Eval(S.Value, Env);
        if (Failed()) return false;
        CurLine = S.Line;

        const FAstPtr& T = S.Target;
        FString VarName;
        FString SwizzleStr;
        if (T->Kind == FAst::EKind::Ident)
        {
            VarName = T->S;
        }
        else if (T->Kind == FAst::EKind::Member && T->Obj && T->Obj->Kind == FAst::EKind::Ident)
        {
            VarName    = T->Obj->S;
            SwizzleStr = T->S;
        }
        else
        {
            Fail(TEXT("unsupported assignment target"));
            return false;
        }

        FVar* Var = Env.Find(VarName);
        if (!Var)
        {
            if (GlobalEnv.Contains(VarName)) Fail(FString::Printf(TEXT("cannot assign to global constant '%s'"), *VarName));
            else                             Fail(FString::Printf(TEXT("assignment to undeclared variable '%s'"), *VarName));
            return false;
        }

        const FString BinOp = S.Op == TEXT("=") ? FString() : S.Op.Left(1);

        if (SwizzleStr.IsEmpty())
        {
            FCodeMatValue NewV = BinOp.IsEmpty() ? Rhs : Bin(BinOp, Var->V, Rhs);
            if (Failed()) return false;
            if (!Var->V.bTexture && !NewV.bTexture) NewV = ConvertTo(NewV, Var->N);
            if (Var->bInt && NewV.bConst) for (float& c : NewV.C) c = FMath::TruncToFloat(c);
            Var->V = NewV;
            return !Failed();
        }

        TArray<int32> Idx;
        if (!ParseSwizzleString(SwizzleStr, Idx)) { Fail(FString::Printf(TEXT("invalid swizzle '.%s'"), *SwizzleStr)); return false; }
        for (int32 i : Idx)
        {
            if (i >= Var->N) { Fail(FString::Printf(TEXT("'.%s' out of range for %s"), *SwizzleStr, *VarName)); return false; }
        }

        FCodeMatValue Cur  = SwizzleIdx(Var->V, Idx);
        FCodeMatValue Part = BinOp.IsEmpty() ? Rhs : Bin(BinOp, Cur, Rhs);
        Part = ConvertTo(Part, Idx.Num());
        if (Failed()) return false;

        FCodeMatValue Base = Var->V.Cmp.IsValid() ? Materialize(Var->V) : Var->V;
        TArray<TPair<FCodeMatValue, int32>> Comps;
        for (int32 c = 0; c < Var->N; ++c) Comps.Add(ResolveComponent(Base, c));
        FCodeMatValue PartM = Part.Cmp.IsValid() ? Materialize(Part) : Part;
        for (int32 k = 0; k < Idx.Num(); ++k) Comps[Idx[k]] = ResolveComponent(PartM, k);
        Var->V = Compose(Comps);
        return !Failed();
    }

    // ------------------------------------------------------------------
    // エントリ
    // ------------------------------------------------------------------

    bool BuildEntry(const FString& Source, const FString& EntryName, FCodeMatValue& OutValue, FString& OutError)
    {
        Err.Empty();
        CurLine = 0;
        Funcs.Reset();
        GlobalEnv.Reset();

        // 既知の外部関数 (UE シェーダーライブラリ) と // @extern 注釈
        ExternTypes.Add(TEXT("SceneTextureLookup"), 4);
        ExternTypes.Add(TEXT("GetDefaultSceneTextureUV"), 2);
        ExternTypes.Add(TEXT("GetSceneTextureUV"), 2);
        ExternTypes.Add(TEXT("ViewportUVToBufferUV"), 2);
        ExternTypes.Add(TEXT("BufferUVToViewportUV"), 2);
        ExternTypes.Add(TEXT("GetPixelPosition"), 2);
        ExternTypes.Add(TEXT("GetViewportUV"), 2);
        ExternTypes.Add(TEXT("ConvertFromDeviceZ"), 1);
        {
            FRegexMatcher M(FRegexPattern(TEXT("@extern\\s+(\\w+)\\s+(\\w+)")), Source);
            while (M.FindNext())
            {
                const int32 N = TypeComponents(M.GetCaptureGroup(1));
                if (N > 0) ExternTypes.Add(M.GetCaptureGroup(2), N);
            }
        }

        TArray<FFunc> FuncList;
        TArray<FStmtPtr> Globals;
        if (!ParseProgram(Source, FuncList, Globals, Err)) { OutError = Err; return false; }
        for (FFunc& F : FuncList) Funcs.Add(F.Name, F);

        // グローバル定数
        {
            FCodeMatValue Dummy;
            for (const FStmtPtr& G : Globals)
            {
                if (ExecStmt(*G, GlobalEnv, Dummy) == EFlow::Error) { OutError = Err; return false; }
            }
        }

        const FFunc* Entry = Funcs.Find(EntryName);
        if (!Entry) { OutError = FString::Printf(TEXT("entry function '%s' not found"), *EntryName); return false; }

        FEnv Env;
        for (const FParam& P : Entry->Params)
        {
            FVar Var;
            if (IsSamplerType(P.Type))
            {
                Var.V.bOpaque = true;
                Var.V.Text    = P.Name;
                Env.Add(P.Name, Var);
                continue;
            }
            FCodeMatValue V;
            if (!Resolve(P.Type, P.Name, V) || !V.IsValid())
            {
                OutError = FString::Printf(
                    TEXT("entry parameter '%s %s' cannot be auto-wired (use uv / WorldPos / ObjectRadius / Time / *Normal* / a @param name)"),
                    *P.Type, *P.Name);
                return false;
            }
            if (IsTextureType(P.Type))
            {
                V.bTexture = true;
                Var.V = V;
            }
            else
            {
                Var.N    = FMath::Max(TypeComponents(P.Type), 1);
                Var.bInt = IsIntType(P.Type);
                Var.V    = ConvertTo(V, Var.N);
            }
            if (Failed()) { OutError = Err; return false; }
            Env.Add(P.Name, Var);
        }

        FCodeMatValue Ret;
        const EFlow Flow = ExecList(Entry->Body, 0, Env, Ret, true);
        if (Flow == EFlow::Error || Failed()) { OutError = Err; return false; }
        if (Flow != EFlow::Return || !Ret.IsValid())
        {
            OutError = FString::Printf(TEXT("'%s' does not return a value"), *EntryName);
            return false;
        }

        const int32 RN = TypeComponents(Entry->RetType);
        Ret = RN > 0 ? ConvertTo(Ret, RN) : Ret;
        if (Failed()) { OutError = Err; return false; }
        if (Ret.bOpaque) { OutError = FString::Printf(TEXT("unknown identifier '%s'"), *Ret.Text); return false; }

        OutValue = Ret;
        return true;
    }
};

// ============================================================================
// Public wrapper
// ============================================================================

FCodeMatNodeBuilder::FCodeMatNodeBuilder(FNewExprFn InNewExpr, FResolveParamFn InResolve)
    : Impl(MakeUnique<FImpl>())
{
    Impl->NewExpr = MoveTemp(InNewExpr);
    Impl->Resolve = MoveTemp(InResolve);
}

FCodeMatNodeBuilder::~FCodeMatNodeBuilder() = default;

bool FCodeMatNodeBuilder::BuildEntry(const FString& Source, const FString& EntryName, FCodeMatValue& OutValue, FString& OutError)
{
    return Impl->BuildEntry(Source, EntryName, OutValue, OutError);
}

FCodeMatValue FCodeMatNodeBuilder::Swizzle(const FCodeMatValue& V, const FString& Comps)
{
    TArray<int32> Idx;
    if (!ParseSwizzleString(Comps, Idx)) return FCodeMatValue();
    return Impl->SwizzleIdx(V, Idx);
}

bool FCodeMatNodeBuilder::Connect(FExpressionInput& Input, const FCodeMatValue& V)
{
    FCodeMatValue M = Impl->Materialize(V);
    if (!M.Expr) return false;
    Input.Expression  = M.Expr;
    Input.OutputIndex = M.OutIdx;
    Input.Mask = 0; Input.MaskR = 0; Input.MaskG = 0; Input.MaskB = 0; Input.MaskA = 0;
    return true;
}

const TArray<FString>& FCodeMatNodeBuilder::GetNotes() const
{
    return Impl->Notes;
}

#endif // WITH_EDITOR
