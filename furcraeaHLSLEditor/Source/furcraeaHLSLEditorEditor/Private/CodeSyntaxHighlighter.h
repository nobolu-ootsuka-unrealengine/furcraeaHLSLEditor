// Private/CodeSyntaxHighlighter.h
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class ITextLayoutMarshaller;

class FCodeSyntaxHighlighter
{
public:
    // SMultiLineEditableTextBox に渡すやつ
    static TSharedRef<ITextLayoutMarshaller> Create();
};
