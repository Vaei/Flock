// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "FlockTypes.h"
#include "ActorFactories/ActorFactory.h"
#include "FlockActorFactory.generated.h"

class UFlockSpeciesData;

/** Places a flock volume, with a species assigned if one can be worked out. */
UCLASS()
class FLOCKEDITOR_API UFlockVolumeFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UFlockVolumeFactory();

	/**
	 * The project default, then a species built on whatever the bake window is pointed at, then the only
	 * species in the project. Anything less certain is left for the user to pick.
	 */
	static UFlockSpeciesData* FindDefaultSpecies();

protected:
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual FString GetDefaultActorLabel(UObject* Asset) const override;
};

/**
 * Places somewhere birds will not fly. One factory per shape: the shape decides which component is
 * drawn, so it has to be set as the actor is placed rather than chosen afterwards.
 */
UCLASS(Abstract)
class FLOCKEDITOR_API UFlockBlockingVolumeFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UFlockBlockingVolumeFactory();

protected:
	UPROPERTY()
	EFlockBlockerShape Shape = EFlockBlockerShape::Box;

	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual FString GetDefaultActorLabel(UObject* Asset) const override;
};

UCLASS()
class FLOCKEDITOR_API UFlockBlockingBoxFactory : public UFlockBlockingVolumeFactory
{
	GENERATED_BODY()

public:
	UFlockBlockingBoxFactory();
};

UCLASS()
class FLOCKEDITOR_API UFlockBlockingSphereFactory : public UFlockBlockingVolumeFactory
{
	GENERATED_BODY()

public:
	UFlockBlockingSphereFactory();
};
