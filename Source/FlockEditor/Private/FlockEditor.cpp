// Copyright (c) Jared Taylor. All Rights Reserved

#include "FlockEditor.h"

#include "AnimToTextureDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Actors/FlockVolume.h"
#include "Components/FlockPerchComponent.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Data/FlockSpeciesData.h"
#include "Editor.h"
#include "FlockDeveloper.h"
#include "FlockBakeSettings.h"
#include "FlockDetails.h"
#include "FlockEditorLog.h"
#include "FlockEditorStyle.h"
#include "FlockPoseMatch.h"
#include "LevelEditorViewport.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "FlockEditorUserSettings.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "SFlockBakeWindow.h"
#include "ToolMenus.h"
#include "Debug/FlockVATPreview.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FlockEditor"

DEFINE_LOG_CATEGORY(LogFlockEditor);

bool FFlockEditorModule::IsToolbarMenuEnabled()
{
	return GetDefault<UFlockEditorUserSettings>()->bShowToolbarMenu;
}

void FFlockEditorModule::HideToolbarMenu()
{
	UFlockEditorUserSettings* Settings = GetMutableDefault<UFlockEditorUserSettings>();
	Settings->bShowToolbarMenu = false;
	Settings->SaveConfig();
}

void FFlockEditorModule::OpenSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
	{
		const UFlockEditorUserSettings* Settings = GetDefault<UFlockEditorUserSettings>();
		SettingsModule->ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(),
			Settings->GetSectionName());
	}
}

bool FFlockEditorModule::CanSpawnPreview()
{
	return GEditor && GEditor->GetEditorWorldContext().World() != nullptr;
}

FVector FFlockEditorModule::GetSpawnLocation()
{
	// In front of the camera if there is one, so it does not land somewhere off screen.
	if (const FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr)
	{
		if (const FLevelEditorViewportClient* ViewportClient =
			static_cast<FLevelEditorViewportClient*>(Viewport->GetClient()))
		{
			return ViewportClient->GetViewLocation() + ViewportClient->GetViewRotation().Vector() * 500.f;
		}
	}
	return FVector::ZeroVector;
}

void FFlockEditorModule::SpawnFlockVolumeInCurrentLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	// Prefer the project default, then a species built on whatever the bake window is pointed at, then the
	// only species in the project. Anything less certain is left for the user to pick.
	UFlockSpeciesData* Species = UFlockDeveloperSettings::Get().DefaultSpecies.LoadSynchronous();

	if (!Species)
	{
		const IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByClass(UFlockSpeciesData::StaticClass()->GetClassPathName(), Assets);

		const UAnimToTextureDataAsset* WantedAnimData = UFlockBakeSettings::Get()->Recipe.BoneDataAsset.LoadSynchronous();

		for (const FAssetData& Asset : Assets)
		{
			UFlockSpeciesData* Candidate = Cast<UFlockSpeciesData>(Asset.GetAsset());
			if (Candidate && WantedAnimData && Candidate->AnimData.LoadSynchronous() == WantedAnimData)
			{
				Species = Candidate;
				break;
			}
		}

		if (!Species && Assets.Num() == 1)
		{
			Species = Cast<UFlockSpeciesData>(Assets[0].GetAsset());
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World->GetCurrentLevel(), AFlockVolume::StaticClass(),
		TEXT("FlockVolume"));
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	AFlockVolume* Volume = World->SpawnActor<AFlockVolume>(AFlockVolume::StaticClass(),
		FTransform(GetSpawnLocation()), SpawnParams);
	if (!Volume)
	{
		return;
	}

	Volume->Species = Species;

	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	GEditor->SelectActor(Volume, /*bInSelected*/ true, /*bNotify*/ true);

	FNotificationInfo Info(Species
		? FText::Format(LOCTEXT("SpawnedVolume", "Spawned a flock volume using {0}. Play to see it."),
			FText::FromString(Species->GetName()))
		: LOCTEXT("SpawnedVolumeNoSpecies",
			"Spawned a flock volume, but found no species to assign. Set Species on it, or set a Default "
			"Species in Project Settings."));
	Info.ExpireDuration = 8.f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FFlockEditorModule::SpawnPreviewInCurrentLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	const UFlockBakeSettings* Settings = UFlockBakeSettings::Get();

	// Prefer whichever mode the bake window is actually set up for.
	UAnimToTextureDataAsset* DataAsset = Settings->Recipe.bBakeBoneAnimation
		? Settings->Recipe.BoneDataAsset.LoadSynchronous() : nullptr;
	if (!DataAsset && Settings->Recipe.bBakeVertexAnimation)
	{
		DataAsset = Settings->Recipe.VertexDataAsset.LoadSynchronous();
	}

	if (!DataAsset)
	{
		FNotificationInfo Info(LOCTEXT("NoDataAssetToPreview",
			"Assign a data asset in the bake window first - the preview needs one to know what to play."));
		Info.ExpireDuration = 8.f;
		Info.Hyperlink = FSimpleDelegate::CreateStatic(&SFlockBakeWindow::Open);
		Info.HyperlinkText = LOCTEXT("OpenBakeWindow", "Open Bake Animation Textures");
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	const FVector Location = GetSpawnLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World->GetCurrentLevel(), AFlockVATPreview::StaticClass(),
		TEXT("FlockVATPreview"));
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	AFlockVATPreview* Preview = World->SpawnActor<AFlockVATPreview>(
		AFlockVATPreview::StaticClass(), FTransform(Location), SpawnParams);
	if (!Preview)
	{
		return;
	}

	Preview->AnimData = DataAsset;
	Preview->SetActorLabel(FString::Printf(TEXT("FlockPreview_%s"), *DataAsset->GetName()));

	// Re-run construction so the instances appear immediately rather than on the next property edit.
	Preview->RerunConstructionScripts();

	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	GEditor->SelectActor(Preview, /*bInSelected*/ true, /*bNotify*/ true);

	FNotificationInfo Info(FText::Format(LOCTEXT("SpawnedPreview", "Spawned a preview using {0}."),
		FText::FromString(DataAsset->GetName())));
	Info.ExpireDuration = 6.f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FFlockEditorModule::BuildPoseMatchTable()
{
	UFlockSpeciesData* Species = UFlockBakeSettings::Get()->Species.LoadSynchronous();

	FText Error;
	const bool bBuilt = FFlockPoseMatch::Build(Species, Error);

	FNotificationInfo Info(bBuilt
		? FText::Format(LOCTEXT("BuiltPoseMatch", "Built the pose match table for {0}."),
			FText::FromString(Species->GetName()))
		: Error);
	Info.ExpireDuration = 8.f;

	if (!bBuilt)
	{
		Info.Hyperlink = FSimpleDelegate::CreateStatic(&SFlockBakeWindow::Open);
		Info.HyperlinkText = LOCTEXT("OpenBakeWindowForPoseMatch", "Open Bake Animation Textures");
	}

	FSlateNotificationManager::Get().AddNotification(Info);
}

void FFlockEditorModule::RebuildAllPerchesInLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RebuildPerches", "Rebuild Flock Perch Slots"));

	int32 Components = 0;
	int32 Slots = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (UFlockPerchComponent* Perch : TInlineComponentArray<UFlockPerchComponent*>(*It))
		{
			Perch->RebuildSlots();
			++Components;
			Slots += Perch->BakedSlots.Num();
		}
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("RebuiltPerches", "Rebuilt {0} perch component(s), {1} slot(s) total."),
		Components, Slots));
	Info.ExpireDuration = 6.f;
	FSlateNotificationManager::Get().AddNotification(Info);

	UE_LOG(LogFlockEditor, Log, TEXT("Rebuilt %d perch components, %d slots."), Components, Slots);
}

void FFlockEditorModule::StartupModule()
{
	FFlockEditorStyle::Register();

	FPropertyEditorModule& PropertyModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	PropertyModule.RegisterCustomClassLayout(AFlockVATPreview::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FFlockVATPreviewDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();

	if (FSlateApplication::IsInitialized())
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FFlockEditorModule::RegisterMenus));
	}
}

void FFlockEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolBar = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"));
	if (!ToolBar)
	{
		return;
	}

	FToolMenuEntry Entry = FToolMenuEntry::InitComboButton(
		TEXT("FlockMenu"),
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction(),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateStatic(&FFlockEditorModule::IsToolbarMenuEnabled)),
		FOnGetContent::CreateRaw(this, &FFlockEditorModule::BuildMenu),
		LOCTEXT("FlockToolbar", "Flock"),
		LOCTEXT("FlockToolbarTip", "Bird flock tools"),
		FSlateIcon(FFlockEditorStyle::GetStyleSetName(), FFlockEditorStyle::GetMenuIconName())
	);

	// The style that gives a toolbar button its label beside the icon, and the position that puts it
	// with the other plugin menus rather than in among the play controls.
	Entry.StyleNameOverride = TEXT("CalloutToolbar");
	Entry.InsertPosition = FToolMenuInsert(TEXT("NinjaTools"), EToolMenuInsertType::After);

	ToolBar->FindOrAddSection(TEXT("PlayGameExtensions")).AddEntry(Entry);
}

TSharedRef<SWidget> FFlockEditorModule::BuildMenu()
{
	FMenuBuilder Menu(/*bInShouldCloseWindowAfterMenuSelection*/ true, nullptr);

	Menu.BeginSection(TEXT("FlockAnimation"), LOCTEXT("FlockAnimationSection", "Vertex Animation"));
	Menu.AddMenuEntry(
		LOCTEXT("BakeTextures", "Bake Animation Textures..."),
		LOCTEXT("BakeTexturesTip",
			"Prepare a bird's static mesh, textures and data asset, and bake its animations into them. "
			"Replaces the AnimToTexture plugin's BP_AnimToTexture utility."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
		FUIAction(FExecuteAction::CreateStatic(&SFlockBakeWindow::Open)));

	Menu.AddMenuEntry(
		LOCTEXT("BuildPoseMatch", "Build Pose Match Table"),
		LOCTEXT("BuildPoseMatchTip",
			"Works out, for every baked frame, which frame of each animation is the closest pose to it, so a "
			"bird changing clip opens the new one near the pose it was holding. Runs on the bake window's "
			"species and leaves its textures alone."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Adjust")),
		FUIAction(FExecuteAction::CreateStatic(&FFlockEditorModule::BuildPoseMatchTable)));

	Menu.AddMenuEntry(
		LOCTEXT("SpawnPreview", "Spawn Preview in Current Level"),
		LOCTEXT("SpawnPreviewTip",
			"Drops a preview actor in front of the camera, wired to the bake window's data asset, and plays "
			"it in the viewport."),
		FSlateIcon(FFlockEditorStyle::GetStyleSetName(), FFlockEditorStyle::GetMenuIconName()),
		FUIAction(FExecuteAction::CreateStatic(&FFlockEditorModule::SpawnPreviewInCurrentLevel),
			FCanExecuteAction::CreateStatic(&FFlockEditorModule::CanSpawnPreview)));
	Menu.EndSection();

	Menu.BeginSection(TEXT("FlockLevel"), LOCTEXT("FlockLevelSection", "This Level"));
	Menu.AddMenuEntry(
		LOCTEXT("RebuildPerchesEntry", "Bake All Perches in Level"),
		LOCTEXT("RebuildPerchesEntryTip",
			"Re-derives the slots on every perch component in the open level. Rebuilds placed instances; a "
			"perch inside a Blueprint is not written back to the asset."),
		FSlateIcon(FFlockEditorStyle::GetStyleSetName(), FFlockEditorStyle::GetMenuIconName()),
		FUIAction(FExecuteAction::CreateStatic(&FFlockEditorModule::RebuildAllPerchesInLevel),
			FCanExecuteAction::CreateStatic(&FFlockEditorModule::CanSpawnPreview)));
	Menu.AddMenuEntry(
		LOCTEXT("SpawnVolume", "Spawn Flock Volume in Current Level"),
		LOCTEXT("SpawnVolumeTip",
			"Drops a flock volume in front of the camera and assigns a species if one can be worked out. "
			"For real use, subclass the volume as a Blueprint per species instead."),
		FSlateIcon(FFlockEditorStyle::GetStyleSetName(), FFlockEditorStyle::GetMenuIconName()),
		FUIAction(FExecuteAction::CreateStatic(&FFlockEditorModule::SpawnFlockVolumeInCurrentLevel),
			FCanExecuteAction::CreateStatic(&FFlockEditorModule::CanSpawnPreview)));
	Menu.EndSection();

	Menu.BeginSection(TEXT("FlockSettings"), LOCTEXT("FlockSettingsSection", "Settings"));
	Menu.AddMenuEntry(
		LOCTEXT("EditorSettings", "Editor Preferences"),
		LOCTEXT("EditorSettingsTip", "Per-developer Flock editor settings. Not checked in."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Toolbar.Settings")),
		FUIAction(FExecuteAction::CreateStatic(&FFlockEditorModule::OpenSettings)));

	Menu.AddMenuEntry(
		LOCTEXT("HideMenu", "Hide This Menu"),
		LOCTEXT("HideMenuTip",
			"Removes the Flock button from your toolbar. Turn it back on under Editor Preferences, Plugins, "
			"Flock Editor."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Visibility")),
		FUIAction(FExecuteAction::CreateStatic(&FFlockEditorModule::HideToolbarMenu)));
	Menu.EndSection();

	return Menu.MakeWidget();
}

void FFlockEditorModule::ShutdownModule()
{
	FFlockEditorStyle::Unregister();

	if (FPropertyEditorModule* PropertyModule =
		FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
	{
		PropertyModule->UnregisterCustomClassLayout(AFlockVATPreview::StaticClass()->GetFName());
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFlockEditorModule, FlockEditor)
