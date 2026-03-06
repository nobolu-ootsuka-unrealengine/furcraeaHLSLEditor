//D:\Sandbox\UE57CreatePlugin4\UE57HLSLEdit\UE57HLSLEdit\Plugins\furcraeaHLSLEditor\Source\furcraeaHLSLEditor\Private\CodeMaterialAsset.cpp

#include "CodeMaterialAsset.h"
#include "CodeMaterialCompiler.h" // ← Public にあるやつ

UCodeMaterialAsset::UCodeMaterialAsset()
{
    // “初期状態で何も無い”だとエディタの見た目が寂しいので、最小のテンプレを入れる
    // （あなたのパーサ/ジェネレータ仕様に合わせて、ここは好きに変えてOK）

    HlslCode =
        TEXT("// CodeMaterialAsset template\n")
        TEXT("// You can write free HLSL here (functions, structs, includes, etc.).\n")
        TEXT("// This text will be written to:\n")
        TEXT("//   Plugins/furcraeaHLSLEditor/Shaders/Private/CodeMatUser.ush\n")
        TEXT("// and included from the material custom node.\n")
        TEXT("\n")
        TEXT("#pragma once\n\n")
        TEXT("float3 MainHLSL(float2 uv)\n")
        TEXT("{\n")
        TEXT("    return float3(uv.x, uv.y, 0.0);\n")
        TEXT("}\n");

        
    OutputMaterial = nullptr;
}

#if WITH_EDITOR
void UCodeMaterialAsset::PostEditChangeProperty(FPropertyChangedEvent& E)
{
    Super::PostEditChangeProperty(E);
    // 自動コンパイルはしない（Saveボタン側だけに集約）
}
#endif
