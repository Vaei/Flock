// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlockDisturbanceComponent.generated.h"

/**
 * Makes its actor alarming to nearby flocks. Add it for anything needing tuning the auto-registered class
 * list cannot express: a rolling boulder, a cat, a thrown object.
 */
UCLASS(ClassGroup=Flock, meta=(BlueprintSpawnableComponent))
class FLOCK_API UFlockDisturbanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFlockDisturbanceComponent();

	/** Relative alarm. 1 is an ordinary walking character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="0.0"))
	float ThreatWeight = 1.f;

	/** Beyond this the source is ignored entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float MaxRadius = 1200.f;

	/**
	 * Which component's position the birds actually track. Leave unset for the actor itself.
	 *
	 * Needed whenever physics simulates on a child rather than the root: the mesh rolls away while the
	 * actor's transform stays where it spawned, so the birds would watch an empty spot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SceneComponent"))
	FComponentReference PositionComponent;

	/** Unticked, the actor goes unnoticed without unregistering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flock")
	bool bEnabled = true;

	UFUNCTION(BlueprintCallable, Category="Flock")
	void SetDisturbanceEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	/** Where the birds should consider this source to be, right now. */
	FVector GetDisturbanceLocation() const;

	bool IsDisturbanceEnabled() const { return bEnabled; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
