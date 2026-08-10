// Copyright (c) Jared Taylor

#include "FlockVisualizer.h"

#include "Components/FlockPerchComponent.h"
#include "Debug/FlockVolumeEditorVisualizer.h"
#include "Editor/UnrealEdEngine.h"
#include "FlockPerchVisualizer.h"
#include "FlockVolumeVisualizer.h"
#include "UnrealEdGlobals.h"

void FFlockVisualizerModule::StartupModule()
{
	if (!GUnrealEd)
	{
		return;
	}

	GUnrealEd->RegisterComponentVisualizer(UFlockPerchComponent::StaticClass()->GetFName(),
		MakeShareable(new FFlockPerchVisualizer()));

	GUnrealEd->RegisterComponentVisualizer(UFlockVolumeEditorVisualizer::StaticClass()->GetFName(),
		MakeShareable(new FFlockVolumeVisualizer()));
}

void FFlockVisualizerModule::ShutdownModule()
{
	if (!UObjectInitialized() || IsEngineExitRequested() || !GUnrealEd)
	{
		return;
	}

	GUnrealEd->UnregisterComponentVisualizer(UFlockPerchComponent::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFlockVolumeEditorVisualizer::StaticClass()->GetFName());
}

IMPLEMENT_MODULE(FFlockVisualizerModule, FlockVisualizer)
