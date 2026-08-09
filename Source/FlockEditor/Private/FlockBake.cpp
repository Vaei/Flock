// Copyright (c) Jared Taylor. All Rights Reserved

#include "FlockBake.h"

#include "AnimToTextureBPLibrary.h"
#include "AnimToTextureDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Data/FlockSpeciesData.h"
#include "FlockBakeSettings.h"
#include "FlockEditorLog.h"
#include "FlockPoseMatch.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/Texture2dFactoryNew.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FlockBake"

namespace FlockBakePrivate
{
	static FString ObjectPathFor(const FString& PackagePath, const FString& AssetName)
	{
		return PackagePath / AssetName + TEXT(".") + AssetName;
	}

	static UTexture2D* CreateTexture(const FString& PackagePath, const FString& AssetName)
	{
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPathFor(PackagePath, AssetName)))
		{
			return Existing;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UTexture2DFactoryNew* Factory = NewObject<UTexture2DFactoryNew>();

		// The bake resizes and reformats these; opening a texture editor per stub is pure noise.
		Factory->bEditAfterNew = false;

		return Cast<UTexture2D>(AssetTools.CreateAsset(AssetName, PackagePath, UTexture2D::StaticClass(), Factory));
	}

	static UAnimToTextureDataAsset* CreateDataAsset(const FString& PackagePath, const FString& AssetName)
	{
		if (UAnimToTextureDataAsset* Existing =
			LoadObject<UAnimToTextureDataAsset>(nullptr, *ObjectPathFor(PackagePath, AssetName)))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(*(PackagePath / AssetName));
		if (!Package)
		{
			return nullptr;
		}

		UAnimToTextureDataAsset* DataAsset = NewObject<UAnimToTextureDataAsset>(
			Package, UAnimToTextureDataAsset::StaticClass(), *AssetName, RF_Public | RF_Standalone);
		if (DataAsset)
		{
			FAssetRegistryModule::AssetCreated(DataAsset);
			Package->MarkPackageDirty();
		}
		return DataAsset;
	}

	static void CollectPackage(UObject* Object, TArray<UPackage*>& OutPackages)
	{
		if (Object)
		{
			OutPackages.AddUnique(Object->GetOutermost());
		}
	}

	/**
	 * Bakes one data asset and pushes its parameters onto the given instances.
	 * The lightmap must be moved off the data asset's UVChannel before AnimationToTexture runs, or it
	 * aborts with "Already used by LightMap".
	 */
	static bool BakeOne(UAnimToTextureDataAsset* DataAsset,
		const TArray<TSoftObjectPtr<UMaterialInstanceConstant>>& MaterialInstances,
		const UFlockBakeSettings& Settings, TArray<UPackage*>& OutTouched, FText& OutError)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FFlockBake::BakeOne);

		UStaticMesh* StaticMesh = DataAsset->StaticMesh.LoadSynchronous();
		if (!StaticMesh)
		{
			OutError = FText::Format(LOCTEXT("NoStaticMesh", "{0} has no StaticMesh assigned."),
				FText::FromString(DataAsset->GetName()));
			return false;
		}

		if (Settings.Recipe.LightmapIndex == DataAsset->UVChannel)
		{
			OutError = FText::Format(LOCTEXT("LightmapCollides",
				"Lightmap index {0} is the same as {1}'s UVChannel. The bake would abort; pick a different one."),
				Settings.Recipe.LightmapIndex, FText::FromString(DataAsset->GetName()));
			return false;
		}

		if (!UAnimToTextureBPLibrary::SetLightMapIndex(StaticMesh, DataAsset->StaticLODIndex,
			Settings.Recipe.LightmapIndex, Settings.Recipe.bGenerateLightmapUVs))
		{
			OutError = FText::Format(LOCTEXT("LightmapFailed",
				"Could not set the lightmap index on {0}. Is LOD {1} valid?"),
				FText::FromString(StaticMesh->GetName()), DataAsset->StaticLODIndex);
			return false;
		}

		bool bBaked;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FFlockBake::AnimationToTexture);
			bBaked = UAnimToTextureBPLibrary::AnimationToTexture(DataAsset);
		}

		if (!bBaked)
		{
			OutError = FText::Format(LOCTEXT("BakeFailed",
				"AnimationToTexture failed for {0}. See the Output Log filtered on LogAnimToTextureEditor."),
				FText::FromString(DataAsset->GetName()));
			return false;
		}

		UE_LOG(LogFlockEditor, Log,
			TEXT("Baked %s: NumFrames=%d NumBones=%d BoneRowsPerFrame=%d Animations=%d"),
			*DataAsset->GetName(), DataAsset->NumFrames, DataAsset->NumBones,
			DataAsset->BoneRowsPerFrame, DataAsset->Animations.Num());

		// The bake happily succeeds with a non-VAT material on the mesh, and the result is a static bird
		// with no error anywhere. Both of these flags are mandatory, so their absence means the material
		// has no MF_BoneAnimation node or was never set up for instancing.
		for (int32 SlotIndex = 0; SlotIndex < StaticMesh->GetStaticMaterials().Num(); ++SlotIndex)
		{
			const UMaterialInterface* Assigned = StaticMesh->GetMaterial(SlotIndex);
			const UMaterial* Base = Assigned ? Assigned->GetMaterial() : nullptr;
			if (!Base)
			{
				UE_LOG(LogFlockEditor, Warning, TEXT("%s slot %d has no material."),
					*StaticMesh->GetName(), SlotIndex);
				continue;
			}

			if (!Base->bUseMaterialAttributes || !Base->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes))
			{
				UE_LOG(LogFlockEditor, Warning,
					TEXT("%s slot %d uses %s, which is not set up for vertex animation ")
					TEXT("(Use Material Attributes=%d, Used With Instanced Static Meshes=%d). ")
					TEXT("The mesh will render but will not animate."),
					*StaticMesh->GetName(), SlotIndex, *Base->GetName(),
					Base->bUseMaterialAttributes ? 1 : 0, Base->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes) ? 1 : 0);
			}
		}

		CollectPackage(DataAsset, OutTouched);
		CollectPackage(StaticMesh, OutTouched);
		CollectPackage(DataAsset->BonePositionTexture.Get(), OutTouched);
		CollectPackage(DataAsset->BoneRotationTexture.Get(), OutTouched);
		CollectPackage(DataAsset->BoneWeightTexture.Get(), OutTouched);
		CollectPackage(DataAsset->VertexPositionTexture.Get(), OutTouched);
		CollectPackage(DataAsset->VertexNormalTexture.Get(), OutTouched);

		TArray<UMaterialInstanceConstant*> Instances;
		for (const TSoftObjectPtr<UMaterialInstanceConstant>& SoftInstance : MaterialInstances)
		{
			if (UMaterialInstanceConstant* Instance = SoftInstance.LoadSynchronous())
			{
				Instances.AddUnique(Instance);
			}
			else
			{
				UE_LOG(LogFlockEditor, Warning, TEXT("Skipping unresolved material instance %s"),
					*SoftInstance.ToString());
			}
		}

		// An instance on the mesh but missing from the list is the silent failure this exists to stop: it
		// keeps the frame count of the bake before this one, so every frame past that count clamps to the
		// last row of the texture. New clips are appended, so it freezes exactly the ones just added.
		for (int32 SlotIndex = 0; SlotIndex < StaticMesh->GetStaticMaterials().Num(); ++SlotIndex)
		{
			UMaterialInstanceConstant* OnMesh = Cast<UMaterialInstanceConstant>(StaticMesh->GetMaterial(SlotIndex));
			if (OnMesh && !Instances.Contains(OnMesh))
			{
				UE_LOG(LogFlockEditor, Log,
					TEXT("%s is on %s slot %d but not in Bone Material Instances. Updating it anyway."),
					*OnMesh->GetName(), *StaticMesh->GetName(), SlotIndex);

				Instances.Add(OnMesh);
			}
		}

		for (UMaterialInstanceConstant* Instance : Instances)
		{

			// The defaults point at the AnimToTexture plugin's Mannequin instances, which are useful to
			// read but live in engine content. Pushing a bird's parameters onto one silently repoints it
			// at that bird's textures, and there is no source control on engine plugin content to undo it.
			if (!Settings.bAllowWritingOutsideProject && !Instance->GetPathName().StartsWith(TEXT("/Game/")))
			{
				UE_LOG(LogFlockEditor, Warning,
					TEXT("Skipping %s: it is outside /Game. Tick Allow Writing Outside Project to include it."),
					*Instance->GetPathName());
				continue;
			}

			UAnimToTextureBPLibrary::UpdateMaterialInstanceFromDataAsset(DataAsset, Instance,
				EMaterialParameterAssociation::GlobalParameter);
			CollectPackage(Instance, OutTouched);
		}

		return true;
	}
}

bool FFlockBake::CanBake(const UFlockBakeSettings& Settings, FText& OutReason)
{
	const bool bWantBone = Settings.Recipe.bBakeBoneAnimation;
	const bool bWantVertex = Settings.Recipe.bBakeVertexAnimation;

	if (!bWantBone && !bWantVertex)
	{
		OutReason = LOCTEXT("NothingSelected", "Enable bone or vertex animation first.");
		return false;
	}

	if (bWantBone && Settings.Recipe.BoneDataAsset.IsNull())
	{
		OutReason = LOCTEXT("NoBoneAsset", "Assign a bone animation data asset.");
		return false;
	}

	if (bWantVertex && Settings.Recipe.VertexDataAsset.IsNull())
	{
		OutReason = LOCTEXT("NoVertexAsset", "Assign a vertex animation data asset.");
		return false;
	}

	OutReason = FText::GetEmpty();
	return true;
}

bool FFlockBake::PrepareAssets(UFlockBakeSettings& Settings, FText& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFlockBake::PrepareAssets);

	USkeletalMesh* SkeletalMesh = Settings.Recipe.SourceSkeletalMesh.LoadSynchronous();
	if (!SkeletalMesh)
	{
		OutError = LOCTEXT("NoSkeletalMesh", "Assign a source skeletal mesh.");
		return false;
	}

	const FString PackagePath = Settings.Recipe.OutputPath.Path;
	if (PackagePath.IsEmpty() || !FPackageName::IsValidLongPackageName(PackagePath / TEXT("Probe")))
	{
		OutError = LOCTEXT("BadOutputPath", "Output path is not a valid content path.");
		return false;
	}

	if (Settings.Recipe.AssetName.IsEmpty())
	{
		OutError = LOCTEXT("NoAssetName", "Give the prepared assets a base name.");
		return false;
	}

	const bool bBoneMode = Settings.Recipe.Mode == EAnimToTextureMode::Bone;
	const FString& Name = Settings.Recipe.AssetName;
	TArray<UPackage*> Touched;

	const FString StaticMeshPackage = PackagePath / FString::Printf(TEXT("SM_%s_VAT"), *Name);

	// ConvertSkeletalMeshToStaticMesh re-copies material slots from the skeletal mesh even when updating an
	// existing mesh, which throws away the VAT material instance every time this is re-run. Snapshot the
	// slots and put them back.
	TArray<FStaticMaterial> PreviousMaterials;
	if (const UStaticMesh* Existing = LoadObject<UStaticMesh>(nullptr,
		*FlockBakePrivate::ObjectPathFor(PackagePath, FString::Printf(TEXT("SM_%s_VAT"), *Name))))
	{
		PreviousMaterials = Existing->GetStaticMaterials();
	}

	// LOD must be explicit: the default -1 fails IsValidLODIndex and returns null.
	UStaticMesh* StaticMesh = UAnimToTextureBPLibrary::ConvertSkeletalMeshToStaticMesh(
		SkeletalMesh, StaticMeshPackage, 0);
	if (!StaticMesh)
	{
		OutError = LOCTEXT("ConvertFailed",
			"ConvertSkeletalMeshToStaticMesh failed. Check the output path and that LOD 0 exists.");
		return false;
	}

	for (int32 SlotIndex = 0; SlotIndex < PreviousMaterials.Num(); ++SlotIndex)
	{
		if (PreviousMaterials[SlotIndex].MaterialInterface && StaticMesh->GetStaticMaterials().IsValidIndex(SlotIndex))
		{
			StaticMesh->SetMaterial(SlotIndex, PreviousMaterials[SlotIndex].MaterialInterface);
		}
	}

	UAnimToTextureDataAsset* DataAsset = FlockBakePrivate::CreateDataAsset(PackagePath,
		FString::Printf(TEXT("DA_%s_%s"), *Name, bBoneMode ? TEXT("BoneAnimation") : TEXT("VertexAnimation")));
	if (!DataAsset)
	{
		OutError = LOCTEXT("DataAssetFailed", "Could not create the data asset.");
		return false;
	}

	DataAsset->SkeletalMesh = SkeletalMesh;
	DataAsset->SkeletalLODIndex = 0;
	DataAsset->StaticMesh = StaticMesh;
	DataAsset->StaticLODIndex = 0;
	DataAsset->UVChannel = 1;
	DataAsset->Mode = Settings.Recipe.Mode;
	DataAsset->Precision = Settings.Recipe.Precision;
	DataAsset->SampleRate = Settings.Recipe.SampleRate;

	// The static mesh is a direct conversion of the skeletal mesh, so every vertex lands exactly on a
	// driver triangle and extra drivers only cost bake time.
	DataAsset->NumDriverTriangles = 1;

	// Flock drives Frame per instance, and AutoPlay derives its frame from absolute engine time so it
	// cannot play a one-shot. This has to live on the data asset rather than the material instance:
	// UpdateMaterialInstanceFromDataAsset pushes bAutoPlay onto the AutoPlay static switch, so setting
	// the switch directly is undone by the next bake.
	DataAsset->bAutoPlay = false;

	if (bBoneMode)
	{
		DataAsset->BonePositionTexture = FlockBakePrivate::CreateTexture(PackagePath,
			FString::Printf(TEXT("TX_%s_BonePosition"), *Name));
		DataAsset->BoneRotationTexture = FlockBakePrivate::CreateTexture(PackagePath,
			FString::Printf(TEXT("TX_%s_BoneRotation"), *Name));
		DataAsset->BoneWeightTexture = FlockBakePrivate::CreateTexture(PackagePath,
			FString::Printf(TEXT("TX_%s_BoneWeight"), *Name));

		if (DataAsset->BonePositionTexture.IsNull() || DataAsset->BoneRotationTexture.IsNull()
			|| DataAsset->BoneWeightTexture.IsNull())
		{
			OutError = LOCTEXT("TextureFailed", "Could not create the bone animation textures.");
			return false;
		}
	}
	else
	{
		DataAsset->VertexPositionTexture = FlockBakePrivate::CreateTexture(PackagePath,
			FString::Printf(TEXT("TX_%s_VertexPosition"), *Name));
		DataAsset->VertexNormalTexture = FlockBakePrivate::CreateTexture(PackagePath,
			FString::Printf(TEXT("TX_%s_VertexNormal"), *Name));

		if (DataAsset->VertexPositionTexture.IsNull() || DataAsset->VertexNormalTexture.IsNull())
		{
			OutError = LOCTEXT("VertexTextureFailed", "Could not create the vertex animation textures.");
			return false;
		}
	}

	// The recipe owns the sequence list, so this replaces whatever the data asset had. A sequence added to the
	// data asset directly would go without a word, and losing one shifts every later animation index the
	// species maps against, so say so instead of replacing.
	{
		TArray<FString> Unlisted;
		for (const FAnimToTextureAnimSequenceInfo& Existing : DataAsset->AnimSequences)
		{
			if (!Existing.bEnabled || !Existing.AnimSequence)
			{
				continue;
			}

			const FSoftObjectPath ExistingPath(Existing.AnimSequence);
			const bool bListed = Settings.Recipe.AnimSequences.ContainsByPredicate(
				[&ExistingPath](const TSoftObjectPtr<UAnimSequence>& Soft)
				{
					return Soft.ToSoftObjectPath() == ExistingPath;
				});

			if (!bListed)
			{
				Unlisted.Add(Existing.AnimSequence->GetName());
			}
		}

		if (!Unlisted.IsEmpty())
		{
			OutError = FText::Format(LOCTEXT("RecipeMissingSequences",
				"{0} has sequences this recipe does not list: {1}. Preparing would drop them, and every "
				"animation index after a dropped one moves. Add them to Anim Sequences in the order they are "
				"already in, or Load the recipe from the species."),
				FText::FromString(DataAsset->GetName()),
				FText::FromString(FString::Join(Unlisted, TEXT(", "))));

			return false;
		}
	}

	DataAsset->AnimSequences.Reset();
	for (const TSoftObjectPtr<UAnimSequence>& SoftSequence : Settings.Recipe.AnimSequences)
	{
		if (UAnimSequence* Sequence = SoftSequence.LoadSynchronous())
		{
			FAnimToTextureAnimSequenceInfo& Info = DataAsset->AnimSequences.AddDefaulted_GetRef();
			Info.bEnabled = true;
			Info.AnimSequence = Sequence;
		}
	}

	if (DataAsset->AnimSequences.IsEmpty())
	{
		UE_LOG(LogFlockEditor, Warning,
			TEXT("%s has no animation sequences. Add some before baking."), *DataAsset->GetName());
	}

	DataAsset->MarkPackageDirty();

	if (bBoneMode)
	{
		Settings.Recipe.BoneDataAsset = DataAsset;
		Settings.Recipe.bBakeBoneAnimation = true;
	}
	else
	{
		Settings.Recipe.VertexDataAsset = DataAsset;
		Settings.Recipe.bBakeVertexAnimation = true;
	}
	Settings.SaveConfig();

	FlockBakePrivate::CollectPackage(DataAsset, Touched);
	FlockBakePrivate::CollectPackage(StaticMesh, Touched);
	FlockBakePrivate::CollectPackage(DataAsset->BonePositionTexture.Get(), Touched);
	FlockBakePrivate::CollectPackage(DataAsset->BoneRotationTexture.Get(), Touched);
	FlockBakePrivate::CollectPackage(DataAsset->BoneWeightTexture.Get(), Touched);
	FlockBakePrivate::CollectPackage(DataAsset->VertexPositionTexture.Get(), Touched);
	FlockBakePrivate::CollectPackage(DataAsset->VertexNormalTexture.Get(), Touched);

	if (Settings.bSaveAfterBake && !Touched.IsEmpty())
	{
		FEditorFileUtils::PromptForCheckoutAndSave(Touched, /*bCheckDirty*/ false, /*bPromptToSave*/ false);
	}

	// The recipe now describes assets that exist, so hand it to the species rather than leaving it in a
	// window that the next bird will overwrite.
	if (Settings.bWriteBackToSpecies)
	{
		Settings.SaveToSpecies();
	}

	UE_LOG(LogFlockEditor, Log, TEXT("Prepared %s in %s"), *DataAsset->GetName(), *PackagePath);
	return true;
}

bool FFlockBake::Bake(const UFlockBakeSettings& Settings, FText& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFlockBake::Bake);

	FText Reason;
	if (!CanBake(Settings, Reason))
	{
		OutError = Reason;
		return false;
	}

	TArray<UPackage*> Touched;

	if (Settings.Recipe.bBakeBoneAnimation)
	{
		UAnimToTextureDataAsset* DataAsset = Settings.Recipe.BoneDataAsset.LoadSynchronous();
		if (!DataAsset)
		{
			OutError = LOCTEXT("BoneAssetMissing", "The bone animation data asset could not be loaded.");
			return false;
		}

		// A null weight texture hits a bare check() inside the bake, and the other two fail silently
		// because AnimationToTexture ignores its own write results.
		if (DataAsset->Mode == EAnimToTextureMode::Bone
			&& (DataAsset->BonePositionTexture.IsNull() || DataAsset->BoneRotationTexture.IsNull()
				|| DataAsset->BoneWeightTexture.IsNull()))
		{
			OutError = FText::Format(LOCTEXT("BoneTexturesMissing",
				"{0} is missing one of its three bone textures. Prepare the asset set first."),
				FText::FromString(DataAsset->GetName()));
			return false;
		}

		if (!FlockBakePrivate::BakeOne(DataAsset, Settings.Recipe.BoneMaterialInstances, Settings, Touched, OutError))
		{
			return false;
		}
	}

	if (Settings.Recipe.bBakeVertexAnimation)
	{
		UAnimToTextureDataAsset* DataAsset = Settings.Recipe.VertexDataAsset.LoadSynchronous();
		if (!DataAsset)
		{
			OutError = LOCTEXT("VertexAssetMissing", "The vertex animation data asset could not be loaded.");
			return false;
		}

		if (DataAsset->Mode == EAnimToTextureMode::Vertex
			&& (DataAsset->VertexPositionTexture.IsNull() || DataAsset->VertexNormalTexture.IsNull()))
		{
			OutError = FText::Format(LOCTEXT("VertexTexturesMissing",
				"{0} is missing one of its vertex textures. Prepare the asset set first."),
				FText::FromString(DataAsset->GetName()));
			return false;
		}

		if (!FlockBakePrivate::BakeOne(DataAsset, Settings.Recipe.VertexMaterialInstances, Settings, Touched, OutError))
		{
			return false;
		}
	}

	// After the textures, because it measures the frame layout the bake just produced. Not fatal on failure:
	// without a table every clip opens on its first frame, which is where it opened before there was one.
	if (Settings.Recipe.bBakeBoneAnimation && Settings.Recipe.bBuildPoseMatchTable)
	{
		if (UFlockSpeciesData* Species = Settings.Species.LoadSynchronous())
		{
			FText PoseError;
			if (FFlockPoseMatch::Build(Species, PoseError))
			{
				Touched.AddUnique(Species->GetPackage());
			}
			else
			{
				UE_LOG(LogFlockEditor, Warning, TEXT("Pose match table not built: %s"), *PoseError.ToString());
			}
		}
	}

	if (Settings.bSaveAfterBake && !Touched.IsEmpty())
	{
		FEditorFileUtils::PromptForCheckoutAndSave(Touched, /*bCheckDirty*/ false, /*bPromptToSave*/ false);
	}

	if (Settings.bWriteBackToSpecies)
	{
		const_cast<UFlockBakeSettings&>(Settings).SaveToSpecies();
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
