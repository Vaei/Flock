// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "FlockProcessors.generated.h"

class UFlockSubsystem;

/**
 * Base for Flock processors. Turns off auto-registration, because nothing in engine-core ticks Mass and the
 * subsystem drives our pipeline instead, and caches the owning subsystem from InitializeInternal so no
 * processor has to declare a subsystem requirement to reach it.
 */
UCLASS(Abstract)
class FLOCK_API UFlockProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UFlockProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager) override;

	UPROPERTY(Transient)
	TObjectPtr<UFlockSubsystem> Subsystem = nullptr;
};

/**
 * Assigns each bird a LOD tier from camera distance and apparent size.
 *
 * Runs at a few hertz rather than every frame: a tier change is an archetype move, so re-deciding constantly
 * would cost more than the LOD saves.
 */
UCLASS()
class FLOCK_API UFlockLODProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockLODProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** Accumulates each bird's alert level from its flock's threat list. */
UCLASS()
class FLOCK_API UFlockThreatProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockThreatProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** Turns alert level into state, facing, and a clip. */
UCLASS()
class FLOCK_API UFlockDecisionProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockDecisionProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/**
 * Gives settled birds something to do: an occasional small turn, using the turn clips they already have.
 *
 * Near and Mid only. A glance is invisible past that, so it is not worth the archetype visit.
 */
UCLASS()
class FLOCK_API UFlockIdleProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockIdleProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** Ramps a launching bird up to speed, then hands it to flight. */
UCLASS()
class FLOCK_API UFlockTakeoffProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockTakeoffProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** Steers airborne birds toward their flock's attractor, and decides when to come down. */
UCLASS()
class FLOCK_API UFlockFlightProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockFlightProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** Brings a bird down onto its home point. */
UCLASS()
class FLOCK_API UFlockLandingProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockLandingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** Advances each bird's clip and resolves it to a baked frame index. */
UCLASS()
class FLOCK_API UFlockAnimProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockAnimProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** Stages every bird's transform and frame into its flock's instanced mesh batches. */
UCLASS()
class FLOCK_API UFlockRenderProcessor : public UFlockProcessor
{
	GENERATED_BODY()

public:
	UFlockRenderProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
