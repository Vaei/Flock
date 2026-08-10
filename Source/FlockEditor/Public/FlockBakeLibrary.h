// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlockBakeLibrary.generated.h"

/**
 * Script access to the bake, so it can be driven from Python or a commandlet as well as the window.
 * Both functions operate on the same settings object the window edits.
 */
UCLASS()
class FLOCKEDITOR_API UFlockBakeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Creates the static mesh, textures and data asset for the configured source skeletal mesh. */
	UFUNCTION(BlueprintCallable, Category="Flock|Bake")
	static bool PrepareFlockAssets();

	/** Bakes the configured data assets and updates their material instances. */
	UFUNCTION(BlueprintCallable, Category="Flock|Bake")
	static bool BakeFlockTextures();

	/** Sets up the bake inputs without going through the window. Paths are asset paths. */
	UFUNCTION(BlueprintCallable, Category="Flock|Bake")
	static void ConfigureFlockBake(const FString& SkeletalMeshPath, const FString& OutputPath,
		const FString& AssetName, const TArray<FString>& AnimSequencePaths, float SampleRate);

	/** Replaces the bone animation material instance list, which the bake pushes parameters onto. */
	UFUNCTION(BlueprintCallable, Category="Flock|Bake")
	static void SetFlockBakeMaterialInstances(const TArray<FString>& MaterialInstancePaths);
};
