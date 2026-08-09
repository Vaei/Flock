// Copyright (c) Jared Taylor. All Rights Reserved

#include "Debug/FlockDebug.h"

#if UE_ENABLE_DEBUG_DRAWING

namespace FFlockCVars
{
	int32 DebugPerception = 0;
	FAutoConsoleVariableRef CVarFlockDebugPerception(
		TEXT("flock.Debug.Perception"),
		DebugPerception,
		TEXT("Flock perception debug draw. 0 off, 1 bird state and alert, 2 also threat sources, ")
		TEXT("3 also flock bounds."),
		ECVF_Cheat);

	int32 DebugSlots = 0;
	FAutoConsoleVariableRef CVarFlockDebugSlots(
		TEXT("flock.Debug.Slots"),
		DebugSlots,
		TEXT("Flock perch slot debug draw. 0 off, 1 slots coloured Free/Reserved/Occupied, ")
		TEXT("2 also the holder."),
		ECVF_Cheat);

	int32 Separation = -1;
	FAutoConsoleVariableRef CVarFlockSeparation(
		TEXT("flock.Separation"),
		Separation,
		TEXT("Override separation between airborne flockmates. -1 leaves it to the species, 0 forces it "
		     "off, 1 forces it on at every tier. An override ignores the species' tier limit."),
		ECVF_Cheat);
}

#endif
