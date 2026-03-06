#pragma once

#include "CoreMinimal.h"

class UCodeMaterialAsset;
struct FCodeMatParseResult;

class FCodeMaterialGenerator
{
public:
    static bool GenerateOrUpdate(UCodeMaterialAsset* Asset, const FCodeMatParseResult& Parse, FString& OutLog);
};
