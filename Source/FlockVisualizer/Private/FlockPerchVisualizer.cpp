// Copyright (c) Jared Taylor. All Rights Reserved

#include "FlockPerchVisualizer.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/FlockPerchComponent.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "SceneManagement.h"
#include "SceneView.h"

namespace FlockPerchVisualizerPrivate
{
	static constexpr float SlotRadius = 9.f;
	static constexpr float FacingLength = 34.f;
	static constexpr int32 MaxLabelledSlots = 96;

	static const FColor PerchColour(90, 200, 130);
	static const FColor GroundColour(205, 170, 95);
	static const FColor SourceColour(120, 120, 140);
}

void FFlockPerchVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	using namespace FlockPerchVisualizerPrivate;

	const UFlockPerchComponent* Perch = Cast<UFlockPerchComponent>(Component);
	if (!Perch)
	{
		return;
	}

	const FTransform& ComponentTransform = Perch->GetComponentTransform();

	// The box a grid is sampled from, so an empty component still shows where it would put slots.
	if (Perch->Source == EFlockPerchSource::Box)
	{
		DrawWireBox(PDI, FBox::BuildAABB(FVector::ZeroVector, Perch->BoxExtent)
			.TransformBy(ComponentTransform), SourceColour, SDPG_Foreground);
	}

	for (const FFlockAuthoredSlot& Slot : Perch->BakedSlots)
	{
		const FVector Position = ComponentTransform.TransformPosition(Slot.LocalPosition);
		const FQuat Rotation = ComponentTransform.GetRotation() * Slot.LocalRotation.Quaternion();
		const FColor Colour = Slot.bPerch ? PerchColour : GroundColour;

		// Flat, so a row of slots along a rail reads as a row rather than a wall of spheres.
		DrawCircle(PDI, Position, FVector::XAxisVector, FVector::YAxisVector, Colour, SlotRadius,
			12, SDPG_Foreground);

		PDI->DrawLine(Position, Position + FVector::UpVector * SlotRadius, Colour, SDPG_Foreground);

		DrawDirectionalArrow(PDI, FQuatRotationTranslationMatrix(Rotation, Position), Colour,
			FacingLength, 6.f, SDPG_Foreground);
	}
}

void FFlockPerchVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport,
	const FSceneView* View, FCanvas* Canvas)
{
	using namespace FlockPerchVisualizerPrivate;

	const UFlockPerchComponent* Perch = Cast<UFlockPerchComponent>(Component);
	if (!Perch || !GEngine)
	{
		return;
	}

	const UFont* Font = GEngine->GetSmallFont();
	if (!Font)
	{
		return;
	}

	const auto DrawAt = [View, Canvas, Font](const FVector& World, const FString& Text, const FLinearColor& Colour)
	{
		FVector2D Pixel;
		if (!View->ScreenToPixel(View->WorldToScreen(World), Pixel))
		{
			return;
		}

		FCanvasTextItem Item(Pixel, FText::FromString(Text), Font, Colour);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	};

	const FTransform& ComponentTransform = Perch->GetComponentTransform();

	if (Perch->BakedSlots.IsEmpty())
	{
		DrawAt(ComponentTransform.GetLocation(), TEXT("No slots - press Rebuild Slots"),
			FLinearColor(1.f, 0.45f, 0.35f));
		return;
	}

	// Indices are what a warning about a specific slot names, but a hundred labels is a wall of text.
	if (Perch->BakedSlots.Num() <= MaxLabelledSlots)
	{
		for (int32 Index = 0; Index < Perch->BakedSlots.Num(); ++Index)
		{
			const FFlockAuthoredSlot& Slot = Perch->BakedSlots[Index];
			const FVector Position = ComponentTransform.TransformPosition(Slot.LocalPosition);

			DrawAt(Position + FVector::UpVector * (SlotRadius + 4.f), FString::FromInt(Index),
				Slot.bPerch ? FLinearColor(0.35f, 0.8f, 0.5f) : FLinearColor(0.8f, 0.67f, 0.37f));
		}
	}
	else
	{
		DrawAt(ComponentTransform.GetLocation(),
			FString::Printf(TEXT("%d slots"), Perch->BakedSlots.Num()), FLinearColor::White);
	}
}
