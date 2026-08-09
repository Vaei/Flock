// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "ComponentVisualizer.h"
#include "CoreMinimal.h"

/**
 * Draws a perch component's baked slots: where a bird will stand, and which way it will face.
 *
 * Slots are otherwise invisible until play, and slot placement is where the authoring time goes.
 */
class FLOCKVISUALIZER_API FFlockPerchVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;

	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport,
		const FSceneView* View, FCanvas* Canvas) override;
};
