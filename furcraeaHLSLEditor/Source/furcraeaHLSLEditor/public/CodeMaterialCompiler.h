// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Public/CodeMaterialCompiler.h
#pragma once
#include "CoreMinimal.h"
#include "CodeMaterialAsset.h"

class UMaterial;

/** マテリアルグラフ生成の入力 (エディタの未保存テキストからも作れるように Asset と分離) */
struct FURCRAEAHLSLEDITOR_API FCodeMatBuildSettings
{
    FString FragmentCode;
    FString VertexCode;
    ECodeMatDomain            Domain     = ECodeMatDomain::Surface;
    ECodeMatBlendableLocation BlendLoc   = ECodeMatBlendableLocation::AfterTonemapping;
    ECodeMatOutputMode        OutputMode = ECodeMatOutputMode::CustomNode;

    /** Python スクリプトの生成先 (例: /Game/Foo と CMA_Bar) */
    FString AssetName;
    FString AssetFolder;

    static FCodeMatBuildSettings FromAsset(const UCodeMaterialAsset* Asset);
};

namespace CodeMat
{
    /** Asset から M_<Name> を生成 / 更新して保存する。OutLog にはノード変換の注意 (フォールバック等) が入る。 */
    FURCRAEAHLSLEDITOR_API bool CompileCodeAssetToMaterial(
        UCodeMaterialAsset* Asset,
        UMaterial*& OutMaterial,
        FString& OutError,
        FString* OutLog = nullptr
    );

    /** 同じグラフを作る Unreal Python スクリプトを生成する (エディタの Python コンソールに貼って実行) */
    FURCRAEAHLSLEDITOR_API bool ExportPythonScript(
        const FCodeMatBuildSettings& Settings,
        FString& OutScript,
        FString& OutError,
        FString* OutLog = nullptr
    );

    /** マテリアルエディタに Ctrl+V で貼れる T3D テキスト (ノードのコピー形式) を生成する */
    FURCRAEAHLSLEDITOR_API bool ExportT3D(
        const FCodeMatBuildSettings& Settings,
        FString& OutText,
        FString& OutError,
        FString* OutLog = nullptr
    );

    /**
     * T3D をマテリアルに貼り付ける (マテリアルエディタの Ctrl+V と同じ処理)。
     * bConnectOutputs = true なら、コメントが "-> Emissive" 等のノードを出力ピンへ自動接続する。
     */
    FURCRAEAHLSLEDITOR_API bool PasteT3DIntoMaterial(
        UMaterial* Material,
        const FString& T3DText,
        bool bConnectOutputs,
        FString& OutLog
    );
}
