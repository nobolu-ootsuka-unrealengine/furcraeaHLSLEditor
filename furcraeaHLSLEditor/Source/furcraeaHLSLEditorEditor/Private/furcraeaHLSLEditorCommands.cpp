// Copyright Epic Games, Inc. All Rights Reserved.

#include "furcraeaHLSLEditorCommands.h"

#define LOCTEXT_NAMESPACE "FfurcraeaHLSLEditorModule"

void FfurcraeaHLSLEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "furcraeaHLSLEditor", "Bring up furcraeaHLSLEditor window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
