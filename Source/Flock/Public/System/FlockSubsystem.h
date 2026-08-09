// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "MassSubsystemBase.h"
#include "System/FlockRenderPool.h"
#include "FlockSubsystem.generated.h"

class AFlockRenderActor;
class UAudioComponent;
class UFlockDisturbanceComponent;
class UFlockPerchComponent;
class UFlockSpeciesData;
class UMassProcessor;

/**
 * A bird began a clip worth reacting to. Game thread, broadcast from the subsystem's own tick.
 *
 * Raised for the rest breaks, so a caw or a preen can carry sound, a notify, or anything else. Takeoff and
 * landing have their own dedicated audio slots on the species and do not come through here.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFlockClipStartedSignature, int32, FlockIndex,
	EFlockClip, Clip, FVector, Position);

USTRUCT(BlueprintType)
struct FLOCK_API FFlockSpawnParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	TObjectPtr<UFlockSpeciesData> Species = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="1"))
	int32 Count = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	FVector Origin = FVector::ZeroVector;

	/** Birds are scattered across this box around Origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	FVector Extent = FVector(1000.f, 1000.f, 0.f);

	/** Chance a spooked bird orbits rather than resettling. Negative takes the species' value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float OrbitPreference = -1.f;

	/**
	 * Trace down through the extent to find the ground for each bird, once at spawn. Without it birds sit on
	 * the plane through Origin, which for a volume is its box centre rather than the floor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	bool bSnapToGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	/**
	 * Nearest another bird may spawn, measured on the ground plane. Placement is dart-thrown against this
	 * rather than gridded, so birds are scattered but never stacked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float MinSpawnSpacing = 80.f;

	/** Kept off the ground by this much, so feet are not buried in it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float GroundOffset = 0.f;

	/**
	 * Steepest surface a bird will spawn on, as the upward part of its normal. 1 is flat, 0 accepts a
	 * vertical wall. Below this the spot is rejected and another is tried.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock",
		meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bSnapToGround"))
	float MinGroundNormalZ = 0.7f;

	/** Clear space a bird needs above its feet. Zero skips the check. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock",
		meta=(ClampMin="0.0", ForceUnits="cm", EditCondition="bSnapToGround"))
	float HeadroomRadius = 20.f;
};

/** A registered alarming actor, flattened so the broadphase touches no UObjects. */
USTRUCT()
struct FFlockSourceRecord
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Actor;

	/** Set for an explicit source, and asked where to look, since that may not be the actor's origin. */
	TWeakObjectPtr<const UFlockDisturbanceComponent> Component;

	float Weight = 1.f;
	float MaxRadius = 1200.f;

	/**
	 * Last known position, so velocity can be measured rather than asked for. AActor::GetVelocity returns the
	 * root's velocity, which is zero whenever physics simulates on a child.
	 */
	FVector LastPosition = FVector::ZeroVector;
	bool bHasLastPosition = false;

	/** Set when the actor came from an explicit component, which outranks the class list. */
	bool bExplicit = false;

	/** Only true when the actor implements the interface, so most sources cost no virtual call. */
	bool bPollsActive = false;
};

/** One place a bird can settle, in world space, resolved from a perch component at BeginPlay. */
USTRUCT()
struct FFlockSlot
{
	GENERATED_BODY()

	FVector Position = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;

	EFlockSlotState State = EFlockSlotState::Free;

	/** Who holds it. Checked for validity so a despawned bird's slot frees itself. */
	FMassEntityHandle ReservedBy;

	bool bPerch = true;
};

/** One flock's live state. */
USTRUCT()
struct FFlockRuntime
{
	GENERATED_BODY()

	int32 FlockIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<AFlockRenderActor> RenderActor = nullptr;

	UPROPERTY()
	TObjectPtr<UFlockSpeciesData> Species = nullptr;

	FFlockRenderPool RenderPool;

	/** Continuous bed for the whole flock. Bursts are pooled one-shots and need no component here. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> Bed = nullptr;

	/** Handle onto the flock's mutable shared fragment, so the broadphase can write its threat list. */
	FSharedStruct RuntimeShared;

	/** Centre and radius the broadphase tests sources against. */
	FVector Centre = FVector::ZeroVector;
	float BoundsRadius = 0.f;

	/** Where the attractor currently is around the flock, in radians. */
	float AttractorAngle = 0.f;

	/** Kept so the flock can be fast forwarded into its starting states once the level has finished loading. */
	TArray<FMassEntityHandle> Entities;

	bool bNeedsSeeding = false;

	int32 NumBirds = 0;
	bool bActive = false;
};

/**
 * Owns the flocks, and executes the Mass processors.
 *
 * Executing them ourselves is not a preference. In 5.8 MassEntity is engine-core, but the only runtime class
 * that hosts an FMassProcessingPhaseManager is UMassSimulationSubsystem in the MassGameplay plugin, which
 * this project does not enable. A processor left at the default bAutoRegisterWithProcessingPhases would
 * therefore never run, and that looks exactly like a query matching nothing. So every Flock processor turns
 * auto-registration off and we drive an FMassRuntimePipeline from Tick.
 *
 * It also buys the thing LOD needs later: separate pipelines per tier, run at different rates.
 */
UCLASS()
class FLOCK_API UFlockSubsystem : public UMassTickableSubsystemBase
{
	GENERATED_BODY()

public:
	static UFlockSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Never on a dedicated server. Birds are cosmetic and simulate per client, so the processors would all be
	 * filtered out by their execution flags anyway; without this the subsystem still exists around them,
	 * holding an audio pool, a spawn handler on every actor, and event queues that nothing ever drains.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Spawns a flock. Returns INDEX_NONE if the species is unusable or the world bird cap is reached. */
	int32 CreateFlock(const FFlockSpawnParams& Params);

	void DestroyFlock(int32 FlockIndex);

	/** Called by the render processor, once per bird per tick. */
	void WriteInstance(int32 FlockIndex, int32 ComponentIndex, int32 InstanceIndex,
		const FTransform& WorldTransform, float Frame);

	// --- Presentation ----------------------------------------------------------------------------------

	/**
	 * Queued from a processor and drained on the game thread in Tick.
	 *
	 * A processor may not spawn an actor, touch a component or broadcast a delegate: it is not guaranteed to
	 * be on the game thread, and doing any of those from a worker crashes. So everything one-shot goes
	 * through a locked queue instead.
	 */
	void DispatchTakeOff(int32 FlockIndex, const FVector& Position);
	void DispatchLand(int32 FlockIndex, const FVector& Position);

	/** A bird started a clip that something might want to hear or react to. */
	void DispatchClipStarted(int32 FlockIndex, EFlockClip Clip, const FVector& Position);

	/**
	 * Fires when a bird begins a rest break, on the game thread, at the moment the clip starts.
	 *
	 * The species' own per-clip Sound and Audio Trigger cover the ordinary case without binding anything;
	 * this is for everything else.
	 */
	UPROPERTY(BlueprintAssignable, Category="Flock")
	FFlockClipStartedSignature OnClipStarted;

	// --- Perch slots -----------------------------------------------------------------------------------

	/** Adds a perch component's baked slots, transformed into world space. */
	void RegisterPerchSlots(const UFlockPerchComponent* Perch);

	/**
	 * Asks for somewhere to land. Resolved on the game thread later this frame, not here: one writer, no
	 * contention, and two birds can never be handed the same slot.
	 */
	void RequestSlot(FMassEntityHandle Entity, int32 FlockIndex, const FVector& From, bool bAvoidThreats);

	/** Marks a reserved slot as taken. */
	void OccupySlot(int32 SlotIndex, FMassEntityHandle Entity);

	/** Hands a slot back, on takeoff or death. */
	void ReleaseSlot(int32 SlotIndex, FMassEntityHandle Entity);

	int32 GetNumSlots() const { return Slots.Num(); }

	// --- View, cached once per tick for LOD -------------------------------------------------------------

	const FVector& GetViewLocation() const { return ViewLocation; }
	const FVector& GetViewForward() const { return ViewForward; }
	float GetTanHalfFOV() const { return TanHalfFOV; }

	/** True on the frames the LOD processor should do its work. Tiers are re-decided at a few hertz. */
	bool ShouldRunLODThisFrame() const { return bRunLODThisFrame; }

	/** Counts ticks, so a processor can stagger per-bird work across frames. */
	uint32 GetFrameCounter() const { return FrameCounter; }

	/** Claims a render slot for a new bird. */
	bool AllocateRenderSlot(int32 FlockIndex, int32& OutComponentIndex, int32& OutInstanceIndex);

	int32 GetNumFlocks() const { return Flocks.Num(); }
	int32 GetNumBirds() const;

	// --- Disturbance sources ---------------------------------------------------------------------------

	/** Registers an actor as alarming. Explicit registration outranks the auto class list. */
	void RegisterSource(AActor* Actor, float Weight, float MaxRadius, const UObject* Instigator = nullptr);

	void UnregisterSource(AActor* Actor);

	/**
	 * Alarms everything within Radius for a moment, with no actor and nothing to unregister.
	 *
	 * It goes in as an ordinary source for a fraction of a second, so it is scored, ranked and felt through
	 * exactly the same path a walking player is. Birds nearest the middle break first and the ones at the edge
	 * only look up, which is what stops a bang reading as a switch being thrown.
	 */
	void AddScare(const FVector& Location, float Radius, float Weight, float Duration, float Falloff);

	int32 GetNumSources() const { return Sources.Num(); }

	/** Bounds the flock's broadphase test. Called by the volume that owns it. */
	void SetFlockBounds(int32 FlockIndex, const FVector& Centre, float Radius);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	FMassEntityManager* GetEntityManager() const { return EntityManager.Get(); }

private:
	TSharedPtr<FMassEntityManager> EntityManager;

	/** Indexed by FlockIndex; entries are left in place when destroyed so live indices stay valid. */
	UPROPERTY()
	TArray<FFlockRuntime> Flocks;

	/**
	 * UPROPERTY is load bearing. The pipeline's Processors array is reflected, but that only keeps them
	 * alive if this struct instance is itself reachable; without it they are collected and the next Run
	 * crashes assigning a stale processor to a weak pointer.
	 */
	UPROPERTY()
	FMassRuntimePipeline Pipeline;

	/**
	 * The same processors again, reflected directly. Belt and braces: their lifetime should not depend on
	 * FMassRuntimePipeline keeping its array reflected. If they are ever collected, the next Run calls
	 * IsActive() on freed memory, survives, then crashes assigning it to a weak pointer - a stack that
	 * points at the Mass debug code rather than anything of ours.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMassProcessor>> ProcessorInstances;

	bool bPipelineReady = false;

	/** Registered alarming actors. Weak, so a despawn self-cleans on the next refresh. */
	TArray<FFlockSourceRecord> Sources;

	/** This frame's live sources, resolved once so the broadphase never re-reads an actor. */
	struct FResolvedSource
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Weight = 1.f;
		float MaxRadius = 0.f;

		/** Both zero and false for an ordinary source, which leaves the species in charge of both. */
		float ProximityExponent = 0.f;
		bool bIgnoreSpeciesRange = false;
	};
	TArray<FResolvedSource> ResolvedSources;

	/** A scare: a source with a lifetime, belonging to no actor. */
	struct FFlockTransientSource
	{
		FVector Position = FVector::ZeroVector;
		float Radius = 0.f;
		float Weight = 1.f;
		float ProximityExponent = 1.f;
		float TimeRemaining = 0.f;
	};

	FCriticalSection ScareLock;
	TArray<FFlockTransientSource> TransientSources;

	FDelegateHandle ActorSpawnedHandle;

	/** Prunes dead sources and snapshots the live ones into ResolvedSources. */
	void RefreshSources();

	/** Fills each flock's threat list from ResolvedSources. */
	void RunBroadphase(float DeltaTime);

	/** Registers Actor if it matches the configured auto-register classes. */
	void TryAutoRegister(AActor* Actor);

	/**
	 * Fast forwards a newly spawned flock into the states it would have reached on its own: a share of it
	 * already perched, a share already circling, the rest on the ground with their timers staggered.
	 *
	 * Deferred to the first tick rather than done in CreateFlock, because perch components register their
	 * slots from their own BeginPlay and the order actors get theirs in is not defined.
	 */
	void SeedFlock(FFlockRuntime& Flock);

	struct FPresentationEvent
	{
		int32 FlockIndex = INDEX_NONE;
		FVector Position = FVector::ZeroVector;
	};

	struct FClipEvent
	{
		int32 FlockIndex = INDEX_NONE;
		EFlockClip Clip = EFlockClip::Idle;
		FVector Position = FVector::ZeroVector;
	};

	FCriticalSection EventLock;
	TArray<FPresentationEvent> PendingTakeOff;
	TArray<FPresentationEvent> PendingLand;
	TArray<FClipEvent> PendingClipStarts;

	/**
	 * A clip's sound waiting out its Sound Delay. Holds the clip rather than the loaded sound, so nothing
	 * here has to be kept alive against the collector. Game thread only, so it needs no lock.
	 */
	struct FScheduledClipAudio
	{
		float FireTime = 0.f;
		FVector Position = FVector::ZeroVector;
		int32 FlockIndex = INDEX_NONE;
		EFlockClip Clip = EFlockClip::Idle;
	};

	TArray<FScheduledClipAudio> ScheduledClipAudio;

	/** Plays a clip's sound and fires its bed trigger, now. */
	void FireClipAudio(int32 FlockIndex, EFlockClip Clip, const FVector& Position);

	/** Fires this frame's queued VFX and audio. Game thread only. */
	void DrainEvents();

	TArray<FFlockSlot> Slots;

	struct FSlotRequest
	{
		FMassEntityHandle Entity;
		int32 FlockIndex = INDEX_NONE;
		FVector From = FVector::ZeroVector;
		bool bAvoidThreats = false;
	};

	FCriticalSection SlotLock;
	TArray<FSlotRequest> PendingSlotRequests;

	/** Hands out slots and sends the birds that got one into landing. Single writer, game thread. */
	void ResolveSlotRequests();

	/** Read once per tick, so no processor goes looking for a camera. */
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	float TanHalfFOV = 0.5f;

	void CacheView();

	/** Counts up to the LOD interval, since tiers are re-decided at a few hertz rather than every frame. */
	float LODAccumulator = 0.f;
	bool bRunLODThisFrame = false;
	uint32 FrameCounter = 0;

	/** Round-robin pool for one-shots spatialised to a single bird. Exhaustion drops the event. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> OneShotPool;

	int32 NextOneShot = 0;

	void PlayOneShot(USoundBase* Sound, const FVector& Position);
};

/**
 * Lets a processor hold the subsystem without being pinned to the game thread. Every write goes through a
 * critical section, and nothing is touched that is not safe off-thread.
 */
template<>
struct TMassExternalSubsystemTraits<UFlockSubsystem> final
{
	enum { GameThreadOnly = false, ThreadSafeWrite = true };
};
