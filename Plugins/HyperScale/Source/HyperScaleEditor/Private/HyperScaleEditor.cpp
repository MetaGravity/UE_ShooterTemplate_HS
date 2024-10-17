// Copyright 2024 Metagravity. All Rights Reserved.

#include "HyperScaleEditor.h"

#include "ContentBrowserModule.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "IAssetTools.h"
#include "Core/HScaleDevSettings.h"
#include "Toolbar/ToolbarHScaleButtonCommands.h"
#include "Toolbar/ToolbarHScaleButtonStyle.h"
#include "IPropertyTypeCustomization.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprint.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ReplicationLayer/Schema/HScaleSchemaDataAsset.h"
#include "Schema/HScaleSchemaToolWidget.h"
#include "Styling/SlateStyleRegistry.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FHyperScaleEditorModule"

TSharedPtr<FSlateStyleSet> FHyperScaleEditorModule::HeaderViewStyleSet;

void FHyperScaleEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	SetupConfigFile();

	// Registers content browser actions for assets like blueprints or C++ classess
	RegisterAssetActions();

	FHScaleToolbarStyle::Create();

	FHScaleToolbarCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FHScaleToolbarCommands::Get().PluginEnableAction,
		FExecuteAction::CreateRaw(this, &FHyperScaleEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHyperScaleEditorModule::RegisterMenus));
}

void FHyperScaleEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UnregisterAssetActions();

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FHScaleToolbarCommands::Unregister();
}

void FHyperScaleEditorModule::SetupConfigFile()
{
#define ENGINE_SECTION TEXT("/Script/Engine.Engine")
#define DRIVER_DEFINITION TEXT("NetDriverDefinitions")

	const FString DefaultEngineIniPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultEngine.ini"));
	const FString DefaultGameIniPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini"));

	const FString FullPathDefaultEngineIniPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*DefaultEngineIniPath);
	const FString FullPathDefaultGameIniPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*DefaultGameIniPath);

	{
		// Load the 'Engine' config file
		const FConfigFile* EngineConfigFile = GConfig ? GConfig->FindConfigFileWithBaseName(FName(TEXT("Engine"))) : nullptr;
		if (EngineConfigFile)
		{
			/////////////////////////////////////////////////
			/// Write default net driver class
			/////////////////////////////////////////////////

			TArray<FString> OutValues;
			EngineConfigFile->GetArray(ENGINE_SECTION, DRIVER_DEFINITION, OutValues);

			bool bHasNetDriverDefinition = false;
			for (const FString& Value : OutValues)
			{
				TArray<FString> Options;
				Value.ParseIntoArray(Options, TEXT(","), true);

				for (const FString& Option : Options)
				{
					if (Option.Contains("DefName") && Option.Contains("HScaleNetDriver"))
					{
						bHasNetDriverDefinition = true;
						break;
					}
				}

				if (bHasNetDriverDefinition)
				{
					break;
				}
			}

			// If the net driver definition doesnt exist, create one (the default one)
			if (!bHasNetDriverDefinition)
			{
				FString DefaultDefinition = TEXT("(DefName=\"HScaleNetDriver\",DriverClassName=\"/Script/HyperScaleRuntime.HScaleNetDriver\",DriverClassNameFallback=\"/Script/HyperScaleRuntime.HScaleNetDriver\")");
				GConfig->SetString(ENGINE_SECTION, DRIVER_DEFINITION, *MoveTemp(DefaultDefinition), FullPathDefaultEngineIniPath);
			}

#undef ENGINE_SECTION
#undef DRIVER_DEFINITION

			/////////////////////////////////////////////////
/// Write default net driver class properties options
/////////////////////////////////////////////////

#define HSCALE_NET_DRIVER_SECTION TEXT("/Script/HyperScaleRuntime.HScaleNetDriver")
			// Sets even net driver properties, because on the first start it could not read them from config file
			UHScaleNetDriver* NetDriver = GetMutableDefault<UHScaleNetDriver>();

#define NET_CONNECTION_CLASS_DEFINITION TEXT("NetConnectionClassName")
			FString OutNetConnectionClass;
			const bool bHasNetConnectionClass = EngineConfigFile->GetString(HSCALE_NET_DRIVER_SECTION, NET_CONNECTION_CLASS_DEFINITION, OutNetConnectionClass);
			if (!bHasNetConnectionClass)
			{
				FString DefaultNetConnectionClass = TEXT("/Script/HyperScaleRuntime.HScaleConnection");
				GConfig->SetString(HSCALE_NET_DRIVER_SECTION, NET_CONNECTION_CLASS_DEFINITION, *DefaultNetConnectionClass, FullPathDefaultEngineIniPath);
				NetDriver->NetConnectionClassName = MoveTemp(DefaultNetConnectionClass);
			}
#undef NET_CONNECTION_CLASS_DEFINITION

#define REP_DRIVER_CLASS_DEFINITION TEXT("ReplicationDriverClassName")
			FString OutRepDriverClass;
			const bool bHasRepDriverClass = EngineConfigFile->GetString(HSCALE_NET_DRIVER_SECTION, REP_DRIVER_CLASS_DEFINITION, OutRepDriverClass);
			if (!bHasRepDriverClass)
			{
				FString DefaultRepDriverClass = TEXT("/Script/HyperScaleRuntime.HScaleRepDriver");
				GConfig->SetString(HSCALE_NET_DRIVER_SECTION, REP_DRIVER_CLASS_DEFINITION, *DefaultRepDriverClass, FullPathDefaultEngineIniPath);
				NetDriver->ReplicationDriverClassName = MoveTemp(DefaultRepDriverClass);
			}
#undef REP_DRIVER_CLASS_DEFINITION

#define CHANNEL_DEFINITION TEXT("+ChannelDefinitions")
			FConfigFile* File = GConfig->Find(FullPathDefaultEngineIniPath);

			TArray<FString> OutActorChannelDef;
			const int32 NumOfDefs = File ? File->GetArray(HSCALE_NET_DRIVER_SECTION, CHANNEL_DEFINITION, OutActorChannelDef) : 0;
			if (File && NumOfDefs == 0)
			{
				// #todo ... Not sure if we need all of these channels (only HScaleActor and control channel are needed)
				TArray<FString> Channels;

				FString HScaleActorChannelDef = TEXT("(ChannelName=Actor, ClassName=/Script/HyperScaleRuntime.HScaleActorChannel, StaticChannelIndex=-1, bTickOnCreate=false, bServerOpen=true, bClientOpen=false, bInitialServer=false, bInitialClient=false)");
				Channels.Add(MoveTemp(HScaleActorChannelDef));

				// FString ActorChannelDef = TEXT("(ChannelName=Actor, ClassName=/Script/Engine.ActorChannel, StaticChannelIndex=-1, bTickOnCreate=false, bServerOpen=true, bClientOpen=false, bInitialServer=false, bInitialClient=false)");
				// Channels.Add(MoveTemp(ActorChannelDef));

				FString ControlChannelDef = TEXT("(ChannelName=Control, ClassName=/Script/Engine.ControlChannel, StaticChannelIndex=0, bTickOnCreate=true, bServerOpen=false, bClientOpen=true, bInitialServer=false, bInitialClient=true)");
				Channels.Add(MoveTemp(ControlChannelDef));

				FString VoiceChannelDef = TEXT("(ChannelName=Voice, ClassName=/Script/Engine.VoiceChannel, StaticChannelIndex=1, bTickOnCreate=true, bServerOpen=true, bClientOpen=true, bInitialServer=true, bInitialClient=true)");
				Channels.Add(MoveTemp(VoiceChannelDef));

				FString DataChannelDef = TEXT("(ChannelName=DataStream, ClassName=/Script/Engine.DataStreamChannel, StaticChannelIndex=2, bTickOnCreate=true, bServerOpen=true, bClientOpen=true, bInitialServer=true, bInitialClient=true)");
				Channels.Add(MoveTemp(DataChannelDef));

				File->SetArray(HSCALE_NET_DRIVER_SECTION, CHANNEL_DEFINITION, Channels);

				FChannelDefinition ActorDefinition;
				ActorDefinition.ChannelName = NAME_Actor;
				ActorDefinition.ClassName = TEXT("/Script/HyperScaleRuntime.HScaleActorChannel");
				ActorDefinition.bServerOpen = true;

				NetDriver->ChannelDefinitions.Add(MoveTemp(ActorDefinition));
			}
#undef CHANNEL_DEFINITION

#undef HSCALE_NET_DRIVER_SECTION

			/////////////////////////////////////////////////
			/// Save config if something changed
			/////////////////////////////////////////////////

			if (!bHasNetDriverDefinition || !bHasNetConnectionClass || !bHasRepDriverClass || NumOfDefs == 0)
			{
				// Checkout file via source control if is that possible
				if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPathDefaultEngineIniPath))
				{
					if (ISourceControlModule::Get().IsEnabled())
					{
						FText ErrorMessage;

						if (!SourceControlHelpers::CheckoutOrMarkForAdd(FullPathDefaultEngineIniPath, FText::FromString(FullPathDefaultEngineIniPath), NULL, ErrorMessage))
						{
							FNotificationInfo Info(ErrorMessage);
							Info.ExpireDuration = 3.0f;
							FSlateNotificationManager::Get().AddNotification(Info);
						}
					}
					else
					{
						if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*FullPathDefaultEngineIniPath, false))
						{
							FNotificationInfo Info(FText::Format(LOCTEXT("FailedToMakeWritable", "Could not make {0} writable."), FText::FromString(FullPathDefaultEngineIniPath)));
							Info.ExpireDuration = 3.0f;
							FSlateNotificationManager::Get().AddNotification(Info);
						}
					}
				}

				GConfig->Flush(false, FullPathDefaultEngineIniPath);
			}
		}
	}

	{
#define ASSET_MANAGER_SECTION TEXT("/Script/Engine.AssetManagerSettings")
#define PRIMARY_ASSETS_DEFINITION TEXT("PrimaryAssetTypesToScan")

		const FConfigFile* GameConfigFile = GConfig ? GConfig->FindConfigFileWithBaseName(FName(TEXT("Game"))) : nullptr;
		if (GameConfigFile)
		{
			TArray<FString> OutValues;
			GameConfigFile->GetArray(ASSET_MANAGER_SECTION, PRIMARY_ASSETS_DEFINITION, OutValues);

			bool bHasAssetToScan = false;
			for (const FString& Value : OutValues)
			{
				// Try to find out if the config file contains asset about schema details
				if (Value.Contains(TEXT("PrimaryAssetType=\"HScaleSchemaDataAsset\"")))
				{
					bHasAssetToScan = true;
					break;
				}
			}

			if (!bHasAssetToScan)
			{
				FConfigFile* File = GConfig->Find(FullPathDefaultGameIniPath);
				FString DefaultDefinition = TEXT("(PrimaryAssetType=\"HScaleSchemaDataAsset\",AssetBaseClass=\"/Script/HyperScaleRuntime.HScaleSchemaDataAsset\",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=((Path=\"/Game\")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=Unknown))");
				OutValues.Add(DefaultDefinition);
				File->SetArray(ASSET_MANAGER_SECTION, PRIMARY_ASSETS_DEFINITION, MoveTemp(OutValues));

				FPrimaryAssetTypeInfo AssetData = FPrimaryAssetTypeInfo(TEXT("HScaleSchemaDataAsset"), UHScaleSchemaDataAsset::StaticClass(), false, true);
				AssetData.GetDirectories().Add(FDirectoryPath{TEXT("/Game")});

				UAssetManagerSettings* Settings = GetMutableDefault<UAssetManagerSettings>();
				Settings->PrimaryAssetTypesToScan.Add(AssetData);
			}
		}
#undef PRIMARY_ASSETS_DEFINITION
#undef ASSET_MANAGER_SECTION
	}
}


void FHyperScaleEditorModule::RegisterAssetActions()
{
	// Code copied and modified based on FBlueprintHeaderViewModule class

	if (!HeaderViewStyleSet)
	{
		const FString PluginContentDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Editor/BlueprintHeaderView/Content")));
		HeaderViewStyleSet = MakeShared<FSlateStyleSet>("HeaderViewStyle");
		HeaderViewStyleSet->SetContentRoot(PluginContentDir);
		HeaderViewStyleSet->Set("Icons.HeaderView", new FSlateVectorImageBrush(HeaderViewStyleSet->RootToContentDir("BlueprintHeader_16", TEXT(".svg")), CoreStyleConstants::Icon16x16));
	}

	if (!ContentBrowserExtenderDelegateHandle.IsValid())
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FHyperScaleEditorModule::OnExtendContentBrowserAssetSelectionMenu));
		ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
	}
}

void FHyperScaleEditorModule::UnregisterAssetActions()
{
	// Code copied and modified based on FBlueprintHeaderViewModule class

	if (ContentBrowserExtenderDelegateHandle.IsValid())
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.RemoveAll([ContentBrowserExtenderDelegateHandle=ContentBrowserExtenderDelegateHandle](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
		{
			return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle;
		});

		ContentBrowserExtenderDelegateHandle.Reset();
	}

	if (HeaderViewStyleSet)
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*HeaderViewStyleSet);
		HeaderViewStyleSet.Reset();
	}
}

TSharedRef<FExtender> FHyperScaleEditorModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	// Code copied and modified based on FBlueprintHeaderViewModule class

	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	if (SelectedAssets.Num() == 1)
	{
		const FAssetData& AssetData = SelectedAssets[0];
		const bool bIsBlueprint = AssetData.GetClass() == UBlueprint::StaticClass();
		const bool bIsSchemaAsset = AssetData.GetClass() == UHScaleSchemaDataAsset::StaticClass();
		const bool bIsNativeClass = bIsBlueprint ? true : AssetData.GetSoftObjectPath().ToString().StartsWith(TEXT("/Script"));
		if (bIsBlueprint || bIsNativeClass)
		{
			Extender->AddMenuExtension("GetAssetActions", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda(
					[AssetData](FMenuBuilder& MenuBuilder)
					{
						MenuBuilder.AddMenuEntry(
							LOCTEXT("OpenSchemaTool", "Open Schema generator"),
							LOCTEXT("OpenSchemaToolTooltip", "Opens tool for generating schema details."),
							FSlateIcon(HeaderViewStyleSet->GetStyleSetName(), "Icons.HeaderView"),
							FUIAction(FExecuteAction::CreateStatic(&FHyperScaleEditorModule::OpenSchemaGeneratorToolForAsset, AssetData))
							);
					})
				);
		}
		else if (bIsSchemaAsset)
		{
			Extender->AddMenuExtension("GetAssetActions", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda(
					[AssetData](FMenuBuilder& MenuBuilder)
					{
						MenuBuilder.AddMenuEntry(
							LOCTEXT("GenerateSchemaTool", "Generate Schema"),
							LOCTEXT("GenerateSchemaToolTooltip", "Generate schema file."),
							FSlateIcon(HeaderViewStyleSet->GetStyleSetName(), "Icons.HeaderView"),
							FUIAction(FExecuteAction::CreateStatic(&FHyperScaleEditorModule::OpenSchemaExtractorToolForAsset, AssetData))
							);
					})
				);
		}
	}

	return Extender;
}

void FHyperScaleEditorModule::OpenSchemaGeneratorToolForAsset(FAssetData InAssetData)
{
	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	
	UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/HyperScale/EUW_SchemaTool"));
	ensureMsgf(WidgetBlueprint && WidgetBlueprint->GeneratedClass, TEXT("Null generated class for WidgetBlueprint [%s]"), *WidgetBlueprint->GetName());
	
	if (WidgetBlueprint && WidgetBlueprint->GeneratedClass && WidgetBlueprint->GeneratedClass->IsChildOf(UEditorUtilityWidget::StaticClass()))
	{
		if (UEditorUtilityWidgetBlueprint* EditorWidget = Cast<UEditorUtilityWidgetBlueprint>(WidgetBlueprint))
		{
			UHScaleSchemaToolWidget* Widget = Cast<UHScaleSchemaToolWidget>(EditorUtilitySubsystem->SpawnAndRegisterTab(EditorWidget));
			if(ensure(Widget))
			{
				FSoftObjectPath SoftObjectPath;
				if(InAssetData.PackageName.ToString().StartsWith(TEXT("/Script")))
				{
					SoftObjectPath = FSoftObjectPath(InAssetData.PackageName, FName(InAssetData.AssetName.ToString()), "");
				}
				else
				{
					SoftObjectPath = FSoftObjectPath(InAssetData.PackageName, FName(InAssetData.AssetName.ToString() + TEXT("_C")), "");
				}

				TSoftClassPtr<UObject> SoftClassPath = TSoftClassPtr<UObject>(SoftObjectPath);
				UClass* LoadedClass = SoftClassPath.LoadSynchronous();
				
				Widget->OnClassAssigned(LoadedClass);
			}
		}
	}
}

void FHyperScaleEditorModule::OpenSchemaExtractorToolForAsset(FAssetData InAssetData)
{
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	UHScaleSchemaDataAsset* LoadedObject = Cast<UHScaleSchemaDataAsset>(Streamable.LoadSynchronous(InAssetData.GetSoftObjectPath()));
	if (LoadedObject)
	{
		LoadedObject->ExtractData();
	}
}

void FHyperScaleEditorModule::PluginButtonClicked()
{
	UHScaleDevSettings::SetHyperScaleEnabled(!UHScaleDevSettings::GetIsHyperScaleEnabled());
	GetMutableDefault<UHScaleDevSettings>()->SaveConfig();
}

void FHyperScaleEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FHScaleToolbarCommands::Get().PluginEnableAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				// Create the Quark status button
				FToolMenuEntry* QuarkButtonEntry = &Section.AddEntry(FToolMenuEntry::InitToolBarButton(FHScaleToolbarCommands::Get().PluginEnableAction));
				QuarkButtonEntry->SetCommandList(PluginCommands);
				QuarkButtonEntry->Icon = TAttribute<FSlateIcon>::CreateStatic(&FHScaleToolbarStyle::GetQuarkStatusIcon);

				// Initialize the combo button
				TSharedRef<SComboButton> ServerSelectorComboButton = SNew(SComboButton)
					.IsEnabled_Lambda([]() { return UHScaleDevSettings::GetIsHyperScaleEnabled(); })
					.OnGetMenuContent_Lambda([]()
					{
						FMenuBuilder MenuBuilder(true, nullptr);
						const TArray<FHScale_ServerConfig>& serverList = UHScaleDevSettings::GetEditorServerList().GetAvailableServers();
						for (const auto& server : serverList)
						{
							TSharedRef<SButton> serverEntry = SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "FlatButton")
								.Text(server.GetDisplayName())
								.ToolTipText(FText::FromString(server.ToString()))
								.OnClicked_Lambda([server]()
								{
									UHScaleDevSettings::GetEditorServerList_Mutable().SelectExistingServer(server);

									return FReply::Handled();
								});
							MenuBuilder.AddWidget(serverEntry, FText());
						}
						return MenuBuilder.MakeWidget();
					})
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text_Lambda([]() { return UHScaleDevSettings::GetEditorServerList().GetSelectedServer().GetDisplayName(); })
					];

				// Add the menu entry to the section
				Section.AddEntry(FToolMenuEntry::InitWidget(
					"ServerSelectorComboButton",
					ServerSelectorComboButton,
					FText::FromString("Server Selector")
					));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHyperScaleEditorModule, HyperScaleEditor)