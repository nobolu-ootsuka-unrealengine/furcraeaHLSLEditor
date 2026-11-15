// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Private/CodeMatCodeValidation.h
//
// 保存前のシェーダーコード整形と簡易チェック (エントリ関数のシグネチャと return の有無)。

#pragma once

#include "CoreMinimal.h"

namespace CodeMatValidation
{
    /** 改行を LF に統一し、行末の空白を削除して末尾の改行を 1 つにする */
    FString NormalizeCode(const FString& In);

    bool IsWhitespaceOnly(const FString& S);

    /** Fragment (必須): float3/float4 <Name>(float2 uv, ...) または (float3 WorldPos, ...) */
    bool ValidateFragmentCode(const FString& Code, FString& OutError);

    /** Vertex (任意): float3 <Name>(float3 VertexNormal, ...)。全てコメントなら WPO 無効として OK */
    bool ValidateVertexCode(const FString& Code, FString& OutError);
}
