// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ScareFlock.generated.h"

/**
 * Startles nearby birds at this point in a montage.
 *
 * On the animation rather than in gameplay code because the moment a swing, a landing or a shout ought to
 * alarm something is a property of the animation, and only whoever authored it knows where it falls.
 */
UCLASS(Blueprintable, meta=(DisplayName="Scare Flock"))
class FLOCK_API UAnimNotify_ScareFlock : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ScareFlock();

	/** How far it carries. Not capped by a species' Max Notice Radius: naming a radius means that radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="cm"))
	float Radius = 2000.f;

	/**
	 * How alarming. Comparable to a disturbance source's threat weight, where a walking character is 1, so
	 * the default is emphatic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0"))
	float Weight = 8.f;

	/** How long it keeps alarming them. Alarm accumulates over this, so birds break a beat after it fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ForceUnits="s"))
	float Duration = 0.6f;

	/** 1 is linear. Raising it pulls the reaction in tight around the middle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.01", ClampMax="8.0"))
	float Falloff = 1.f;

	/**
	 * Centre it on this socket rather than the actor. Worth setting for a weapon swing, where the noise
	 * comes from the blade rather than from the middle of the character.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	FName SocketName;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
