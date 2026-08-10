// Copyright (c) Jared Taylor

#include "Actors/FlockBlockingVolume.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "System/FlockSubsystem.h"

AFlockBlockingVolume::AFlockBlockingVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxVisual = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxVisual"));
	SetRootComponent(BoxVisual);

	SphereVisual = CreateDefaultSubobject<USphereComponent>(TEXT("SphereVisual"));
	SphereVisual->SetupAttachment(BoxVisual);

	for (UShapeComponent* Component : { static_cast<UShapeComponent*>(BoxVisual),
		static_cast<UShapeComponent*>(SphereVisual) })
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetHiddenInGame(true);
		Component->ShapeColor = FColor(220, 120, 40);
	}

	SyncVisuals();
}

void AFlockBlockingVolume::SyncVisuals()
{
	const bool bBox = Shape == EFlockBlockerShape::Box;

	BoxVisual->SetBoxExtent(BoxExtent, /*bUpdateOverlaps*/ false);
	SphereVisual->SetSphereRadius(SphereRadius, /*bUpdateOverlaps*/ false);

	BoxVisual->SetVisibility(bBox);
	SphereVisual->SetVisibility(!bBox);
}

float AFlockBlockingVolume::GetBoundingRadius() const
{
	const FVector Scale = GetActorScale3D();
	if (Shape == EFlockBlockerShape::Sphere)
	{
		return SphereRadius * Scale.GetAbsMax() + AvoidMargin;
	}

	return (BoxExtent * Scale).Size() + AvoidMargin;
}

FFlockBlocker AFlockBlockingVolume::MakeBlocker() const
{
	const FVector Scale = GetActorScale3D();

	FFlockBlocker Blocker;
	Blocker.Centre = FVector3f(GetActorLocation());
	Blocker.Rotation = FQuat4f(GetActorQuat());
	Blocker.Margin = AvoidMargin;
	Blocker.Shape = Shape;

	if (Shape == EFlockBlockerShape::Sphere)
	{
		// A non-uniform scale cannot be a sphere, so the largest axis wins rather than silently squashing it.
		Blocker.Extent = FVector3f(SphereRadius * Scale.GetAbsMax());
	}
	else
	{
		Blocker.Extent = FVector3f(BoxExtent * Scale.GetAbs());
	}

	return Blocker;
}

void AFlockBlockingVolume::BeginPlay()
{
	Super::BeginPlay();

	if (UFlockSubsystem* Subsystem = UFlockSubsystem::Get(this))
	{
		Subsystem->RegisterBlockingVolume(this);
	}
}

void AFlockBlockingVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFlockSubsystem* Subsystem = UFlockSubsystem::Get(this))
	{
		Subsystem->UnregisterBlockingVolume(this);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AFlockBlockingVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SyncVisuals();
}
#endif
