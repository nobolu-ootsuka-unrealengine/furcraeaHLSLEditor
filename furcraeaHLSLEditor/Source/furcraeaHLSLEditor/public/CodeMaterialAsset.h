#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CodeMaterialAsset.generated.h"

class UMaterial;

UCLASS(BlueprintType)
class FURCRAEAHLSLEDITOR_API UCodeMaterialAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UCodeMaterialAsset();

    // Fragment シェーダーコード（FragmentShader エントリ関数を含む・必須）
    UPROPERTY(EditAnywhere, Category = "HLSL", meta = (MultiLine = true))
    FString FragmentShaderCode;

    // Vertex シェーダーコード（VertexShader エントリ関数を含む・省略可）
    UPROPERTY(EditAnywhere, Category = "HLSL", meta = (MultiLine = true))
    FString VertexShaderCode;

    // �������ꂽMaterial�i�L���b�V���j
    UPROPERTY(VisibleAnywhere, Category = "HLSL")
    TObjectPtr<UMaterial> OutputMaterial;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
