// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlockVolume.generated.h"

class UBoxComponent;
class UFlockSpeciesData;

/**
 * Place one to get a flock. Its box is both where birds are scattered and, later, the bounds the disturbance
 * broadphase tests against.
 */
UCLASS()
class FLOCK_API AFlockVolume : public AActor
{
	GENERATED_BODY()

public:
	AFlockVolume();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TObjectPtr<UFlockSpeciesData> Species;

	/** How many to spawn. Named generically because a flock need not always be birds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1"))
	int32 SpawnCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bSpawnOnBeginPlay = true;

	/**
	 * Chance a spooked bird wheels overhead rather than heading straight back down. Negative takes the
	 * species' value. This is the knob that makes one flock visibly split: some resettle, some circle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float OrbitPreference = -1.f;

	/**
	 * Trace down through the box to find the ground for each bird, once at spawn. Off puts them on the plane
	 * through this actor, which is the box's centre rather than its floor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bSnapToGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(EditCondition="bSnapToGround"))
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	/** Nearest another bird may spawn, on the ground plane. Scattered randomly, but never stacked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float MinSpawnSpacing = 80.f;

	/** Held off the ground by this much, so feet are not buried in it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0.0", ForceUnits="cm", EditCondition="bSnapToGround"))
	float GroundOffset = 0.f;

	/**
	 * Steepest surface a bird will spawn on, as the upward part of its normal. 1 is flat, 0 accepts a
	 * vertical wall. Raise it if birds are appearing on sloped or awkward geometry inside the box.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bSnapToGround"))
	float MinGroundNormalZ = 0.7f;

	/** Clear space a bird needs above its feet, so none is placed inside geometry. Zero skips the check. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0.0", ForceUnits="cm", EditCondition="bSnapToGround"))
	float HeadroomRadius = 20.f;

	/** Spawns the flock now. Safe to call once; a second call is ignored while one is alive. */
	UFUNCTION(BlueprintCallable, Category="Flock")
	void SpawnFlock();

	UFUNCTION(BlueprintCallable, Category="Flock")
	void DespawnFlock();

	UBoxComponent* GetBounds() const { return Bounds; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Flock")
	TObjectPtr<UBoxComponent> Bounds;

#if WITH_EDITORONLY_DATA
	/** Gives the volume something clickable in the viewport; a wireframe box alone is hard to hit. */
	UPROPERTY(VisibleAnywhere, Category="Flock")
	TObjectPtr<class UBillboardComponent> Sprite;

	UPROPERTY()
	TObjectPtr<class UFlockVolumeEditorVisualizer> EditorVisualizer;
#endif

	int32 FlockIndex = INDEX_NONE;
};
