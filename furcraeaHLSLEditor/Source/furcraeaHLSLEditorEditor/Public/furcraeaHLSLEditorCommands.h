// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "furcraeaHLSLEditorStyle.h"

class FfurcraeaHLSLEditorCommands : public TCommands<FfurcraeaHLSLEditorCommands>
{
public:

	FfurcraeaHLSLEditorCommands()
		: TCommands<FfurcraeaHLSLEditorCommands>(TEXT("furcraeaHLSLEditor"), NSLOCTEXT("Contexts", "furcraeaHLSLEditor", "furcraeaHLSLEditor Plugin"), NAME_None, FfurcraeaHLSLEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};