// Copyright (c) Jared Taylor

#include "FlockVolumeVisualizer.h"

#include "Actors/FlockVolume.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/BoxComponent.h"
#include "Data/FlockSpeciesData.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "FlockDeveloper.h"
#include "SceneManagement.h"
#include "SceneView.h"

namespace FlockVolumeVisualizerPrivate
{
	static const FColor CircuitColour(90, 190, 220);
	static const FColor PlaneColour(210, 150, 90);

	static const UFlockSpeciesData* ResolveSpecies(const AFlockVolume& Volume)
	{
		if (Volume.Species)
		{
			return Volume.Species;
		}

		// A volume with nothing assigned still spawns, using the project default, so the circuit it would
		// wheel around is still worth drawing.
		return UFlockDeveloperSettings::Get().DefaultSpecies.LoadSynchronous();
	}
}

void FFlockVolumeVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	using namespace FlockVolumeVisualizerPrivate;

	const AFlockVolume* Volume = Component ? Cast<AFlockVolume>(Component->GetOwner()) : nullptr;
	if (!Volume)
	{
		return;
	}

	const FVector Origin = Volume->GetActorLocation();

	// Where birds land if no ground is found, or if tracing is off entirely: the plane through the actor,
	// which is the box's middle rather than its floor. Worth seeing, because a volume centred at eye height
	// puts a flock in the air.
	if (const UBoxComponent* Bounds = Volume->GetBounds())
	{
		const FVector Extent = Bounds->GetScaledBoxExtent();
		const FVector Corners[4] = {
			Origin + FVector(+Extent.X, +Extent.Y, 0.f),
			Origin + FVector(-Extent.X, +Extent.Y, 0.f),
			Origin + FVector(-Extent.X, -Extent.Y, 0.f),
			Origin + FVector(+Extent.X, -Extent.Y, 0.f)
		};

		const FColor Colour = Volume->bSnapToGround ? PlaneColour.WithAlpha(90) : PlaneColour;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			PDI->DrawLine(Corners[Index], Corners[(Index + 1) % 4], Colour, SDPG_World);
		}
	}

	const UFlockSpeciesData* Species = ResolveSpecies(*Volume);
	if (!Species)
	{
		return;
	}

	const float Radius = Species->Flight.CruiseRadius;
	const float Ceiling = Species->Flight.CruiseCeiling;
	if (Radius <= 0.f)
	{
		return;
	}

	// The circuit airborne birds wheel around. Invisible otherwise, and the usual surprise is that it
	// reaches through a roof or a cliff the volume itself sits clear of.
	const FVector Centre = Origin + FVector(0.f, 0.f, Ceiling);
	DrawCircle(PDI, Centre, FVector::XAxisVector, FVector::YAxisVector, CircuitColour, Radius, 48,
		SDPG_World);

	PDI->DrawLine(Origin, Centre, CircuitColour, SDPG_World);
	PDI->DrawLine(Centre + FVector(Radius, 0.f, 0.f), Origin + FVector(Radius, 0.f, 0.f),
		CircuitColour.WithAlpha(120), SDPG_World);
}

void FFlockVolumeVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport,
	const FSceneView* View, FCanvas* Canvas)
{
	using namespace FlockVolumeVisualizerPrivate;

	const AFlockVolume* Volume = Component ? Cast<AFlockVolume>(Component->GetOwner()) : nullptr;
	if (!Volume || !GEngine)
	{
		return;
	}

	const UFont* Font = GEngine->GetSmallFont();
	if (!Font)
	{
		return;
	}

	const UFlockSpeciesData* Species = ResolveSpecies(*Volume);

	TArray<FString> Lines;
	if (!Species)
	{
		Lines.Add(TEXT("No species, and no default set in Project Settings"));
	}
	else
	{
		if (!Volume->Species)
		{
			Lines.Add(TEXT("Using the project default species"));
		}

		Lines.Add(FString::Printf(TEXT("%d birds"), Volume->SpawnCount));
		Lines.Add(FString::Printf(TEXT("Circuit %.0f at %.0f up"),
			Species->Flight.CruiseRadius, Species->Flight.CruiseCeiling));

		if (!Volume->bSnapToGround)
		{
			Lines.Add(TEXT("Not snapping to ground"));
		}
	}

	FVector2D Pixel;
	if (!View->ScreenToPixel(View->WorldToScreen(Volume->GetActorLocation()), Pixel))
	{
		return;
	}

	for (const FString& Line : Lines)
	{
		FCanvasTextItem Item(Pixel, FText::FromString(Line), Font,
			Species ? FLinearColor::White : FLinearColor(1.f, 0.45f, 0.35f));
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);

		Pixel.Y += Font->GetMaxCharHeight();
	}
}
