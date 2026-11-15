// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatGraphUtils.h
//
// マテリアルグラフ (UMaterialExpression 群) に対する汎用操作。

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "SceneTypes.h"

class UMaterial;
class UMaterialGraph;
class UMaterialEditorOnlyData;
struct FExpressionInput;

namespace CodeMatGraph
{
    using FOutputPin = TPair<FExpressionInput*, EMaterialProperty>;

    /** 生成グラフが使うマテリアル出力ピン (グラフのルート) 一覧 */
    TArray<FOutputPin> GetMaterialOutputPins(UMaterialEditorOnlyData* ED);

    /** マテリアル出力から辿れないノードを削除する (パラメータは MI 用に残す) */
    void RemoveUnreachableExpressions(UMaterialEditorOnlyData* ED);

    /** 列 = 出力からの最長距離、列内の順序 = 接続先の Y 平均 で左方向へ整列する */
    void LayoutNodeGraph(UMaterialEditorOnlyData* ED);

    /**
     * FExpressionInput のマスク (例: VectorParameter.rgb) を ComponentMask ノードに置き換える。
     * Python / T3D はピン接続しか表現できないため、エクスポート前に正規化する。
     */
    void ReplaceInputMasksWithNodes(UMaterial* Material);

    /** マテリアルエディタと同じ手順で UMaterialGraph を (無ければ) 作る */
    UMaterialGraph* GetOrCreateMaterialGraph(UMaterial* Material);

    /** 出力ピンの表示名 ("Emissive" 等) */
    FString GetOutputPinDisplayName(EMaterialProperty Property);
}

#endif // WITH_EDITOR
