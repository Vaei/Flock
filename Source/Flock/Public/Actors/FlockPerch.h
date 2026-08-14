// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlockPerch.generated.h"

class UFlockPerchComponent;

/**
 * Somewhere birds can land, as an actor rather than a component.
 *
 * A perch belongs on the thing birds settle on, so the component is the normal way in: put one on the fence,
 * the roof, the tree. This is for everywhere there is nothing to put it on, or nothing worth opening a
 * Blueprint for - a ledge that is level geometry, a patch of ground under a wall.
 */
UCLASS(Blueprintable)
class FLOCK_API AFlockPerch : public AActor
{
	GENERATED_BODY()

public:
	AFlockPerch();

	UFlockPerchComponent* GetPerch() const { return Perch; }

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Flock")
	TObjectPtr<UFlockPerchComponent> Perch;

#if WITH_EDITORONLY_DATA
	/** Gives it something clickable in the viewport; the slots only draw once it is selected. */
	UPROPERTY(VisibleAnywhere, Category="Flock")
	TObjectPtr<class UBillboardComponent> Sprite;
#endif
};
