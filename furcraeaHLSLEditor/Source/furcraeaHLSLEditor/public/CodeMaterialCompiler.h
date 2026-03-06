// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Public/CodeMaterialCompiler.h
#pragma once
#include "CoreMinimal.h"

class UCodeMaterialAsset;
class UMaterial;

namespace CodeMat
{
    // ★これを付ける（Runtimeモジュール名の API マクロ）
    FURCRAEAHLSLEDITOR_API bool CompileCodeAssetToMaterial(
        UCodeMaterialAsset* Asset,
        UMaterial*& OutMaterial,
        FString& OutError
    );
}
