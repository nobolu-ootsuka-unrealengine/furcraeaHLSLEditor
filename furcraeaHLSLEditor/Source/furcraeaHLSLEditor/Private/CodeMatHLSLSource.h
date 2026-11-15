// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatHLSLSource.h
//
// HLSL ソーステキストを扱う軽量ユーティリティ (正規表現ベース)。
// Custom ノード生成とエントリ関数の検出に使う。本格的な構文解析は CodeMatHLSLParser 側。

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

namespace CodeMatSource
{
    /**
     * HLSL / Effects の予約語を関数名に使っているとコンパイルに失敗するため安全な名前へ置換する。
     * (VertexShader → WPO_Main, PixelShader → PS_Main など)
     */
    FString SafeifyHLSL(const FString& Code);

    /** // コメントを除去する */
    FString StripLineComments(const FString& Code);

    /** 裸の Time 参照がある場合に "float Time = View.GameTime;" を先頭へ注入する */
    FString InjectHLSLBuiltins(const FString& Body);

    /**
     * エントリ関数名を返す (コメント内は除外)。
     *   1. 既知のエントリ名 (FragmentShader / MainHLSL / PS_Main / WPO_Main / MainWPO) があればそれ
     *   2. なければ最後の float3/float4 関数 (ヘルパーはエントリより前に書く規約)
     */
    FString DetectFunctionName(const FString& Code);

    /** エントリが float4 を返すなら 4、それ以外は 3 */
    int32 DetectReturnComponents(const FString& Code);

    /** FuncName の { } 内側を返す (見つからなければ空) */
    FString ExtractFunctionBody(const FString& Code, const FString& FuncName);

    /** エントリ関数より前に書かれたヘルパー関数群 (関数定義が無ければ空) */
    FString ExtractHelperFunctions(const FString& Code, const FString& MainFuncName);

    /** "float3 F(float2 uv, float3 C)" → [("float2","uv"), ("float3","C")] */
    TArray<TPair<FString, FString>> ExtractFuncParams(const FString& Code, const FString& FuncName);

    /** // @param 注釈で宣言されたマテリアルパラメータ */
    struct FParamDecl
    {
        enum class EType : uint8 { Scalar, Vector3, Vector4, Texture2D };

        EType        Type = EType::Scalar;
        FString      Name;
        float        DefaultScalar = 0.f;
        FLinearColor DefaultColor  = FLinearColor(0, 0, 0, 1);
        FString      TexturePath;   // Texture2D のみ
    };

    /**
     * 構文 (HLSL コメント内):
     *   // @param float     Name = 0.5
     *   // @param float3    Name = 0.0,0.0,0.0
     *   // @param float4    Name = 0.0,0.0,0.0,1.0
     *   // @param Texture2D Name = /Game/Path/T_Tex.T_Tex   (デフォルト省略可)
     */
    TArray<FParamDecl> ParseParamDecls(const FString& Code);
}

#endif // WITH_EDITOR
