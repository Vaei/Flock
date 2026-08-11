// Copyright (c) Jared Taylor

#include "FlockActorFactory.h"

#include "AnimToTextureDataAsset.h"
#include "FlockBakeSettings.h"
#include "FlockDeveloper.h"
#include "Actors/FlockBlockingVolume.h"
#include "Actors/FlockVolume.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Data/FlockSpeciesData.h"

#define LOCTEXT_NAMESPACE "FlockActorFactory"

UFlockVolumeFactory::UFlockVolumeFactory()
{
	NewActorClass = AFlockVolume::StaticClass();
	bShowInEditorQuickMenu = true;
	DisplayName = LOCTEXT("FlockVolume", "Flock Volume");
}

UFlockSpeciesData* UFlockVolumeFactory::FindDefaultSpecies()
{
	if (UFlockSpeciesData* Default = UFlockDeveloperSettings::Get().DefaultSpecies.LoadSynchronous())
	{
		return Default;
	}

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByClass(UFlockSpeciesData::StaticClass()->GetClassPathName(), Assets);

	const UAnimToTextureDataAsset* WantedAnimData =
		UFlockBakeSettings::Get()->Recipe.BoneDataAsset.LoadSynchronous();

	for (const FAssetData& Asset : Assets)
	{
		UFlockSpeciesData* Candidate = Cast<UFlockSpeciesData>(Asset.GetAsset());
		if (Candidate && WantedAnimData && Candidate->AnimData.LoadSynchronous() == WantedAnimData)
		{
			return Candidate;
		}
	}

	return Assets.Num() == 1 ? Cast<UFlockSpeciesData>(Assets[0].GetAsset()) : nullptr;
}

void UFlockVolumeFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	if (AFlockVolume* Volume = Cast<AFlockVolume>(NewActor); Volume && !Volume->Species)
	{
		Volume->Species = FindDefaultSpecies();
	}
}

FString UFlockVolumeFactory::GetDefaultActorLabel(UObject* Asset) const
{
	return TEXT("FlockVolume");
}

UFlockBlockingVolumeFactory::UFlockBlockingVolumeFactory()
{
	NewActorClass = AFlockBlockingVolume::StaticClass();
	bShowInEditorQuickMenu = true;
}

void UFlockBlockingVolumeFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	if (AFlockBlockingVolume* Volume = Cast<AFlockBlockingVolume>(NewActor))
	{
		Volume->Shape = Shape;

		// Applies Shape to which component is drawn, so the one that was asked for is the one shown.
		Volume->PostEditChange();
	}
}

FString UFlockBlockingVolumeFactory::GetDefaultActorLabel(UObject* Asset) const
{
	return Shape == EFlockBlockerShape::Box ? TEXT("FlockBlockingBox") : TEXT("FlockBlockingSphere");
}

UFlockBlockingBoxFactory::UFlockBlockingBoxFactory()
{
	Shape = EFlockBlockerShape::Box;
	DisplayName = LOCTEXT("BlockingBox", "Flock Blocking Box");
}

UFlockBlockingSphereFactory::UFlockBlockingSphereFactory()
{
	Shape = EFlockBlockerShape::Sphere;
	DisplayName = LOCTEXT("BlockingSphere", "Flock Blocking Sphere");
}

#undef LOCTEXT_NAMESPACE
