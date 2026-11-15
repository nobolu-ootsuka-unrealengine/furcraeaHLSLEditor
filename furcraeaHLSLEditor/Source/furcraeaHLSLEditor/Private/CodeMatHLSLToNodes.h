// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatHLSLToNodes.h
//
// HLSL (サブセット) → 本物のマテリアルノード (UMaterialExpression DAG) 変換器。
//
// 対応している主な構文:
//   - 変数宣言 / 代入 / 複合代入 (+= -= *= /=) / スウィズル代入 (q.x += ...)
//   - 四則演算, 単項マイナス, 比較 (< <= > >= == !=), && || !, 三項演算子
//   - floatN(...) コンストラクタ, スウィズル (.xy / .rgb / .yx など)
//   - if / else (分岐した変数は If ノードで合成), 定数回数の for ループ (展開)
//   - 関数先頭/末尾の return, トップレベルでの早期 return (if (...) return a; return b;)
//   - ユーザー定義ヘルパー関数 (インライン展開)
//   - 組み込み関数: sin cos tan asin acos atan atan2 abs frac floor ceil round trunc sign
//     sqrt rsqrt rcp exp exp2 log log2 log10 pow min max clamp saturate lerp smoothstep
//     step length distance dot cross normalize reflect fmod mad ddx ddy
//   - Texture2DSample(Tex, TexSampler, uv) / Tex.Sample(TexSampler, uv)
//   - View.GameTime / Time → Time ノード
//   - 未対応関数のうち型が分かるもの (SceneTextureLookup 等, // @extern float4 Func で追加可)
//     → その呼び出しだけを小さな Custom ノードにする
//
// 定数同士の演算はコンパイル時に畳み込み、同一ノードは共有 (CSE) される。

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

class UMaterialExpression;
struct FExpressionInput;

/** 変換中の値 (ノード出力 / 定数 / テクスチャ / 比較結果 / 生テキスト) */
struct FCodeMatValue
{
    UMaterialExpression* Expr = nullptr;
    int32 OutIdx = 0;
    int32 N = 1;                 // 成分数 1..4

    bool  bConst = false;
    float C[4] = { 0.f, 0.f, 0.f, 0.f };

    bool  bTexture = false;      // Texture2D オブジェクト
    bool  bOpaque  = false;      // HLSL テキストのまま (Custom ノード引数にのみ使用可)
    FString Text;

    // 比較結果 (遅延生成: Select では If ノードに直結する)
    TSharedPtr<struct FCodeMatCmp> Cmp;

    bool IsValid() const { return Expr != nullptr || bConst || bOpaque || Cmp.IsValid(); }

    static FCodeMatValue MakeConst(float V)
    {
        FCodeMatValue R; R.bConst = true; R.N = 1; R.C[0] = R.C[1] = R.C[2] = R.C[3] = V; return R;
    }
    static FCodeMatValue MakeExpr(UMaterialExpression* E, int32 InOutIdx, int32 InN)
    {
        FCodeMatValue R; R.Expr = E; R.OutIdx = InOutIdx; R.N = InN; return R;
    }
};

struct FCodeMatCmp
{
    FString Op;          // "<" "<=" ">" ">=" "==" "!="
    FCodeMatValue A;
    FCodeMatValue B;
};

class FCodeMatNodeBuilder
{
public:
    /** マテリアルにノードを 1 つ追加して返す (ExpressionCollection への登録まで行うこと) */
    using FNewExprFn = TFunction<UMaterialExpression*(UClass*)>;

    /**
     * エントリ関数の引数を外部ノードへ解決する。
     * Type は "float2" / "float3" / "Texture2D" などの HLSL 型名。
     * 解決できなければ false を返す。
     */
    using FResolveParamFn = TFunction<bool(const FString& Type, const FString& Name, FCodeMatValue& Out)>;

    FCodeMatNodeBuilder(FNewExprFn InNewExpr, FResolveParamFn InResolve);
    ~FCodeMatNodeBuilder();

    /** Source 中の EntryName 関数をノード化し、返り値を OutValue に入れる */
    bool BuildEntry(const FString& Source, const FString& EntryName, FCodeMatValue& OutValue, FString& OutError);

    /** スウィズル (例: "rgb", "a") */
    FCodeMatValue Swizzle(const FCodeMatValue& V, const FString& Comps);

    /** 値を FExpressionInput に接続する (定数等は必要に応じてノード化) */
    bool Connect(FExpressionInput& Input, const FCodeMatValue& V);

    /** 変換中に出た注意 (Custom ノードへ逃がした呼び出し等) */
    const TArray<FString>& GetNotes() const;

private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};

#endif // WITH_EDITOR
