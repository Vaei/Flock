// Copyright (c) Jared Taylor. All Rights Reserved

#include "System/FlockStatics.h"

#include "System/FlockSubsystem.h"
#include "GameFramework/Actor.h"

void UFlockStatics::ScareFlock(const UObject* WorldContextObject, FVector Location, float Radius,
	float Weight, float Duration, float Falloff)
{
	if (UFlockSubsystem* Subsystem = UFlockSubsystem::Get(WorldContextObject))
	{
		Subsystem->AddScare(Location, Radius, Weight, Duration, Falloff);
	}
}

void UFlockStatics::ScareFlockAtActor(const AActor* Actor, float Radius, float Weight, float Duration,
	float Falloff)
{
	if (Actor)
	{
		ScareFlock(Actor, Actor->GetActorLocation(), Radius, Weight, Duration, Falloff);
	}
}
