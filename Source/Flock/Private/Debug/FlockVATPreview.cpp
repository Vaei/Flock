// Copyright (c) Jared Taylor

#include "Debug/FlockVATPreview.h"

#include "AnimToTextureDataAsset.h"
#include "FlockLog.h"
#include "FlockStats.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "System/FlockRenderPool.h"

AFlockVATPreview::AFlockVATPreview()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(false);
	SetRootComponent(Instances);
}

void AFlockVATPreview::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildInstances();
}

void AFlockVATPreview::RebuildInstances()
{
	FLOCK_SCOPE(PreviewRebuild);

	Instances->ClearInstances();
	Phases.Reset();

	UStaticMesh* Mesh = AnimData ? AnimData->StaticMesh.LoadSynchronous() : nullptr;
	if (!Mesh)
	{
		return;
	}

	Instances->SetStaticMesh(Mesh);

	// The same layout the render pool writes: Frame, PrevFrame, NextFrame. Must agree with the material's
	// AutoPlay switch being off, or the shader reads the wrong slots and freezes on the bind pose.
	Instances->SetNumCustomDataFloats(FlockCustomDataFloats);

	for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
	{
		Instances->SetMaterial(Slot, MaterialOverride);
	}

	const int32 Count = CountX * CountY;
	Phases.Reserve(Count);

	const FRandomStream Stream(0x5EED);
	for (int32 Y = 0; Y < CountY; ++Y)
	{
		for (int32 X = 0; X < CountX; ++X)
		{
			const FVector Location(X * Spacing, Y * Spacing, 0.f);
			Instances->AddInstance(FTransform(Location), /*bWorldSpace*/ false);
			Phases.Add(bRandomPhase ? Stream.FRand() : 0.f);
		}
	}
}

void AFlockVATPreview::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FLOCK_SCOPE(PreviewTick);

	const int32 Count = Instances->GetInstanceCount();
	if (!AnimData || Count == 0)
	{
		return;
	}

	if (!AnimData->Animations.IsValidIndex(AnimationIndex))
	{
		UE_LOG(LogFlock, Warning, TEXT("%s: AnimationIndex %d is out of range, %d clips were baked."),
			*GetName(), AnimationIndex, AnimData->Animations.Num());
		return;
	}

	ElapsedTime += DeltaSeconds;

	const FAnimToTextureAnimInfo& Range = AnimData->Animations[AnimationIndex];
	const float NumClipFrames = static_cast<float>(Range.EndFrame - Range.StartFrame + 1);
	if (NumClipFrames <= 0.f)
	{
		return;
	}

	CustomData.SetNumUninitialized(Count * FlockCustomDataFloats);

	if (HoldFrame >= 0.f && bHoldFrameIsAbsolute)
	{
		const float Absolute = FMath::Clamp(HoldFrame, 0.f, static_cast<float>(AnimData->NumFrames));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			CustomData[Index * FlockCustomDataFloats + 0] = bInterpolate ? Absolute : FMath::FloorToFloat(Absolute);
			CustomData[Index * FlockCustomDataFloats + 1] = CustomData[Index * FlockCustomDataFloats + 0];
			CustomData[Index * FlockCustomDataFloats + 2] = bInterpolate
				? FMath::FloorToFloat(Absolute) + 1.f : CustomData[Index * FlockCustomDataFloats + 0];
		}

		FLOCK_SCOPE(RenderFlush);
		Instances->SetCustomData(0, Count - 1, CustomData, /*bMarkRenderStateDirty*/ false);
		return;
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		float Local;
		if (HoldFrame >= 0.f)
		{
			Local = FMath::Min(HoldFrame, NumClipFrames - 1.f);
		}
		else
		{
			const float Phase = Phases.IsValidIndex(Index) ? Phases[Index] : 0.f;
			const float Elapsed = (ElapsedTime + Phase * NumClipFrames / AnimData->SampleRate)
				* PlayRate * AnimData->SampleRate;

			Local = bLoop ? FMath::Fmod(Elapsed, NumClipFrames)
						  : FMath::Min(Elapsed, NumClipFrames - 1.f);
		}

		// Texture row N is animation frame N. The bake reserves an extra row for the reference pose but the
		// frames are not shifted by it, so no offset belongs here: adding one plays each clip's successor's
		// first frame at the end of every loop.
		const float Frame = Range.StartFrame + (bInterpolate ? Local : FMath::FloorToFloat(Local));

		const float NextLocal = bLoop
			? FMath::Fmod(FMath::FloorToFloat(Local) + 1.f, NumClipFrames)
			: FMath::Min(FMath::FloorToFloat(Local) + 1.f, NumClipFrames - 1.f);

		CustomData[Index * FlockCustomDataFloats + 0] = Frame;
		CustomData[Index * FlockCustomDataFloats + 1] = Frame;
		CustomData[Index * FlockCustomDataFloats + 2] = bInterpolate
			? Range.StartFrame + NextLocal : Frame;
	}

	{
		FLOCK_SCOPE(RenderFlush);

		// Not bMarkRenderStateDirty: SetCustomData already calls CustomDataChanged per instance, which is
		// the incremental GPU update. Marking dirty additionally destroys and recreates the scene proxy
		// every frame, which is both expensive and visible as flicker.
		Instances->SetCustomData(0, Count - 1, CustomData, /*bMarkRenderStateDirty*/ false);
	}

	SET_DWORD_STAT(STAT_Flock_InstancesWritten, Count);
}
