// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

/** `stat flock` in the console, or the Flock group in Unreal Insights. */
DECLARE_STATS_GROUP(TEXT("Flock"), STATGROUP_Flock, STATCAT_Advanced);

// Subsystem tick, broken into the phases that can each be the one that costs.
DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick"), STAT_Flock_Tick, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RefreshSources"), STAT_Flock_RefreshSources, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Broadphase"), STAT_Flock_Broadphase, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("SlotRequests"), STAT_Flock_SlotRequests, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("DrainEvents"), STAT_Flock_DrainEvents, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderFlush"), STAT_Flock_RenderFlush, STATGROUP_Flock, FLOCK_API);

// Processors.
DECLARE_CYCLE_STAT_EXTERN(TEXT("LOD"), STAT_Flock_LOD, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Threat"), STAT_Flock_Threat, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Decision"), STAT_Flock_Decision, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Idle"), STAT_Flock_Idle, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Takeoff"), STAT_Flock_Takeoff, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Flight"), STAT_Flock_Flight, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Landing"), STAT_Flock_Landing, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim"), STAT_Flock_Anim, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Render"), STAT_Flock_Render, STATGROUP_Flock, FLOCK_API);

// Preview harness, so it never gets mistaken for simulation cost.
DECLARE_CYCLE_STAT_EXTERN(TEXT("PreviewTick"), STAT_Flock_PreviewTick, STATGROUP_Flock, FLOCK_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("PreviewRebuild"), STAT_Flock_PreviewRebuild, STATGROUP_Flock, FLOCK_API);

// Counters. What the numbers mean is usually the first question a cycle count raises.
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Flocks"), STAT_Flock_NumFlocks, STATGROUP_Flock, FLOCK_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Birds"), STAT_Flock_NumBirds, STATGROUP_Flock, FLOCK_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Sources"), STAT_Flock_NumSources, STATGROUP_Flock, FLOCK_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Instances Written"), STAT_Flock_InstancesWritten, STATGROUP_Flock, FLOCK_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Birds (Near)"), STAT_Flock_BirdsNear, STATGROUP_Flock, FLOCK_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Birds (Mid)"), STAT_Flock_BirdsMid, STATGROUP_Flock, FLOCK_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Birds (Far)"), STAT_Flock_BirdsFar, STATGROUP_Flock, FLOCK_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Birds (Culled)"), STAT_Flock_BirdsCulled, STATGROUP_Flock, FLOCK_API);

/**
 * Both a `stat flock` cycle counter and an Insights scope in one line, so the two can never drift apart.
 * Name matches the STAT_Flock_<Name> id, e.g. FLOCK_SCOPE(Threat).
 */
#define FLOCK_SCOPE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE(Flock_##Name); \
	SCOPE_CYCLE_COUNTER(STAT_Flock_##Name)
