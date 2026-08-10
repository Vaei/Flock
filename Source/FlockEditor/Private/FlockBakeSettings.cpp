// Copyright (c) Jared Taylor

#include "FlockBakeSettings.h"

#include "Data/FlockSpeciesData.h"
#include "FlockEditorLog.h"
#include "FileHelpers.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/Package.h"

namespace FlockBakeDefaults
{
	static const TCHAR* BoneDataAsset = TEXT("/AnimToTexture/Characters/Mannequin/Data/DA_BoneAnimation.DA_BoneAnimation");
	static const TCHAR* VertexDataAsset = TEXT("/AnimToTexture/Characters/Mannequin/Data/DA_VertexAnimation.DA_VertexAnimation");
	static const TCHAR* BoneMaterialInstance = TEXT("/AnimToTexture/Characters/Mannequin/Materials/BoneAnimation/MI_Body_BoneAnimation.MI_Body_BoneAnimation");
	static const TCHAR* VertexMaterialInstance = TEXT("/AnimToTexture/Characters/Mannequin/Materials/VertexAnimation/MI_Body_VertexAnimation.MI_Body_VertexAnimation");
}

UFlockBakeSettings::UFlockBakeSettings()
{
	// The AnimToTexture plugin's own reference assets. Baking those first proves the tool before any project
	// content is involved, which is why they are the starting point rather than empty slots.
	Recipe.AssetName = TEXT("Crow");
	Recipe.OutputPath.Path = TEXT("/Game/Characters/Creatures/Crow/VAT");
	Recipe.BoneDataAsset = TSoftObjectPtr<UAnimToTextureDataAsset>(
		FSoftObjectPath(FlockBakeDefaults::BoneDataAsset));
	Recipe.VertexDataAsset = TSoftObjectPtr<UAnimToTextureDataAsset>(
		FSoftObjectPath(FlockBakeDefaults::VertexDataAsset));

	Recipe.BoneMaterialInstances.Add(TSoftObjectPtr<UMaterialInstanceConstant>(
		FSoftObjectPath(FlockBakeDefaults::BoneMaterialInstance)));
	Recipe.VertexMaterialInstances.Add(TSoftObjectPtr<UMaterialInstanceConstant>(
		FSoftObjectPath(FlockBakeDefaults::VertexMaterialInstance)));
}

bool UFlockBakeSettings::LoadFromSpecies()
{
#if WITH_EDITORONLY_DATA
	const UFlockSpeciesData* Asset = Species.LoadSynchronous();
	if (!Asset)
	{
		return false;
	}

	// An untouched species has an empty recipe. Overwriting a working window with that loses the setup for
	// nothing, so an empty one is left alone and the current recipe simply becomes its starting point.
	if (!Asset->BakeRecipe.SourceSkeletalMesh.IsNull() || Asset->BakeRecipe.IsUsable())
	{
		Recipe = Asset->BakeRecipe;
		OnRecipeReplaced.Broadcast();
	}

	SaveConfig();
	return true;
#else
	return false;
#endif
}

bool UFlockBakeSettings::SaveToSpecies()
{
#if WITH_EDITORONLY_DATA
	UFlockSpeciesData* Asset = Species.LoadSynchronous();
	if (!Asset)
	{
		return false;
	}

	Asset->Modify();
	Asset->BakeRecipe = Recipe;

	// Point the species at what the bake produced, which is the whole reason the two live together.
	if (Recipe.bBakeBoneAnimation && !Recipe.BoneDataAsset.IsNull())
	{
		Asset->AnimData = Recipe.BoneDataAsset;
	}
	else if (Recipe.bBakeVertexAnimation && !Recipe.VertexDataAsset.IsNull())
	{
		Asset->AnimData = Recipe.VertexDataAsset;
	}

	if (const UAnimToTextureDataAsset* Baked = Asset->AnimData.LoadSynchronous())
	{
		if (!Baked->StaticMesh.IsNull())
		{
			Asset->Mesh = Baked->StaticMesh;
		}
	}

	TArray<UPackage*> Packages = { Asset->GetOutermost() };
	FEditorFileUtils::PromptForCheckoutAndSave(Packages, /*bCheckDirty*/ false, /*bPromptToSave*/ false);

	UE_LOG(LogFlockEditor, Log, TEXT("Wrote the bake recipe back to %s."), *Asset->GetName());
	return true;
#else
	return false;
#endif
}

#if WITH_EDITOR
void UFlockBakeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFlockBakeSettings, Species))
	{
		LoadFromSpecies();
	}

	SaveConfig();
}
#endif
