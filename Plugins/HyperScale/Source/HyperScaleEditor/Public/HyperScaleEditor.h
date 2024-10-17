// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


class FHScale_SchGenSet_AssetActions;

class FHyperScaleEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	void SetupConfigFile();
	
	void RegisterAssetActions();
	void UnregisterAssetActions();
	
	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	static void OpenSchemaGeneratorToolForAsset(FAssetData InAssetData);
	static void OpenSchemaExtractorToolForAsset(FAssetData InAssetData);
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();
	
private:
	void RegisterMenus();

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	
	/** Style set for the header view */
	static TSharedPtr<FSlateStyleSet> HeaderViewStyleSet;
	
	/** Handle to our delegate so we can remove it at module shutdown */
	FDelegateHandle ContentBrowserExtenderDelegateHandle;
};
