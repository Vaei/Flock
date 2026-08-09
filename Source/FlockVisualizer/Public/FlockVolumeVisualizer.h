// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "ComponentVisualizer.h"
#include "CoreMinimal.h"

/**
 * Draws what a flock volume does that its box does not show: the circuit airborne birds wheel around, and
 * the plane birds are placed on.
 */
class FLOCKVISUALIZER_API FFlockVolumeVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;

	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport,
		const FSceneView* View, FCanvas* Canvas) override;
};
