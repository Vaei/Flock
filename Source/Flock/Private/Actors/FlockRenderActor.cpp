// Copyright (c) Jared Taylor

#include "Actors/FlockRenderActor.h"

#include "Components/SceneComponent.h"

AFlockRenderActor::AFlockRenderActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	// Birds are cosmetic and the subsystem drives them wholesale, so there is nothing for world
	// partition or replication to do here.
	bIsSpatiallyLoaded = false;
	bReplicates = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
