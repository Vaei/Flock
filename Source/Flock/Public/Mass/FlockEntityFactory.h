// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "FlockTypes.h"

struct FFlockStateFragment;
struct FMassEntityHandle;
struct FMassExecutionContext;

namespace UE::Flock
{
	/**
	 * The one place bird state changes.
	 *
	 * State is held twice: as an enum, cheap to read in a loop, and as a tag, which splits archetypes so
	 * processors can be pruned to the birds they care about. This keeps the two in step. Anything else
	 * writing one and not the other puts birds in the wrong processor set, and it presents as an
	 * intermittent, position-dependent bug.
	 *
	 * The tag changes are deferred, so they land when the command buffer flushes at the end of the run.
	 * A bird therefore enters its new processor next tick, which is invisible at bird scale.
	 */
	FLOCK_API void SetBirdState(FMassExecutionContext& Context, const FMassEntityHandle Entity,
		FFlockStateFragment& State, EFlockBirdState NewState);
}
