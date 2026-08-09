// Copyright (c) Jared Taylor. All Rights Reserved

#include "System/FlockSubsystem.h"

#include "Actors/FlockRenderActor.h"
#include "Components/FlockDisturbanceComponent.h"
#include "Components/FlockPerchComponent.h"
#include "Data/FlockSpeciesData.h"
#include "Debug/FlockDebug.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Interfaces/FlockDisturbanceInterface.h"
#include "FlockDeveloper.h"
#include "FlockLog.h"
#include "FlockStats.h"
#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Mass/EntityFragments.h"
#include "Mass/FlockFragments.h"
#include "Mass/FlockProcessors.h"
#include "Mass/FlockTagFragments.h"

UFlockSubsystem* UFlockSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject,
		EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UFlockSubsystem>() : nullptr;
}

bool UFlockSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (World && World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	return Super::ShouldCreateSubsystem(Outer);
}

void UFlockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UMassEntitySubsystem* MassSubsystem =
		Cast<UMassEntitySubsystem>(Collection.InitializeDependency<UMassEntitySubsystem>()))
	{
		EntityManager = MassSubsystem->GetMutableEntityManager().AsShared();
	}

	if (!EntityManager.IsValid())
	{
		UE_LOG(LogFlock, Error, TEXT("No Mass entity manager; flocks will not simulate."));
		return;
	}

	const TArray<TSubclassOf<UMassProcessor>> ProcessorClasses = {
		UFlockLODProcessor::StaticClass(),
		UFlockThreatProcessor::StaticClass(),
		UFlockDecisionProcessor::StaticClass(),
		UFlockIdleProcessor::StaticClass(),
		UFlockTakeoffProcessor::StaticClass(),
		UFlockFlightProcessor::StaticClass(),
		UFlockLandingProcessor::StaticClass(),
		UFlockAnimProcessor::StaticClass(),
		UFlockRenderProcessor::StaticClass(),
	};

	Pipeline.InitializeFromClassArray(ProcessorClasses, *this, EntityManager.ToSharedRef());

	// A fixed pool, sized once. Exhaustion drops events rather than growing, so a huge cascade cannot turn
	// into a hundred audio components.
	OneShotPool.Reset();
	if (UWorld* PoolWorld = GetWorld())
	{
		const int32 PoolSize = FMath::Max(0, UFlockDeveloperSettings::Get().OneShotPoolSize);
		for (int32 Index = 0; Index < PoolSize; ++Index)
		{
			UAudioComponent* Component = NewObject<UAudioComponent>(this);
			Component->bAutoActivate = false;
			Component->bAllowSpatialization = true;
			Component->bAutoDestroy = false;
			Component->RegisterComponentWithWorld(PoolWorld);
			OneShotPool.Add(Component);
		}
	}

	ProcessorInstances.Reset();
	for (const TObjectPtr<UMassProcessor>& Processor : Pipeline.GetProcessors())
	{
		ProcessorInstances.Add(Processor);
	}

	bPipelineReady = ProcessorInstances.Num() == ProcessorClasses.Num();
	if (!bPipelineReady)
	{
		// Expected in an editor world, where Mass reports EditorWorld execution flags and every Flock
		// processor asks for Standalone or Client. Only a game world missing them is worth an error.
		const UWorld* World = GetWorld();
		const bool bGameWorld = World && World->IsGameWorld();

		UE_CLOG(bGameWorld, LogFlock, Error, TEXT("Pipeline built %d of %d processors."),
			ProcessorInstances.Num(), ProcessorClasses.Num());

		UE_CLOG(!bGameWorld, LogFlock, Log,
			TEXT("Not simulating in this world; %d of %d processors matched its execution flags."),
			ProcessorInstances.Num(), ProcessorClasses.Num());
	}
}

void UFlockSubsystem::Deinitialize()
{
	for (FFlockRuntime& Flock : Flocks)
	{
		Flock.RenderPool.Reset();
		if (Flock.RenderActor)
		{
			Flock.RenderActor->Destroy();
			Flock.RenderActor = nullptr;
		}
	}
	Flocks.Reset();

	Pipeline.Reset();
	ProcessorInstances.Reset();
	bPipelineReady = false;
	EntityManager.Reset();

	Super::Deinitialize();
}

TStatId UFlockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFlockSubsystem, STATGROUP_Tickables);
}

int32 UFlockSubsystem::GetNumBirds() const
{
	int32 Total = 0;
	for (const FFlockRuntime& Flock : Flocks)
	{
		Total += Flock.bActive ? Flock.NumBirds : 0;
	}
	return Total;
}

void UFlockSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Actors already in the level, then everything spawned later. The sweep is needed because a level's
	// actors exist well before this runs.
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		TryAutoRegister(*It);
	}

	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UFlockSubsystem::TryAutoRegister));
}

void UFlockSubsystem::TryAutoRegister(AActor* Actor)
{
	if (!Actor || Actor->ActorHasTag(TEXT("FlockIgnore")))
	{
		return;
	}

	// An explicit component registers itself, and outranks this.
	if (Actor->FindComponentByClass<UFlockDisturbanceComponent>())
	{
		return;
	}

	const UFlockDeveloperSettings& Settings = UFlockDeveloperSettings::Get();
	for (const TSoftClassPtr<AActor>& SoftClass : Settings.AutoRegisterDisturbanceClasses)
	{
		const UClass* Class = SoftClass.Get();
		if (!Class)
		{
			Class = SoftClass.LoadSynchronous();
		}

		if (Class && Actor->IsA(Class))
		{
			RegisterSource(Actor, Settings.AutoSourceThreatWeight, Settings.AutoSourceRadius);
			return;
		}
	}
}

void UFlockSubsystem::RegisterSource(AActor* Actor, float Weight, float MaxRadius, const UObject* Instigator)
{
	if (!Actor)
	{
		return;
	}

	const bool bExplicit = Instigator != nullptr;

	// Opportunistic prune, so a long-lived world does not accumulate dead entries.
	Sources.RemoveAll([](const FFlockSourceRecord& Record) { return !Record.Actor.IsValid(); });

	for (FFlockSourceRecord& Existing : Sources)
	{
		if (Existing.Actor.Get() == Actor)
		{
			// Component beats class list; never let the class list downgrade a tuned source.
			if (bExplicit || !Existing.bExplicit)
			{
				Existing.Weight = Weight;
				Existing.MaxRadius = MaxRadius;
				Existing.bExplicit |= bExplicit;
			}
			return;
		}
	}

	FFlockSourceRecord& Record = Sources.AddDefaulted_GetRef();
	Record.Actor = Actor;
	Record.Weight = Weight;
	Record.MaxRadius = MaxRadius;
	Record.bExplicit = bExplicit;
	Record.Component = Cast<UFlockDisturbanceComponent>(Instigator);
	Record.bPollsActive = Actor->Implements<UFlockDisturbanceInterface>();

	// The interface may also supply the numbers, when nothing more specific did.
	if (!bExplicit && Record.bPollsActive)
	{
		if (const IFlockDisturbanceInterface* AsInterface = Cast<IFlockDisturbanceInterface>(Actor))
		{
			Record.Weight = AsInterface->GetFlockThreatWeight();
			Record.MaxRadius = AsInterface->GetFlockThreatRadius();
		}
	}
}

void UFlockSubsystem::UnregisterSource(AActor* Actor)
{
	Sources.RemoveAll([Actor](const FFlockSourceRecord& Record)
	{
		return !Record.Actor.IsValid() || Record.Actor.Get() == Actor;
	});
}

void UFlockSubsystem::AddScare(const FVector& Location, float Radius, float Weight, float Duration,
	float Falloff)
{
	if (Radius <= 0.f || Weight <= 0.f)
	{
		return;
	}

	FFlockTransientSource Scare;
	Scare.Position = Location;
	Scare.Radius = Radius;
	Scare.Weight = Weight;
	Scare.ProximityExponent = FMath::Max(0.01f, Falloff);

	// Always lives at least one frame, so a zero duration is a single-frame bang rather than nothing at all.
	Scare.TimeRemaining = FMath::Max(0.f, Duration);

	FScopeLock Lock(&ScareLock);
	TransientSources.Add(Scare);
}

void UFlockSubsystem::SetFlockBounds(int32 FlockIndex, const FVector& Centre, float Radius)
{
	if (Flocks.IsValidIndex(FlockIndex))
	{
		Flocks[FlockIndex].Centre = Centre;
		Flocks[FlockIndex].BoundsRadius = Radius;
	}
}

void UFlockSubsystem::RefreshSources()
{
	FLOCK_SCOPE(RefreshSources);

	ResolvedSources.Reset(Sources.Num());

	const float Dt = FMath::Max(KINDA_SMALL_NUMBER, GetWorld() ? GetWorld()->GetDeltaSeconds() : 1.f / 60.f);

	for (int32 Index = Sources.Num() - 1; Index >= 0; --Index)
	{
		FFlockSourceRecord& Record = Sources[Index];

		AActor* Actor = Record.Actor.Get();
		if (!Actor)
		{
			Sources.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		if (Record.bPollsActive)
		{
			if (const IFlockDisturbanceInterface* AsInterface = Cast<IFlockDisturbanceInterface>(Actor))
			{
				if (!AsInterface->IsFlockThreatActive())
				{
					continue;
				}
			}
		}

		const UFlockDisturbanceComponent* Component = Record.Component.Get();
		if (Component && !Component->IsDisturbanceEnabled())
		{
			continue;
		}

		// The component may nominate a different component to track, because physics on a child leaves the
		// actor's own transform sitting where it spawned.
		const FVector Position = Component ? Component->GetDisturbanceLocation() : Actor->GetActorLocation();

		// Measured, not asked for: AActor::GetVelocity reports the root's velocity, which is zero for exactly
		// the same reason. A delta works for anything that moves, however it moves.
		FVector Velocity = Record.bHasLastPosition
			? (Position - Record.LastPosition) / Dt
			: Actor->GetVelocity();

		Record.LastPosition = Position;
		Record.bHasLastPosition = true;

		FResolvedSource& Resolved = ResolvedSources.AddDefaulted_GetRef();
		Resolved.Position = Position;
		Resolved.Velocity = Velocity;
		Resolved.Weight = Record.Weight;
		Resolved.MaxRadius = Record.MaxRadius;
	}

	// Scares join the same list, so they are scored against real sources for the flock's four threat slots and
	// felt through exactly the same falloff. Ageing happens after they are resolved, so one raised with no
	// duration still reaches the birds for a frame.
	{
		FScopeLock Lock(&ScareLock);

		for (int32 Index = TransientSources.Num() - 1; Index >= 0; --Index)
		{
			const FFlockTransientSource& Scare = TransientSources[Index];

			FResolvedSource& Resolved = ResolvedSources.AddDefaulted_GetRef();
			Resolved.Position = Scare.Position;
			Resolved.Weight = Scare.Weight;
			Resolved.MaxRadius = Scare.Radius;
			Resolved.ProximityExponent = Scare.ProximityExponent;
			Resolved.bIgnoreSpeciesRange = true;

			TransientSources[Index].TimeRemaining -= Dt;
			if (TransientSources[Index].TimeRemaining <= 0.f)
			{
				TransientSources.RemoveAtSwap(Index, EAllowShrinking::No);
			}
		}
	}

	SET_DWORD_STAT(STAT_Flock_NumSources, ResolvedSources.Num());
}

void UFlockSubsystem::RunBroadphase(float DeltaTime)
{
	FLOCK_SCOPE(Broadphase);

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Sources times flocks, both small. The point is not this loop's cost but what it buys: each flock ends
	// up with at most MaxFlockThreats entries, so the per-bird pass is independent of how many sources exist.
	for (FFlockRuntime& Flock : Flocks)
	{
		if (!Flock.bActive || !Flock.RuntimeShared.IsValid())
		{
			continue;
		}

		FFlockRuntimeSharedFragment& Shared = Flock.RuntimeShared.Get<FFlockRuntimeSharedFragment>();

		// Read before the counters are cleared: they hold last frame's totals, tallied by the render pass.
		if (Flock.Species)
		{
			const float Fraction = Flock.Species->Flight.AmbientAirborneFraction;
			const int32 Target = FMath::CeilToInt(Shared.NumSeen * Fraction);

			Shared.bWantsAirborne = Fraction > 0.f && Shared.NumSeen > 0 && Shared.NumAirborne < Target;
		}

		Shared.NumThreats = 0;

		// Cleared here and re-raised by the threat processor, so it lags by a frame rather than latching on.
		Shared.bAnyAlert = false;

		Shared.NumSeen = 0;
		Shared.NumAirborne = 0;
		Shared.AlertSum = 0.f;

		// The attractor sweeps around the flock, and every airborne bird is pulled toward it. That one moving
		// point is what makes a flock wheel, without any bird ever looking at another.
		if (Flock.Species)
		{
			const FFlockFlightParams& F = Flock.Species->Flight;
			Flock.AttractorAngle = FMath::Fmod(
				Flock.AttractorAngle + FMath::DegreesToRadians(F.AttractorSweepDegrees) * DeltaTime,
				UE_TWO_PI);

			Shared.AttractorPosition = FVector3f(FVector(Flock.Centre)
				+ FVector(FMath::Cos(Flock.AttractorAngle) * F.CruiseRadius,
					FMath::Sin(Flock.AttractorAngle) * F.CruiseRadius,
					F.CruiseCeiling));
		}

		// Contagion is time-limited; clearing the strength is what lets the chunk filter skip the flock
		// again once the panic is over.
		if (Now >= Shared.ContagionUntil)
		{
			Shared.ContagionStrength = 0.f;
		}

		// Score is weight over distance to the flock's bounds, so the most pressing sources win the slots.
		float Scores[MaxFlockThreats] = { 0.f };

		for (const FResolvedSource& Source : ResolvedSources)
		{
			const float DistSq = FVector::DistSquared(Source.Position, Flock.Centre);
			const float Reach = Source.MaxRadius + Flock.BoundsRadius;
			if (DistSq > FMath::Square(Reach))
			{
				continue;
			}

			const float Score = Source.Weight / FMath::Max(1.f, DistSq);

			// Insertion sort into a fixed four, evicting the weakest once full.
			int32 Slot = Shared.NumThreats;
			if (Slot >= MaxFlockThreats)
			{
				int32 Weakest = 0;
				for (int32 i = 1; i < MaxFlockThreats; ++i)
				{
					if (Scores[i] < Scores[Weakest])
					{
						Weakest = i;
					}
				}
				if (Score <= Scores[Weakest])
				{
					continue;
				}
				Slot = Weakest;
			}
			else
			{
				++Shared.NumThreats;
			}

			Scores[Slot] = Score;

			FFlockThreat& Threat = Shared.Threats[Slot];
			Threat.Position = FVector3f(Source.Position);
			Threat.Velocity = FVector3f(Source.Velocity);
			Threat.Weight = Source.Weight;
			Threat.MaxRadius = Source.MaxRadius;
			Threat.ProximityExponent = Source.ProximityExponent;
			Threat.bIgnoreSpeciesRange = Source.bIgnoreSpeciesRange;
		}

#if UE_ENABLE_DEBUG_DRAWING
		if (UWorld* DebugWorld = GetWorld())
		{
			if (FLOCK_DEBUG_PERCEPTION(3))
			{
				DrawDebugSphere(DebugWorld, Flock.Centre, Flock.BoundsRadius, 16, FColor::Silver, false, -1.f);

				// The attractor is what airborne birds are steering toward, so seeing it explains the wheeling.
				DrawDebugSphere(DebugWorld, FVector(Shared.AttractorPosition), 60.f, 8, FColor::Cyan, false, -1.f);
			}

			if (FLOCK_DEBUG_PERCEPTION(2))
			{
				for (int32 Index = 0; Index < Shared.NumThreats; ++Index)
				{
					const FFlockThreat& Threat = Shared.Threats[Index];
					const FVector Position(Threat.Position);

					DrawDebugSphere(DebugWorld, Position, Threat.MaxRadius, 16, FColor::Red, false, -1.f);
					DrawDebugString(DebugWorld, Position + FVector(0.f, 0.f, 120.f),
						FString::Printf(TEXT("threat %.2f"), Threat.Weight), nullptr, FColor::Red, 0.f);
				}
			}
		}
#endif
	}
}

void UFlockSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FLOCK_SCOPE(Tick);

	if (!bPipelineReady || !EntityManager.IsValid() || Flocks.IsEmpty())
	{
		return;
	}

	if (!UFlockDeveloperSettings::Get().bEnableFlock)
	{
		return;
	}

	++FrameCounter;

	// Before anything reads a bird. A flock spawned last frame is still standing in rows on the ground.
	for (FFlockRuntime& Flock : Flocks)
	{
		if (Flock.bActive && Flock.bNeedsSeeding)
		{
			SeedFlock(Flock);
		}
	}

	CacheView();

	// A tier change is an archetype move, so deciding them every frame would cost more than the LOD saves.
	const float LODInterval = 1.f / FMath::Max(1.f, UFlockDeveloperSettings::Get().LODRateHz);
	LODAccumulator += DeltaTime;
	bRunLODThisFrame = LODAccumulator >= LODInterval;
	if (bRunLODThisFrame)
	{
		LODAccumulator = 0.f;
	}

	RefreshSources();
	RunBroadphase(DeltaTime);

	{
		UE::Mass::FProcessingContext ProcessingContext(EntityManager.ToSharedRef(), DeltaTime);
		UE::Mass::Executor::Run(Pipeline, ProcessingContext);
	}

	for (FFlockRuntime& Flock : Flocks)
	{
		if (Flock.bActive)
		{
			Flock.RenderPool.Flush();
		}
	}

	ResolveSlotRequests();
	DrainEvents();

	SET_DWORD_STAT(STAT_Flock_NumFlocks, Flocks.Num());
	SET_DWORD_STAT(STAT_Flock_NumBirds, GetNumBirds());
}

int32 UFlockSubsystem::CreateFlock(const FFlockSpawnParams& Params)
{
	if (!EntityManager.IsValid() || !Params.Species)
	{
		UE_LOG(LogFlock, Warning, TEXT("CreateFlock: no species, or Mass is unavailable."));
		return INDEX_NONE;
	}

	// Entities cannot be created while Mass is processing, and there is no deferred path for it.
	if (EntityManager->IsProcessing())
	{
		UE_LOG(LogFlock, Error,
			TEXT("CreateFlock called during Mass processing. Call it from the game thread outside a processor."));
		return INDEX_NONE;
	}

	const UFlockDeveloperSettings& Settings = UFlockDeveloperSettings::Get();

	const int32 Count = FMath::Min(Params.Count, Settings.MaxBirdsTotal - GetNumBirds());
	if (Count <= 0)
	{
		UE_LOG(LogFlock, Warning, TEXT("CreateFlock: refused, world bird cap of %d reached."),
			Settings.MaxBirdsTotal);
		return INDEX_NONE;
	}
	if (Count < Params.Count)
	{
		UE_LOG(LogFlock, Warning, TEXT("CreateFlock: clamped %d birds to %d by the world cap."),
			Params.Count, Count);
	}

	UStaticMesh* Mesh = Params.Species->ResolveMesh();
	if (!Mesh)
	{
		UE_LOG(LogFlock, Error, TEXT("%s has no mesh."), *Params.Species->GetName());
		return INDEX_NONE;
	}

	FFlockSpeciesConfigFragment Config;
	const int32 FlockIndex = Flocks.Num();
	if (!Params.Species->BuildConfigFragment(Config, FlockIndex))
	{
		return INDEX_NONE;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return INDEX_NONE;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	AFlockRenderActor* RenderActor = World->SpawnActor<AFlockRenderActor>(
		AFlockRenderActor::StaticClass(), FTransform(Params.Origin), SpawnParams);
	if (!RenderActor)
	{
		return INDEX_NONE;
	}

	FFlockRuntime& Flock = Flocks.AddDefaulted_GetRef();
	Flock.FlockIndex = FlockIndex;
	Flock.RenderActor = RenderActor;
	Flock.Species = Params.Species;
	Flock.NumBirds = Count;
	Flock.bActive = true;
	Flock.RenderPool.Initialise(RenderActor, Mesh, Count, Settings.MaxInstancesPerComponent);
	Flock.Centre = Params.Origin;
	Flock.BoundsRadius = Params.Extent.Size2D();

	// Fragments and tags go in one list; CreateArchetype takes both.
	const TArray<const UScriptStruct*> Composition = {
		FTransformFragment::StaticStruct(),
		FFlockBirdFragment::StaticStruct(),
		FFlockStateFragment::StaticStruct(),
		FFlockAnimFragment::StaticStruct(),
		FFlockRenderFragment::StaticStruct(),
		FFlockVelocityFragment::StaticStruct(),
		FFlockLODFragment::StaticStruct(),
		FFlockGroundedTag::StaticStruct(),
		FFlockLODNearTag::StaticStruct(),
	};

	const FMassArchetypeHandle Archetype = EntityManager->CreateArchetype(Composition);

	// FlockIndex makes each flock's value unique, so Mass gives every flock its own chunks and the threat
	// list is a per-chunk read rather than a lookup.
	FFlockRuntimeSharedFragment RuntimeShared;
	RuntimeShared.FlockIndex = FlockIndex;
	RuntimeShared.Centre = FVector3f(Params.Origin);
	RuntimeShared.AttractorPosition = FVector3f(Params.Origin + FVector(0.f, 0.f, Config.Flight.CruiseCeiling));
	RuntimeShared.OrbitPreference = Params.OrbitPreference >= 0.f
		? Params.OrbitPreference : Config.Flight.OrbitPreference;
	Flock.RuntimeShared = EntityManager->GetOrCreateSharedFragment(RuntimeShared);

	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(EntityManager->GetOrCreateConstSharedFragment(Config));
	SharedValues.Add(Flock.RuntimeShared);
	SharedValues.Sort();

	// Outside the batch scope, because the summary is logged after it closes.
	int32 NumUngrounded = 0;
	int32 NumCramped = 0;

	TArray<FMassEntityHandle> Entities;
	{
		// The creation context has to outlive the fragment writes below: it keeps the batch open so
		// observers and command flushing happen once, at scope exit.
		const auto CreationContext = EntityManager->BatchCreateEntities(Archetype, SharedValues, Count, Entities);

		const float Now = World->GetTimeSeconds();
		const FFlockClipRange& Idle = Config.GetClip(EFlockClip::Idle);
		// The per-bird play rate is jittered below, so this is only the unjittered length. Close enough for
		// seeding a start phase, which is random by definition.
		const float IdleSeconds = Idle.bValid ? Config.GetClipSeconds(Idle, 1.f) : 0.f;

		FRandomStream Stream(FlockIndex * 7919 + Count);

		// Chosen positions so far, for the spacing test below.
		TArray<FVector> Placed;
		Placed.Reserve(Count);

		for (int32 Index = 0; Index < Entities.Num(); ++Index)
		{
			const FMassEntityHandle Entity = Entities[Index];
			const uint8 Seed = static_cast<uint8>(Stream.RandRange(0, 255));

			FFlockBirdFragment& Bird = EntityManager->GetFragmentDataChecked<FFlockBirdFragment>(Entity);
			Bird.FlockIndex = FlockIndex;
			Bird.SpeciesIndex = 0;
			Bird.RandomSeed = Seed;

			int32 ComponentIndex = INDEX_NONE;
			int32 InstanceIndex = INDEX_NONE;
			Flock.RenderPool.AllocateSlot(ComponentIndex, InstanceIndex);

			FFlockRenderFragment& Render = EntityManager->GetFragmentDataChecked<FFlockRenderFragment>(Entity);
			Render.ComponentIndex = ComponentIndex;
			Render.InstanceIndex = InstanceIndex;

			// Dart throwing: take the first candidate that both stands somewhere a bird could stand and is
			// far enough from everything already placed. Keeps the scatter random rather than gridded,
			// without ever stacking two birds.
			const float SpacingSq = FMath::Square(Params.MinSpawnSpacing);
			const float Span = FMath::Max(Params.Extent.Z, 1.f);

			FVector Location = FVector::ZeroVector;
			float BestSeparation = -1.f;

			// 2 stands clear, 1 has ground under it but no room, 0 has nothing. A cramped spot on real ground
			// still beats hanging in the air, so the tiers are ranked rather than pass/fail.
			int32 BestRank = 0;

			for (int32 Attempt = 0; Attempt < 24; ++Attempt)
			{
				FVector Candidate = Params.Origin + FVector(
					Stream.FRandRange(-Params.Extent.X, Params.Extent.X),
					Stream.FRandRange(-Params.Extent.Y, Params.Extent.Y),
					Params.bSnapToGround ? 0.f : Stream.FRandRange(-Params.Extent.Z, Params.Extent.Z));

				// The trace has to happen before the spacing test, not after: a spot chosen for its spacing and
				// only then found to be a wall face is a bird standing in a wall, and nothing later moves it,
				// because a walking bird holds the height it spawned at and aims from where it spawned.
				int32 Rank = 2;
				if (Params.bSnapToGround)
				{
					Rank = 0;

					FHitResult Hit;
					FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FlockGroundSnap));
					QueryParams.AddIgnoredActor(RenderActor);

					const bool bHit = World->LineTraceSingleByChannel(Hit,
						Candidate + FVector(0.f, 0.f, Span),
						Candidate - FVector(0.f, 0.f, Span * 2.f),
						Params.GroundTraceChannel, QueryParams);

					// A wall, a railing or the underside of something is not a floor. Without this the first hit
					// going down is taken whatever it was, which is how a bird ends up part way up a wall.
					if (bHit && Hit.ImpactNormal.Z >= Params.MinGroundNormalZ)
					{
						Candidate.Z = Hit.ImpactPoint.Z + Params.GroundOffset;
						Rank = 1;

						// Room to stand, so a floor found inside a wall's footprint is rejected. The sphere is
						// lifted clear of the feet: centred at exactly its own radius it rests on the floor it is
						// standing on and reports that as the obstruction, which fails every spot on open ground.
						const float Clearance = Params.HeadroomRadius + 4.f;

						if (Params.HeadroomRadius <= 0.f || !World->OverlapAnyTestByChannel(
							Candidate + FVector(0.f, 0.f, Clearance),
							FQuat::Identity, Params.GroundTraceChannel,
							FCollisionShape::MakeSphere(Params.HeadroomRadius), QueryParams))
						{
							Rank = 2;
						}
					}
				}

				float Nearest = TNumericLimits<float>::Max();
				for (const FVector& Taken : Placed)
				{
					// Flat distance: the ground trace moves birds in Z, and that is not separation.
					Nearest = FMath::Min(Nearest, FVector::DistSquared2D(Candidate, Taken));
				}

				if (Rank > BestRank || (Rank == BestRank && Nearest > BestSeparation))
				{
					BestSeparation = Nearest;
					BestRank = Rank;
					Location = Candidate;
				}

				if (Rank == 2 && Nearest >= SpacingSq)
				{
					break;
				}
			}

			if (Params.bSnapToGround && BestRank == 0)
			{
				// Nothing under any attempt. The volume's own plane is at least where the author put it, unlike
				// the bottom of the box, which is as likely to be underground as not.
				Location.Z = Params.Origin.Z + Params.GroundOffset;
				++NumUngrounded;
			}
			else if (BestRank == 1)
			{
				++NumCramped;
			}

			Placed.Add(Location);

			const float Scale = Stream.FRandRange(Params.Species->ScaleMin, Params.Species->ScaleMax);

			FTransformFragment& Transform = EntityManager->GetFragmentDataChecked<FTransformFragment>(Entity);
			Transform.GetMutableTransform() = FTransform(
				FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f).Quaternion(),
				Location,
				FVector(Scale));

			Bird.HomeLocation = FVector3f(Location);
			Bird.GroundLocation = FVector3f(Location);

			FFlockAnimFragment& Anim = EntityManager->GetFragmentDataChecked<FFlockAnimFragment>(Entity);
			Anim.Clip = EFlockClip::Idle;
			Anim.PlayRate = 1.f + Stream.FRandRange(-Params.Species->PlayRateJitter,
				Params.Species->PlayRateJitter);

			// Seeding the start time backwards enters idle mid-cycle, so a settled flock does not breathe
			// in unison.
			Anim.ClipStartTime = Idle.bRandomStartPhase
				? Now - Stream.FRand() * IdleSeconds
				: Now;
		}
	}

	// Every bird is standing on the ground in its spawn spot right now. Which of them should already be perched
	// or already circling cannot be decided here, because perch components register their slots from their own
	// BeginPlay and nothing orders that against this. The first tick settles it.
	Flock.Entities = MoveTemp(Entities);
	Flock.bNeedsSeeding = true;

	UE_LOG(LogFlock, Log, TEXT("Flock %d: %d birds of %s across %d ISM components."),
		FlockIndex, Count, *Params.Species->GetName(), Flock.RenderPool.GetNumComponents());

	if (NumUngrounded > 0)
	{
		UE_LOG(LogFlock, Warning,
			TEXT("Flock %d: %d bird(s) found no ground at all and were left on the volume's own plane. "
			     "Move the volume so its box covers open ground."), FlockIndex, NumUngrounded);
	}

	if (NumCramped > 0)
	{
		UE_LOG(LogFlock, Log,
			TEXT("Flock %d: %d bird(s) stand on ground with less than Headroom Radius of clear space."),
			FlockIndex, NumCramped);
	}

	return FlockIndex;
}

void UFlockSubsystem::SeedFlock(FFlockRuntime& Flock)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Flock_SeedFlock);

	Flock.bNeedsSeeding = false;

	if (!EntityManager.IsValid() || !Flock.Species || Flock.Entities.IsEmpty())
	{
		return;
	}

	const FFlockFlightParams& F = Flock.Species->Flight;
	const FFlockIdleParams& I = Flock.Species->Idle;
	const FFlockPerception& P = Flock.Species->Perception;

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	const bool bCanFly = Flock.Species->Clips.Contains(EFlockClip::Fly);

	FRandomStream Stream(Flock.FlockIndex * 104729 + Flock.Entities.Num());

	int32 NumPerched = 0;
	int32 NumAirborne = 0;

	for (const FMassEntityHandle Entity : Flock.Entities)
	{
		if (!EntityManager->IsEntityValid(Entity))
		{
			continue;
		}

		FFlockBirdFragment* Bird = EntityManager->GetFragmentDataPtr<FFlockBirdFragment>(Entity);
		FFlockStateFragment* State = EntityManager->GetFragmentDataPtr<FFlockStateFragment>(Entity);
		FFlockAnimFragment* Anim = EntityManager->GetFragmentDataPtr<FFlockAnimFragment>(Entity);
		FTransformFragment* Transform = EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity);
		FFlockVelocityFragment* Velocity = EntityManager->GetFragmentDataPtr<FFlockVelocityFragment>(Entity);

		if (!Bird || !State || !Anim || !Transform)
		{
			continue;
		}

		// Every bird gets its timers scattered, wherever it ends up standing.
		State->RestlessTimer = Stream.FRandRange(F.RestlessIntervalMin, F.RestlessIntervalMax);
		State->RestTimer = Stream.FRandRange(I.RestIntervalMin, I.RestIntervalMax);
		State->WalkTimer = Stream.FRandRange(I.WalkIntervalMin, I.WalkIntervalMax);
		State->GlanceTimer = Stream.FRandRange(P.GlanceIntervalMin, P.GlanceIntervalMax);

		const float Roll = Stream.FRand();

		// --- already sitting on a perch ---
		if (Roll < F.InitialPerchedFraction)
		{
			const FVector From(Bird->GroundLocation);

			int32 Best = INDEX_NONE;
			float BestDistSq = FMath::Square(F.LandSearchRadius);

			for (int32 Index = 0; Index < Slots.Num(); ++Index)
			{
				if (Slots[Index].State != EFlockSlotState::Free || !Slots[Index].bPerch)
				{
					continue;
				}

				const float DistSq = FVector::DistSquared(Slots[Index].Position, From);
				if (DistSq < BestDistSq)
				{
					Best = Index;
					BestDistSq = DistSq;
				}
			}

			if (Best != INDEX_NONE)
			{
				Slots[Best].State = EFlockSlotState::Occupied;
				Slots[Best].ReservedBy = Entity;

				Bird->HomeSlotIndex = Best;
				Bird->HomeLocation = FVector3f(Slots[Best].Position);

				FTransform& Placed = Transform->GetMutableTransform();
				Placed.SetLocation(Slots[Best].Position);
				Placed.SetRotation(Slots[Best].Rotation);

				// Perched counts as grounded, so this needs no tag change and no archetype move.
				State->State = EFlockBirdState::Perched;
				++NumPerched;
				continue;
			}

			// Nowhere to sit. It stays on the ground, which is what it would have done anyway.
		}
		else if (bCanFly && Roll < F.InitialPerchedFraction + F.InitialAirborneFraction)
		{
			// --- already part way round a circuit ---
			const float Angle = Stream.FRandRange(0.f, UE_TWO_PI);
			const float Reach = F.CruiseRadius * Stream.FRandRange(0.7f, 1.15f);

			const FVector Position = Flock.Centre
				+ FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach,
					F.CruiseCeiling * Stream.FRandRange(0.8f, 1.2f));

			// Along the circle rather than across it, so the first frame of flight is a continuation of a lap
			// instead of a bird correcting hard toward the attractor.
			const FVector Tangent(-FMath::Sin(Angle), FMath::Cos(Angle), 0.f);

			FTransform& Placed = Transform->GetMutableTransform();
			Placed.SetLocation(Position);

			FRotator Facing = Tangent.Rotation();
			Facing.Yaw += Flock.Species->MeshYawOffset;
			Placed.SetRotation(Facing.Quaternion());

			if (Velocity)
			{
				Velocity->Velocity = FVector3f(Tangent * F.CruiseSpeed);
			}

			State->State = EFlockBirdState::Flying;
			State->bVoluntaryMove = true;
			State->bPrefersOrbit = true;
			State->bTargetPerch = Stream.FRand() < F.PerchPreference;
			State->OrbitDuration = Stream.FRandRange(F.OrbitTimeMin, F.OrbitTimeMax);

			// Part way through, so the first one comes down within seconds rather than after a full lap.
			State->StateTime = Stream.FRandRange(0.f, State->OrbitDuration);

			Anim->Clip = EFlockClip::Fly;
			Anim->ClipStartTime = Now - Stream.FRand();

			// Last, and nothing may touch those fragment pointers after it: a tag change moves the entity to
			// another archetype there and then, and every one of them dangles.
			EntityManager->RemoveTagFromEntity(Entity, FFlockGroundedTag::StaticStruct());
			EntityManager->AddTagToEntity(Entity, FFlockFlyingTag::StaticStruct());

			++NumAirborne;
			continue;
		}
	}

	UE_LOG(LogFlock, Log, TEXT("Flock %d seeded: %d perched, %d airborne, %d on the ground."),
		Flock.FlockIndex, NumPerched, NumAirborne, Flock.Entities.Num() - NumPerched - NumAirborne);
}

void UFlockSubsystem::DestroyFlock(int32 FlockIndex)
{
	if (!Flocks.IsValidIndex(FlockIndex) || !Flocks[FlockIndex].bActive)
	{
		return;
	}

	FFlockRuntime& Flock = Flocks[FlockIndex];
	Flock.RenderPool.Reset();

	// Owned by the render actor, so destroying it takes it with it; nulled so nothing stale is reused.
	Flock.Bed = nullptr;

	if (Flock.RenderActor)
	{
		Flock.RenderActor->Destroy();
		Flock.RenderActor = nullptr;
	}

	// Left in place rather than removed, so every other flock's index stays valid.
	Flock.Entities.Reset();
	Flock.bNeedsSeeding = false;
	Flock.bActive = false;
	Flock.NumBirds = 0;
}

bool UFlockSubsystem::AllocateRenderSlot(int32 FlockIndex, int32& OutComponentIndex, int32& OutInstanceIndex)
{
	if (!Flocks.IsValidIndex(FlockIndex))
	{
		return false;
	}
	return Flocks[FlockIndex].RenderPool.AllocateSlot(OutComponentIndex, OutInstanceIndex);
}

void UFlockSubsystem::WriteInstance(int32 FlockIndex, int32 ComponentIndex, int32 InstanceIndex,
	const FTransform& WorldTransform, float Frame)
{
	if (!Flocks.IsValidIndex(FlockIndex) || !Flocks[FlockIndex].bActive)
	{
		return;
	}

	Flocks[FlockIndex].RenderPool.WriteInstance(ComponentIndex, InstanceIndex, WorldTransform, Frame);
}

void UFlockSubsystem::DispatchTakeOff(int32 FlockIndex, const FVector& Position)
{
	FScopeLock Lock(&EventLock);
	PendingTakeOff.Add({ FlockIndex, Position });
}

void UFlockSubsystem::DispatchLand(int32 FlockIndex, const FVector& Position)
{
	FScopeLock Lock(&EventLock);
	PendingLand.Add({ FlockIndex, Position });
}

void UFlockSubsystem::DispatchClipStarted(int32 FlockIndex, EFlockClip Clip, const FVector& Position)
{
	FScopeLock Lock(&EventLock);
	PendingClipStarts.Add({ FlockIndex, Clip, Position });
}

void UFlockSubsystem::FireClipAudio(int32 FlockIndex, EFlockClip Clip, const FVector& Position)
{
	if (!Flocks.IsValidIndex(FlockIndex) || !Flocks[FlockIndex].bActive)
	{
		return;
	}

	FFlockRuntime& Flock = Flocks[FlockIndex];

	const TArray<TSoftObjectPtr<USoundBase>>* Sounds = nullptr;
	FName Trigger;
	float Delay = 0.f;

	if (!Flock.Species || !Flock.Species->GetClipAudio(Clip, Sounds, Trigger, Delay))
	{
		return;
	}

	if (Flock.Bed && !Trigger.IsNone())
	{
		Flock.Bed->SetTriggerParameter(Trigger);
	}

	if (UFlockDeveloperSettings::Get().bEnableOneShotAudio && Sounds)
	{
		PlayOneShot(UFlockSpeciesData::PickSound(*Sounds), Position);
	}
}

void UFlockSubsystem::PlayOneShot(USoundBase* Sound, const FVector& Position)
{
	if (!Sound || OneShotPool.IsEmpty())
	{
		return;
	}

	// Round robin, and a busy component is simply skipped. A late caw is worse than a missing one, so the
	// pool is a hard cap rather than a queue.
	for (int32 Attempt = 0; Attempt < OneShotPool.Num(); ++Attempt)
	{
		UAudioComponent* Component = OneShotPool[NextOneShot];
		NextOneShot = (NextOneShot + 1) % OneShotPool.Num();

		if (Component && !Component->IsPlaying())
		{
			Component->SetWorldLocation(Position);
			Component->SetSound(Sound);
			Component->Play();
			return;
		}
	}
}

void UFlockSubsystem::DrainEvents()
{
	FLOCK_SCOPE(DrainEvents);
	check(IsInGameThread());

	TArray<FPresentationEvent> TakeOffs;
	TArray<FPresentationEvent> Lands;
	TArray<FClipEvent> ClipStarts;
	{
		// Moved out under the lock, then handled unlocked: playing a sound can re-enter this subsystem.
		FScopeLock Lock(&EventLock);
		TakeOffs = MoveTemp(PendingTakeOff);
		Lands = MoveTemp(PendingLand);
		ClipStarts = MoveTemp(PendingClipStarts);
		PendingTakeOff.Reset();
		PendingLand.Reset();
		PendingClipStarts.Reset();
	}

	const UWorld* World = GetWorld();
	const UFlockDeveloperSettings& Settings = UFlockDeveloperSettings::Get();

	const float Now = World ? World->GetTimeSeconds() : 0.f;

	for (FFlockRuntime& Flock : Flocks)
	{
		if (!Flock.bActive || !Flock.Species || !Flock.RenderActor)
		{
			continue;
		}

		const bool bWantOneShots = Settings.bEnableOneShotAudio;

		// --- bursts, one pooled spawn per event ---
		// Not an array data interface: these are one-shots that need their own lifetime, and driving that from
		// an array forces the spawn count through a loop duration rather than the frame. A cascade is a
		// handful of spawns spread over its window, which costs nothing.
		auto SpawnBursts = [World](const TSoftObjectPtr<UNiagaraSystem>& SystemAsset,
			const TArray<FPresentationEvent>& Events, int32 ForFlock)
		{
			if (!World || Events.IsEmpty() || SystemAsset.IsNull())
			{
				return;
			}

			UNiagaraSystem* System = SystemAsset.LoadSynchronous();
			if (!System)
			{
				return;
			}

			for (const FPresentationEvent& Event : Events)
			{
				if (Event.FlockIndex != ForFlock)
				{
					continue;
				}

				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Event.Position,
					FRotator::ZeroRotator, FVector::OneVector, /*bAutoDestroy*/ true,
					/*bAutoActivate*/ true, ENCPoolMethod::AutoRelease);
			}
		};

		SpawnBursts(Flock.Species->TakeOffVFX, TakeOffs, Flock.FlockIndex);
		SpawnBursts(Flock.Species->LandVFX, Lands, Flock.FlockIndex);

		// --- the flock's continuous bed ---
		if (USoundBase* BedSound = Flock.Species->FlockBed.LoadSynchronous())
		{
			if (!Flock.Bed)
			{
				Flock.Bed = NewObject<UAudioComponent>(Flock.RenderActor);
				Flock.Bed->bAutoActivate = false;
				Flock.Bed->bAllowSpatialization = true;
				Flock.Bed->SetSound(BedSound);
				Flock.Bed->SetupAttachment(Flock.RenderActor->GetRootComponent());
				Flock.Bed->RegisterComponent();
				Flock.Bed->Play();
			}

			const FFlockRuntimeSharedFragment& Shared =
				Flock.RuntimeShared.Get<FFlockRuntimeSharedFragment>();

			const int32 Seen = FMath::Max(1, Shared.NumSeen);

			// The MetaSound owns its own falloff; it is handed a distance, not a volume.
			float Distance = 0.f;
			if (World && World->GetFirstPlayerController())
			{
				FVector ListenerLocation;
				FRotator ListenerRotation;
				World->GetFirstPlayerController()->GetPlayerViewPoint(ListenerLocation, ListenerRotation);
				Distance = FVector::Dist(ListenerLocation, Flock.Centre);
			}

			Flock.Bed->SetWorldLocation(Flock.Centre);
			Flock.Bed->SetFloatParameter(TEXT("Distance"), Distance);
			Flock.Bed->SetFloatParameter(TEXT("BirdCount"), static_cast<float>(Shared.NumSeen));
			Flock.Bed->SetFloatParameter(TEXT("Alert"), Shared.AlertSum / Seen);
			Flock.Bed->SetFloatParameter(TEXT("AirborneRatio"),
				static_cast<float>(Shared.NumAirborne) / Seen);
		}

		// --- per-bird one-shots, and the cascade triggers on the bed ---
		for (const FPresentationEvent& Event : TakeOffs)
		{
			if (Event.FlockIndex != Flock.FlockIndex)
			{
				continue;
			}

			if (Flock.Bed)
			{
				Flock.Bed->SetTriggerParameter(TEXT("TakeOff"));
			}
			if (bWantOneShots)
			{
				PlayOneShot(UFlockSpeciesData::PickSound(Flock.Species->TakeOffOneShots), Event.Position);
			}
		}

		for (const FPresentationEvent& Event : Lands)
		{
			if (Event.FlockIndex != Flock.FlockIndex)
			{
				continue;
			}

			if (Flock.Bed)
			{
				Flock.Bed->SetTriggerParameter(TEXT("Land"));
			}
			if (bWantOneShots)
			{
				PlayOneShot(UFlockSpeciesData::PickSound(Flock.Species->LandOneShots), Event.Position);
			}
		}
	}

	// --- clip starts ---
	// After the flock loop, because that is where a bed gets created, and the first caw of a level would
	// otherwise have nothing to trigger.
	//
	// The delegate fires when the clip does; only the audio waits. A caw's sound belongs a few frames in, once
	// the beak is open, and that offset is per clip.
	for (const FClipEvent& Event : ClipStarts)
	{
		const FFlockRuntime* Flock = Flocks.IsValidIndex(Event.FlockIndex) ? &Flocks[Event.FlockIndex] : nullptr;

		const TArray<TSoftObjectPtr<USoundBase>>* Sounds = nullptr;
		FName Trigger;
		float Delay = 0.f;
		const bool bHasAudio = Flock && Flock->Species
			&& Flock->Species->GetClipAudio(Event.Clip, Sounds, Trigger, Delay);

		if (bHasAudio && Delay > 0.f)
		{
			ScheduledClipAudio.Add({ Now + Delay, Event.Position, Event.FlockIndex, Event.Clip });
		}
		else
		{
			FireClipAudio(Event.FlockIndex, Event.Clip, Event.Position);
		}

		OnClipStarted.Broadcast(Event.FlockIndex, Event.Clip, Event.Position);
	}

	for (int32 Index = ScheduledClipAudio.Num() - 1; Index >= 0; --Index)
	{
		if (Now >= ScheduledClipAudio[Index].FireTime)
		{
			const FScheduledClipAudio Scheduled = ScheduledClipAudio[Index];
			ScheduledClipAudio.RemoveAtSwap(Index, EAllowShrinking::No);

			FireClipAudio(Scheduled.FlockIndex, Scheduled.Clip, Scheduled.Position);
		}
	}
}

void UFlockSubsystem::RegisterPerchSlots(const UFlockPerchComponent* Perch)
{
	if (!Perch)
	{
		return;
	}

	const FTransform ToWorld = Perch->GetComponentTransform();

	// Baked in component space, so this is the only transform they ever need.
	for (const FFlockAuthoredSlot& Authored : Perch->BakedSlots)
	{
		FFlockSlot& Slot = Slots.AddDefaulted_GetRef();
		Slot.Position = ToWorld.TransformPosition(Authored.LocalPosition);
		Slot.Rotation = ToWorld.TransformRotation(Authored.LocalRotation.Quaternion());
		Slot.bPerch = Authored.bPerch;
		Slot.State = EFlockSlotState::Free;
	}

	UE_LOG(LogFlock, Log, TEXT("Registered %d slots from %s; %d in the world."),
		Perch->BakedSlots.Num(), *Perch->GetReadableName(), Slots.Num());
}

void UFlockSubsystem::RequestSlot(FMassEntityHandle Entity, int32 FlockIndex, const FVector& From,
	bool bAvoidThreats)
{
	FScopeLock Lock(&SlotLock);
	PendingSlotRequests.Add({ Entity, FlockIndex, From, bAvoidThreats });
}

void UFlockSubsystem::OccupySlot(int32 SlotIndex, FMassEntityHandle Entity)
{
	if (Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].ReservedBy == Entity)
	{
		Slots[SlotIndex].State = EFlockSlotState::Occupied;
	}
}

void UFlockSubsystem::ReleaseSlot(int32 SlotIndex, FMassEntityHandle Entity)
{
	if (Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].ReservedBy == Entity)
	{
		Slots[SlotIndex].State = EFlockSlotState::Free;
		Slots[SlotIndex].ReservedBy = FMassEntityHandle();
	}
}

void UFlockSubsystem::ResolveSlotRequests()
{
	FLOCK_SCOPE(SlotRequests);

	if (!EntityManager.IsValid())
	{
		return;
	}

#if UE_ENABLE_DEBUG_DRAWING
	if (FLOCK_DEBUG_SLOTS(1))
	{
		if (UWorld* DebugWorld = GetWorld())
		{
			for (const FFlockSlot& Slot : Slots)
			{
				const FColor Colour = Slot.State == EFlockSlotState::Free ? FColor::Green
					: (Slot.State == EFlockSlotState::Reserved ? FColor::Yellow : FColor::Red);

				DrawDebugSphere(DebugWorld, Slot.Position, 18.f, 8, Colour, false, -1.f);
				DrawDebugDirectionalArrow(DebugWorld, Slot.Position,
					Slot.Position + Slot.Rotation.GetForwardVector() * 60.f, 12.f, Colour, false, -1.f);
			}

			DrawDebugString(DebugWorld, ViewLocation + ViewForward * 200.f,
				FString::Printf(TEXT("Flock slots: %d"), Slots.Num()), nullptr, FColor::White, 0.f);
		}
	}
#endif

	TArray<FSlotRequest> Requests;
	{
		FScopeLock Lock(&SlotLock);
		Requests = MoveTemp(PendingSlotRequests);
		PendingSlotRequests.Reset();
	}

	// A slot whose holder no longer exists is free again, however it died.
	for (FFlockSlot& Slot : Slots)
	{
		if (Slot.State != EFlockSlotState::Free && !EntityManager->IsEntityValid(Slot.ReservedBy))
		{
			Slot.State = EFlockSlotState::Free;
			Slot.ReservedBy = FMassEntityHandle();
		}
	}

	if (Requests.IsEmpty() || Slots.IsEmpty())
	{
		return;
	}

	for (const FSlotRequest& Request : Requests)
	{
		if (!EntityManager->IsEntityValid(Request.Entity))
		{
			continue;
		}

		FFlockBirdFragment* Bird = EntityManager->GetFragmentDataPtr<FFlockBirdFragment>(Request.Entity);
		FFlockStateFragment* State = EntityManager->GetFragmentDataPtr<FFlockStateFragment>(Request.Entity);
		if (!Bird || !State)
		{
			continue;
		}

		const FFlockRuntime* Flock = Flocks.IsValidIndex(Request.FlockIndex)
			? &Flocks[Request.FlockIndex] : nullptr;

		const UFlockSpeciesData* Species = Flock ? Flock->Species.Get() : nullptr;
		const float SearchRadius = Species ? Species->Flight.LandSearchRadius : 3000.f;
		const float SafeDistance = Species ? Species->Flight.SafeRelocateDistance : 800.f;

		// Where the threats are, so a relocating bird does not pick somewhere just as bad.
		const FFlockRuntimeSharedFragment* Shared = Flock && Flock->RuntimeShared.IsValid()
			? &Flock->RuntimeShared.Get<FFlockRuntimeSharedFragment>() : nullptr;

		int32 Best = INDEX_NONE;
		float BestDistSq = TNumericLimits<float>::Max();

		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const FFlockSlot& Slot = Slots[Index];
			if (Slot.State != EFlockSlotState::Free)
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(Slot.Position, Request.From);
			if (DistSq > FMath::Square(SearchRadius) || DistSq >= BestDistSq)
			{
				continue;
			}

			// A relocator wants somewhere away from what spooked it, and on the far side of it.
			if (Request.bAvoidThreats && Shared)
			{
				bool bTooClose = false;
				for (int32 T = 0; T < Shared->NumThreats; ++T)
				{
					const FVector ThreatPos(Shared->Threats[T].Position);
					if (FVector::DistSquared(Slot.Position, ThreatPos) < FMath::Square(SafeDistance))
					{
						bTooClose = true;
						break;
					}
				}
				if (bTooClose)
				{
					continue;
				}
			}

			Best = Index;
			BestDistSq = DistSq;
		}

		if (Best == INDEX_NONE)
		{
			// Nothing suitable. The bird keeps flying and asks again, which is why the request is a one-shot
			// flag rather than a permanent state.
			State->bSlotRequested = false;
			continue;
		}

		Slots[Best].State = EFlockSlotState::Reserved;
		Slots[Best].ReservedBy = Request.Entity;

		Bird->HomeSlotIndex = Best;
		Bird->HomeLocation = FVector3f(Slots[Best].Position);

		// Outside processing, so the tag can be set directly rather than deferred.
		State->State = EFlockBirdState::Landing;
		State->StateTime = 0.f;
		State->bSlotRequested = false;

		// This is the normal way into a landing, so it owns starting the clip: the flight processor only
		// does that on the two paths that come down without a slot.
		if (FFlockAnimFragment* Anim = EntityManager->GetFragmentDataPtr<FFlockAnimFragment>(Request.Entity))
		{
			const FFlockSpeciesConfigFragment* Config =
				EntityManager->GetConstSharedFragmentDataPtr<FFlockSpeciesConfigFragment>(Request.Entity);

			if (Config && Config->GetClip(EFlockClip::Land).bValid)
			{
				const UWorld* ClipWorld = GetWorld();

				Anim->Clip = EFlockClip::Land;
				Anim->ClipStartTime = ClipWorld ? ClipWorld->GetTimeSeconds() : 0.f;
			}
		}

		EntityManager->RemoveTagFromEntity(Request.Entity, FFlockFlyingTag::StaticStruct());
		EntityManager->AddTagToEntity(Request.Entity, FFlockLandingTag::StaticStruct());
	}
}

void UFlockSubsystem::CacheView()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	FVector Location;
	FRotator Rotation;
	PC->GetPlayerViewPoint(Location, Rotation);

	ViewLocation = Location;
	ViewForward = Rotation.Vector();

	// Read from the camera manager rather than assumed, so an FOV change moves the LOD boundaries with it.
	if (const APlayerCameraManager* Camera = PC->PlayerCameraManager)
	{
		TanHalfFOV = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.f, Camera->GetFOVAngle()) * 0.5f));
	}
}
