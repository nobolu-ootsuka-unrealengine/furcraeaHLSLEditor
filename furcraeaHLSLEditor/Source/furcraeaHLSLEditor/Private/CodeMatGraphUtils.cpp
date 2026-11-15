// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatGraphUtils.cpp

#include "CodeMatGraphUtils.h"

#if WITH_EDITOR

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace CodeMatGraph
{

TArray<FOutputPin> GetMaterialOutputPins(UMaterialEditorOnlyData* ED)
{
    return {
        { &ED->EmissiveColor,       MP_EmissiveColor },
        { &ED->Opacity,             MP_Opacity },
        { &ED->OpacityMask,         MP_OpacityMask },
        { &ED->WorldPositionOffset, MP_WorldPositionOffset },
        { &ED->BaseColor,           MP_BaseColor },
        { &ED->Metallic,            MP_Metallic },
        { &ED->Specular,            MP_Specular },
        { &ED->Roughness,           MP_Roughness },
        { &ED->Normal,              MP_Normal },
    };
}

FString GetOutputPinDisplayName(EMaterialProperty Property)
{
    return StaticEnum<EMaterialProperty>()->GetDisplayNameTextByValue((int64)Property).ToString();
}

void RemoveUnreachableExpressions(UMaterialEditorOnlyData* ED)
{
    TSet<UMaterialExpression*> Reachable;
    TArray<UMaterialExpression*> Stack;
    for (const FOutputPin& Pin : GetMaterialOutputPins(ED))
        if (Pin.Key->Expression) Stack.Add(Pin.Key->Expression);

    while (Stack.Num() > 0)
    {
        UMaterialExpression* E = Stack.Pop(EAllowShrinking::No);
        if (!E || Reachable.Contains(E)) continue;
        Reachable.Add(E);
        for (FExpressionInputIterator It{ E }; It; ++It)
            if (It.Input->Expression) Stack.Add(It.Input->Expression);
    }

    const TArray<TObjectPtr<UMaterialExpression>> All = ED->ExpressionCollection.Expressions;
    for (UMaterialExpression* E : All)
    {
        if (E && !Reachable.Contains(E) && !E->HasAParameterName())
            ED->ExpressionCollection.RemoveExpression(E);
    }
}

void LayoutNodeGraph(UMaterialEditorOnlyData* ED)
{
    constexpr float ColumnWidth = 320.f;
    constexpr float RowSpacing  = 150.f;
    constexpr float Unconnected = 100000.f;   // 未接続ノード (パラメータ等) は列の下へ

    TArray<UMaterialExpression*> Nodes;
    for (UMaterialExpression* E : ED->ExpressionCollection.Expressions)
        if (E && !E->IsA<UMaterialExpressionComment>()) Nodes.Add(E);

    // 消費者 (このノードの出力を使うノード) と未処理の消費者数
    TMap<UMaterialExpression*, TArray<UMaterialExpression*>> Consumers;
    TMap<UMaterialExpression*, int32> Pending;
    for (UMaterialExpression* E : Nodes) Pending.Add(E, 0);
    for (UMaterialExpression* E : Nodes)
    {
        for (FExpressionInputIterator It{ E }; It; ++It)
        {
            UMaterialExpression* Src = It.Input->Expression;
            if (!Src || !Pending.Contains(Src)) continue;
            Consumers.FindOrAdd(Src).Add(E);
            Pending[Src]++;
        }
    }

    // 出力ピンの並び (Emissive が上) を初期 Y にする
    TMap<UMaterialExpression*, float> RootY;
    {
        int32 i = 0;
        for (const FOutputPin& Pin : GetMaterialOutputPins(ED))
        {
            if (Pin.Key->Expression && !RootY.Contains(Pin.Key->Expression)) RootY.Add(Pin.Key->Expression, i * 160.f);
            ++i;
        }
    }

    // Kahn 法: 消費者 → 生産者の順に深さ (列) を決める
    TMap<UMaterialExpression*, int32> Depth;
    TArray<UMaterialExpression*> Queue;
    for (UMaterialExpression* E : Nodes) if (Pending[E] == 0) { Queue.Add(E); Depth.Add(E, 0); }
    for (int32 q = 0; q < Queue.Num(); ++q)
    {
        UMaterialExpression* E = Queue[q];
        for (FExpressionInputIterator It{ E }; It; ++It)
        {
            UMaterialExpression* Src = It.Input->Expression;
            if (!Src || !Pending.Contains(Src)) continue;
            int32& D = Depth.FindOrAdd(Src, 0);
            D = FMath::Max(D, Depth[E] + 1);
            if (--Pending[Src] == 0) Queue.Add(Src);
        }
    }
    for (UMaterialExpression* E : Nodes) if (!Depth.Contains(E)) Depth.Add(E, 0); // 循環は無いはずだが念のため

    int32 MaxDepth = 0;
    for (const auto& KV : Depth) MaxDepth = FMath::Max(MaxDepth, KV.Value);

    TArray<TArray<UMaterialExpression*>> Columns;
    Columns.SetNum(MaxDepth + 1);
    for (UMaterialExpression* E : Queue) Columns[Depth[E]].Add(E);
    for (UMaterialExpression* E : Nodes) if (!Queue.Contains(E)) Columns[Depth[E]].Add(E);

    TMap<UMaterialExpression*, float> PosY;
    for (int32 d = 0; d <= MaxDepth; ++d)
    {
        TArray<TPair<float, UMaterialExpression*>> Order;
        for (UMaterialExpression* E : Columns[d])
        {
            float Want = Unconnected;
            if (const float* R = RootY.Find(E))
            {
                Want = *R;
            }
            else if (const TArray<UMaterialExpression*>* Cs = Consumers.Find(E))
            {
                float Sum = 0.f;
                int32 Cnt = 0;
                for (UMaterialExpression* C : *Cs) if (const float* Y = PosY.Find(C)) { Sum += *Y; ++Cnt; }
                Want = Cnt > 0 ? Sum / Cnt : 0.f;
            }
            Order.Add(TPair<float, UMaterialExpression*>(Want, E));
        }
        Order.StableSort([](const TPair<float, UMaterialExpression*>& A, const TPair<float, UMaterialExpression*>& B) { return A.Key < B.Key; });

        float Prev = -1e9f;
        for (const auto& O : Order)
        {
            const float Y = FMath::Max(O.Key == Unconnected ? Prev + RowSpacing : O.Key, Prev + RowSpacing);
            Prev = Y;
            PosY.Add(O.Value, Y);
            O.Value->MaterialExpressionEditorX = -(int32)(ColumnWidth * (d + 1));
            O.Value->MaterialExpressionEditorY = (int32)Y;
        }
    }
}

void ReplaceInputMasksWithNodes(UMaterial* Material)
{
    UMaterialEditorOnlyData* ED = Material->GetEditorOnlyData();
    auto Fix = [&](FExpressionInput& In)
    {
        if (!In.Expression || !In.Mask) return;
        auto* M = NewObject<UMaterialExpressionComponentMask>(Material, NAME_None, RF_Transactional);
        M->Material = Material;
        ED->ExpressionCollection.AddExpression(M);
        M->Input.Expression  = In.Expression;
        M->Input.OutputIndex = In.OutputIndex;
        M->R = In.MaskR; M->G = In.MaskG; M->B = In.MaskB; M->A = In.MaskA;
        M->MaterialExpressionEditorX = In.Expression->MaterialExpressionEditorX + 200;
        M->MaterialExpressionEditorY = In.Expression->MaterialExpressionEditorY + 60;
        In.Expression  = M;
        In.OutputIndex = 0;
        In.Mask = 0; In.MaskR = 0; In.MaskG = 0; In.MaskB = 0; In.MaskA = 0;
    };

    const TArray<TObjectPtr<UMaterialExpression>> All = ED->ExpressionCollection.Expressions;
    for (UMaterialExpression* E : All)
    {
        if (!E) continue;
        for (FExpressionInputIterator It{ E }; It; ++It) Fix(*It.Input);
    }
    for (const FOutputPin& Pin : GetMaterialOutputPins(ED)) Fix(*Pin.Key);
}

UMaterialGraph* GetOrCreateMaterialGraph(UMaterial* Material)
{
    if (!Material->MaterialGraph)
    {
        Material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(
            Material, NAME_None, UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass()));
        Material->MaterialGraph->Material = Material;
        Material->MaterialGraph->RebuildGraph();
    }
    return Material->MaterialGraph;
}

} // namespace CodeMatGraph

#endif // WITH_EDITOR
