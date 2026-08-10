// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "FlockRenderPool.generated.h"

class AFlockRenderActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Per-instance custom data floats: [0] Frame, [1] PrevFrame, [2] NextFrame.
 *
 * The first two are AnimToTexture's own layout, matched exactly because the shipped material reads them by
 * position. The third is the frame after the one being played, which a material that interpolates blends
 * towards and one that does not never reads.
 */
inline constexpr int32 FlockCustomDataFloats = 3;

/** One ISM component and the data staged for it this frame. */
USTRUCT()
struct FFlockRenderBatch
{
	GENERATED_BODY()

	/** Reflected so the component survives GC on its own rather than relying on the host actor. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Component = nullptr;

	TArray<FTransform> Transforms;

	/** Flat runs of FlockCustomDataFloats per instance. */
	TArray<float> CustomData;
};

/**
 * One flock's instanced mesh components, plus the scratch buffers the render processor fills.
 *
 * Per flock rather than one global pool: an ISM is a single primitive, so a world-spanning one would never
 * frustum cull, and with occlusion queries off frustum culling is the only culling left.
 */
USTRUCT()
struct FLOCK_API FFlockRenderPool
{
	GENERATED_BODY()

	/** Creates enough components on Host to hold Capacity instances of Mesh. */
	void Initialise(AFlockRenderActor* Host, UStaticMesh* Mesh, int32 Capacity, int32 MaxPerComponent);

	/** Frees the components. The host actor is the subsystem's to destroy. */
	void Reset();

	/**
	 * Claims the next free slot. Returns false once full.
	 * Slots are handed out in order and never reshuffled, so an entity's indices stay valid for its life.
	 */
	bool AllocateSlot(int32& OutComponentIndex, int32& OutInstanceIndex);

	/** Stages one instance. Transform is world space; it is converted against the host on the way in. */
	void WriteInstance(int32 ComponentIndex, int32 InstanceIndex, const FTransform& WorldTransform, float Frame,
		float NextFrame);

	/** Pushes every component's staged data to the renderer. Two calls per component, no more. */
	void Flush();

	int32 GetNumComponents() const { return Components.Num(); }
	int32 GetNumAllocated() const { return NumAllocated; }

private:
	UPROPERTY()
	TArray<FFlockRenderBatch> Components;

	TWeakObjectPtr<AFlockRenderActor> HostActor;
	FTransform HostInverse = FTransform::Identity;

	int32 MaxInstancesPerComponent = 256;
	int32 NumAllocated = 0;
};
