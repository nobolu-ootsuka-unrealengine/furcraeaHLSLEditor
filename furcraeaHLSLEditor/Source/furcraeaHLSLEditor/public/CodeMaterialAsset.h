#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CodeMaterialAsset.generated.h"

class UMaterial;

/** マテリアルドメイン（Surface = 通常メッシュ / PostProcess = ポストプロセス） */
UENUM(BlueprintType)
enum class ECodeMatDomain : uint8
{
    Surface     UMETA(DisplayName = "Surface"),
    PostProcess UMETA(DisplayName = "Post Process"),
};

/** PP マテリアルの Blendable Location（どのパスで合成するか） */
UENUM(BlueprintType)
enum class ECodeMatBlendableLocation : uint8
{
    AfterTonemapping   UMETA(DisplayName = "After Tonemapping"),
    BeforeTonemapping  UMETA(DisplayName = "Before Tonemapping"),
    BeforeTranslucency UMETA(DisplayName = "Before Translucency"),
    ReplacingTonemapper UMETA(DisplayName = "Replacing Tonemapper"),
};

/** 生成するマテリアルグラフの形式 */
UENUM(BlueprintType)
enum class ECodeMatOutputMode : uint8
{
    /** HLSL をそのまま Custom ノード 1 個に埋め込む (従来の動作・全構文対応) */
    CustomNode    UMETA(DisplayName = "Custom Node (HLSL)"),
    /** HLSL を解析して Add / Multiply / Lerp ... の本物のマテリアルノードに展開する */
    MaterialNodes UMETA(DisplayName = "Material Nodes"),
};

UCLASS(BlueprintType)
class FURCRAEAHLSLEDITOR_API UCodeMaterialAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UCodeMaterialAsset();

    /** マテリアルドメイン。PostProcess を選ぶと SceneColor / CustomDepth 等が自動配線される */
    UPROPERTY(EditAnywhere, Category = "HLSL")
    ECodeMatDomain Domain = ECodeMatDomain::Surface;

    /** PP ドメイン時のみ有効: マテリアルを合成するパス */
    UPROPERTY(EditAnywhere, Category = "HLSL")
    ECodeMatBlendableLocation BlendableLocation = ECodeMatBlendableLocation::AfterTonemapping;

    /** 変換先: Custom ノード or マテリアルノード (ノード化できない構文は自動で Custom ノードにフォールバック) */
    UPROPERTY(EditAnywhere, Category = "HLSL")
    ECodeMatOutputMode OutputMode = ECodeMatOutputMode::CustomNode;

    // Fragment シェーダーコード（FragmentShader エントリ関数を含む・必須）
    UPROPERTY(EditAnywhere, Category = "HLSL", meta = (MultiLine = true))
    FString FragmentShaderCode;

    // Vertex シェーダーコード（VertexShader エントリ関数を含む・省略可・Surface のみ有効）
    UPROPERTY(EditAnywhere, Category = "HLSL", meta = (MultiLine = true))
    FString VertexShaderCode;

    // 生成された Material (キャッシュ)
    UPROPERTY(VisibleAnywhere, Category = "HLSL")
    TObjectPtr<UMaterial> OutputMaterial;

    /** 新規アセット / Reset ボタンで使うテンプレート (キャラクターアウトライン) */
    static FString GetDefaultFragmentCode();
    static FString GetDefaultVertexCode();

#if WITH_EDITOR
    virtual void PostLoad() override;
#endif
};
