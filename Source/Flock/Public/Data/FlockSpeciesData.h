// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Data/FlockBakeRecipe.h"
#include "Engine/DataAsset.h"
#include "FlockTypes.h"
#include "FlockSpeciesData.generated.h"

class UAnimToTextureDataAsset;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
struct FFlockSpeciesConfigFragment;

/** Which baked animation a clip maps to, and how it plays. */
USTRUCT(BlueprintType)
struct FFlockClipMapping
{
	GENERATED_BODY()

	/** Index into the data asset's Animations array, which counts enabled clips only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0"))
	int32 AnimationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bLoop = true;

	/** Enter at a random point instead of the first frame. Wanted on idle, wrong on a one-shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bRandomStartPhase = false;

	/**
	 * Finish before another clip may replace this one. There is no blending between VAT clips, so an
	 * interrupted one snaps. Wanted on the turns; leave off for Idle, which is long enough that waiting for
	 * it would delay every reaction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bMustComplete = false;

	/**
	 * Relative chance of being picked, against the other clips that compete for the same pick. Only the rest
	 * breaks compete: raise it on a preen and lower it on a full body shake, and one shows up often while the
	 * other stays a treat. Ignored everywhere else.
	 */
	/**
	 * Multiplies the bird's own play rate for this clip alone. Below 1 is slower.
	 *
	 * The way to fix one clip that was authored at the wrong speed, or to slow a walk down to match the
	 * ground speed it is driven at, without going back to the bake.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.01", UIMin="0.1", UIMax="3.0"))
	float PlayRate = 1.f;

	/** Played where the bird is when this clip starts, one picked at random, from the shared one-shot pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TArray<TSoftObjectPtr<USoundBase>> Sounds;

	/**
	 * Fired on the flock's bed when this clip starts, so a MetaSound can decide for itself what the moment
	 * sounds like. Leave as None for nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FName AudioTrigger;

	/**
	 * How far into the clip the sound and the trigger land. A caw is not on the first frame: the beak has to
	 * open first, and lining the two up by eye is the difference between a bird cawing and a bird miming.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float SoundDelay = 0.f;
};

/**
 * One idle break from standing still: a preen, a head cock, a caw.
 *
 * A list rather than a set of numbered slots, because a numbered slot cannot tell you which one is the caw.
 * The Name is only a label, and it is what the list shows.
 */
USTRUCT(BlueprintType)
struct FFlockRestBreak
{
	GENERATED_BODY()

	/** What this break is. Shown in the list; nothing reads it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FName Name;

	/** Index into the data asset's Animations, the same as any other clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0"))
	int32 AnimationIndex = 0;

	/**
	 * How likely this break is, relative to the others. Raise it on a preen and lower it on a full body
	 * shake and one shows up often while the other stays a treat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0.01", UIMin="0.1", UIMax="3.0"))
	float PlayRate = 1.f;

	/**
	 * A second animation for the mirrored half: a wing stretch left and right, a head cock to either side.
	 *
	 * One entry, so the pair spends one Weight between them. Authored as two separate breaks it would be
	 * twice as likely as one authored as a single animation, which is backwards - paired breaks tend to be
	 * the big conspicuous ones you wanted rare.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bMirrored = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0", EditCondition="bMirrored"))
	int32 MirrorAnimationIndex = 0;

	/** Chance of taking the mirrored side, once this break has already won its pick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bMirrored"))
	float MirrorChance = 0.5f;

	/**
	 * Played at the bird when the break starts, one picked at random. Both sides of a mirrored pair share
	 * the list.
	 *
	 * A list rather than one sound because a flock of crows cawing the same wave over and over is the thing
	 * that gives an ambience away, and because separate assets do not contend for one asset's concurrency
	 * budget the way repeats of a single one do.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TArray<TSoftObjectPtr<USoundBase>> Sounds;

	/** Fired on the flock's bed when the break starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FName AudioTrigger;

	/** How far into the clip the sound and trigger land. Set this on a caw: the beak has to open first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float SoundDelay = 0.f;
};

/** Which authored break, and which side of it, a runtime rest slot came from. */
struct FFlockRestSlotRef
{
	int32 BreakIndex = INDEX_NONE;
	bool bMirrored = false;

	bool IsValid() const { return BreakIndex != INDEX_NONE; }
};

/**
 * Everything one kind of bird needs: its baked mesh, its animation textures, and its tuning.
 */
UCLASS(BlueprintType)
class FLOCK_API UFlockSpeciesData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The baked VAT static mesh, not the source skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Supplies the per-clip frame ranges and the sample rate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TSoftObjectPtr<UAnimToTextureDataAsset> AnimData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TMap<EFlockClip, FFlockClipMapping> Clips;

	/**
	 * The idle breaks this bird has. Add as many as you like; the list shows each one's Name.
	 *
	 * They compete against each other by Weight, and a bird plays one and returns to idle. Anything past
	 * what the playback slots can hold is dropped with a warning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(TitleProperty="Name"))
	TArray<FFlockRestBreak> RestBreaks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float PlayRateJitter = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FFlockPerception Perception;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FFlockFlightParams Flight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FFlockIdleParams Idle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.01"))
	float ScaleMin = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.01"))
	float ScaleMax = 1.05f;

	/**
	 * Yaw correction, in degrees, for a mesh that does not face down +X. Applied everywhere facing is derived
	 * from a direction, so a bird whose art faces +Y flies forwards rather than sideways.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	float MeshYawOffset = 0.f;

	/** Burst played where a bird leaves the ground. Batched: one component per flock, not one per bird. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TSoftObjectPtr<UNiagaraSystem> TakeOffVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TSoftObjectPtr<UNiagaraSystem> LandVFX;

	/**
	 * Continuous bed for the whole flock, fed Distance, BirdCount, Alert and AirborneRatio, plus TakeOff and
	 * Land triggers. One component per flock whatever its size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TSoftObjectPtr<USoundBase> FlockBed;

	/** Spatialised to one bird, one picked at random. Drawn from a small pool, so a cascade drops surplus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TArray<TSoftObjectPtr<USoundBase>> TakeOffOneShots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TArray<TSoftObjectPtr<USoundBase>> LandOneShots;

	/**
	 * Blend between the two frames either side of where a clip has reached, instead of snapping to the
	 * nearer one.
	 *
	 * Only does anything if this bird's material reads the third custom data float. That blend costs a
	 * second set of bone texture fetches in the vertex shader, so the material decides whether it is willing
	 * to pay and this decides which birds ask.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bInterpolateFrames = false;

	/** Furthest tier that still interpolates. Past it a bird asks for whole frames and the shader skips it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(EditCondition="bInterpolateFrames"))
	EFlockLODTier InterpolateMaxTier = EFlockLODTier::Mid;

	/** Used for the screen-size estimate that drives LOD. Roughly the bird's radius in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float BoundsRadius = 30.f;

	/**
	 * For every baked frame, which frame of each baked animation is the closest pose to it.
	 *
	 * Built by the bake, read once per clip change, and the reason a looping clip does not open on a pose
	 * unrelated to the one it replaced. Empty means every clip opens on its first frame.
	 */
	UPROPERTY()
	TArray<uint16> PoseMatchTable;

	/** Rows in the table, one per baked frame. Zero when there is none. */
	UPROPERTY(VisibleAnywhere, Category="Pose Matching")
	int32 PoseMatchNumFrames = 0;

	/** Columns in the table, one per baked animation. */
	UPROPERTY(VisibleAnywhere, Category="Pose Matching")
	int32 PoseMatchNumAnimations = 0;

#if WITH_EDITORONLY_DATA
	/**
	 * How this bird's baked assets were made. Lives here so the bake window can be pointed at a species and
	 * pick up where it left off, instead of the recipe existing only until the next bird is set up.
	 */
	UPROPERTY(EditAnywhere, Category="Bake")
	FFlockBakeRecipe BakeRecipe;
#endif

	/**
	 * Resolves the clip mappings against the data asset into the POD fragment the processors read.
	 * Loads both soft references. Returns false when there is nothing usable to render.
	 */
	bool BuildConfigFragment(FFlockSpeciesConfigFragment& OutConfig, int32 SpeciesIndex) const;

	/** Loads and returns the mesh, or null. */
	UStaticMesh* ResolveMesh() const;

	/**
	 * Which authored break each runtime rest slot holds, indexed 0..NumFlockRestClips-1.
	 *
	 * The single place the expansion order is decided, so the build and the audio lookup cannot disagree
	 * about which slot is which break.
	 */
	/**
	 * Whether the mesh's materials agree with the baked data about how many frames the textures hold.
	 *
	 * A material instance carries its own copy of the frame count, so one left out of a re-bake keeps the
	 * old one and every frame past it clamps to the last row of the texture. Clips are appended, so what
	 * freezes is exactly the clips that were just added, silently.
	 */
	bool ValidateMaterialFrameCount(FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	void BuildRestSlotTable(TStaticArray<FFlockRestSlotRef, NumFlockRestClips>& OutTable) const;

	/**
	 * The sounds, trigger and delay a clip carries, wherever it was authored. False when it carries none.
	 * OutSounds points into this asset, so it is only valid for as long as the caller holds the species.
	 */
	bool GetClipAudio(EFlockClip Clip, const TArray<TSoftObjectPtr<USoundBase>>*& OutSounds, FName& OutTrigger,
		float& OutDelay) const;

	/** Loads one of a sound list at random. Null for an empty list. */
	static USoundBase* PickSound(const TArray<TSoftObjectPtr<USoundBase>>& Sounds);
};
