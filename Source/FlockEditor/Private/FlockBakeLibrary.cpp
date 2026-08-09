// Copyright (c) Jared Taylor. All Rights Reserved

#include "FlockBakeLibrary.h"

#include "FlockBake.h"
#include "FlockBakeSettings.h"
#include "FlockEditorLog.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceConstant.h"

bool UFlockBakeLibrary::PrepareFlockAssets()
{
	FText Error;
	if (FFlockBake::PrepareAssets(*UFlockBakeSettings::Get(), Error))
	{
		return true;
	}

	UE_LOG(LogFlockEditor, Error, TEXT("PrepareFlockAssets failed: %s"), *Error.ToString());
	return false;
}

bool UFlockBakeLibrary::BakeFlockTextures()
{
	FText Error;
	if (FFlockBake::Bake(*UFlockBakeSettings::Get(), Error))
	{
		return true;
	}

	UE_LOG(LogFlockEditor, Error, TEXT("BakeFlockTextures failed: %s"), *Error.ToString());
	return false;
}

void UFlockBakeLibrary::ConfigureFlockBake(const FString& SkeletalMeshPath, const FString& OutputPath,
	const FString& AssetName, const TArray<FString>& AnimSequencePaths, float SampleRate)
{
	UFlockBakeSettings* Settings = UFlockBakeSettings::Get();

	Settings->Recipe.SourceSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshPath));
	Settings->Recipe.OutputPath.Path = OutputPath;
	Settings->Recipe.AssetName = AssetName;
	Settings->Recipe.SampleRate = SampleRate;

	Settings->Recipe.AnimSequences.Reset();
	for (const FString& Path : AnimSequencePaths)
	{
		Settings->Recipe.AnimSequences.Add(TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(Path)));
	}

	Settings->SaveConfig();
}

void UFlockBakeLibrary::SetFlockBakeMaterialInstances(const TArray<FString>& MaterialInstancePaths)
{
	UFlockBakeSettings* Settings = UFlockBakeSettings::Get();

	Settings->Recipe.BoneMaterialInstances.Reset();
	for (const FString& Path : MaterialInstancePaths)
	{
		Settings->Recipe.BoneMaterialInstances.Add(
			TSoftObjectPtr<UMaterialInstanceConstant>(FSoftObjectPath(Path)));
	}

	Settings->SaveConfig();
}
