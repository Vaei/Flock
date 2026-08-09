// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "FlockTypes.h"
#include "GameFramework/Actor.h"
#include "FlockBlockingVolume.generated.h"

class UBoxComponent;
class USphereComponent;

/**
 * Somewhere birds will not fly.
 *
 * Nothing about a bird touches physics: no traces, no collision, no queries against the world at all, which
 * is most of why a flock costs what it does. The inside of a roof is therefore not something a bird can
 * discover, and one of these is how it gets told. Place them around whatever a flock would otherwise fly
 * into and leave the rest of the level alone.
 *
 * A box or a sphere, because between them they cover a room, a stairwell and a tree, and an exact shape
 * would cost more than the problem is worth.
 */
UCLASS(Blueprintable)
class FLOCK_API AFlockBlockingVolume : public AActor
{
	GENERATED_BODY()

public:
	AFlockBlockingVolume();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	EFlockBlockerShape Shape = EFlockBlockerShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="1.0", ForceUnits="cm", EditCondition="Shape == EFlockBlockerShape::Box"))
	FVector BoxExtent = FVector(400.f, 400.f, 400.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="1.0", ForceUnits="cm", EditCondition="Shape == EFlockBlockerShape::Sphere"))
	float SphereRadius = 400.f;

	/**
	 * How far outside the surface birds start turning away.
	 *
	 * Nothing gets through either way. This is the difference between a flock that curves around a building
	 * and one that pulls up short against the wall of it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float AvoidMargin = 200.f;

	/** Off leaves it in the level doing nothing, for switching one out without deleting it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bEnabled = true;

	/** Resolved to the POD the shared fragment holds, in world space with the actor's scale applied. */
	FFlockBlocker MakeBlocker() const;

	/** Radius of a sphere containing the whole shape, for the broadphase. */
	float GetBoundingRadius() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Both exist and one is shown, because a subobject cannot change class when Shape does. */
	UPROPERTY()
	TObjectPtr<UBoxComponent> BoxVisual = nullptr;

	UPROPERTY()
	TObjectPtr<USphereComponent> SphereVisual = nullptr;

	/** Sizes both components and shows whichever Shape names. Neither ever collides with anything. */
	void SyncVisuals();
};
