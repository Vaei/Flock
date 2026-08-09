// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"

/**
 * Debug draw cvars, one per concern so they can be turned on independently. Add siblings under
 * flock.Debug.* rather than growing levels onto an existing one.
 */

#if UE_ENABLE_DEBUG_DRAWING

namespace FFlockCVars
{
	/** 0 off, 1 bird state and alert, 2 also threat sources, 3 also flock bounds. */
	extern FLOCK_API int32 DebugPerception;

	/** 0 off, 1 slots coloured by state, 2 also which bird holds each. */
	extern FLOCK_API int32 DebugSlots;
}

namespace FFlockCVars
{
	/** -1 leaves separation to the species, 0 forces it off, 1 forces it on at every tier. */
	extern FLOCK_API int32 Separation;
}

#define FLOCK_DEBUG_PERCEPTION(Level) (FFlockCVars::DebugPerception >= (Level))
#define FLOCK_DEBUG_SLOTS(Level) (FFlockCVars::DebugSlots >= (Level))

/**
 * Separation is a species setting, not a debug toggle, so the shipping build resolves to "no override"
 * rather than to "off". Only the ability to override it from a console is stripped.
 */
#define FLOCK_SEPARATION_OVERRIDE() (FFlockCVars::Separation)

#else

#define FLOCK_DEBUG_PERCEPTION(Level) false
#define FLOCK_DEBUG_SLOTS(Level) false
#define FLOCK_SEPARATION_OVERRIDE() (-1)

#endif
