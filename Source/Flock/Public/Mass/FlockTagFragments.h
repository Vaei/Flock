// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "FlockTagFragments.generated.h"

/**
 * State lives twice: as the enum in FFlockStateFragment, which is cheap to read inside a loop, and as a tag,
 * which splits archetypes so a processor can be pruned to only the birds it cares about.
 *
 * Exactly one function may write either of them. Anything else writing one and not the other puts birds in
 * the wrong processor set.
 */

USTRUCT()
struct FFlockGroundedTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockPerchedTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockAlertTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockTakingOffTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockFlyingTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockLandingTag : public FMassTag { GENERATED_BODY() };

/**
 * LOD tier as a tag, not just an enum, so tiers land in different archetypes and a processor that excludes
 * one never visits its chunks at all. That is where the saving comes from: not a branch per bird.
 */
USTRUCT()
struct FFlockLODNearTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockLODMidTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockLODFarTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FFlockLODCulledTag : public FMassTag { GENERATED_BODY() };
