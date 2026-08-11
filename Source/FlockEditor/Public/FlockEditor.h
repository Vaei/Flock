// Copyright (c) Jared Taylor

#pragma once

#include "FlockTypes.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"

class FFlockEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	TSharedRef<SWidget> BuildMenu();

	/** A menu row that can be dragged into the level, falling back to a plain clickable entry. */
	static void AddPlaceEntry(class FMenuBuilder& Menu, UClass* FactoryClass, const FText& Label,
		const FText& ToolTip, const FSlateIcon& Icon, FExecuteAction OnClicked);

	/** Adds a category to the Place Actors panel, so a volume can be dragged into the level. */
	void RegisterPlacement();

	/** Drives the toolbar entry's visibility, so toggling the setting hides it without a restart. */
	static bool IsToolbarMenuEnabled();
	static void HideToolbarMenu();
	static void OpenSettings();

	/** Drops a preview actor into the open level, wired to whichever data asset the bake window is set to. */
	static void SpawnPreviewInCurrentLevel();

	/** Drops a flock volume into the open level, with a species assigned if one can be found. */
	static void SpawnFlockVolumeInCurrentLevel();

	/** Drops somewhere birds will not fly into the open level, sized to be visible and then adjusted. */
	static void SpawnBlockingVolumeInCurrentLevel(EFlockBlockerShape Shape);

	/** Re-bakes every perch component in the open level. */
	static void RebuildAllPerchesInLevel();

	/** Rebuilds the pose match table for the bake window's species, without touching its textures. */
	static void BuildPoseMatchTable();

	static bool CanSpawnPreview();

	/** Where a newly spawned actor should land: in front of the camera, else the origin. */
	static FVector GetSpawnLocation();
};
