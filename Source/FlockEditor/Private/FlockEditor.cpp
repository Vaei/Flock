// Copyright (c) Jared Taylor

#include "FlockEditor.h"

#include "AnimToTextureDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Actors/FlockBlockingVolume.h"
#include "Actors/FlockPerch.h"
#include "Actors/FlockVolume.h"
#include "Components/FlockPerchComponent.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "Data/FlockSpeciesData.h"
#include "Editor.h"
#include "FlockDeveloper.h"
#include "FlockBakeSettings.h"
#include "FlockActorFactory.h"
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
#include "SFlockMenuEntry.h"
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

void FFlockEditorModule::SpawnBlockingVolumeInCurrentLevel(EFlockBlockerShape Shape)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	const bool bBox = Shape == EFlockBlockerShape::Box;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World->GetCurrentLevel(), AFlockBlockingVolume::StaticClass(),
		bBox ? TEXT("FlockBlockingBox") : TEXT("FlockBlockingSphere"));
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	AFlockBlockingVolume* Volume = World->SpawnActor<AFlockBlockingVolume>(
		AFlockBlockingVolume::StaticClass(), FTransform(GetSpawnLocation()), SpawnParams);
	if (!Volume)
	{
		return;
	}

	Volume->Shape = Shape;
	Volume->SetActorLabel(bBox ? TEXT("FlockBlockingBox") : TEXT("FlockBlockingSphere"));

	// Applies Shape to which component is drawn, so the one that was asked for is the one shown.
	Volume->PostEditChange();

	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	GEditor->SelectActor(Volume, /*bInSelected*/ true, /*bNotify*/ true);

	FNotificationInfo Info(LOCTEXT("SpawnedBlockingVolume",
		"Spawned a blocking volume. Scale it over whatever birds should not fly into; nothing else needs "
		"setting."));
	Info.ExpireDuration = 8.f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FFlockEditorModule::SpawnFlockVolumeInCurrentLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	UFlockSpeciesData* Species = UFlockVolumeFactory::FindDefaultSpecies();

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

void FFlockEditorModule::SpawnPerchInCurrentLevel()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World->GetCurrentLevel(), AFlockPerch::StaticClass(),
		TEXT("FlockPerch"));
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	AFlockPerch* Perch = World->SpawnActor<AFlockPerch>(AFlockPerch::StaticClass(),
		FTransform(GetSpawnLocation()), SpawnParams);
	if (!Perch)
	{
		return;
	}

	Perch->SetActorLabel(TEXT("FlockPerch"));

	// Bakes against whatever is under where it landed rather than where it was spawned.
	Perch->PostEditMove(/*bFinished*/ true);

	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	GEditor->SelectActor(Perch, /*bInSelected*/ true, /*bNotify*/ true);

	const int32 Slots = Perch->GetPerch() ? Perch->GetPerch()->BakedSlots.Num() : 0;
	FNotificationInfo Info(Slots > 0
		? FText::Format(LOCTEXT("SpawnedPerch", "Spawned a perch with {0} slot(s). Move or scale it and the "
			"slots re-bake."), Slots)
		: LOCTEXT("SpawnedPerchNoSlots",
			"Spawned a perch, but nothing is under it to stand on. Move it over something, or turn off Trace "
			"Down to keep the slots where the box puts them."));
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

void FFlockEditorModule::AddPlaceEntry(FMenuBuilder& Menu, UClass* FactoryClass, const FText& Label,
	const FText& ToolTip, const FSlateIcon& Icon, FExecuteAction OnClicked)
{
	UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(FactoryClass) : nullptr;
	if (!Factory)
	{
		Menu.AddMenuEntry(Label, ToolTip, Icon, FUIAction(OnClicked));
		return;
	}

	// A widget rather than a menu entry, because a menu entry cannot be dragged out into the level.
	Menu.AddWidget(SNew(SFlockMenuEntry, Factory).Label(Label).ToolTip(ToolTip).Icon(Icon),
		FText::GetEmpty(), true);
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
	Menu.EndSection();

	Menu.BeginSection(TEXT("FlockPlace"), LOCTEXT("FlockPlaceSection", "Place"));
	AddPlaceEntry(Menu, UFlockVolumeFactory::StaticClass(),
		LOCTEXT("SpawnVolume", "Flock Volume"),
		LOCTEXT("SpawnVolumeTip",
			"Where birds live. A species is assigned if one can be worked out. For real use, subclass the "
			"volume as a Blueprint per species instead."),
		FSlateIcon(FFlockEditorStyle::GetStyleSetName(), FFlockEditorStyle::GetMenuIconName()),
		FExecuteAction::CreateStatic(&FFlockEditorModule::SpawnFlockVolumeInCurrentLevel));

	AddPlaceEntry(Menu, UFlockPerchFactory::StaticClass(),
		LOCTEXT("SpawnPerch", "Perch"),
		LOCTEXT("SpawnPerchTip",
			"Somewhere birds can land, where there is nothing to put a perch component on. Its slots bake onto "
			"whatever is under it, and re-bake whenever it is moved or scaled."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.SceneComponent")),
		FExecuteAction::CreateStatic(&FFlockEditorModule::SpawnPerchInCurrentLevel));

	AddPlaceEntry(Menu, UFlockBlockingBoxFactory::StaticClass(),
		LOCTEXT("SpawnBlockingBox", "Blocking Box"),
		LOCTEXT("SpawnBlockingBoxTip",
			"Somewhere birds will not fly. Birds have no collision of any kind, so this is how a flock is "
			"told a building is there. Scale it over what they should stay out of."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.BoxComponent")),
		FExecuteAction::CreateStatic(&FFlockEditorModule::SpawnBlockingVolumeInCurrentLevel,
			EFlockBlockerShape::Box));

	AddPlaceEntry(Menu, UFlockBlockingSphereFactory::StaticClass(),
		LOCTEXT("SpawnBlockingSphere", "Blocking Sphere"),
		LOCTEXT("SpawnBlockingSphereTip",
			"The same, rounded. Better for a tree or anything birds should curve around rather than square "
			"off against."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.SphereComponent")),
		FExecuteAction::CreateStatic(&FFlockEditorModule::SpawnBlockingVolumeInCurrentLevel,
			EFlockBlockerShape::Sphere));
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
	FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);

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
