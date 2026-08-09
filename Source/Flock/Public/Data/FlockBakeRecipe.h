// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "AnimToTextureDataAsset.h"
#include "FlockBakeRecipe.generated.h"

class UAnimSequence;
class UMaterialInstanceConstant;
class USkeletalMesh;

/**
 * Everything needed to reproduce one bird's vertex animation bake.
 *
 * Kept on the species asset rather than only in the bake window, so switching birds does not throw the
 * setup away. Editor-only: it describes how the baked assets were made and is never read at runtime.
 */
USTRUCT(BlueprintType)
struct FLOCK_API FFlockBakeRecipe
{
	GENERATED_BODY()

	/** Skeletal mesh the static mesh and textures are built from. */
	UPROPERTY(EditAnywhere, Category="Bake")
	TSoftObjectPtr<USkeletalMesh> SourceSkeletalMesh;

	/** Content path the prepared assets are created in. */
	UPROPERTY(EditAnywhere, Category="Bake", meta=(ContentDir))
	FDirectoryPath OutputPath;

	/** Base name for the prepared assets: "Crow" gives SM_Crow_VAT, TX_Crow_BonePosition, DA_Crow. */
	UPROPERTY(EditAnywhere, Category="Bake")
	FString AssetName;

	/** Clips to bake, in order. Their order fixes the animation indices the species maps against. */
	UPROPERTY(EditAnywhere, Category="Bake")
	TArray<TSoftObjectPtr<UAnimSequence>> AnimSequences;

	/**
	 * Frames per second the clips are sampled at. A time step, not a frame count: if it does not match the
	 * source clips' own rate the bake walks off the end of every animation.
	 */
	UPROPERTY(EditAnywhere, Category="Bake", meta=(ClampMin="1.0"))
	float SampleRate = 30.f;

	/** Bone stores per-bone position and rotation plus a weight texture. Vertex stores per-vertex. */
	UPROPERTY(EditAnywhere, Category="Bake")
	EAnimToTextureMode Mode = EAnimToTextureMode::Bone;

	/** Eight bits quantises position to 256 steps across the whole animation bounds, which stair-steps. */
	UPROPERTY(EditAnywhere, Category="Bake")
	EAnimToTexturePrecision Precision = EAnimToTexturePrecision::SixteenBits;

	UPROPERTY(EditAnywhere, Category="Bake|Bone Animation")
	bool bBakeBoneAnimation = true;

	UPROPERTY(EditAnywhere, Category="Bake|Bone Animation", meta=(EditCondition="bBakeBoneAnimation"))
	TSoftObjectPtr<UAnimToTextureDataAsset> BoneDataAsset;

	/** Every instance here has its parameters pushed from the data asset after the bake. */
	UPROPERTY(EditAnywhere, Category="Bake|Bone Animation", meta=(EditCondition="bBakeBoneAnimation"))
	TArray<TSoftObjectPtr<UMaterialInstanceConstant>> BoneMaterialInstances;

	UPROPERTY(EditAnywhere, Category="Bake|Vertex Animation")
	bool bBakeVertexAnimation = false;

	UPROPERTY(EditAnywhere, Category="Bake|Vertex Animation", meta=(EditCondition="bBakeVertexAnimation"))
	TSoftObjectPtr<UAnimToTextureDataAsset> VertexDataAsset;

	UPROPERTY(EditAnywhere, Category="Bake|Vertex Animation", meta=(EditCondition="bBakeVertexAnimation"))
	TArray<TSoftObjectPtr<UMaterialInstanceConstant>> VertexMaterialInstances;

	/**
	 * UV channel the lightmap is generated into. Must differ from the data asset's UVChannel, which is where
	 * the bake writes its lookup UVs. A freshly converted mesh defaults to 1 and so does UVChannel, which is
	 * the most common first-run bake failure.
	 */
	UPROPERTY(EditAnywhere, Category="Bake|Static Mesh", meta=(ClampMin="0", ClampMax="7"))
	int32 LightmapIndex = 2;

	UPROPERTY(EditAnywhere, Category="Bake|Static Mesh")
	bool bGenerateLightmapUVs = true;

	/** Whether there is enough here to bake anything. */
	bool IsUsable() const
	{
		return (bBakeBoneAnimation && !BoneDataAsset.IsNull())
			|| (bBakeVertexAnimation && !VertexDataAsset.IsNull());
	}
};
