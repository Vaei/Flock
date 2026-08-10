// Copyright (c) Jared Taylor

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

	int32 PoseMatch = -1;
	FAutoConsoleVariableRef CVarFlockPoseMatch(
		TEXT("flock.PoseMatch"),
		PoseMatch,
		TEXT("Override where a clip opens when a bird changes clip. -1 uses the species' baked pose match "
		     "table, 0 forces every clip to open on its first frame."),
		ECVF_Cheat);

	int32 Interpolate = -1;
	FAutoConsoleVariableRef CVarFlockInterpolate(
		TEXT("flock.Interpolate"),
		Interpolate,
		TEXT("Override blending between the two frames either side of where a clip has reached. -1 leaves it "
		     "to the species, 0 forces whole frames, 1 forces it on at every tier. Does nothing unless the "
		     "bird's material reads the third custom data float."),
		ECVF_Cheat);
}

#endif
