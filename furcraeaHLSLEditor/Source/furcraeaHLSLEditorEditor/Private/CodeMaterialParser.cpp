#include "CodeMaterialParser.h"

bool FCodeMaterialParser::Parse(const FString& Source, FCodeMatParseResult& Out, FString& OutError)
{
    Out = FCodeMatParseResult();
    Out.MainBody = Source;

    OutError.Reset();
    return true;
}
