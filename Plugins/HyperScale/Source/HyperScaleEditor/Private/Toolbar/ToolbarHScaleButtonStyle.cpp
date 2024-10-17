// Copyright Epic Games, Inc. All Rights Reserved.

#include "Toolbar/ToolbarHScaleButtonStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Core/HScaleDevSettings.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<ISlateStyle> FHScaleToolbarStyle::StyleInstance = nullptr;

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

void FHScaleToolbarStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("HyperScale")->GetBaseDir() / TEXT("Resources"));
	
	//#todo .. use vector brush to get better results - IMAGE_BRUSH_SVG
	Style->Set("HScaleToolbar.QuarkEnabled", new IMAGE_BRUSH(TEXT("T_Hyperscale_On"), Icon20x20));
	Style->Set("HScaleToolbar.QuarkDisabled", new IMAGE_BRUSH(TEXT("T_Hyperscale_Off"), Icon20x20));

	if (StyleInstance.IsValid())
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance.Get());

	StyleInstance = Style;

	if (StyleInstance.IsValid())
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance.Get());
}

FSlateIcon FHScaleToolbarStyle::GetQuarkStatusIcon()
{
	return FSlateIcon(GetStyleSetName(), UHScaleDevSettings::GetIsHyperScaleEnabled() ? "HScaleToolbar.QuarkEnabled" : "HScaleToolbar.QuarkDisabled");
}
