// Copyright (c) Jared Taylor

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "FlockVolumeEditorVisualizer.generated.h"

/**
 * Nothing but something for a component visualizer to be registered against.
 *
 * Visualizers key off a component class, so drawing a volume's cruise circuit needs a class only the volume
 * has. Registering against its box would draw on every box component in the project.
 */
UCLASS(ClassGroup=Flock, NotBlueprintable, HideCategories=(Activation, Cooking, AssetUserData, Replication))
class FLOCK_API UFlockVolumeEditorVisualizer : public UActorComponent
{
	GENERATED_BODY()

public:
	UFlockVolumeEditorVisualizer()
	{
		PrimaryComponentTick.bCanEverTick = false;
		bIsEditorOnly = true;
	}
};
