// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FlockDisturbanceInterface.generated.h"

UINTERFACE(MinimalAPI)
class UFlockDisturbanceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Lets an actor say how alarming it is without carrying a component, for types the owning module already
 * controls. The weight and radius are read once at registration; only IsFlockThreatActive is polled, and
 * only for actors that actually implement it.
 */
class IFlockDisturbanceInterface
{
	GENERATED_BODY()

public:
	/** Relative alarm. 1 is an ordinary walking character; a boulder should be higher. */
	virtual float GetFlockThreatWeight() const { return 1.f; }

	/** Beyond this distance the source is ignored entirely. */
	virtual float GetFlockThreatRadius() const { return 1200.f; }

	/** Polled every frame. Return false to go unnoticed, e.g. while crouched or hidden. */
	virtual bool IsFlockThreatActive() const { return true; }
};
