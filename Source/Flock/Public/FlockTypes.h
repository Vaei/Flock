// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "FlockTypes.generated.h"

/** The clips a bird can play. Order is ours; the mapping to baked animation indices lives on the species. */
UENUM(BlueprintType)
enum class EFlockClip : uint8
{
	Idle,
	TurnLeft,
	TurnRight,
	TakeOff,

	/**
	 * Optional. Bridges the gap between Take Off finishing and the bird being fully airborne, for a launch
	 * that lasts longer than the clip. Falls back to Fly.
	 */
	TakeOffLoop,

	Fly,

	/** Optional. Wings held, played instead of Fly while descending faster than Glide Descent Rate. */
	Glide,

	/** Optional. Played instead of Fly while turning harder than Bank Clip Yaw Rate. */
	BankLeft,
	BankRight,

	Land,

	/**
	 * Optional. Bridges the gap between Land finishing and the bird actually touching down, which is most of
	 * the descent. Falls back to Fly.
	 */
	LandLoop,

	/** A slow dawdle across the ground, between idles. */
	Walk,

	/**
	 * Playback slots for the rest breaks. Hidden, and never authored directly: a species lists its breaks by
	 * name in Rest Breaks, and those are expanded into these when the flock spawns. Only the clip fragment
	 * and the processors ever name one.
	 */
	Rest1 UMETA(Hidden),
	Rest2 UMETA(Hidden),
	Rest3 UMETA(Hidden),
	Rest4 UMETA(Hidden),
	Rest5 UMETA(Hidden),
	Rest6 UMETA(Hidden),
	Rest7 UMETA(Hidden),
	Rest8 UMETA(Hidden),
	Rest9 UMETA(Hidden),
	Rest10 UMETA(Hidden),
	Rest11 UMETA(Hidden),
	Rest12 UMETA(Hidden),

	Count UMETA(Hidden)
};

inline constexpr int32 NumFlockClips = static_cast<int32>(EFlockClip::Count);
inline constexpr int32 FirstFlockRestClip = static_cast<int32>(EFlockClip::Rest1);
inline constexpr int32 NumFlockRestClips = NumFlockClips - FirstFlockRestClip;

inline bool IsFlockRestClip(EFlockClip Clip)
{
	const int32 Index = static_cast<int32>(Clip);
	return Index >= FirstFlockRestClip && Index < NumFlockClips;
}

UENUM(BlueprintType)
enum class EFlockBirdState : uint8
{
	Grounded,
	Perched,
	Alert,
	TakingOff,
	Flying,
	Landing
};

/**
 * A clip's baked frame range, resolved from the species' data asset once at spawn so the hot loop never
 * touches a UObject. Trivially copyable on purpose: this lives inside a Mass shared fragment.
 */
USTRUCT()
struct FFlockClipRange
{
	GENERATED_BODY()

	int32 StartFrame = 0;
	int32 EndFrame = 0;

	/** Which baked animation this came from. Names a column of the pose match table. */
	int32 AnimationIndex = INDEX_NONE;

	bool bLoop = true;

	/**
	 * Enter at a random point rather than the first frame, so a settled flock does not move in unison.
	 *
	 * Only honoured where a flock wants desynchronising: the first idle after spawning, and the first after
	 * coming down. Re-entering a clip part way through a bird's life keeps continuity instead.
	 */
	bool bRandomStartPhase = false;

	/**
	 * Play to the end of a cycle before allowing another clip in. A clip change is a pose jump unless the
	 * pose match table softens it, and a clip swapped out halfway is the worst of them.
	 */
	bool bMustComplete = false;

	/** False when the species left this clip unmapped, or its animation index was out of range. */
	bool bValid = false;

	/** Relative chance of being picked. Only the rest breaks compete with each other for a pick. */
	float Weight = 1.f;

	/** Multiplies the bird's own play rate, so one clip can be slowed or sped without re-baking. */
	float PlayRate = 1.f;

	/** The mirrored half of this clip, if it has one. Chosen after this clip has already won its pick. */
	int32 VariantIndex = INDEX_NONE;
	float VariantChance = 0.f;

	/** Something else names this as its variant, so it never competes for a pick in its own right. */
	bool bIsVariant = false;

	int32 NumFrames() const { return EndFrame - StartFrame + 1; }
};

/** How much simulation a bird is worth right now. */
UENUM(BlueprintType)
enum class EFlockLODTier : uint8
{
	Near,
	Mid,
	Far,

	/** Off screen or too distant to make out. Rendered at zero scale, simulated barely at all. */
	Culled
};

UENUM(BlueprintType)
enum class EFlockSlotState : uint8
{
	Free,
	Reserved,
	Occupied
};

/**
 * A perch position as authored, relative to its component. Baked in the editor and cooked into the owning
 * asset, so nothing traces at runtime.
 */
USTRUCT(BlueprintType)
struct FFlockAuthoredSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FVector LocalPosition = FVector::ZeroVector;

	/** Which way a bird faces once settled here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FRotator LocalRotation = FRotator::ZeroRotator;

	/** A perch is a branch or ledge; otherwise it is open ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bPerch = true;
};

/** Most threats a flock tracks at once. Bounded so per-bird cost is independent of how many sources exist. */
inline constexpr int32 MaxFlockThreats = 4;

/** How many blocking volumes one flock can be kept out of at once. */
inline constexpr int32 MaxFlockBlockers = 6;

UENUM(BlueprintType)
enum class EFlockBlockerShape : uint8
{
	Box,
	Sphere
};

/**
 * A volume birds stay out of, resolved to POD once per frame so the shared fragment can hold it.
 *
 * Birds have no collision of any kind, which is what makes them cheap. This is the whole of their world
 * awareness: enough to keep a flock off the inside of a roof, and nothing more.
 */
USTRUCT()
struct FFlockBlocker
{
	GENERATED_BODY()

	FVector3f Centre = FVector3f::ZeroVector;

	/** Box half extents. A sphere takes its radius from X. */
	FVector3f Extent = FVector3f::ZeroVector;

	/** Box orientation. A sphere ignores it. */
	FQuat4f Rotation = FQuat4f::Identity;

	/** How far outside the surface birds begin steering away, rather than being stopped at it. */
	float Margin = 150.f;

	EFlockBlockerShape Shape = EFlockBlockerShape::Box;
};

/** One alarming thing, as the birds see it. POD; lives in a Mass shared fragment. */
USTRUCT()
struct FFlockThreat
{
	GENERATED_BODY()

	FVector3f Position = FVector3f::ZeroVector;
	FVector3f Velocity = FVector3f::ZeroVector;
	float Weight = 1.f;
	float MaxRadius = 1200.f;

	/** Overrides the species' falloff curve when above zero. A deliberate scare wants a flatter one. */
	float ProximityExponent = 0.f;

	/**
	 * Skips the species' MaxNoticeRadius cap. Set only by an explicit scare: when a caller names a radius it
	 * means that radius, whatever a species would notice on its own.
	 */
	bool bIgnoreSpeciesRange = false;
};

/** Per-species reaction tuning. */
USTRUCT(BlueprintType)
struct FFlockPerception
{
	GENERATED_BODY()

	/** Inside this, proximity is already at full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float SafeRadius = 150.f;

	/**
	 * Furthest this species notices anything, whatever radius the source claims. The species' own attention
	 * span, so one loud source cannot make every bird in the level jumpy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0", ForceUnits="cm"))
	float MaxNoticeRadius = 1200.f;

	/**
	 * How sharply alarm falls off with distance. This is the knob for *where* birds break, far more than the
	 * radii are.
	 *
	 * At 1 the falloff is linear, so something at the edge of its radius is already mildly alarming and a
	 * heavy source builds enough alarm to flee from a long way off. Raising it concentrates the reaction near
	 * the source: at 3 the halfway point contributes an eighth as much.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0", ClampMax="8.0"))
	float ProximityExponent = 3.f;

	/** Closing speed treated as maximally alarming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float ClosingSpeedRef = 400.f;

	/** Absolute speed treated as maximally alarming, whatever the direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float SpeedRef = 600.f;

	/** How much mere presence, approach, and raw speed each contribute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float BaseWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float ClosingWeight = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float SpeedWeight = 0.3f;

	/** Ceiling on the summed contribution, so a crowd is not instantly terrifying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float MaxThreatRate = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float AlertGain = 1.5f;

	/** How fast alert bleeds away once nothing is alarming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float AlertDecay = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PerkThreshold = 0.25f;

	/** Release fraction of the perk threshold. Below 1 so birds do not flicker in and out of alert. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.1", ClampMax="1.0"))
	float PerkReleaseRatio = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FleeThreshold = 0.75f;

	/** Inside this, take off at once regardless of the accumulator. Catches a sprint or a teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float PanicRadius = 120.f;

	/** Per-bird threshold jitter, so a flock reacts raggedly rather than as one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ThresholdJitter = 0.25f;

	/** Degrees per second an alert bird turns to face what alarmed it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float TurnRateDegrees = 180.f;

	/** Yaw error below which the turn clips give way to idle, so they do not chatter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float TurnDeadbandDegrees = 12.f;

	/**
	 * A settled bird glances about on this interval, turning a little and playing a turn clip. Uses the turn
	 * clips it already has, so it needs no extra animation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float GlanceIntervalMin = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float GlanceIntervalMax = 7.f;

	/** How far a glance turns. Small: this is a look, not a manoeuvre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="180.0"))
	float GlanceYawDegrees = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bAllowGlances = true;

	/** After landing, alert decays faster and the flee bar is raised. Stops land-takeoff ping-pong. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float LandedCooldown = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float RefractoryDecayScale = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float RefractoryFleeBonus = 0.15f;

	/**
	 * How long one bird taking off keeps alarming its flockmates, and how hard. This is what makes a flock
	 * erupt together instead of one bird at a time, and it costs two floats rather than a neighbour query.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float ContagionWindow = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float ContagionStrength = 1.2f;
};

/**
 * What a settled bird does with itself. Glances are tuned on the perception block, because an alert bird
 * turns the same way; everything here is behaviour a calm bird invents on its own.
 */
USTRUCT(BlueprintType)
struct FFlockIdleParams
{
	GENERATED_BODY()

	/** A settled bird plays one of its rest clips on this interval. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float RestIntervalMin = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float RestIntervalMax = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bAllowRestBreaks = true;

	/** A bird standing on open ground wanders a few steps on this interval. Perched birds never do. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float WalkIntervalMin = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float WalkIntervalMax = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bAllowWalking = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float WalkSpeed = 45.f;

	/**
	 * Furthest a dawdle strays from the spot the bird spawned on. Nothing traces while walking, so the bird
	 * holds the height it was placed at: keep this short on uneven ground.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float WalkRadius = 150.f;

	/** Shortest a dawdle is worth making, so a bird never takes half a step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float WalkMinDistance = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0", ForceUnits="cm"))
	float WalkArriveDistance = 8.f;

	/** Degrees per second it turns to line up with where it is going before it gets going. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float WalkTurnRateDegrees = 220.f;
};

/** Per-species flight tuning. */
USTRUCT(BlueprintType)
struct FFlockFlightParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.01", ForceUnits="s"))
	float TakeoffTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float TakeoffSpeed = 600.f;

	/** Launch direction is a blend of straight up and directly away from whatever spooked it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float TakeoffUpBias = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float TakeoffAwayBias = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float CruiseSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float TurnRateDegrees = 160.f;

	/** How hard birds are pulled toward the flock's shared attractor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float CohesionWeight = 1.f;

	/**
	 * Per-bird wander. Also standing in for true separation: at bird scale and bird speed a staggered
	 * jitter phase is visually indistinguishable, and costs nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float JitterAmplitude = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float JitterFrequency = 1.7f;

	/** Degrees of roll at the hardest turn the bird can make, scaled down for gentler ones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="deg"))
	float BankScale = 35.f;

	/** How quickly roll reaches the angle the current turn asks for. Low is languid; high snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.1"))
	float BankInterpSpeed = 6.f;

	/**
	 * Turning this hard swaps Fly for the Bank clip on that side, where the species maps one. Capped by Turn
	 * Rate Degrees, so a value at or above that never triggers. Zero never banks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="deg/s"))
	float BankClipYawRate = 60.f;

	/** Descending this fast swaps Fly for the Glide clip, where the species maps one. Zero never glides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float GlideDescentRate = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float CruiseRadius = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float CruiseCeiling = 900.f;

	/** Degrees per second the shared attractor sweeps around the flock, setting the wheeling speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float AttractorSweepDegrees = 70.f;

	/** Shortest time airborne before a bird will consider coming down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float MinFlightTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float OrbitTimeMin = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float OrbitTimeMax = 9.f;

	/** Chance a spooked bird wheels overhead rather than heading straight back down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float OrbitPreference = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float LandSpeed = 350.f;

	/** Height above the target a bird lines up at before dropping onto it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float LandApproachHeight = 250.f;

	/**
	 * How close to directly above the target counts as lined up, at twice this. Arrival itself is exact: a
	 * bird lands on the frame it would otherwise overshoot, so there is no final hop onto the spot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0", ForceUnits="cm"))
	float LandArriveDistance = 30.f;

	/**
	 * How quickly a bird that has just committed to landing swings onto its descent.
	 *
	 * Committing changes both the speed and the direction it wants, and taking either instantly is a visible
	 * kink in the flight path. Low is a wide, unhurried turn onto the approach; high is abrupt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.1"))
	float LandVelocityInterpSpeed = 2.5f;

	/**
	 * How hard birds are pushed away from a Flock Blocking Volume they are heading into.
	 *
	 * Steering handles it at a distance; being stopped at the surface is the backstop for when steering was
	 * not enough. Zero still stops them, it just gives no warning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float BlockerAvoidStrength = 2.f;

	/** How far a bird will look for somewhere to settle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0", ForceUnits="cm"))
	float LandSearchRadius = 3000.f;

	/** A relocating bird will not settle within this of whatever spooked it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float SafeRelocateDistance = 800.f;

	/**
	 * Birds move of their own accord on this interval, not only when frightened, alternating between a perch
	 * and the ground spot they started on. Randomised per bird so a flock never shuffles in unison.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float RestlessIntervalMin = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float RestlessIntervalMax = 110.f;

	/**
	 * Chance a voluntary move takes a lap overhead first rather than hopping straight across. Some birds
	 * circling for no reason is most of what makes a flock look like it has its own life.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RestlessOrbitChance = 0.3f;

	/**
	 * Fraction of the flock it tries to keep in the air at all times, with nothing disturbing it.
	 *
	 * Left to the restless interval alone a lap overhead is far too rare to ever catch: on ten birds it works
	 * out at one short orbit every twenty seconds or so. This makes it a standing target instead, so there is
	 * usually something wheeling above a settled flock. Zero switches it off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AmbientAirborneFraction = 0.15f;

	/**
	 * How much sooner a bird comes up for a lap while the flock is under that target. The restless countdown
	 * runs this many times faster, and the move it triggers is always an orbit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="1.0"))
	float AmbientAirborneUrgency = 8.f;

	/**
	 * Chance a voluntary move targets a perch rather than the bird's ground spot. At 0.5 a bird will sometimes
	 * pick the same kind of place twice, which reads less mechanically than strict alternation. 0 keeps birds
	 * on the ground, 1 keeps them fighting over perches.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PerchPreference = 0.5f;

	/** Zero switches voluntary moves off entirely, leaving birds where they land. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bAllowRestlessMoves = true;

	/**
	 * Fraction of the flock already sitting on a perch when play begins, teleported there rather than flown.
	 *
	 * Without this every bird starts on the ground and the flock's first few seconds are one big burst of
	 * activity as they all sort themselves out, which reads as the level starting rather than the birds
	 * having been there all along.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float InitialPerchedFraction = 0.4f;

	/** Fraction already airborne and part way through a circuit when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float InitialAirborneFraction = 0.15f;

	/**
	 * Airborne birds inside Separation Radius of a flockmate push apart, instead of relying on their jitter
	 * phases to keep them from converging.
	 *
	 * The console can override this either way with flock.Separation, which is for measuring what it costs;
	 * this is the answer that ships.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bEnableSeparation = true;

	/**
	 * Separation runs at this tier and every closer one. It is O(n^2) within a chunk, so the further out it
	 * is allowed to run the more it costs, and the less of it anyone can see.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(EditCondition="bEnableSeparation"))
	EFlockLODTier SeparationMaxTier = EFlockLODTier::Near;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0.0", ForceUnits="cm", EditCondition="bEnableSeparation"))
	float SeparationRadius = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(ClampMin="0.0", EditCondition="bEnableSeparation"))
	float SeparationStrength = 400.f;

	/** With no slot free for this long, a bird gives up and drops onto open ground where it started. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float MaxOrbitTime = 20.f;
};
