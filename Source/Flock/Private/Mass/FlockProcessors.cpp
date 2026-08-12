// Copyright (c) Jared Taylor

#include "Mass/FlockProcessors.h"

#include "Debug/FlockDebug.h"
#include "FlockDeveloper.h"
#include "FlockStats.h"
#include "MassExecutionContext.h"
#include "Mass/EntityFragments.h"
#include "Mass/FlockEntityFactory.h"
#include "Mass/FlockFragments.h"
#include "Mass/FlockTagFragments.h"
#include "System/FlockSubsystem.h"

UFlockProcessor::UFlockProcessor()
{
	// The subsystem runs our pipeline. Left on, these would silently never execute.
	bAutoRegisterWithProcessingPhases = false;

	// Birds are cosmetic; a dedicated server must never simulate them.
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Client);

	// Decorative while we drive execution, but it keeps the Mass debugger's graph readable.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	bRequiresGameThreadExecution = true;
}

void UFlockProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& InEntityManager)
{
	Super::InitializeInternal(Owner, InEntityManager);

	// The pipeline is initialised with the subsystem as owner, so this is always it.
	Subsystem = Cast<UFlockSubsystem>(&Owner);
}

UFlockLODProcessor::UFlockLODProcessor()
	: EntityQuery(*this)
{
}

void UFlockLODProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockLODFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();
}

void UFlockLODProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(LOD);

	UFlockSubsystem* LocalSubsystem = Subsystem;
	if (!LocalSubsystem || !LocalSubsystem->ShouldRunLODThisFrame())
	{
		return;
	}

	const FVector ViewLocation = LocalSubsystem->GetViewLocation();
	const FVector ViewForward = LocalSubsystem->GetViewForward();
	const float TanHalfFOV = LocalSubsystem->GetTanHalfFOV();

	const UFlockDeveloperSettings& Settings = UFlockDeveloperSettings::Get();
	const float Hysteresis = FMath::Max(1.f, Settings.LODHysteresis);

	int32 Counts[4] = { 0, 0, 0, 0 };

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FFlockLODFragment> LODs = Context.GetMutableFragmentView<FFlockLODFragment>();

		const float Dt = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			FFlockLODFragment& LOD = LODs[*It];
			LOD.TierCooldown = FMath::Max(0.f, LOD.TierCooldown - Dt);

			const FVector Position = Transforms[*It].GetTransform().GetLocation();
			const FVector ToBird = Position - ViewLocation;

			LOD.DistSqToView = ToBird.SizeSquared();
			const float Distance = FMath::Sqrt(LOD.DistSqToView);

			// Analytic, not a projection: radius over distance times the FOV term is close enough to decide a
			// tier, and costs a divide.
			LOD.ScreenRadius = Distance > 1.f
				? Config.BoundsRadius / (Distance * FMath::Max(0.01f, TanHalfFOV))
				: 1.f;

			// With occlusion queries off there is nothing else to cull by, so behind the camera is the one
			// cheap test worth making.
			const bool bBehind = FVector::DotProduct(ToBird, ViewForward) < 0.f;

			EFlockLODTier Wanted;
			if (bBehind && Distance > Settings.NearDistance)
			{
				Wanted = EFlockLODTier::Culled;
			}
			else
			{
				// Demote at the threshold times the hysteresis, promote at the threshold, so the boundary is
				// not a single value a drifting bird can sit on.
				const float Slack = LOD.Tier > EFlockLODTier::Near ? Hysteresis : 1.f;
				if (Distance < Settings.NearDistance * Slack)
				{
					Wanted = EFlockLODTier::Near;
				}
				else if (Distance < Settings.MidDistance * Slack)
				{
					Wanted = EFlockLODTier::Mid;
				}
				else if (Distance < Settings.FarDistance * Slack)
				{
					Wanted = EFlockLODTier::Far;
				}
				else
				{
					Wanted = EFlockLODTier::Culled;
				}
			}

			++Counts[static_cast<int32>(Wanted)];

			if (Wanted == LOD.Tier || LOD.TierCooldown > 0.f)
			{
				continue;
			}

			const FMassEntityHandle Entity = Context.GetEntity(*It);
			switch (LOD.Tier)
			{
			case EFlockLODTier::Near:   Context.Defer().RemoveTag<FFlockLODNearTag>(Entity); break;
			case EFlockLODTier::Mid:    Context.Defer().RemoveTag<FFlockLODMidTag>(Entity); break;
			case EFlockLODTier::Far:    Context.Defer().RemoveTag<FFlockLODFarTag>(Entity); break;
			case EFlockLODTier::Culled: Context.Defer().RemoveTag<FFlockLODCulledTag>(Entity); break;
			}

			switch (Wanted)
			{
			case EFlockLODTier::Near:   Context.Defer().AddTag<FFlockLODNearTag>(Entity); break;
			case EFlockLODTier::Mid:    Context.Defer().AddTag<FFlockLODMidTag>(Entity); break;
			case EFlockLODTier::Far:    Context.Defer().AddTag<FFlockLODFarTag>(Entity); break;
			case EFlockLODTier::Culled: Context.Defer().AddTag<FFlockLODCulledTag>(Entity); break;
			}

			LOD.Tier = Wanted;
			LOD.TierCooldown = Settings.LODDwellTime;
			LOD.FrameStride = Wanted == EFlockLODTier::Mid ? FMath::Max(1, Settings.MidFrameDivisor)
				: (Wanted == EFlockLODTier::Far ? FMath::Max(1, Settings.FarFrameDivisor) : 1);
		}
	});

	SET_DWORD_STAT(STAT_Flock_BirdsNear, Counts[0]);
	SET_DWORD_STAT(STAT_Flock_BirdsMid, Counts[1]);
	SET_DWORD_STAT(STAT_Flock_BirdsFar, Counts[2]);
	SET_DWORD_STAT(STAT_Flock_BirdsCulled, Counts[3]);
}

namespace FlockProcessorPrivate
{
	/**
	 * Whether this bird does its expensive work this frame, and the delta to use if so.
	 *
	 * Offsetting the phase by the bird's seed matters as much as the stride: without it every Far bird would
	 * land on the same frame and the saving would be a stutter rather than a saving.
	 */
	static bool ShouldStep(const FFlockLODFragment& LOD, uint8 Seed, uint32 FrameCounter, float Dt,
		float& OutDt)
	{
		const int32 Stride = FMath::Max(1, LOD.FrameStride);
		if (Stride == 1)
		{
			OutDt = Dt;
			return true;
		}

		if (static_cast<int32>((FrameCounter + Seed) % static_cast<uint32>(Stride)) != 0)
		{
			return false;
		}

		OutDt = Dt * Stride;
		return true;
	}

	/** Below this, horizontal velocity is too small for its direction to mean anything. */
	static constexpr float MinHeadingSpeed = 5.f;

	/**
	 * Heading and lean, and nothing else.
	 *
	 * No pitch: a bird's climb and dive are in its animation, and taking pitch from the velocity fights that
	 * and snaps the moment the velocity turns vertical. Roll is applied about the direction of travel rather
	 * than as a rotator component, so it reads as a bank whichever axis the mesh was authored down.
	 */
	/**
	 * A facing built from the yaw the bird holds rather than from where it is going, so a turn can be rate
	 * limited and still roll about the direction of travel.
	 */
	static FQuat MakeFacingFromYaw(float FacingYaw, float MeshYawOffset, float RollDegrees)
	{
		const FQuat Yaw = FRotator(0.f, FacingYaw, 0.f).Quaternion();
		if (FMath::IsNearlyZero(RollDegrees, 0.01f))
		{
			return Yaw;
		}

		const FVector Axis = FRotator(0.f, FacingYaw - MeshYawOffset, 0.f).Vector();
		return FQuat(Axis, FMath::DegreesToRadians(RollDegrees)) * Yaw;
	}

	static FQuat MakeFacing(const FVector& TravelDir, float MeshYawOffset, float RollDegrees)
	{
		return MakeFacingFromYaw(TravelDir.Rotation().Yaw + MeshYawOffset, MeshYawOffset, RollDegrees);
	}

	/**
	 * Nearest point on a blocker's surface to Position, and the outward direction there.
	 *
	 * Signed: negative means Position is inside. Inside a box the outward direction is the nearest face,
	 * which is why a bird that does get in leaves by the shortest way rather than through the far side.
	 */
	static float BlockerDistance(const FFlockBlocker& Blocker, const FVector& Position, FVector& OutOutward)
	{
		if (Blocker.Shape == EFlockBlockerShape::Sphere)
		{
			const FVector Radial = Position - FVector(Blocker.Centre);
			const float Length = Radial.Size();
			const float Radius = Blocker.Extent.X;

			OutOutward = Length > KINDA_SMALL_NUMBER ? Radial / Length : FVector::UpVector;
			return Length - Radius;
		}

		const FQuat Rotation(Blocker.Rotation);
		const FVector Extent(Blocker.Extent);
		const FVector Local = Rotation.UnrotateVector(Position - FVector(Blocker.Centre));

		const FVector Overshoot = Local.GetAbs() - Extent;
		if (Overshoot.GetMax() > 0.f)
		{
			const FVector Outside = Overshoot.ComponentMax(FVector::ZeroVector);
			const FVector LocalOutward(
				Overshoot.X > 0.f ? FMath::Sign(Local.X) : 0.f,
				Overshoot.Y > 0.f ? FMath::Sign(Local.Y) : 0.f,
				Overshoot.Z > 0.f ? FMath::Sign(Local.Z) : 0.f);

			OutOutward = Rotation.RotateVector(
				(LocalOutward * Outside).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
			return Outside.Size();
		}

		// Inside, so every component is negative and the closest face is the largest of them.
		int32 Axis = 0;
		if (Overshoot.Y > Overshoot[Axis])
		{
			Axis = 1;
		}
		if (Overshoot.Z > Overshoot[Axis])
		{
			Axis = 2;
		}

		FVector LocalOutward = FVector::ZeroVector;
		LocalOutward[Axis] = Local[Axis] >= 0.f ? 1.f : -1.f;

		OutOutward = Rotation.RotateVector(LocalOutward);
		return Overshoot[Axis];
	}

	/**
	 * Moves a position out of any blocker it is inside. The hard half of avoidance, and the only half that
	 * guarantees anything: steering cannot promise a bird stays out at a finite turn rate.
	 */
	static void PushOutOfBlockers(const FFlockRuntimeSharedFragment& Runtime, FVector& Position)
	{
		for (int32 Index = 0; Index < Runtime.NumBlockers; ++Index)
		{
			FVector Outward;
			const float Distance = BlockerDistance(Runtime.Blockers[Index], Position, Outward);

			if (Distance < 0.f)
			{
				// A hair outside, so the next frame's test does not read as still inside.
				Position += Outward * (-Distance + 1.f);
			}
		}
	}

	/**
	 * A nudge away from blockers the position is near, so a flock reads as knowing a building is there rather
	 * than pulling up short against it.
	 *
	 * Clamped to one bird's worth of speed. Left unclamped it can exceed whatever the bird was trying to do
	 * and cancel it outright, which is a bird hovering next to a volume forever instead of steering past it.
	 */
	static FVector SteerFromBlockers(const FFlockRuntimeSharedFragment& Runtime, float Strength,
		const FVector& Position)
	{
		FVector Steer = FVector::ZeroVector;

		for (int32 Index = 0; Index < Runtime.NumBlockers; ++Index)
		{
			const FFlockBlocker& Blocker = Runtime.Blockers[Index];

			FVector Outward;
			const float Distance = BlockerDistance(Blocker, Position, Outward);

			if (Distance < 0.f)
			{
				Steer += Outward * Strength;
			}
			else if (Blocker.Margin > 0.f && Distance < Blocker.Margin)
			{
				Steer += Outward * (Strength * (1.f - Distance / Blocker.Margin));
			}
		}

		return Steer.GetClampedToMaxSize(1.f);
	}

	/** The way the bird is already facing, as a direction, for when its velocity cannot say. */
	static FVector HeldHeading(const FTransform& Transform, float MeshYawOffset)
	{
		return FRotator(0.f, Transform.GetRotation().Rotator().Yaw - MeshYawOffset, 0.f).Vector();
	}

	/**
	 * How far a bird leans to hold a turn at this speed and this rate of turn, capped at MaxDegrees.
	 *
	 * The real relationship rather than a fraction of the turn-rate limit, so a wide slow orbit leans a
	 * little and a hard break leans a lot, without either being tuned for.
	 */
	static float CoordinatedBank(float Speed2D, float YawRateDegrees, float MaxDegrees)
	{
		constexpr float GravityCmS2 = 980.f;

		// Negated because the roll is applied as a rotation about the direction of travel, where a positive
		// angle lifts the right wing.
		const float Lean = -FMath::RadiansToDegrees(
			FMath::Atan2(Speed2D * FMath::DegreesToRadians(YawRateDegrees), GravityCmS2));

		return FMath::Clamp(Lean, -MaxDegrees, MaxDegrees);
	}

	/**
	 * Bank, glide or level flight, for a bird already cruising. Both alternatives are optional, so anything
	 * unmapped falls back to Fly and a species with neither never changes clip here.
	 */
	static void SelectFlightClip(const FFlockSpeciesConfigFragment& Config, const FFlockFlightParams& F,
		FFlockAnimFragment& Anim, float Now, uint8 Seed, float YawRate, bool bTurningRight, float DescentRate)
	{
		const EFlockClip Current = Anim.Clip;
		const bool bBanking = Current == EFlockClip::BankLeft || Current == EFlockClip::BankRight;
		const bool bGliding = Current == EFlockClip::Glide;

		// Only ever swaps between the cruise clips. A launch or a flare owns the clip until it is done.
		if (Current != EFlockClip::Fly && !bBanking && !bGliding)
		{
			return;
		}

		// A clip already playing holds until well under the threshold that started it, or it chatters frame
		// to frame around the boundary.
		constexpr float Release = 0.6f;

		EFlockClip Wanted = EFlockClip::Fly;
		if (F.BankClipYawRate > 0.f && YawRate >= F.BankClipYawRate * (bBanking ? Release : 1.f))
		{
			Wanted = bTurningRight ? EFlockClip::BankRight : EFlockClip::BankLeft;
		}
		else if (F.GlideDescentRate > 0.f && DescentRate >= F.GlideDescentRate * (bGliding ? Release : 1.f))
		{
			Wanted = EFlockClip::Glide;
		}

		if (!Config.GetClip(Wanted).bValid)
		{
			Wanted = EFlockClip::Fly;
		}

		if (Wanted != Current && Config.GetClip(Wanted).bValid && Config.CanLeaveClip(Anim, Now))
		{
			Config.StartClip(Anim, Wanted, Now, Seed);
		}
	}
}

UFlockThreatProcessor::UFlockThreatProcessor()
	: EntityQuery(*this)
{
}

void UFlockThreatProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FFlockLODFragment>(EMassFragmentAccess::ReadOnly);
	// A culled bird is not worth simulating; excluding the tag means its chunks are never visited.
	EntityQuery.AddTagRequirement<FFlockLODCulledTag>(EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();
	EntityQuery.AddSharedRequirement<FFlockRuntimeSharedFragment>(EMassFragmentAccess::ReadWrite);

	// Only birds on the ground care about threats; airborne ones are already away.
	EntityQuery.AddTagRequirement<FFlockGroundedTag>(EMassFragmentPresence::All);

	// A flock with nothing near it and no contagion left is skipped wholesale, without visiting a single
	// bird. This is why the threat list lives in a shared fragment.
	EntityQuery.SetChunkFilter([](const FMassExecutionContext& Context)
	{
		const FFlockRuntimeSharedFragment& Runtime = Context.GetSharedFragment<FFlockRuntimeSharedFragment>();

		// bAnyAlert matters as much as the threats: decay happens in this processor, so a flock skipped for
		// having nothing near it would otherwise stay alarmed forever.
		return Runtime.HasThreats() || Runtime.ContagionStrength > 0.f || Runtime.bAnyAlert;
	});
}

void UFlockThreatProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Threat);

	const UWorld* ThreatWorld = EntityManager.GetWorld();
	const float Now = ThreatWorld ? ThreatWorld->GetTimeSeconds() : 0.f;
	const uint32 Frame = Subsystem ? Subsystem->GetFrameCounter() : 0;

	EntityQuery.ForEachEntityChunk(Context, [Now, Frame](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		FFlockRuntimeSharedFragment& Runtime = Context.GetMutableSharedFragment<FFlockRuntimeSharedFragment>();
		const FFlockPerception& P = Config.Perception;

		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FFlockBirdFragment> Birds = Context.GetFragmentView<FFlockBirdFragment>();
		const TConstArrayView<FFlockLODFragment> LODs = Context.GetFragmentView<FFlockLODFragment>();
		const TArrayView<FFlockStateFragment> States = Context.GetMutableFragmentView<FFlockStateFragment>();

		const float FrameDt = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			float Dt;
			if (!FlockProcessorPrivate::ShouldStep(LODs[*It], Birds[*It].RandomSeed, Frame, FrameDt, Dt))
			{
				continue;
			}

			const FVector BirdPos = Transforms[*It].GetTransform().GetLocation();
			FFlockStateFragment& State = States[*It];

			float ThreatRate = 0.f;
			float NearestDistSq = TNumericLimits<float>::Max();
			FVector NearestPos = FVector::ZeroVector;

			for (int32 Index = 0; Index < Runtime.NumThreats; ++Index)
			{
				const FFlockThreat& Threat = Runtime.Threats[Index];
				const FVector ThreatPos(Threat.Position);
				const FVector ToBird = BirdPos - ThreatPos;

				// The species caps the source's own reach, so a heavy source cannot reach across the level.
				// A deliberate scare opts out of that: a caller naming a radius means that radius.
				const float Reach = Threat.bIgnoreSpeciesRange
					? Threat.MaxRadius : FMath::Min(Threat.MaxRadius, P.MaxNoticeRadius);

				const float DistSq = ToBird.SizeSquared();
				if (DistSq > FMath::Square(Reach))
				{
					continue;
				}

				if (DistSq < NearestDistSq)
				{
					NearestDistSq = DistSq;
					NearestPos = ThreatPos;
				}

				const float Dist = FMath::Sqrt(DistSq);

				// Curved, not linear. Linear falloff means a heavy source is already alarming at the edge of
				// its radius, so birds break far too early and no radius tweak fixes it - the exponent does.
				const float Linear = 1.f - FMath::Clamp(
					(Dist - P.SafeRadius) / FMath::Max(1.f, Reach - P.SafeRadius), 0.f, 1.f);

				const float Exponent = Threat.ProximityExponent > 0.f
					? Threat.ProximityExponent : P.ProximityExponent;
				const float Proximity = FMath::Pow(Linear, Exponent);

				const FVector ThreatVel(Threat.Velocity);
				const FVector Away = Dist > KINDA_SMALL_NUMBER ? ToBird / Dist : FVector::ZeroVector;

				// Only approach counts; something walking away is not getting more alarming.
				const float Closing = FMath::Clamp(
					FVector::DotProduct(ThreatVel, Away) / P.ClosingSpeedRef, 0.f, 1.f);
				const float SpeedTerm = FMath::Clamp(ThreatVel.Size() / P.SpeedRef, 0.f, 1.f);

				ThreatRate += Threat.Weight * Proximity
					* (P.BaseWeight + P.ClosingWeight * Closing + P.SpeedWeight * SpeedTerm);
			}

			ThreatRate = FMath::Min(ThreatRate, P.MaxThreatRate);

			// A flockmate taking off is alarming in itself. This is what makes a flock erupt as one.
			if (Now < Runtime.ContagionUntil)
			{
				ThreatRate += Runtime.ContagionStrength;
			}

				// Only read here, never decayed here: this processor is skipped wholesale for a flock with
				// nothing near it, which is precisely when a landed bird needs to come out of its cooldown.
				// The decision processor owns the countdown.

				// Just-landed birds shed alarm faster, so they do not bounce straight back up.
			const float Decay = State.RefractoryTime > 0.f
				? P.AlertDecay * P.RefractoryDecayScale : P.AlertDecay;

			State.Alert = FMath::Clamp(ThreatRate > 0.f
				? State.Alert + ThreatRate * P.AlertGain * Dt
				: State.Alert - Decay * Dt, 0.f, 1.f);

			// Something already on top of the bird bypasses the accumulator entirely; a sprint or a
			// teleport would otherwise arrive before alarm had time to build.
			if (NearestDistSq < FMath::Square(P.PanicRadius))
			{
				State.Alert = 1.f;
			}

			if (State.Alert > 0.f)
			{
				Runtime.bAnyAlert = true;
			}

			// Signed yaw toward the nearest threat, for the turn clips and the facing. Zeroed when there is
			// nothing to look at, so a bird never keeps turning toward a source that has gone.
			if (NearestDistSq < TNumericLimits<float>::Max())
			{
				const FVector ToThreat = NearestPos - BirdPos;
				const float Desired = FMath::RadiansToDegrees(FMath::Atan2(ToThreat.Y, ToThreat.X))
					+ Config.MeshYawOffset;
				const float Current = Transforms[*It].GetTransform().GetRotation().Rotator().Yaw;
				State.YawDelta = FRotator::NormalizeAxis(Desired - Current);
			}
			else
			{
				State.YawDelta = 0.f;
			}
		}
	});
}

UFlockDecisionProcessor::UFlockDecisionProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UFlockThreatProcessor::StaticClass()->GetFName());
}

void UFlockDecisionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FFlockLODFragment>(EMassFragmentAccess::ReadOnly);
	// A culled bird is not worth simulating; excluding the tag means its chunks are never visited.
	EntityQuery.AddTagRequirement<FFlockLODCulledTag>(EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockAnimFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();
	EntityQuery.AddSharedRequirement<FFlockRuntimeSharedFragment>(EMassFragmentAccess::ReadWrite);

	// Airborne birds are driven by the flight processors, which own their own clips.
	EntityQuery.AddTagRequirement<FFlockGroundedTag>(EMassFragmentPresence::All);
}

void UFlockDecisionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Decision);

	const UWorld* World = EntityManager.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const uint32 Frame = Subsystem ? Subsystem->GetFrameCounter() : 0;

	EntityQuery.ForEachEntityChunk(Context, [Now, Frame](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		FFlockRuntimeSharedFragment& Runtime = Context.GetMutableSharedFragment<FFlockRuntimeSharedFragment>();
		const FFlockPerception& P = Config.Perception;
		const FFlockFlightParams& F = Config.Flight;

		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FFlockBirdFragment> Birds = Context.GetFragmentView<FFlockBirdFragment>();
		const TArrayView<FFlockStateFragment> States = Context.GetMutableFragmentView<FFlockStateFragment>();
		const TArrayView<FFlockAnimFragment> Anims = Context.GetMutableFragmentView<FFlockAnimFragment>();
		const TConstArrayView<FFlockLODFragment> LODs = Context.GetFragmentView<FFlockLODFragment>();

		const float FrameDt = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			float Dt;
			if (!FlockProcessorPrivate::ShouldStep(LODs[*It], Birds[*It].RandomSeed, Frame, FrameDt, Dt))
			{
				continue;
			}

			FFlockStateFragment& State = States[*It];
			FFlockAnimFragment& Anim = Anims[*It];

			State.StateTime += Dt;

			// Counted down here rather than in the threat processor, which never visits a calm flock at all.
			// Left there, a bird that landed once held its cooldown forever and could never move of its own
			// accord again: no dawdle, no shuffle, nothing.
			State.RefractoryTime = FMath::Max(0.f, State.RefractoryTime - Dt);

			// --- voluntary moves ---
			// Rolled, not alternated, so a bird sometimes picks the same kind of place twice. Strict
			// alternation is noticeably rhythmic once you watch one bird for a while.
			if (F.bAllowRestlessMoves && State.RestlessTimer > 0.f)
			{
				// While the flock is under its airborne target the countdown runs fast, so there is usually
				// something in the air. Once it is met this drops back to the ordinary shuffle interval and
				// the flock settles again.
				State.RestlessTimer -= Runtime.bWantsAirborne
					? Dt * FMath::Max(1.f, F.AmbientAirborneUrgency) : Dt;
			}

			const bool bSettled = State.State == EFlockBirdState::Grounded
				|| State.State == EFlockBirdState::Perched;

			if (F.bAllowRestlessMoves && bSettled && State.RestlessTimer <= 0.f
				&& State.Alert < P.PerkThreshold && State.RefractoryTime <= 0.f
				&& Config.GetClip(EFlockClip::Fly).bValid)
			{
				State.bVoluntaryMove = true;
				State.bWalking = false;
				State.GlanceRemaining = 0.f;

				// Varies per bird and per move, so the flock does not decide together.
				const float Roll = FMath::Frac(Birds[*It].RandomSeed * 0.0173f + Now * 0.137f);
				State.bTargetPerch = Roll < F.PerchPreference;

				// Some of these are a lap overhead rather than a hop across, which is what a flock with
				// nothing wrong still looks busy doing. A move made to fill the airborne target always is.
				State.bPrefersOrbit = Runtime.bWantsAirborne
					|| FMath::Frac(Roll * 5.41f) < F.RestlessOrbitChance;
				State.OrbitDuration = FMath::Lerp(F.OrbitTimeMin, F.OrbitTimeMax, Roll);
				State.bJustTookOff = true;

				// Deliberately no contagion: one bird choosing to move should not panic the flock.
				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::TakingOff);

				Config.StartClip(Anim,
					Config.GetClip(EFlockClip::TakeOff).bValid ? EFlockClip::TakeOff : EFlockClip::Fly,
					Now, Birds[*It].RandomSeed);
				continue;
			}

			// --- flee ---
			const float FleeBar = FMath::Min(1.f, P.FleeThreshold
				+ (State.RefractoryTime > 0.f ? P.RefractoryFleeBonus : 0.f));

			if (State.Alert >= FleeBar && Config.GetClip(EFlockClip::Fly).bValid)
			{
				const float Roll = Birds[*It].RandomSeed / 255.f;
				State.bVoluntaryMove = false;
				State.bWalking = false;
				State.GlanceRemaining = 0.f;
				State.bPrefersOrbit = Roll < Runtime.OrbitPreference;
				State.OrbitDuration = FMath::Lerp(F.OrbitTimeMin, F.OrbitTimeMax, Roll);
				State.bJustTookOff = true;

				// Alarm the rest of the flock, so the next bird goes without waiting to notice for itself.
				Runtime.ContagionUntil = Now + P.ContagionWindow;
				Runtime.ContagionStrength = P.ContagionStrength;

				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::TakingOff);

				Config.StartClip(Anim,
					Config.GetClip(EFlockClip::TakeOff).bValid ? EFlockClip::TakeOff : EFlockClip::Fly,
					Now, Birds[*It].RandomSeed);
				continue;
			}

			// Per-bird jitter, so a flock perks up raggedly instead of as one animal.
			const float Jitter = (Birds[*It].RandomSeed / 255.f - 0.5f) * 2.f * P.ThresholdJitter;
			const float Perk = FMath::Max(0.01f, P.PerkThreshold + Jitter * P.PerkThreshold);
			const float Release = Perk * P.PerkReleaseRatio;

			const bool bWasAlert = State.State == EFlockBirdState::Alert;

			// Asymmetric thresholds: rises at Perk, only relaxes below Release.
			if (!bWasAlert && State.Alert > Perk)
			{
				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::Alert);
			}
			else if (bWasAlert && State.Alert < Release)
			{
				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::Grounded);
			}

			EFlockClip Wanted = EFlockClip::Idle;

			if (State.State == EFlockBirdState::Alert)
			{
				const float AbsYaw = FMath::Abs(State.YawDelta);

				// Latch the direction on entry and hold it until the bird is most of the way there. Re-picking
				// per frame restarts the clip every time the error wobbles across the deadband, which is what
				// reads as snapping.
				if (State.TurnSign == 0)
				{
					if (AbsYaw > P.TurnDeadbandDegrees)
					{
						State.TurnSign = State.YawDelta > 0.f ? 1 : -1;
					}
				}
				else if (AbsYaw < P.TurnDeadbandDegrees * 0.35f && Config.CanLeaveClip(Anim, Now))
				{
					// Held until the turn clip is also ready to end, so the bird keeps rotating for as long as
					// it is playing the animation of rotating.
					State.TurnSign = 0;
				}

				if (State.TurnSign != 0)
				{
					// Never overshoot: the step is capped by the error that is actually left.
					const float Step = FMath::Min(AbsYaw, P.TurnRateDegrees * Dt) * State.TurnSign;

					FTransform& Transform = Transforms[*It].GetMutableTransform();
					FRotator Rotation = Transform.GetRotation().Rotator();
					Rotation.Yaw += Step;
					Transform.SetRotation(Rotation.Quaternion());

					State.YawDelta = FRotator::NormalizeAxis(State.YawDelta - Step);

					Wanted = State.TurnSign > 0 ? EFlockClip::TurnLeft : EFlockClip::TurnRight;
				}
			}
			else
			{
				State.TurnSign = 0;
			}

			// Fall back to idle for a clip the species never mapped.
			if (!Config.GetClip(Wanted).bValid)
			{
				Wanted = EFlockClip::Idle;
			}

			// The idle processor owns the clip of a calm bird that is dawdling, glancing or part way through a
			// rest break. Without this the default of idle here would cancel every one of them the frame after
			// it started.
			if (State.State != EFlockBirdState::Alert
				&& (State.bWalking || State.GlanceRemaining != 0.f || IsFlockRestClip(Anim.Clip)))
			{
				Wanted = Anim.Clip;
			}

			// Let a must-complete clip finish. Without this a turn is cut off the instant the bird is facing
			// close enough, and with no blending between VAT clips that is a visible snap.
			if (Wanted != Anim.Clip && !Config.CanLeaveClip(Anim, Now))
			{
				Wanted = Anim.Clip;
			}

			if (Anim.Clip != Wanted)
			{
				Config.StartClip(Anim, Wanted, Now, Birds[*It].RandomSeed);
			}
		}
	});
}

UFlockIdleProcessor::UFlockIdleProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UFlockDecisionProcessor::StaticClass()->GetFName());
}

void UFlockIdleProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockAnimFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();
	EntityQuery.AddTagRequirement<FFlockGroundedTag>(EMassFragmentPresence::All);

	// Far birds are still visited, at their stride, because a bird part way through a dawdle has to finish it
	// wherever it is. Culled ones are not rendered at all, so nothing they do could be seen.
	EntityQuery.AddTagRequirement<FFlockLODCulledTag>(EMassFragmentPresence::None);
}

void UFlockIdleProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Idle);

	const UWorld* World = EntityManager.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const uint32 Frame = Subsystem ? Subsystem->GetFrameCounter() : 0;
	UFlockSubsystem* LocalSubsystem = Subsystem;

	EntityQuery.ForEachEntityChunk(Context, [Now, Frame, LocalSubsystem](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		const FFlockPerception& P = Config.Perception;
		const FFlockIdleParams& I = Config.Idle;

		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FFlockBirdFragment> Birds = Context.GetFragmentView<FFlockBirdFragment>();
		const TArrayView<FFlockStateFragment> States = Context.GetMutableFragmentView<FFlockStateFragment>();
		const TArrayView<FFlockAnimFragment> Anims = Context.GetMutableFragmentView<FFlockAnimFragment>();
		const TConstArrayView<FFlockLODFragment> LODs = Context.GetFragmentView<FFlockLODFragment>();

		const float FrameDt = Context.GetDeltaTimeSeconds();
		const bool bCanWalk = I.bAllowWalking && Config.GetClip(EFlockClip::Walk).bValid;

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			float Dt;
			if (!FlockProcessorPrivate::ShouldStep(LODs[*It], Birds[*It].RandomSeed, Frame, FrameDt, Dt))
			{
				continue;
			}

			FFlockStateFragment& State = States[*It];
			FFlockAnimFragment& Anim = Anims[*It];

			// Anything alarming ends all of this at once. The decision processor owns an alert bird's facing
			// and its clip, so nothing here may keep hold of either.
			if (State.State == EFlockBirdState::Alert)
			{
				State.bWalking = false;
				State.GlanceRemaining = 0.f;
				continue;
			}

			// --- a dawdle in progress ---
			if (State.bWalking)
			{
				FTransform& Transform = Transforms[*It].GetMutableTransform();
				const FVector Position = Transform.GetLocation();

				FVector ToTarget = FVector(State.WalkTarget) - Position;
				ToTarget.Z = 0.f;
				const float Distance = ToTarget.Size2D();

				if (Distance <= I.WalkArriveDistance)
				{
					State.bWalking = false;

					Config.StartClip(Anim, EFlockClip::Idle, Now, Birds[*It].RandomSeed);
					continue;
				}

				const FVector Direction = ToTarget / Distance;

				FRotator Rotation = Transform.GetRotation().Rotator();
				const float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X))
					+ Config.MeshYawOffset;
				const float YawError = FRotator::NormalizeAxis(DesiredYaw - Rotation.Yaw);
				const float YawStep = FMath::Clamp(YawError,
					-I.WalkTurnRateDegrees * Dt, I.WalkTurnRateDegrees * Dt);

				Rotation.Yaw += YawStep;
				Transform.SetRotation(Rotation.Quaternion());

				// Slows into a turn rather than crabbing sideways, which with no blending is the only thing
				// separating a walk from a slide.
				const float Alignment = FMath::Max(0.f, FMath::Cos(FMath::DegreesToRadians(YawError)));
				const float Step = FMath::Min(Distance, I.WalkSpeed * Alignment * Dt);

				FVector Moved = Position + Direction * Step;

				// Nothing traces here. The bird holds the height it was placed at, which is why the radius is
				// meant to stay short.
				Moved.Z = Birds[*It].GroundLocation.Z;
				Transform.SetLocation(Moved);
				continue;
			}

			// --- a glance in progress ---
			if (State.GlanceRemaining != 0.f)
			{
				const float Step = FMath::Sign(State.GlanceRemaining)
					* FMath::Min(FMath::Abs(State.GlanceRemaining), P.TurnRateDegrees * 0.45f * Dt);

				FTransform& Transform = Transforms[*It].GetMutableTransform();
				FRotator Rotation = Transform.GetRotation().Rotator();
				Rotation.Yaw += Step;
				Transform.SetRotation(Rotation.Quaternion());

				State.GlanceRemaining -= Step;

				// Hand the clip back to idle only once the turn clip is willing to end, or it snaps.
				if (FMath::IsNearlyZero(State.GlanceRemaining, 0.5f) && Config.CanLeaveClip(Anim, Now))
				{
					State.GlanceRemaining = 0.f;
					Config.StartClip(Anim, EFlockClip::Idle, Now, Birds[*It].RandomSeed);
				}
				continue;
			}

			State.RestTimer -= Dt;
			State.WalkTimer -= Dt;
			State.GlanceTimer -= Dt;

			// A rest break runs to its end and returns itself to idle from the anim processor, so all that is
			// needed here is not to interrupt it.
			if (!Config.CanLeaveClip(Anim, Now))
			{
				continue;
			}

			// One decision per bird per frame, in order of how much each one is worth watching.
			const float Roll = FMath::Frac(Birds[*It].RandomSeed * 0.0211f + Now * 0.0917f);

			if (I.bAllowRestBreaks && State.RestTimer <= 0.f)
			{
				State.RestTimer = FMath::Lerp(I.RestIntervalMin, I.RestIntervalMax, Roll);

				const EFlockClip Rest = Config.PickRestClip(FMath::Frac(Roll * 7.13f),
					FMath::Frac(Roll * 19.73f));
				if (Rest != EFlockClip::Count)
				{
					Config.StartClip(Anim, Rest, Now, Birds[*It].RandomSeed);

					// Queued, never played from here: a processor is not guaranteed to be on the game thread,
					// and touching an audio component from a worker crashes.
					if (LocalSubsystem)
					{
						LocalSubsystem->DispatchClipStarted(Birds[*It].FlockIndex, Rest,
							Transforms[*It].GetTransform().GetLocation());
					}
					continue;
				}
			}

			// Only a bird standing on open ground wanders. One holding a slot would be walking off a branch,
			// and leaving a reserved perch stood empty behind it.
			if (bCanWalk && State.WalkTimer <= 0.f && State.State == EFlockBirdState::Grounded
				&& Birds[*It].HomeSlotIndex == INDEX_NONE && State.RefractoryTime <= 0.f)
			{
				State.WalkTimer = FMath::Lerp(I.WalkIntervalMin, I.WalkIntervalMax, Roll);

				// Aimed from the spot it spawned on rather than from wherever it currently stands, so a bird
				// that dawdles all day never drifts out of its flock.
				const float Angle = FMath::Frac(Roll * 3.77f) * UE_TWO_PI;
				const float Reach = FMath::Lerp(I.WalkMinDistance, FMath::Max(I.WalkMinDistance, I.WalkRadius),
					FMath::Frac(Roll * 11.31f));

				const FVector Anchor(Birds[*It].GroundLocation);
				const FVector Target = Anchor
					+ FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, 0.f);

				if (FVector::DistSquared2D(Target, Transforms[*It].GetTransform().GetLocation())
					>= FMath::Square(I.WalkMinDistance))
				{
					State.WalkTarget = FVector3f(Target);
					State.bWalking = true;

					Config.StartClip(Anim, EFlockClip::Walk, Now, Birds[*It].RandomSeed);
					continue;
				}
			}

			if (P.bAllowGlances && State.GlanceTimer <= 0.f)
			{
				State.GlanceTimer = FMath::Lerp(P.GlanceIntervalMin, P.GlanceIntervalMax, Roll);
				State.GlanceRemaining = (Roll < 0.5f ? 1.f : -1.f) * P.GlanceYawDegrees * (0.5f + Roll);

				const EFlockClip Wanted = State.GlanceRemaining > 0.f
					? EFlockClip::TurnLeft : EFlockClip::TurnRight;

				Config.StartClip(Anim, Wanted, Now, Birds[*It].RandomSeed);
			}
		}
	});
}

UFlockTakeoffProcessor::UFlockTakeoffProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UFlockDecisionProcessor::StaticClass()->GetFName());
}

void UFlockTakeoffProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockAnimFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();
	EntityQuery.AddSharedRequirement<FFlockRuntimeSharedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FFlockTakingOffTag>(EMassFragmentPresence::All);
}

void UFlockTakeoffProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Takeoff);

	const UWorld* World = EntityManager.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	UFlockSubsystem* LocalSubsystem = Subsystem;

	EntityQuery.ForEachEntityChunk(Context, [Now, LocalSubsystem](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		const FFlockRuntimeSharedFragment& Runtime = Context.GetSharedFragment<FFlockRuntimeSharedFragment>();
		const FFlockFlightParams& F = Config.Flight;

		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FFlockVelocityFragment> Velocities = Context.GetMutableFragmentView<FFlockVelocityFragment>();
		const TArrayView<FFlockBirdFragment> Birds = Context.GetMutableFragmentView<FFlockBirdFragment>();
		const TArrayView<FFlockStateFragment> States = Context.GetMutableFragmentView<FFlockStateFragment>();
		const TArrayView<FFlockAnimFragment> Anims = Context.GetMutableFragmentView<FFlockAnimFragment>();

		const float Dt = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			FFlockStateFragment& State = States[*It];
			State.StateTime += Dt;

			FTransform& Transform = Transforms[*It].GetMutableTransform();
			const FVector Position = Transform.GetLocation();

			// Edge triggered, and cleared as it is consumed, so the burst can fire once and only once.
			if (State.bJustTookOff)
			{
				State.bJustTookOff = false;
				if (LocalSubsystem)
				{
					LocalSubsystem->DispatchTakeOff(Runtime.FlockIndex, Position);

					// It has left, so whatever it was standing on is free for someone else. The index goes
					// with it: it means the slot the bird holds right now, and anything reading it as one
					// still held would keep a bird pinned to a perch it left.
					if (Birds[*It].HomeSlotIndex != INDEX_NONE)
					{
						LocalSubsystem->ReleaseSlot(Birds[*It].HomeSlotIndex, Context.GetEntity(*It));
						Birds[*It].HomeSlotIndex = INDEX_NONE;
					}

					// And the destination falls back to the ground spot, which is always somewhere a bird
					// can stand. Left pointing at the perch it just gave up, a bird that never wins a new
					// slot lands on that old position anyway - on top of whoever took it, or inside
					// whatever the perch was mounted to. Granting a slot overwrites this again.
					Birds[*It].HomeLocation = Birds[*It].GroundLocation;
				}
			}

			// Up, plus away from whatever is nearest. Computed once and then held, so the launch reads as a
			// committed burst rather than a bird steering while it climbs.
			FVector Direction = FVector::UpVector * F.TakeoffUpBias;
			if (Runtime.NumThreats > 0)
			{
				FVector Away = Position - FVector(Runtime.Threats[0].Position);
				Away.Z = 0.f;
				if (Away.Normalize())
				{
					Direction += Away * F.TakeoffAwayBias;
				}
			}
			Direction = Direction.GetSafeNormal(1.e-4f, FVector::UpVector);

			// Ease out, so it leaves hard and blends into cruise instead of snapping to speed.
			const float Alpha = FMath::Clamp(State.StateTime / FMath::Max(0.01f, F.TakeoffTime), 0.f, 1.f);
			const float Speed = FMath::InterpEaseOut(0.f, F.TakeoffSpeed, Alpha, 2.f);

			// A launch is the one time a bird reliably heads into a ceiling, so the blockers get a say even
			// though nothing else about the climb steers.
			FVector Launch = Direction * Speed;
			FVector Climbed = Position + Launch * Dt;

			if (Runtime.HasBlockers())
			{
				FlockProcessorPrivate::PushOutOfBlockers(Runtime, Climbed);

				const FVector Steer = FlockProcessorPrivate::SteerFromBlockers(Runtime,
					F.BlockerAvoidStrength, Climbed);

				Launch = (Launch + Steer * Speed).GetSafeNormal(1.e-4f, Direction) * Speed;
			}

			Velocities[*It].Velocity = FVector3f(Launch);
			Transform.SetLocation(Climbed);

			// Turns to face the way it is leaving over the course of the launch. Left until the bird is
			// airborne, the first frame of flight snaps it round to whatever direction it happened to pick.
			const FVector FlatLaunch(Direction.X, Direction.Y, 0.f);
			if (FlatLaunch.SizeSquared() > FMath::Square(0.05f))
			{
				const FRotator Facing = Transform.GetRotation().Rotator();
				const float Wanted = FlatLaunch.Rotation().Yaw + Config.MeshYawOffset;

				Transform.SetRotation(
					FRotator(0.f, FMath::FixedTurn(Facing.Yaw, Wanted, F.TurnRateDegrees * Dt), 0.f)
						.Quaternion());
			}

			if (Alpha >= 1.f)
			{
				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::Flying);

				// A launch clip marked Must Complete finishes first. The anim processor hands it to Fly when
				// it does, so this only skips the write rather than losing the transition.
				if (Config.CanLeaveClip(Anims[*It], Now))
				{
					Config.StartClip(Anims[*It], EFlockClip::Fly, Now, Birds[*It].RandomSeed);
				}
			}
		}
	});
}

UFlockFlightProcessor::UFlockFlightProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UFlockTakeoffProcessor::StaticClass()->GetFName());
}

void UFlockFlightProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FFlockLODFragment>(EMassFragmentAccess::ReadOnly);
	// A culled bird is not worth simulating; excluding the tag means its chunks are never visited.
	EntityQuery.AddTagRequirement<FFlockLODCulledTag>(EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockAnimFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();
	EntityQuery.AddSharedRequirement<FFlockRuntimeSharedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FFlockFlyingTag>(EMassFragmentPresence::All);
}

void UFlockFlightProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Flight);

	const UWorld* World = EntityManager.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	UFlockSubsystem* LocalSubsystem = Subsystem;
	const uint32 Frame = Subsystem ? Subsystem->GetFrameCounter() : 0;

	EntityQuery.ForEachEntityChunk(Context, [Now, LocalSubsystem, Frame](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		const FFlockRuntimeSharedFragment& Runtime = Context.GetSharedFragment<FFlockRuntimeSharedFragment>();
		const FFlockFlightParams& F = Config.Flight;
		const FFlockPerception& P = Config.Perception;

		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FFlockVelocityFragment> Velocities = Context.GetMutableFragmentView<FFlockVelocityFragment>();
		const TConstArrayView<FFlockBirdFragment> Birds = Context.GetFragmentView<FFlockBirdFragment>();
		const TArrayView<FFlockStateFragment> States = Context.GetMutableFragmentView<FFlockStateFragment>();
		const TArrayView<FFlockAnimFragment> Anims = Context.GetMutableFragmentView<FFlockAnimFragment>();

		const TConstArrayView<FFlockLODFragment> LODs = Context.GetFragmentView<FFlockLODFragment>();

		const float FrameDt = Context.GetDeltaTimeSeconds();
		const FVector Attractor(Runtime.AttractorPosition);
		// The species decides; the console can force it either way for measuring what it costs. A forced-on
		// override ignores the tier limit, since the point of forcing it is to see it everywhere.
		const int32 SeparationOverride = FLOCK_SEPARATION_OVERRIDE();
		const bool bSeparate = SeparationOverride < 0 ? F.bEnableSeparation : SeparationOverride > 0;
		const EFlockLODTier SeparationMaxTier = SeparationOverride > 0
			? EFlockLODTier::Culled : F.SeparationMaxTier;

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			float Dt;
			if (!FlockProcessorPrivate::ShouldStep(LODs[*It], Birds[*It].RandomSeed, Frame, FrameDt, Dt))
			{
				continue;
			}

			FFlockStateFragment& State = States[*It];
			State.StateTime += Dt;

			// The threat processor only visits grounded birds, so nothing would ever lower this while airborne
			// and the bird could never become calm enough to look for a slot.
			State.Alert = FMath::Max(0.f, State.Alert - P.AlertDecay * Dt);

			FTransform& Transform = Transforms[*It].GetMutableTransform();
			const FVector Position = Transform.GetLocation();
			const FVector Velocity(Velocities[*It].Velocity);

			// Cohesion is flock-level, from the shared attractor, so no bird looks at any other bird.
			FVector Desired = (Attractor - Position).GetSafeNormal() * F.CruiseSpeed * F.CohesionWeight;

			// Per-bird phase offset stands in for separation; birds never converge because they never
			// wander in step.
			const float Phase = Birds[*It].RandomSeed * 0.0246f;
			const float T = Now * F.JitterFrequency + Phase;
			Desired += FVector(FMath::Sin(T), FMath::Cos(T * 1.31f), FMath::Sin(T * 0.7f) * 0.5f)
				* F.JitterAmplitude;

			// Chunk-local on purpose: a flock's value partitions its own chunks, so the birds in this chunk
			// are the ones close enough to matter, and no spatial structure has to be rebuilt each frame.
			// Tier-limited because it is O(n^2) within the chunk.
			if (bSeparate && LODs[*It].Tier <= SeparationMaxTier)
			{
				const float RadiusSq = FMath::Square(F.SeparationRadius);
				FVector Push = FVector::ZeroVector;

				for (FMassExecutionContext::FEntityIterator Other = Context.CreateEntityIterator(); Other; ++Other)
				{
					if (*Other == *It)
					{
						continue;
					}

					const FVector Away = Position - Transforms[*Other].GetTransform().GetLocation();
					const float AwaySq = Away.SizeSquared();
					if (AwaySq > KINDA_SMALL_NUMBER && AwaySq < RadiusSq)
					{
						// Falls off with distance, so a near miss nudges and a near collision shoves.
						Push += Away.GetUnsafeNormal() * (1.f - FMath::Sqrt(AwaySq) / F.SeparationRadius);
					}
				}

				Desired += Push * F.SeparationStrength;
			}

			// Steered around before the turn limit, so avoiding a building costs a bird the same wide arc as
			// any other change of direction rather than letting it pivot on the spot.
			FVector Moved = Position;
			if (Runtime.HasBlockers())
			{
				FlockProcessorPrivate::PushOutOfBlockers(Runtime, Moved);

				Desired += FlockProcessorPrivate::SteerFromBlockers(Runtime, F.BlockerAvoidStrength, Moved)
					* F.CruiseSpeed;
			}

			// Turn-rate limited, so nothing snaps direction.
			const FVector NewVelocity = Velocity.IsNearlyZero()
				? Desired
				: FMath::VInterpNormalRotationTo(Velocity.GetSafeNormal(), Desired.GetSafeNormal(),
					Dt, F.TurnRateDegrees) * F.CruiseSpeed;

			Velocities[*It].Velocity = FVector3f(NewVelocity);

			// Integrated from wherever the push-out left it, then pushed out again: one frame of travel can
			// cross a thin volume outright, and the second test is what makes "never inside" true rather
			// than merely likely.
			Moved += NewVelocity * Dt;
			if (Runtime.HasBlockers())
			{
				FlockProcessorPrivate::PushOutOfBlockers(Runtime, Moved);
			}

			Transform.SetLocation(Moved);

			// Face travel, leaning into the turn.
			{
				const FVector Flat(NewVelocity.X, NewVelocity.Y, 0.f);
				const float Speed2D = Flat.Size();
				const bool bHasHeading = Speed2D > FlockProcessorPrivate::MinHeadingSpeed;

				float TargetRoll = 0.f;
				if (bHasHeading)
				{
					const FVector OldFlat(Velocity.X, Velocity.Y, 0.f);
					const float YawRate = Dt > 0.f
							&& OldFlat.SizeSquared() > FMath::Square(FlockProcessorPrivate::MinHeadingSpeed)
						? FRotator::NormalizeAxis(Flat.Rotation().Yaw - OldFlat.Rotation().Yaw) / Dt
						: 0.f;

					TargetRoll = FlockProcessorPrivate::CoordinatedBank(Speed2D, YawRate, F.BankScale);

					FlockProcessorPrivate::SelectFlightClip(Config, F, Anims[*It], Now,
						Birds[*It].RandomSeed, FMath::Abs(YawRate), YawRate > 0.f, -NewVelocity.Z);
				}

				State.BankRoll = FMath::FInterpTo(State.BankRoll, TargetRoll, Dt, F.BankInterpSpeed);

				// Climbing straight up has no heading to face, so it keeps the one it has.
				const FVector Heading = bHasHeading
					? Flat : FlockProcessorPrivate::HeldHeading(Transform, Config.MeshYawOffset);

				Transform.SetRotation(
					FlockProcessorPrivate::MakeFacing(Heading, Config.MeshYawOffset, State.BankRoll));
			}

			// Come down once it has been up long enough, the flock is calm, and either it never wanted to
			// orbit or its orbit is over.
			// A relocator asks at once; an orbiter waits out its circuit and for the flock to settle.
			//
			// Calm is measured by how alarmed the bird still is, not by whether anything is nearby. Keyed on
			// mere presence, a source that came to rest near the flock would keep it un-calm forever and no
			// orbiter would ever ask for a slot.
			const bool bCalm = State.Alert < P.PerkThreshold && Now >= Runtime.ContagionUntil;
			const float Required = State.bPrefersOrbit
				? FMath::Max(F.MinFlightTime, State.OrbitDuration) : F.MinFlightTime;

			const bool bReady = State.bPrefersOrbit
				? (bCalm && State.StateTime >= Required)
				: State.StateTime >= F.MinFlightTime;

			// A voluntary move to the ground needs no slot at all, so it skips the request entirely.
			if (bReady && State.bVoluntaryMove && !State.bTargetPerch
				&& Config.GetClip(EFlockClip::Land).bValid)
			{
				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::Landing);

				Config.StartClip(Anims[*It], Config.GetDescentClip(), Now, Birds[*It].RandomSeed);
				continue;
			}

			if (bReady && !State.bSlotRequested && LocalSubsystem
				&& Config.GetClip(EFlockClip::Land).bValid)
			{
				// The subsystem hands out slots, one at a time on the game thread, so two birds can never be
				// given the same one. It also owns the transition into landing.
				State.bSlotRequested = true;
				LocalSubsystem->RequestSlot(Context.GetEntity(*It), Birds[*It].FlockIndex, Position,
					!State.bPrefersOrbit && !State.bVoluntaryMove);
			}

			// Nowhere to settle for long enough: drop onto where it started rather than circle forever.
			if (State.StateTime >= F.MaxOrbitTime && Config.GetClip(EFlockClip::Land).bValid)
			{
				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::Landing);

				Config.StartClip(Anims[*It], Config.GetDescentClip(), Now, Birds[*It].RandomSeed);
			}
		}
	});
}

UFlockLandingProcessor::UFlockLandingProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UFlockFlightProcessor::StaticClass()->GetFName());
}

void UFlockLandingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockAnimFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();

	// Read for the blocking volumes: a descent can be aimed straight through one.
	EntityQuery.AddSharedRequirement<FFlockRuntimeSharedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FFlockLandingTag>(EMassFragmentPresence::All);
}

void UFlockLandingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Landing);

	const UWorld* World = EntityManager.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	UFlockSubsystem* LocalSubsystem = Subsystem;

	EntityQuery.ForEachEntityChunk(Context, [Now, LocalSubsystem](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		const FFlockRuntimeSharedFragment& Runtime = Context.GetSharedFragment<FFlockRuntimeSharedFragment>();
		const FFlockFlightParams& F = Config.Flight;
		const FFlockPerception& P = Config.Perception;

		const TArrayView<FTransformFragment> Transforms = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FFlockVelocityFragment> Velocities = Context.GetMutableFragmentView<FFlockVelocityFragment>();
		const TConstArrayView<FFlockBirdFragment> Birds = Context.GetFragmentView<FFlockBirdFragment>();
		const TArrayView<FFlockStateFragment> States = Context.GetMutableFragmentView<FFlockStateFragment>();
		const TArrayView<FFlockAnimFragment> Anims = Context.GetMutableFragmentView<FFlockAnimFragment>();

		const float Dt = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			FFlockStateFragment& State = States[*It];
			State.StateTime += Dt;

			FTransform& Transform = Transforms[*It].GetMutableTransform();
			const FVector Position = Transform.GetLocation();

			// Heading for the ground means the original spawn spot, not whatever slot it last held.
			const bool bToGround = State.bVoluntaryMove && !State.bTargetPerch;
			const FVector Home(bToGround ? Birds[*It].GroundLocation : Birds[*It].HomeLocation);

			// Line up above the target first, then drop onto it. Going straight for it makes birds dive
			// through the ground at a shallow angle.
			const bool bOverhead = FVector::DistSquared2D(Position, Home)
				< FMath::Square(F.LandArriveDistance * 2.f);
			const FVector Target = bOverhead ? Home : Home + FVector(0.f, 0.f, F.LandApproachHeight);

			const FVector ToTarget = Target - Position;
			const float Distance = ToTarget.Size();

			const FVector Direction = Distance > KINDA_SMALL_NUMBER ? ToTarget / Distance : FVector::DownVector;

			// Committing to a landing changes both the speed and the direction the bird wants. Taking either
			// instantly is the jarring part, so both are eased from whatever it was already doing.
			const FVector Desired = Direction * F.LandSpeed;
			FVector Velocity = FMath::VInterpTo(FVector(Velocities[*It].Velocity), Desired, Dt,
				F.LandVelocityInterpSpeed);

			// Eased velocity lags where the bird is pointed, which is fine out at the top of a descent and
			// wrong at the bottom of one: the lag carries it past the spot and it sinks through the ground
			// before turning back up. So the easing is faded out over the approach, leaving the last of it
			// tracking the spot exactly.
			const float Converge = FMath::GetMappedRangeValueClamped(
				FVector2f(F.LandArriveDistance, FMath::Max(F.LandArriveDistance + 1.f, F.LandApproachHeight)),
				FVector2f(1.f, 0.f), Distance);

			Velocity = FMath::Lerp(Velocity, Desired, Converge);

			// Arrives on the frame it would otherwise overshoot, so the bird is placed exactly once and there
			// is no final hop onto the spot.
			if (bOverhead && Velocity.Size() * Dt >= Distance)
			{
				Transform.SetLocation(Home);

				FRotator Settled = Transform.GetRotation().Rotator();
				Settled.Pitch = 0.f;
				Settled.Roll = 0.f;
				Transform.SetRotation(Settled.Quaternion());

				Velocities[*It].Velocity = FVector3f::ZeroVector;

				State.Alert = 0.f;
				State.RefractoryTime = P.LandedCooldown;

				State.BankRoll = 0.f;
				State.bJustLanded = false;
				State.bSlotRequested = false;
				State.bVoluntaryMove = false;
				State.bWalking = false;
				State.GlanceRemaining = 0.f;

				const float Roll = FMath::Frac(Birds[*It].RandomSeed * 0.0137f + Now * 0.031f);
				State.RestlessTimer = FMath::Lerp(F.RestlessIntervalMin, F.RestlessIntervalMax, Roll);

				// Staggered from the moment it lands, so a flock that came down together does not then all
				// preen together.
				const FFlockIdleParams& I = Config.Idle;
				State.RestTimer = FMath::Lerp(I.RestIntervalMin, I.RestIntervalMax, FMath::Frac(Roll * 3.19f));
				State.WalkTimer = FMath::Lerp(I.WalkIntervalMin, I.WalkIntervalMax, FMath::Frac(Roll * 5.77f));
				if (LocalSubsystem)
				{
					LocalSubsystem->DispatchLand(Birds[*It].FlockIndex, Home);
					LocalSubsystem->OccupySlot(Birds[*It].HomeSlotIndex, Context.GetEntity(*It));
				}

				UE::Flock::SetBirdState(Context, Context.GetEntity(*It), State, EFlockBirdState::Grounded);

				// The touchdown plays here, on the ground, and hands over to idle when it finishes. It is
				// must-complete like every one-shot, so nothing cuts it short.
				const EFlockClip Touchdown = Config.GetClip(EFlockClip::Land).bValid
					? EFlockClip::Land : EFlockClip::Idle;

				// A flock comes down together, so this is one of the two entries that wants its birds
				// scattered through the clip rather than holding the pose they arrived on.
				Config.StartClip(Anims[*It], Touchdown, Now, Birds[*It].RandomSeed,
					/*bAllowRandomPhase*/ true);
				continue;
			}

			Velocities[*It].Velocity = FVector3f(Velocity);

			// Kept out of a blocker, never steered away from one. A landing is aimed at an authored spot, and
			// a push away from where a bird is going is a bird that hovers instead of arriving. Stopped once
			// it is nearly there, so a blocker clipping the spot cannot eject it every frame forever.
			FVector Landed = Position + Velocity * Dt;
			if (Runtime.HasBlockers() && Distance > F.LandArriveDistance)
			{
				FlockProcessorPrivate::PushOutOfBlockers(Runtime, Landed);
			}

			Transform.SetLocation(Landed);

			// The last of a descent is straight down, which has no heading to take.
			const FVector Flat(Velocity.X, Velocity.Y, 0.f);
			const FVector Heading = Flat.Size() > FlockProcessorPrivate::MinHeadingSpeed
				? Flat : FlockProcessorPrivate::HeldHeading(Transform, Config.MeshYawOffset);

			// Levels out over the descent rather than snapping upright the moment it commits to landing.
			State.BankRoll = FMath::FInterpTo(State.BankRoll, 0.f, Dt, Config.Flight.BankInterpSpeed);

			// Rate limited like every other turn. Set from the heading directly, a bird swung round to face
			// its target the instant it decided to land.
			const float WantedYaw = Heading.Rotation().Yaw + Config.MeshYawOffset;
			const float FacingYaw = FMath::FixedTurn(Transform.GetRotation().Rotator().Yaw, WantedYaw,
				F.TurnRateDegrees * Dt);

			Transform.SetRotation(
				FlockProcessorPrivate::MakeFacingFromYaw(FacingYaw, Config.MeshYawOffset, State.BankRoll));
		}
	});
}

UFlockAnimProcessor::UFlockAnimProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UFlockLandingProcessor::StaticClass()->GetFName());
}

void UFlockAnimProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FFlockAnimFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);

	// Read so a finished one-shot knows whether the state it belongs to is still running, and so picks the
	// bridging loop rather than Fly.
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadOnly);

	// Read so a bird past the interpolation tier stops asking for the frame after the one it is on.
	EntityQuery.AddRequirement<FFlockLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FFlockSpeciesConfigFragment>();
}

void UFlockAnimProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Anim);

	const UWorld* World = EntityManager.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	EntityQuery.ForEachEntityChunk(Context, [Now](FMassExecutionContext& Context)
	{
		const FFlockSpeciesConfigFragment& Config = Context.GetConstSharedFragment<FFlockSpeciesConfigFragment>();
		const TArrayView<FFlockAnimFragment> Anims = Context.GetMutableFragmentView<FFlockAnimFragment>();
		const TConstArrayView<FFlockBirdFragment> Birds = Context.GetFragmentView<FFlockBirdFragment>();
		const TConstArrayView<FFlockStateFragment> States = Context.GetFragmentView<FFlockStateFragment>();
		const TConstArrayView<FFlockLODFragment> LODs = Context.GetFragmentView<FFlockLODFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			FFlockAnimFragment& Anim = Anims[*It];

			const FFlockClipRange* Range = &Config.GetClip(Anim.Clip);
			if (!Range->bValid)
			{
				continue;
			}

			float Elapsed = (Now - Anim.ClipStartTime) * Config.GetClipFrameRate(*Range, Anim.PlayRate);

			// A one-shot that has run out hands over here. Left to itself it holds its last frame for as long
			// as the bird stays in the state, which is a visibly frozen bird part way up a launch or most of
			// the way down a descent.
			if (!Range->bLoop && Elapsed >= Range->NumFrames() - 1.f)
			{
				const EFlockClip Next = Config.GetOneShotSuccessor(Anim.Clip, States[*It].State);
				if (Next != EFlockClip::Count && Next != Anim.Clip && Config.GetClip(Next).bValid)
				{
					Config.StartClip(Anim, Next, Now, Birds[*It].RandomSeed);

					Range = &Config.GetClip(Next);
					Elapsed = (Now - Anim.ClipStartTime) * Config.GetClipFrameRate(*Range, Anim.PlayRate);
				}
			}

			const float NumFrames = static_cast<float>(Range->NumFrames());

			// Frame maps straight onto the baked index. No reference-pose offset: Fmod already keeps Local
			// inside [0, NumFrames), so adding one would play the next clip's first frame every loop.
			const float Local = Range->bLoop
				? FMath::Fmod(FMath::Max(Elapsed, 0.f), NumFrames)
				: FMath::Clamp(Elapsed, 0.f, NumFrames - 1.f);

			Anim.Frame = Range->StartFrame + Local;

			if (Config.bInterpolateFrames && LODs[*It].Tier <= Config.InterpolateMaxTier)
			{
				const float Whole = FMath::FloorToFloat(Local);
				const float Next = Range->bLoop
					? FMath::Fmod(Whole + 1.f, NumFrames)
					: FMath::Min(Whole + 1.f, NumFrames - 1.f);

				Anim.NextFrame = Range->StartFrame + Next;
			}
			else
			{
				// Whole frames on both, which leaves the material a blend weight of zero and lets its branch
				// skip the second set of bone fetches.
				Anim.Frame = FMath::FloorToFloat(Anim.Frame);
				Anim.NextFrame = Anim.Frame;
			}
		}
	});
}

UFlockRenderProcessor::UFlockRenderProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UFlockAnimProcessor::StaticClass()->GetFName());
}

void UFlockRenderProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& InEntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockAnimFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockRenderFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FFlockBirdFragment>(EMassFragmentAccess::ReadOnly);

	// This processor already visits every bird, so it is the cheapest place to read state for debug draw and
	// to total up what the audio bed needs.
	EntityQuery.AddRequirement<FFlockStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSharedRequirement<FFlockRuntimeSharedFragment>(EMassFragmentAccess::ReadWrite);
}

void UFlockRenderProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FLOCK_SCOPE(Render);

	UFlockSubsystem* LocalSubsystem = Subsystem;
	if (!LocalSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [LocalSubsystem](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Transforms = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FFlockAnimFragment> Anims = Context.GetFragmentView<FFlockAnimFragment>();
		const TConstArrayView<FFlockRenderFragment> Renders = Context.GetFragmentView<FFlockRenderFragment>();
		const TConstArrayView<FFlockBirdFragment> Birds = Context.GetFragmentView<FFlockBirdFragment>();
		const TConstArrayView<FFlockStateFragment> States = Context.GetFragmentView<FFlockStateFragment>();
		FFlockRuntimeSharedFragment& Runtime = Context.GetMutableSharedFragment<FFlockRuntimeSharedFragment>();

		// Summed per chunk and committed once at the end: a flock's chunks share one fragment between them.
		int32 NumSeen = 0;
		int32 NumAirborne = 0;
		float AlertSum = 0.f;

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FFlockStateFragment& State = States[*It];

			++NumSeen;
			AlertSum += State.Alert;
			if (State.State != EFlockBirdState::Grounded && State.State != EFlockBirdState::Alert
				&& State.State != EFlockBirdState::Perched)
			{
				++NumAirborne;
			}

			const FFlockRenderFragment& Render = Renders[*It];
			if (Render.InstanceIndex == INDEX_NONE)
			{
				continue;
			}

			const FTransform& Transform = Transforms[*It].GetTransform();
			LocalSubsystem->WriteInstance(Birds[*It].FlockIndex, Render.ComponentIndex, Render.InstanceIndex,
				Transform, Anims[*It].Frame, Anims[*It].NextFrame);

#if UE_ENABLE_DEBUG_DRAWING
			if (FLOCK_DEBUG_PERCEPTION(1))
			{
				LocalSubsystem->QueueDebugBird(Transform.GetLocation(), State.Alert, State.State,
					Anims[*It].Clip, Anims[*It].Frame);
			}
#endif
		}

		LocalSubsystem->AccumulateCounts(Runtime, NumSeen, NumAirborne, AlertSum);
	});
}
