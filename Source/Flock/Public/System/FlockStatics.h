// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlockStatics.generated.h"

/** Reaching the flocks from ordinary gameplay code, without holding onto anything. */
UCLASS()
class FLOCK_API UFlockStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Alarms every bird within Radius of Location. Call it from a damage window, an explosion, a door, or
	 * anything else that ought to startle something.
	 *
	 * It is a source with a lifetime rather than a command: birds still decide for themselves, so the ones in
	 * the middle break and scatter while the ones at the edge only look round. Nothing is registered and there
	 * is nothing to clean up.
	 *
	 * @param Location	Centre of the scare.
	 * @param Radius	How far it carries. Unlike a walking actor, this is not capped by what a species would
	 *					notice on its own: naming a radius means that radius.
	 * @param Weight	How alarming it is. Roughly comparable to a source's threat weight, where a walking
	 *					character is 1, so the default is emphatic.
	 * @param Duration	How long it keeps alarming them. Alarm accumulates over this, which is what makes birds
	 *					break a beat after the bang rather than on the same frame.
	 * @param Falloff	How sharply it weakens with distance. 1 is linear; raising it pulls the reaction in
	 *					tight around the middle.
	 */
	UFUNCTION(BlueprintCallable, Category="Flock",
		meta=(WorldContext="WorldContextObject", AdvancedDisplay="Weight,Duration,Falloff"))
	static void ScareFlock(const UObject* WorldContextObject, FVector Location, float Radius = 1500.f,
		float Weight = 8.f, float Duration = 0.6f, float Falloff = 1.f);

	/** ScareFlock centred on an actor. Reads its location at the moment of the call, and does not track it. */
	UFUNCTION(BlueprintCallable, Category="Flock", meta=(AdvancedDisplay="Weight,Duration,Falloff"))
	static void ScareFlockAtActor(const AActor* Actor, float Radius = 1500.f, float Weight = 8.f,
		float Duration = 0.6f, float Falloff = 1.f);
};
