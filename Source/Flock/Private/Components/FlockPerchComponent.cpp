// Copyright (c) Jared Taylor

#include "Components/FlockPerchComponent.h"

#include "Components/MeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FlockLog.h"
#include "System/FlockSubsystem.h"

UFlockPerchComponent::UFlockPerchComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

USplineComponent* UFlockPerchComponent::ResolveSpline() const
{
	return Cast<USplineComponent>(Spline.GetComponent(GetOwner()));
}

UMeshComponent* UFlockPerchComponent::ResolveSocketMesh() const
{
	return Cast<UMeshComponent>(SocketMesh.GetComponent(GetOwner()));
}

void UFlockPerchComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bAutoRegister || BakedSlots.IsEmpty())
	{
		if (bAutoRegister)
		{
			UE_LOG(LogFlock, Warning,
				TEXT("%s has no baked slots. Press Rebuild Slots on it."), *GetReadableName());
		}
		return;
	}

	if (UFlockSubsystem* Subsystem = UFlockSubsystem::Get(this))
	{
		Subsystem->RegisterPerchSlots(this);
	}
}

void UFlockPerchComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Bake on registration as well as on edit, because the Rebuild Slots button cannot appear in the
	// Blueprint editor: FObjectDetails drops archetype objects and then bails, which is every component
	// inside a Blueprint. Relying on the button would mean it only ever worked on placed instances.
	if (BakedSlots.IsEmpty() && Source != EFlockPerchSource::Explicit && GetWorld()
		&& !GetWorld()->IsGameWorld())
	{
		RebuildSlotsInternal();
	}
#endif
}

void UFlockPerchComponent::RebuildSlots()
{
#if WITH_EDITOR
	RebuildSlotsInternal();
#endif
}

#if WITH_EDITOR
void UFlockPerchComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Editing the shape should re-derive the slots, but editing the slots themselves must not stomp them.
	const FName Name = PropertyChangedEvent.GetPropertyName();
	static const TSet<FName> Rebuilders = {
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, Source),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, SocketPrefix),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, SocketMesh),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, Spline),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, Spacing),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, bAlignToTangent),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, BoxExtent),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, GridSpacing),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, bTraceDown),
		GET_MEMBER_NAME_CHECKED(UFlockPerchComponent, PositionJitter),
	};

	if (Rebuilders.Contains(Name))
	{
		RebuildSlotsInternal();
	}
}

void UFlockPerchComponent::RebuildSlotsInternal()
{
	if (Source == EFlockPerchSource::Explicit)
	{
		return;
	}

	Modify();
	BakedSlots.Reset();

	const FTransform ToLocal = GetComponentTransform().Inverse();
	FRandomStream Stream(GetFName().GetNumber() * 7919 + 13);

	switch (Source)
	{
	case EFlockPerchSource::Sockets:
		{
			const UMeshComponent* Mesh = ResolveSocketMesh();
			if (!Mesh)
			{
				UE_LOG(LogFlock, Warning, TEXT("%s: no socket mesh set."), *GetReadableName());
				break;
			}

			const FString Prefix = SocketPrefix.ToString();
			for (const FName SocketName : Mesh->GetAllSocketNames())
			{
				if (!SocketName.ToString().StartsWith(Prefix))
				{
					continue;
				}

				const FTransform Socket = Mesh->GetSocketTransform(SocketName, RTS_World) * ToLocal;

				FFlockAuthoredSlot& Slot = BakedSlots.AddDefaulted_GetRef();
				Slot.LocalPosition = Socket.GetLocation();
				Slot.LocalRotation = Socket.Rotator();
				Slot.bPerch = !bIsGround;
			}
		}
		break;

	case EFlockPerchSource::Spline:
		{
			const USplineComponent* SplineComponent = ResolveSpline();
			if (!SplineComponent)
			{
				UE_LOG(LogFlock, Warning, TEXT("%s: no spline set."), *GetReadableName());
				break;
			}

			const float Length = SplineComponent->GetSplineLength();
			const float Step = FMath::Max(1.f, Spacing);

			for (float Distance = 0.f; Distance <= Length; Distance += Step)
			{
				const float Jittered = FMath::Clamp(
					Distance + Stream.FRandRange(-1.f, 1.f) * Step * PositionJitter, 0.f, Length);

				const FVector World = SplineComponent->GetLocationAtDistanceAlongSpline(
					Jittered, ESplineCoordinateSpace::World);

				FFlockAuthoredSlot& Slot = BakedSlots.AddDefaulted_GetRef();
				Slot.LocalPosition = ToLocal.TransformPosition(World);
				Slot.bPerch = !bIsGround;

				if (bAlignToTangent)
				{
					const FVector Tangent = SplineComponent->GetTangentAtDistanceAlongSpline(
						Jittered, ESplineCoordinateSpace::World).GetSafeNormal();

					// Along the rail, not across it, so a row of birds faces the same way.
					Slot.LocalRotation = ToLocal.TransformVector(Tangent).Rotation();
				}
			}
		}
		break;

	case EFlockPerchSource::Box:
		{
			const float Step = FMath::Max(1.f, GridSpacing);
			const UWorld* World = GetWorld();

			for (float X = -BoxExtent.X; X <= BoxExtent.X; X += Step)
			{
				for (float Y = -BoxExtent.Y; Y <= BoxExtent.Y; Y += Step)
				{
					FVector Local(
						X + Stream.FRandRange(-1.f, 1.f) * Step * PositionJitter,
						Y + Stream.FRandRange(-1.f, 1.f) * Step * PositionJitter,
						0.f);

					FVector WorldPoint = GetComponentTransform().TransformPosition(Local);

					if (bTraceDown && World)
					{
						FHitResult Hit;
						FCollisionQueryParams Params(SCENE_QUERY_STAT(FlockPerchBake));
						Params.AddIgnoredActor(GetOwner());

						if (World->LineTraceSingleByChannel(Hit,
							WorldPoint + FVector(0.f, 0.f, BoxExtent.Z),
							WorldPoint - FVector(0.f, 0.f, BoxExtent.Z),
							ECC_WorldStatic, Params))
						{
							WorldPoint = Hit.ImpactPoint;
						}
						else
						{
							// Nothing under this cell, so it is not somewhere a bird could stand.
							continue;
						}
					}

					FFlockAuthoredSlot& Slot = BakedSlots.AddDefaulted_GetRef();
					Slot.LocalPosition = ToLocal.TransformPosition(WorldPoint);
					Slot.LocalRotation = FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f);
					Slot.bPerch = !bIsGround;
				}
			}
		}
		break;

	default:
		break;
	}

	UE_LOG(LogFlock, Log, TEXT("%s baked %d slots."), *GetReadableName(), BakedSlots.Num());
}
#endif
