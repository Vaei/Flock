// Copyright (c) Jared Taylor

#include "FlockPoseMatch.h"

#include "AnimToTextureDataAsset.h"
#include "Editor.h"
#include "FlockEditorLog.h"
#include "Animation/AnimSequence.h"
#include "Async/ParallelFor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/FlockSpeciesData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "FlockPoseMatch"

namespace FlockPoseMatchPrivate
{
	/**
	 * Every bone of every baked frame, in component space, laid out frame-major.
	 *
	 * Component space rather than the deltas the textures hold, because a difference between two frames is
	 * the same either way and this needs no unpacking to read back.
	 */
	struct FSampledBake
	{
		TArray<FVector3f> Positions;
		TArray<FQuat4f> Rotations;

		/** Forward difference within an animation. The last frame of one repeats the frame before it. */
		TArray<FVector3f> Velocities;

		int32 NumFrames = 0;
		int32 NumBones = 0;

		int32 At(int32 Frame, int32 Bone) const { return Frame * NumBones + Bone; }
	};

	/** What the bake's frame layout says an animation covers, so the sampling cannot drift from it. */
	static bool Sample(const UAnimToTextureDataAsset& DataAsset, FSampledBake& Out, FText& OutError)
	{
		USkeletalMesh* SkeletalMesh = DataAsset.GetSkeletalMesh();
		const int32 NumBones = SkeletalMesh->GetRefSkeleton().GetRawBoneNum();

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			OutError = LOCTEXT("NoWorld", "There is no editor world to sample the animations in.");
			return false;
		}

		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor)
		{
			OutError = LOCTEXT("NoActor", "A temporary actor to sample the animations on could not be spawned.");
			return false;
		}

		ON_SCOPE_EXIT { Actor->Destroy(); };

		// The same setup the bake uses. A pose sampled any other way is not the pose that was baked.
		USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Actor);
		Component->SetSkeletalMesh(SkeletalMesh);
		Component->SetForcedLOD(1);
		Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Component->SetUpdateAnimationInEditor(true);
		Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Component->RegisterComponent();

		Out.NumBones = NumBones;
		Out.NumFrames = DataAsset.NumFrames;
		Out.Positions.SetNumUninitialized(Out.NumFrames * NumBones);
		Out.Rotations.SetNumUninitialized(Out.NumFrames * NumBones);

		FScopedSlowTask Progress(static_cast<float>(DataAsset.NumFrames),
			LOCTEXT("SamplingPoses", "Sampling poses for the pose match table..."));
		Progress.MakeDialog();

		const float SampleInterval = 1.f / FMath::Max(1.f, DataAsset.SampleRate);

		int32 AnimIndex = 0;
		for (const FAnimToTextureAnimSequenceInfo& Info : DataAsset.AnimSequences)
		{
			if (!Info.bEnabled || !Info.AnimSequence)
			{
				continue;
			}

			if (!DataAsset.Animations.IsValidIndex(AnimIndex))
			{
				OutError = FText::Format(LOCTEXT("MoreSequencesThanBaked",
					"{0} has more enabled sequences than it has baked animations. Re-bake it first."),
					FText::FromString(DataAsset.GetName()));
				return false;
			}

			const FAnimToTextureAnimInfo& Baked = DataAsset.Animations[AnimIndex];
			const int32 NumAnimFrames = Baked.EndFrame - Baked.StartFrame + 1;
			const int32 StartKey = Info.bUseCustomRange ? Info.StartFrame : 0;
			const float StartTime = Info.AnimSequence->GetTimeAtFrame(StartKey);

			Component->SetAnimation(Info.AnimSequence);

			for (int32 Local = 0; Local < NumAnimFrames; ++Local)
			{
				const float DeltaTime = Local * SampleInterval;

				Component->SetPosition(StartTime + DeltaTime);
				Component->TickAnimation(DeltaTime, /*bNeedsValidRootMotion*/ false);
				Component->RefreshBoneTransforms(/*TickFunction*/ nullptr);

				const TArray<FTransform>& Transforms = Component->GetComponentSpaceTransforms();
				if (Transforms.Num() < NumBones)
				{
					OutError = LOCTEXT("TooFewTransforms",
						"The sampled pose has fewer bones than the skeleton. The mesh may still be building.");
					return false;
				}

				const int32 Frame = Baked.StartFrame + Local;
				for (int32 Bone = 0; Bone < NumBones; ++Bone)
				{
					Out.Positions[Out.At(Frame, Bone)] = FVector3f(Transforms[Bone].GetLocation());
					Out.Rotations[Out.At(Frame, Bone)] = FQuat4f(Transforms[Bone].GetRotation());
				}

				Progress.EnterProgressFrame();
			}

			++AnimIndex;
		}

		if (AnimIndex != DataAsset.Animations.Num())
		{
			OutError = FText::Format(LOCTEXT("SequenceCountMismatch",
				"{0} baked {1} animations but {2} of its sequences are enabled. Re-bake it first."),
				FText::FromString(DataAsset.GetName()), DataAsset.Animations.Num(), AnimIndex);
			return false;
		}

		// Forward difference, held inside each animation: a bird is never travelling from the end of one
		// clip into the start of the next.
		Out.Velocities.SetNumZeroed(Out.NumFrames * NumBones);
		for (const FAnimToTextureAnimInfo& Baked : DataAsset.Animations)
		{
			for (int32 Frame = Baked.StartFrame; Frame <= Baked.EndFrame; ++Frame)
			{
				const int32 Next = FMath::Min(Frame + 1, Baked.EndFrame);
				for (int32 Bone = 0; Bone < NumBones; ++Bone)
				{
					Out.Velocities[Out.At(Frame, Bone)] =
						Out.Positions[Out.At(Next, Bone)] - Out.Positions[Out.At(Frame, Bone)];
				}
			}
		}

		return true;
	}

	/**
	 * The typical size of each term over one frame of movement, so the three weights mean what they say
	 * rather than being swamped by whichever term happens to be in the largest units.
	 */
	struct FNormalisers
	{
		double Position = 1.0;
		double Rotation = 1.0;
		double Velocity = 1.0;
	};

	static FNormalisers MeasureNormalisers(const FSampledBake& Bake,
		const TArray<FAnimToTextureAnimInfo>& Animations)
	{
		double PositionSum = 0.0;
		double RotationSum = 0.0;
		double VelocitySum = 0.0;
		int64 Count = 0;

		for (const FAnimToTextureAnimInfo& Anim : Animations)
		{
			for (int32 Frame = Anim.StartFrame; Frame < Anim.EndFrame; ++Frame)
			{
				for (int32 Bone = 0; Bone < Bake.NumBones; ++Bone)
				{
					const int32 A = Bake.At(Frame, Bone);
					const int32 B = Bake.At(Frame + 1, Bone);

					PositionSum += FVector3f::DistSquared(Bake.Positions[A], Bake.Positions[B]);
					RotationSum += 1.f - FMath::Abs(Bake.Rotations[A] | Bake.Rotations[B]);
					VelocitySum += FVector3f::DistSquared(Bake.Velocities[A], Bake.Velocities[B]);
					++Count;
				}
			}
		}

		FNormalisers Out;
		if (Count > 0)
		{
			Out.Position = FMath::Max(UE_DOUBLE_KINDA_SMALL_NUMBER, PositionSum / Count);
			Out.Rotation = FMath::Max(UE_DOUBLE_KINDA_SMALL_NUMBER, RotationSum / Count);
			Out.Velocity = FMath::Max(UE_DOUBLE_KINDA_SMALL_NUMBER, VelocitySum / Count);
		}
		return Out;
	}
}

bool FFlockPoseMatch::CanBuild(const UFlockSpeciesData* Species, FText& OutReason)
{
	if (!Species)
	{
		OutReason = LOCTEXT("NoSpecies", "Assign a species first.");
		return false;
	}

	const UAnimToTextureDataAsset* DataAsset = Species->AnimData.LoadSynchronous();
	if (!DataAsset)
	{
		OutReason = LOCTEXT("NoAnimData", "The species has no animation data asset.");
		return false;
	}

	if (DataAsset->Animations.IsEmpty() || DataAsset->NumFrames <= 0)
	{
		OutReason = FText::Format(LOCTEXT("NotBaked", "{0} has not been baked yet."),
			FText::FromString(DataAsset->GetName()));
		return false;
	}

	if (!DataAsset->GetSkeletalMesh())
	{
		OutReason = FText::Format(LOCTEXT("NoSkeletalMesh", "{0} has no source skeletal mesh."),
			FText::FromString(DataAsset->GetName()));
		return false;
	}

	if (DataAsset->NumFrames > MAX_uint16)
	{
		OutReason = FText::Format(LOCTEXT("TooManyFrames",
			"{0} baked {1} frames. The pose match table holds frame indices as 16 bits, so it tops out at "
			"{2}."), FText::FromString(DataAsset->GetName()), DataAsset->NumFrames, MAX_uint16);
		return false;
	}

	return true;
}

bool FFlockPoseMatch::Build(UFlockSpeciesData* Species, FText& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFlockPoseMatch::Build);

	using namespace FlockPoseMatchPrivate;

	if (!CanBuild(Species, OutError))
	{
		return false;
	}

	const UAnimToTextureDataAsset* DataAsset = Species->AnimData.LoadSynchronous();

	FSampledBake Bake;
	if (!Sample(*DataAsset, Bake, OutError))
	{
		return false;
	}

	const FNormalisers Norm = MeasureNormalisers(Bake, DataAsset->Animations);

	const FFlockBakeRecipe& Recipe = Species->BakeRecipe;
	const double PositionWeight = FMath::Max(0.f, Recipe.PoseMatchPositionWeight) / Norm.Position;
	const double RotationWeight = FMath::Max(0.f, Recipe.PoseMatchRotationWeight) / Norm.Rotation;
	const double VelocityWeight = FMath::Max(0.f, Recipe.PoseMatchVelocityWeight) / Norm.Velocity;

	const int32 NumAnimations = DataAsset->Animations.Num();
	const int32 NumBones = Bake.NumBones;

	TArray<uint16> Table;
	Table.SetNumUninitialized(Bake.NumFrames * NumAnimations);

	FScopedSlowTask Progress(static_cast<float>(Bake.NumFrames),
		LOCTEXT("MatchingPoses", "Matching poses..."));
	Progress.MakeDialog();

	const auto MatchOneFrame = [&](int32 Leaving)
	{
		for (int32 Animation = 0; Animation < NumAnimations; ++Animation)
		{
			const FAnimToTextureAnimInfo& Anim = DataAsset->Animations[Animation];

			int32 Best = Anim.StartFrame;
			double BestCost = TNumericLimits<double>::Max();

			for (int32 Candidate = Anim.StartFrame; Candidate <= Anim.EndFrame; ++Candidate)
			{
				double Cost = 0.0;
				for (int32 Bone = 0; Bone < NumBones; ++Bone)
				{
					const int32 A = Bake.At(Leaving, Bone);
					const int32 B = Bake.At(Candidate, Bone);

					Cost += PositionWeight * FVector3f::DistSquared(Bake.Positions[A], Bake.Positions[B]);
					Cost += RotationWeight
						* (1.f - FMath::Abs(Bake.Rotations[A] | Bake.Rotations[B]));
					Cost += VelocityWeight * FVector3f::DistSquared(Bake.Velocities[A], Bake.Velocities[B]);

					// Nothing later in this candidate can bring the cost back down.
					if (Cost >= BestCost)
					{
						break;
					}
				}

				if (Cost < BestCost)
				{
					BestCost = Cost;
					Best = Candidate;
				}
			}

			Table[Leaving * NumAnimations + Animation] = static_cast<uint16>(Best);
		}
	};

	// Batched only so the progress bar can be advanced from the game thread between batches.
	const int32 BatchSize = FMath::Max(1, Bake.NumFrames / 32);
	for (int32 Base = 0; Base < Bake.NumFrames; Base += BatchSize)
	{
		const int32 Count = FMath::Min(BatchSize, Bake.NumFrames - Base);
		ParallelFor(Count, [&](int32 Index) { MatchOneFrame(Base + Index); });
		Progress.EnterProgressFrame(static_cast<float>(Count));
	}

	Species->Modify();
	Species->PoseMatchTable = MoveTemp(Table);
	Species->PoseMatchNumFrames = Bake.NumFrames;
	Species->PoseMatchNumAnimations = NumAnimations;
	Species->MarkPackageDirty();

	UE_LOG(LogFlockEditor, Log,
		TEXT("Built a pose match table for %s: %d frames x %d animations, %d bones, %d KB."),
		*Species->GetName(), Bake.NumFrames, NumAnimations, NumBones,
		(Bake.NumFrames * NumAnimations * static_cast<int32>(sizeof(uint16))) / 1024);

	return true;
}

#undef LOCTEXT_NAMESPACE
