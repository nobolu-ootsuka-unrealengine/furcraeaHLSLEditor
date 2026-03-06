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

    // エディタで編集するHLSLコード本文
    UPROPERTY(EditAnywhere, Category = "HLSL", meta = (MultiLine = true))
    FString HlslCode;

    // 生成されたMaterial（キャッシュ）
    UPROPERTY(VisibleAnywhere, Category = "HLSL")
    TObjectPtr<UMaterial> OutputMaterial;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
