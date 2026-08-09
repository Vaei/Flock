// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlockVATPreview.generated.h"

class UAnimToTextureDataAsset;
class UInstancedStaticMeshComponent;

/**
 * Drop-in harness for eyeballing a baked vertex animation before any of the flock simulation exists.
 * Ticks in the editor viewport, so it needs no PIE.
 */
UCLASS()
class FLOCK_API AFlockVATPreview : public AActor
{
	GENERATED_BODY()

public:
	AFlockVATPreview();

	/** The baked data asset. Its static mesh is what gets instanced. */
	UPROPERTY(EditAnywhere, Category="Preview")
	TObjectPtr<UAnimToTextureDataAsset> AnimData;

	/** Index into the data asset's Animations array, which counts enabled clips only. */
	UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="0"))
	int32 AnimationIndex = 0;

	UPROPERTY(EditAnywhere, Category="Preview")
	bool bLoop = true;

	UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="0.0"))
	float PlayRate = 1.f;

	/** Stagger each instance's start so a grid does not animate in unison. */
	UPROPERTY(EditAnywhere, Category="Preview")
	bool bRandomPhase = true;

	/** Hold a single frame instead of playing. Negative plays normally. */
	UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="-1.0"))
	float HoldFrame = -1.f;

	/**
	 * Treat Hold Frame as a raw baked frame index rather than an offset into the clip, ignoring the clip
	 * range. This is how to tell a bad bake apart from a bad frame mapping.
	 */
	UPROPERTY(EditAnywhere, Category="Preview", meta=(EditCondition="HoldFrame >= 0"))
	bool bHoldFrameIsAbsolute = false;

	UPROPERTY(EditAnywhere, Category="Preview|Grid", meta=(ClampMin="1"))
	int32 CountX = 5;

	UPROPERTY(EditAnywhere, Category="Preview|Grid", meta=(ClampMin="1"))
	int32 CountY = 5;

	UPROPERTY(EditAnywhere, Category="Preview|Grid", meta=(ClampMin="0.0"))
	float Spacing = 150.f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The only thing that makes an actor tick in the editor viewport rather than only in PIE. */
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:
	void RebuildInstances();

	UPROPERTY(VisibleAnywhere, Category="Preview")
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	/** Per-instance phase offset in seconds. */
	TArray<float> Phases;

	/** Flat pairs of Frame and PrevFrame, reused so the tick allocates nothing. */
	TArray<float> CustomData;

	float ElapsedTime = 0.f;
};
