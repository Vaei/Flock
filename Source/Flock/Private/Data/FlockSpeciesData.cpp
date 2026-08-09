// Copyright (c) Jared Taylor. All Rights Reserved

#include "Data/FlockSpeciesData.h"

#include "AnimToTextureDataAsset.h"
#include "FlockLog.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Mass/FlockFragments.h"

UStaticMesh* UFlockSpeciesData::ResolveMesh() const
{
	return Mesh.LoadSynchronous();
}

void UFlockSpeciesData::BuildRestSlotTable(TStaticArray<FFlockRestSlotRef, NumFlockRestClips>& OutTable) const
{
	OutTable = TStaticArray<FFlockRestSlotRef, NumFlockRestClips>();

	int32 Slot = 0;
	for (int32 Index = 0; Index < RestBreaks.Num(); ++Index)
	{
		const int32 Needed = RestBreaks[Index].bMirrored ? 2 : 1;
		if (Slot + Needed > NumFlockRestClips)
		{
			UE_LOG(LogFlock, Warning,
				TEXT("%s has more rest breaks than the %d playback slots hold. '%s' onwards are dropped."),
				*GetName(), NumFlockRestClips, *RestBreaks[Index].Name.ToString());
			break;
		}

		OutTable[Slot].BreakIndex = Index;
		OutTable[Slot].bMirrored = false;
		++Slot;

		if (RestBreaks[Index].bMirrored)
		{
			OutTable[Slot].BreakIndex = Index;
			OutTable[Slot].bMirrored = true;
			++Slot;
		}
	}
}

bool UFlockSpeciesData::GetClipAudio(EFlockClip Clip, const TArray<TSoftObjectPtr<USoundBase>>*& OutSounds,
	FName& OutTrigger, float& OutDelay) const
{
	if (IsFlockRestClip(Clip))
	{
		TStaticArray<FFlockRestSlotRef, NumFlockRestClips> Table;
		BuildRestSlotTable(Table);

		const int32 Slot = static_cast<int32>(Clip) - FirstFlockRestClip;
		if (!Table[Slot].IsValid())
		{
			return false;
		}

		// Both halves of a mirrored pair share the one entry's audio: a left and a right preen sound alike.
		const FFlockRestBreak& Break = RestBreaks[Table[Slot].BreakIndex];
		OutSounds = &Break.Sounds;
		OutTrigger = Break.AudioTrigger;
		OutDelay = Break.SoundDelay;
		return true;
	}

	if (const FFlockClipMapping* Mapping = Clips.Find(Clip))
	{
		OutSounds = &Mapping->Sounds;
		OutTrigger = Mapping->AudioTrigger;
		OutDelay = Mapping->SoundDelay;
		return true;
	}

	return false;
}

USoundBase* UFlockSpeciesData::PickSound(const TArray<TSoftObjectPtr<USoundBase>>& Sounds)
{
	if (Sounds.IsEmpty())
	{
		return nullptr;
	}

	// Uniform rather than shuffled. A shuffle bag would avoid immediate repeats, but a flock is many birds
	// picking independently, so there is no sequence for one to be a repeat of.
	return Sounds[FMath::RandHelper(Sounds.Num())].LoadSynchronous();
}

bool UFlockSpeciesData::BuildConfigFragment(FFlockSpeciesConfigFragment& OutConfig, int32 SpeciesIndex) const
{
	OutConfig.SpeciesIndex = SpeciesIndex;

	const UAnimToTextureDataAsset* Baked = AnimData.LoadSynchronous();
	if (!Baked)
	{
		UE_LOG(LogFlock, Error, TEXT("%s has no AnimData; birds would render but never animate."), *GetName());
		return false;
	}

	if (Baked->Animations.IsEmpty())
	{
		UE_LOG(LogFlock, Error, TEXT("%s references %s, which has not been baked yet."),
			*GetName(), *Baked->GetName());
		return false;
	}

	OutConfig.SampleRate = Baked->SampleRate > 0.f ? Baked->SampleRate : 30.f;
	OutConfig.Perception = Perception;
	OutConfig.Flight = Flight;
	OutConfig.Idle = Idle;
	OutConfig.MeshYawOffset = MeshYawOffset;
	OutConfig.BoundsRadius = BoundsRadius;

	for (const TPair<EFlockClip, FFlockClipMapping>& Pair : Clips)
	{
		const int32 ClipSlot = static_cast<int32>(Pair.Key);
		if (ClipSlot < 0 || ClipSlot >= NumFlockClips)
		{
			// None is selectable so a Variant can be cleared, which also makes it selectable as a key here.
			UE_LOG(LogFlock, Warning, TEXT("%s maps an animation to None. Ignoring it."), *GetName());
			continue;
		}

		if (!Baked->Animations.IsValidIndex(Pair.Value.AnimationIndex))
		{
			UE_LOG(LogFlock, Warning,
				TEXT("%s maps clip %d to animation %d, but %s only baked %d. Leaving it unmapped."),
				*GetName(), ClipSlot, Pair.Value.AnimationIndex, *Baked->GetName(), Baked->Animations.Num());
			continue;
		}

		const FAnimToTextureAnimInfo& Info = Baked->Animations[Pair.Value.AnimationIndex];

		FFlockClipRange& Range = OutConfig.Clips[ClipSlot];
		Range.StartFrame = Info.StartFrame;
		Range.EndFrame = Info.EndFrame;
		Range.bLoop = Pair.Value.bLoop;
		Range.bRandomStartPhase = Pair.Value.bRandomStartPhase;
		// A non-looping clip interrupted mid-play always snaps, so it is never interruptible whatever the
		// asset says. The flag is only a choice for loops, where cutting at a cycle boundary is fine.
		Range.bMustComplete = Pair.Value.bMustComplete || !Pair.Value.bLoop;
		Range.PlayRate = FMath::Max(0.01f, Pair.Value.PlayRate);
		Range.bValid = Range.NumFrames() > 0;

		// Rest slots are filled from RestBreaks below, so a mapping aimed at one would be overwritten and
		// look like it had been ignored.
		if (IsFlockRestClip(Pair.Key))
		{
			UE_LOG(LogFlock, Warning,
				TEXT("%s maps a rest playback slot directly. Use Rest Breaks; this entry is ignored."),
				*GetName());

			Range = FFlockClipRange();
		}
		else if (!Range.bLoop
			&& (Pair.Key == EFlockClip::Walk
				|| Pair.Key == EFlockClip::TakeOffLoop
				|| Pair.Key == EFlockClip::LandLoop
				|| Pair.Key == EFlockClip::Glide
				|| Pair.Key == EFlockClip::BankLeft
				|| Pair.Key == EFlockClip::BankRight))
		{
			// These are all held for as long as something else takes, so a one-shot would freeze on its last
			// frame rather than fill the time it was mapped to fill.
			UE_LOG(LogFlock, Warning, TEXT("%s maps %s as a one-shot. Looping it instead."),
				*GetName(), *StaticEnum<EFlockClip>()->GetNameStringByValue(static_cast<int64>(Pair.Key)));

			Range.bLoop = true;
			Range.bMustComplete = false;
		}
	}

	// --- rest breaks, expanded into the playback slots ---
	TStaticArray<FFlockRestSlotRef, NumFlockRestClips> RestTable;
	BuildRestSlotTable(RestTable);

	auto FillRestSlot = [&](int32 Slot, int32 AnimationIndex, const FFlockRestBreak& Break, bool bMirror)
	{
		if (!Baked->Animations.IsValidIndex(AnimationIndex))
		{
			UE_LOG(LogFlock, Warning,
				TEXT("%s rest break '%s' points at animation %d, but %s only baked %d."),
				*GetName(), *Break.Name.ToString(), AnimationIndex, *Baked->GetName(),
				Baked->Animations.Num());
			return;
		}

		const FAnimToTextureAnimInfo& Info = Baked->Animations[AnimationIndex];

		FFlockClipRange& Range = OutConfig.Clips[FirstFlockRestClip + Slot];
		Range.StartFrame = Info.StartFrame;
		Range.EndFrame = Info.EndFrame;

		// A break ends and hands the bird back to idle, so it is always a one-shot that runs to completion.
		// Looping one would trap the bird in it forever.
		Range.bLoop = false;
		Range.bMustComplete = true;
		Range.bRandomStartPhase = false;
		Range.Weight = Break.Weight;
		Range.PlayRate = FMath::Max(0.01f, Break.PlayRate);
		Range.bIsVariant = bMirror;
		Range.bValid = Range.NumFrames() > 0;
	};

	for (int32 Slot = 0; Slot < NumFlockRestClips; ++Slot)
	{
		const FFlockRestSlotRef& Ref = RestTable[Slot];
		if (!Ref.IsValid())
		{
			continue;
		}

		const FFlockRestBreak& Break = RestBreaks[Ref.BreakIndex];
		FillRestSlot(Slot, Ref.bMirrored ? Break.MirrorAnimationIndex : Break.AnimationIndex, Break,
			Ref.bMirrored);
	}

	// Link each pair, now that both halves have slots. Only the first half competes for a pick; the mirrored
	// one is reached through it, so the pair spends one weight between them.
	for (int32 Slot = 0; Slot < NumFlockRestClips; ++Slot)
	{
		const FFlockRestSlotRef& Ref = RestTable[Slot];
		if (!Ref.IsValid() || Ref.bMirrored || !RestBreaks[Ref.BreakIndex].bMirrored)
		{
			continue;
		}

		for (int32 Other = Slot + 1; Other < NumFlockRestClips; ++Other)
		{
			if (RestTable[Other].BreakIndex == Ref.BreakIndex && RestTable[Other].bMirrored)
			{
				FFlockClipRange& Primary = OutConfig.Clips[FirstFlockRestClip + Slot];
				Primary.VariantIndex = FirstFlockRestClip + Other;
				Primary.VariantChance = RestBreaks[Ref.BreakIndex].MirrorChance;
				break;
			}
		}
	}

	// Idle is the fallback every bird starts in, so without it there is nothing to play.
	if (!OutConfig.GetClip(EFlockClip::Idle).bValid)
	{
		UE_LOG(LogFlock, Error, TEXT("%s has no valid Idle clip mapping."), *GetName());
		return false;
	}

	return true;
}
