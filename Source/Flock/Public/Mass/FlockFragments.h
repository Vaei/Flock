// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "FlockTypes.h"
#include "Mass/EntityElementTypes.h"
#include "FlockFragments.generated.h"

/**
 * Fragments must be trivially copyable, so none of these hold a TArray, FString or object pointer. Where a
 * UObject's data is needed it is resolved to POD at spawn time and carried in the shared config fragment.
 */

USTRUCT()
struct FFlockBirdFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 FlockIndex = INDEX_NONE;

	/** -1 means loose on the ground rather than owning a perch slot. */
	int32 HomeSlotIndex = INDEX_NONE;

	uint8 SpeciesIndex = 0;

	/** Per-bird jitter source: phase, scale, play rate, and the flee-role roll. */
	uint8 RandomSeed = 0;

	/** Wherever it is currently headed or settled: a perch slot, or its ground spot. */
	FVector3f HomeLocation = FVector3f::ZeroVector;

	/** The spot it spawned on, kept separately so a bird can always find its way back to the ground. */
	FVector3f GroundLocation = FVector3f::ZeroVector;
};

USTRUCT()
struct FFlockVelocityFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector3f Velocity = FVector3f::ZeroVector;
};

USTRUCT()
struct FFlockStateFragment : public FMassFragment
{
	GENERATED_BODY()

	EFlockBirdState State = EFlockBirdState::Grounded;
	float StateTime = 0.f;
	float Alert = 0.f;
	float RefractoryTime = 0.f;

	/** Signed yaw error toward whatever alarmed it, in degrees. Drives the turn clips. */
	float YawDelta = 0.f;

	/**
	 * Which way it is currently turning: -1, 0 or 1. Latched while a turn is in progress, so the clip does
	 * not get re-picked every frame as the error wobbles across the deadband.
	 */
	int8 TurnSign = 0;

	/** Rolled at takeoff: wheel overhead, or head back down as soon as it can. */
	bool bPrefersOrbit = false;

	/** How long this flight's orbit lasts, randomised per bird. */
	float OrbitDuration = 0.f;

	/** A slot has been asked for and not yet answered, so the request is not repeated every frame. */
	bool bSlotRequested = false;

	/** Counts down to the next voluntary move. */
	float RestlessTimer = 0.f;

	/**
	 * How far the bird is currently leaning, in degrees.
	 *
	 * Held here rather than read back off the transform: pulling one Euler angle out of a quaternion is
	 * unstable near a vertical heading, and a descending bird is exactly that.
	 */
	float BankRoll = 0.f;

	/** Counts down to the next idle glance, and how much of that glance is left to turn. */
	float GlanceTimer = 0.f;
	float GlanceRemaining = 0.f;

	/** Counts down to the next rest break, and to the next dawdle across the ground. */
	float RestTimer = 0.f;
	float WalkTimer = 0.f;

	/** Where a dawdle is headed. Only meaningful while bWalking. */
	FVector3f WalkTarget = FVector3f::ZeroVector;
	bool bWalking = false;

	/** This flight was chosen, not forced, so it does not avoid threats or wait out an orbit. */
	bool bVoluntaryMove = false;

	/** Where a voluntary move is headed: a perch, or back to the ground spot. */
	bool bTargetPerch = false;

	/** Edge triggers for one-shot presentation. Set for one frame, cleared by whoever consumes them. */
	bool bJustTookOff = false;
	bool bJustLanded = false;
};

USTRUCT()
struct FFlockAnimFragment : public FMassFragment
{
	GENERATED_BODY()

	EFlockClip Clip = EFlockClip::Idle;

	/** World time the clip began. Seeded backwards to fake a random start phase. */
	float ClipStartTime = 0.f;
	float PlayRate = 1.f;

	/** What gets written to per-instance custom data. Maps straight onto the baked frame index. */
	float Frame = 0.f;
};

USTRUCT()
struct FFlockLODFragment : public FMassFragment
{
	GENERATED_BODY()

	float DistSqToView = 0.f;

	/** Rough fraction of half the screen height the bird covers. Cheaper than a real projection. */
	float ScreenRadius = 0.f;

	EFlockLODTier Tier = EFlockLODTier::Near;

	/** Minimum dwell before another change, so a bird on a boundary does not thrash archetypes. */
	float TierCooldown = 0.f;

	/** Run expensive work one frame in this many. Set from the tier. */
	int32 FrameStride = 1;
};

USTRUCT()
struct FFlockRenderFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Stable slot in the flock's ISM pool, assigned at spawn and never reshuffled. */
	int32 ComponentIndex = INDEX_NONE;
	int32 InstanceIndex = INDEX_NONE;
};

/**
 * Per-species tuning, resolved from UFlockSpeciesData. A const shared fragment, so every bird of a species
 * reads one copy and entities of different species land in different chunks for free.
 */
USTRUCT()
struct FFlockSpeciesConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	TStaticArray<FFlockClipRange, NumFlockClips> Clips;

	FFlockPerception Perception;
	FFlockFlightParams Flight;
	FFlockIdleParams Idle;

	float SampleRate = 30.f;

	/** Yaw correction for a mesh that does not face down +X. */
	float MeshYawOffset = 0.f;

	/** Roughly the bird's radius, for the screen-size estimate that drives LOD. */
	float BoundsRadius = 30.f;

	/** Index back into the subsystem's species array, for the rare case something needs the asset. */
	int32 SpeciesIndex = INDEX_NONE;

	const FFlockClipRange& GetClip(EFlockClip Clip) const
	{
		return Clips[static_cast<int32>(Clip)];
	}

	/**
	 * Picks a rest break by weight from whichever ones the species mapped, then picks a side.
	 *
	 * The two rolls answer separate questions, which is why there are two. Roll decides *which break*, over
	 * clips weighted against each other; VariantRoll decides *which half of a mirrored pair* once that break
	 * has already won. So a wing stretch stays as rare as a wing stretch should be whether or not it comes in
	 * a left and a right, instead of being twice as likely for having been authored twice.
	 *
	 * Returns EFlockClip::Count when the species mapped none, which is the signal to keep standing there.
	 */
	EFlockClip PickRestClip(float Roll, float VariantRoll) const
	{
		float Total = 0.f;
		for (int32 Index = FirstFlockRestClip; Index < NumFlockClips; ++Index)
		{
			Total += Clips[Index].bValid && !Clips[Index].bIsVariant
				? FMath::Max(0.f, Clips[Index].Weight) : 0.f;
		}

		if (Total <= 0.f)
		{
			return EFlockClip::Count;
		}

		float Pick = FMath::Clamp(Roll, 0.f, 1.f) * Total;
		for (int32 Index = FirstFlockRestClip; Index < NumFlockClips; ++Index)
		{
			if (!Clips[Index].bValid || Clips[Index].bIsVariant)
			{
				continue;
			}

			Pick -= FMath::Max(0.f, Clips[Index].Weight);
			if (Pick <= 0.f)
			{
				const FFlockClipRange& Won = Clips[Index];
				const bool bUseVariant = Won.VariantIndex != INDEX_NONE
					&& Clips[Won.VariantIndex].bValid
					&& VariantRoll < Won.VariantChance;

				return static_cast<EFlockClip>(bUseVariant ? Won.VariantIndex : Index);
			}
		}

		return EFlockClip::Count;
	}

	/** Frames per second this bird plays that clip at: the species' sample rate, both play rates applied. */
	float GetClipFrameRate(const FFlockClipRange& Range, float BirdPlayRate) const
	{
		return SampleRate * FMath::Max(0.01f, BirdPlayRate * Range.PlayRate);
	}

	/** How long one cycle of a clip lasts for this bird. Used to seed a random start phase. */
	float GetClipSeconds(const FFlockClipRange& Range, float BirdPlayRate) const
	{
		const float Rate = GetClipFrameRate(Range, BirdPlayRate);
		return Rate > 0.f ? Range.NumFrames() / Rate : 0.f;
	}

	/**
	 * What follows a one-shot that has reached its last frame. Count means hold that frame, because something
	 * else owns the transition.
	 *
	 * Takeoff and landing both outlast their clips, so without this a bird holds the last pose of a launch or
	 * a flare for the rest of the climb or the whole descent.
	 */
	EFlockClip GetOneShotSuccessor(EFlockClip Clip, EFlockBirdState BirdState) const
	{
		if (IsFlockRestClip(Clip))
		{
			return EFlockClip::Idle;
		}

		const auto Bridge = [this](EFlockClip Loop, bool bStillInState, EFlockClip Preferred) -> EFlockClip
		{
			if (bStillInState && GetClip(Loop).bValid)
			{
				return Loop;
			}
			if (GetClip(Preferred).bValid)
			{
				return Preferred;
			}
			return GetClip(EFlockClip::Fly).bValid ? EFlockClip::Fly : EFlockClip::Count;
		};

		switch (Clip)
		{
		case EFlockClip::TakeOff:
			return Bridge(EFlockClip::TakeOffLoop, BirdState == EFlockBirdState::TakingOff, EFlockClip::Fly);

		// The rest of a descent is a glide where the species has one, since that is what it is.
		case EFlockClip::Land:
			return Bridge(EFlockClip::LandLoop, BirdState == EFlockBirdState::Landing, EFlockClip::Glide);

		default:
			return EFlockClip::Count;
		}
	}

	/**
	 * Whether the bird's current clip will tolerate being replaced this frame.
	 *
	 * Clips not marked bMustComplete can go at any time. One that is waits for the end of a cycle, because
	 * VAT cannot blend and swapping mid-pose is a visible snap.
	 */
	bool CanLeaveClip(const FFlockAnimFragment& Anim, float Now) const
	{
		const FFlockClipRange& Range = GetClip(Anim.Clip);
		if (!Range.bMustComplete || !Range.bValid)
		{
			return true;
		}

		const float Frames = static_cast<float>(Range.NumFrames());
		if (Frames <= 1.f)
		{
			return true;
		}

		const float Elapsed = FMath::Max(0.f,
			(Now - Anim.ClipStartTime) * GetClipFrameRate(Range, Anim.PlayRate));

		// A one-shot is done once it reaches its last frame. A loop offers a chance to leave at the end of
		// every cycle, which for a short turn clip is a fraction of a second.
		return Range.bLoop
			? FMath::Fmod(Elapsed, Frames) >= Frames - 1.5f
			: Elapsed >= Frames - 1.f;
	}
};

/**
 * Per-flock live state, written by the subsystem and read by the processors.
 *
 * A mutable shared fragment, so its value partitions chunks by flock: reading a flock's threats is an O(1)
 * per-chunk access, and a chunk filter can skip a whole calm flock without touching a single bird.
 */
USTRUCT()
struct FFlockRuntimeSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	int32 FlockIndex = INDEX_NONE;

	TStaticArray<FFlockThreat, MaxFlockThreats> Threats;
	int32 NumThreats = 0;

	/** Where the flock lives, and the point airborne birds are drawn toward as it sweeps around. */
	FVector3f Centre = FVector3f::ZeroVector;
	FVector3f AttractorPosition = FVector3f::ZeroVector;

	/** Chance a spooked bird orbits rather than resettling. Authored per flock on the volume. */
	float OrbitPreference = 0.5f;

	/** While world time is under this, one bird's takeoff is still alarming the rest. */
	float ContagionUntil = 0.f;
	float ContagionStrength = 0.f;

	/**
	 * Set by the threat processor whenever any bird still holds alarm. The chunk filter needs it: alert
	 * decays inside that processor, so without this a flock skipped for having no threats would never calm
	 * down, and its birds would stay alert forever holding a stale yaw error.
	 */
	bool bAnyAlert = false;

	/** The flock is below its ambient airborne target, so settled birds should be going up for a lap. */
	bool bWantsAirborne = false;

	/**
	 * Accumulated by the render processor, which already visits every bird, and read by the audio bed. Reset
	 * each frame by the broadphase. Cheaper than a gather pass for what is only ever a few sound parameters.
	 */
	int32 NumSeen = 0;
	int32 NumAirborne = 0;
	float AlertSum = 0.f;

	bool HasThreats() const { return NumThreats > 0; }
};
