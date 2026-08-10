// Copyright (c) Jared Taylor

#include "Components/FlockDisturbanceComponent.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "System/FlockSubsystem.h"

UFlockDisturbanceComponent::UFlockDisturbanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector UFlockDisturbanceComponent::GetDisturbanceLocation() const
{
	if (const USceneComponent* Tracked = Cast<USceneComponent>(PositionComponent.GetComponent(GetOwner())))
	{
		return Tracked->GetComponentLocation();
	}

	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UFlockDisturbanceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UFlockSubsystem* Subsystem = UFlockSubsystem::Get(this))
	{
		Subsystem->RegisterSource(GetOwner(), ThreatWeight, MaxRadius, this);
	}
}

void UFlockDisturbanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFlockSubsystem* Subsystem = UFlockSubsystem::Get(this))
	{
		Subsystem->UnregisterSource(GetOwner());
	}

	Super::EndPlay(EndPlayReason);
}
