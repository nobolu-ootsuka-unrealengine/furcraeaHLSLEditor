// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatCustomNode.h
//
// Custom Node モード: エントリ関数を Custom ノード 1 個分の設定 (Code / Inputs / AdditionalDefines) に変換する。
//
// UE は Custom ノードを次のように展開する:
//     #ifndef <DefineName>
//     #define <DefineName> <DefineValue>
//     #endif
//     <Ret> CustomExpressionN(FMaterial{Pixel|Vertex}Parameters Parameters, <inputs>) { <Code> }
//
// ヘルパー関数がある場合は DefineValue を "1\n<ヘルパー群>\n<実装関数>" にして、
// ラッパーより前のグローバルスコープへ置く (HLSL は関数をネストできないため)。
// #ifndef ガードにより、同じノードが複数回展開されても多重定義にならない。
// Code は実装関数を呼ぶだけ:  return <Entry>_impl(Parameters, <args>);

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionCustom.h"

namespace CodeMatCustom
{
    enum class EStage : uint8
    {
        Pixel,   // Emissive / Opacity
        Vertex,  // World Position Offset
    };

    struct FCustomNodeSpec
    {
        ECustomMaterialOutputType OutputType = CMOT_Float3;
        FString Description;                         // ノードのタイトル (エントリ関数名)
        FString Code;                                // UMaterialExpressionCustom::Code
        TArray<TPair<FString, FString>> Inputs;      // (HLSL 型, 名前) — 自動配線は呼び出し側
        TArray<FCustomDefine> Defines;               // ヘルパーがある場合のみ 1 件
        FString Error;                               // 空なら成功
    };

    /** Source 内の FuncName をエントリとした Custom ノードの設定を作る */
    FCustomNodeSpec MakeSpec(const FString& Source, const FString& FuncName, ECustomMaterialOutputType OutputType, EStage Stage);

    /** Spec を Custom ノードへ適用する (Inputs は名前だけ設定し、接続はしない) */
    void ApplySpec(UMaterialExpressionCustom* Custom, const FCustomNodeSpec& Spec);
}

#endif // WITH_EDITOR
