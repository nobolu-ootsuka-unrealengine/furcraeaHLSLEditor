// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatGraphBuilder.h
//
// FCodeMatBuildSettings (HLSL + ドメイン + 出力モード) からマテリアルグラフを構築する。

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

class UMaterial;
struct FCodeMatBuildSettings;

namespace CodeMatGraph
{
    /**
     * Material の既存グラフを破棄して Settings のグラフを構築する。
     *   bFinalize = true : PreEditChange / PostEditChange を行い、シェーダーコンパイルを 1 回だけ起動する
     *   bFinalize = false: エクスポート用の一時マテリアル (コンパイルしない)
     * OutLog にはノード変換の注意 (Custom ノードへのフォールバック等) が追記される。
     */
    void BuildMaterialGraph(UMaterial* Material, const FCodeMatBuildSettings& Settings, bool bFinalize, FString& OutLog);
}

#endif // WITH_EDITOR
