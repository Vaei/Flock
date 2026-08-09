// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"

class UFlockBakeSettings;

/**
 * Drives the AnimToTexture bake from C++.
 *
 * Replaces the plugin's BP_AnimToTexture editor utility, and does three things it does not: creates the
 * destination assets (AnimationToTexture only ever writes into textures that already exist, and blank
 * Texture2D assets cannot be made from the content browser), forces the lightmap off the UV channel the
 * bake needs, and validates every texture up front because the bake asserts on a null bone weight
 * texture and silently no-ops on the other two.
 */
class FLOCKEDITOR_API FFlockBake
{
public:
	/** Whether Bake would do anything, and why not when it wouldn't. */
	static bool CanBake(const UFlockBakeSettings& Settings, FText& OutReason);

	/** Creates the static mesh, textures and data asset for SourceSkeletalMesh, then points Settings at them. */
	static bool PrepareAssets(UFlockBakeSettings& Settings, FText& OutError);

	/** Bakes the assigned data assets and pushes their parameters onto the listed material instances. */
	static bool Bake(const UFlockBakeSettings& Settings, FText& OutError);
};
