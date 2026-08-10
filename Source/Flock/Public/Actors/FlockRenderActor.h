// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlockRenderActor.generated.h"

/**
 * Transient host for one flock's instanced mesh components. Spawned by the subsystem, never placed.
 *
 * Instance transforms are stored local to this actor, which keeps the floats small and lets a whole flock be
 * repositioned by moving one actor.
 */
UCLASS(NotPlaceable, Transient)
class FLOCK_API AFlockRenderActor : public AActor
{
	GENERATED_BODY()

public:
	AFlockRenderActor();
};
