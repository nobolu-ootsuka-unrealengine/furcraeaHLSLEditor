// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditorEditor/Public/CodeMatScriptLibrary.h
//
// Python / Blueprint (Editor Utility) から HLSL → マテリアル変換を呼ぶための関数群。
//
//   asset = unreal.load_asset("/Game/Foo/CMA_Bar")
//   L = unreal.CodeMatScriptLibrary
//   mat, log = L.compile_to_material(asset, unreal.CodeMatOutputMode.MATERIAL_NODES)
//   script, log = L.export_python_script(asset, unreal.CodeMatOutputMode.MATERIAL_NODES)
//   t3d, log = L.export_t3d(asset, unreal.CodeMatOutputMode.MATERIAL_NODES)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CodeMaterialAsset.h"
#include "CodeMatScriptLibrary.generated.h"

class UMaterial;

UCLASS()
class FURCRAEAHLSLEDITOREDITOR_API UCodeMatScriptLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Asset の OutputMode を Mode に設定して M_<Name> を生成・保存する。失敗時は nullptr。 */
    UFUNCTION(BlueprintCallable, Category = "furcraeaHLSL")
    static UMaterial* CompileToMaterial(UCodeMaterialAsset* Asset, ECodeMatOutputMode Mode, FString& Log);

    /** 同じグラフを作る Python スクリプトを返す (Asset は変更しない) */
    UFUNCTION(BlueprintCallable, Category = "furcraeaHLSL")
    static FString ExportPythonScript(UCodeMaterialAsset* Asset, ECodeMatOutputMode Mode, FString& Log);

    /** マテリアルエディタに貼り付けられる T3D テキストを返す (Asset は変更しない) */
    UFUNCTION(BlueprintCallable, Category = "furcraeaHLSL")
    static FString ExportT3D(UCodeMaterialAsset* Asset, ECodeMatOutputMode Mode, FString& Log);

    /**
     * T3D テキストをマテリアルに貼り付ける (マテリアルエディタの Ctrl+V と同じ処理)。
     * bConnectOutputs = true なら、コメントに "-> Emissive" 等が付いたノードを出力ピンへ自動接続する。
     */
    UFUNCTION(BlueprintCallable, Category = "furcraeaHLSL")
    static bool PasteT3DIntoMaterial(UMaterial* Material, const FString& T3DText, bool bConnectOutputs, FString& Log);

    /** マテリアルのシェーダーコンパイル完了を待ち、エラー一覧を返す (空なら成功) */
    UFUNCTION(BlueprintCallable, Category = "furcraeaHLSL")
    static TArray<FString> GetMaterialCompileErrors(UMaterial* Material);
};
