// Copyright (c) Jared Taylor

#include "System/FlockRenderPool.h"

#include "Actors/FlockRenderActor.h"
#include "FlockLog.h"
#include "FlockStats.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

void FFlockRenderPool::Initialise(AFlockRenderActor* Host, UStaticMesh* Mesh, int32 Capacity,
	int32 MaxPerComponent)
{
	Reset();

	if (!Host || !Mesh || Capacity <= 0)
	{
		return;
	}

	HostActor = Host;
	HostInverse = Host->GetActorTransform().Inverse();
	MaxInstancesPerComponent = FMath::Max(1, MaxPerComponent);

	const int32 NumComponents = FMath::DivideAndRoundUp(Capacity, MaxInstancesPerComponent);
	Components.Reserve(NumComponents);

	int32 Remaining = Capacity;
	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
	{
		const int32 Count = FMath::Min(Remaining, MaxInstancesPerComponent);
		Remaining -= Count;

		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Host);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetMobility(EComponentMobility::Movable);

		// Frame mode: [0] Frame, [1] PrevFrame, [2] NextFrame. The first two are AnimToTexture's own layout
		// and must agree with the material's AutoPlay switch being off; the third is ours, and a material
		// that does not interpolate simply never reads it.
		// This reallocates and zeroes, so it has to happen before any instance is added.
		Component->SetNumCustomDataFloats(FlockCustomDataFloats);

		Component->SetupAttachment(Host->GetRootComponent());
		Component->RegisterComponent();

		FFlockRenderBatch& Batch = Components.AddDefaulted_GetRef();
		Batch.Component = Component;
		Batch.Transforms.SetNum(Count);
		Batch.CustomData.SetNumZeroed(Count * FlockCustomDataFloats);

		// Instances exist up front so their indices are stable; unclaimed ones are parked at zero scale.
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Batch.Transforms[Index] = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
			Component->AddInstance(Batch.Transforms[Index], /*bWorldSpace*/ false);
		}
	}

	UE_LOG(LogFlock, Verbose, TEXT("Render pool: %d components for %d instances."),
		Components.Num(), Capacity);
}

void FFlockRenderPool::Reset()
{
	for (FFlockRenderBatch& Batch : Components)
	{
		if (Batch.Component)
		{
			Batch.Component->DestroyComponent();
		}
	}

	Components.Reset();
	HostActor.Reset();
	HostInverse = FTransform::Identity;
	NumAllocated = 0;
}

bool FFlockRenderPool::AllocateSlot(int32& OutComponentIndex, int32& OutInstanceIndex)
{
	if (Components.IsEmpty())
	{
		return false;
	}

	const int32 ComponentIndex = NumAllocated / MaxInstancesPerComponent;
	const int32 InstanceIndex = NumAllocated % MaxInstancesPerComponent;

	if (!Components.IsValidIndex(ComponentIndex)
		|| !Components[ComponentIndex].Transforms.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	OutComponentIndex = ComponentIndex;
	OutInstanceIndex = InstanceIndex;
	++NumAllocated;
	return true;
}

void FFlockRenderPool::WriteInstance(int32 ComponentIndex, int32 InstanceIndex,
	const FTransform& WorldTransform, float Frame, float NextFrame)
{
	if (!Components.IsValidIndex(ComponentIndex))
	{
		return;
	}

	FFlockRenderBatch& Batch = Components[ComponentIndex];
	if (!Batch.Transforms.IsValidIndex(InstanceIndex))
	{
		return;
	}

	Batch.Transforms[InstanceIndex] = WorldTransform * HostInverse;
	Batch.CustomData[InstanceIndex * FlockCustomDataFloats + 0] = Frame;
	Batch.CustomData[InstanceIndex * FlockCustomDataFloats + 1] = Frame;
	Batch.CustomData[InstanceIndex * FlockCustomDataFloats + 2] = NextFrame;
}

void FFlockRenderPool::Flush()
{
	FLOCK_SCOPE(RenderFlush);

	int32 Written = 0;

	for (FFlockRenderBatch& Batch : Components)
	{
		if (!Batch.Component || Batch.Transforms.IsEmpty())
		{
			continue;
		}

		// bTeleport: no TAA and no motion blur here, so there is nothing for prev-transform tracking to
		// serve. Not bMarkRenderStateDirty on either call: both already flag the instances they touch, and
		// marking dirty destroys and recreates the scene proxy every frame.
		Batch.Component->BatchUpdateInstancesTransforms(0, Batch.Transforms,
			/*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ false, /*bTeleport*/ true);

		Batch.Component->SetCustomData(0, Batch.Transforms.Num() - 1, Batch.CustomData,
			/*bMarkRenderStateDirty*/ false);

		Written += Batch.Transforms.Num();
	}

	INC_DWORD_STAT_BY(STAT_Flock_InstancesWritten, Written);
}
