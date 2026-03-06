#pragma once
#include "CoreMinimal.h"

struct FCodeMatParseResult
{
    FString MainBody;
    TArray<FString> Inputs;
    TArray<FString> InputTypes;
    FString OutputType = TEXT("float3");

    bool bOk = true;
    FString Error;
};

class FCodeMaterialParser
{
public:
    static bool Parse(const FString& Source, FCodeMatParseResult& Out, FString& OutError);
};
