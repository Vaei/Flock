// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"

class UFlockSpeciesData;

/**
 * Builds a species' pose match table: for every baked frame, the frame of each baked animation whose pose is
 * closest to it.
 *
 * Nothing blends one baked pose into the next, so a change of clip is a cut. Which frame it cuts to is the
 * only control there is over how hard that reads, and answering it offline costs one table lookup at
 * runtime instead of a second set of texture fetches in the vertex shader.
 */
class FLOCKEDITOR_API FFlockPoseMatch
{
public:
	/** Whether Build would do anything, and why not when it wouldn't. */
	static bool CanBuild(const UFlockSpeciesData* Species, FText& OutReason);

	/**
	 * Fills the species' table from its baked animations and marks the package dirty.
	 *
	 * Re-samples every clip, so it takes about as long as the bake did. It does not touch the textures, and
	 * can be re-run on its own after changing the matching weights.
	 */
	static bool Build(UFlockSpeciesData* Species, FText& OutError);
};
