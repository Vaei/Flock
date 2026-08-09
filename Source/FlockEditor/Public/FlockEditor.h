// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "Modules/ModuleManager.h"

class FFlockEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	TSharedRef<SWidget> BuildMenu();

	/** Drives the toolbar entry's visibility, so toggling the setting hides it without a restart. */
	static bool IsToolbarMenuEnabled();
	static void HideToolbarMenu();
	static void OpenSettings();

	/** Drops a preview actor into the open level, wired to whichever data asset the bake window is set to. */
	static void SpawnPreviewInCurrentLevel();

	/** Drops a flock volume into the open level, with a species assigned if one can be found. */
	static void SpawnFlockVolumeInCurrentLevel();

	/** Re-bakes every perch component in the open level. */
	static void RebuildAllPerchesInLevel();

	static bool CanSpawnPreview();

	/** Where a newly spawned actor should land: in front of the camera, else the origin. */
	static FVector GetSpawnLocation();
};
