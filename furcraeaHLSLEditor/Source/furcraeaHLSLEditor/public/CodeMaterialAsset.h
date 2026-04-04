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

    // Fragment シェーダーコード（FragmentShader エントリ関数を含む・必須）
    UPROPERTY(EditAnywhere, Category = "HLSL", meta = (MultiLine = true))
    FString FragmentShaderCode;

    // Vertex シェーダーコード（VertexShader エントリ関数を含む・省略可・Surface のみ有効）
    UPROPERTY(EditAnywhere, Category = "HLSL", meta = (MultiLine = true))
    FString VertexShaderCode;

    // �������ꂽMaterial�i�L���b�V���j
    UPROPERTY(VisibleAnywhere, Category = "HLSL")
    TObjectPtr<UMaterial> OutputMaterial;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostLoad() override;
#endif
};
