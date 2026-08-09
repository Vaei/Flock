// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "FlockTypes.h"
#include "FlockPerchComponent.generated.h"

class UMeshComponent;
class USplineComponent;

UENUM(BlueprintType)
enum class EFlockPerchSource : uint8
{
	/** Slots typed in by hand. */
	Explicit,

	/** Every socket on the mesh whose name starts with the prefix. */
	Sockets,

	/** Sampled along a spline at a fixed spacing - a fence rail, a wire, a branch. */
	Spline,

	/** A grid over the box, optionally traced down onto whatever is under it. */
	Box
};

/**
 * Somewhere birds can settle. Add it to any actor: a fence blueprint, a rooftop, a tree.
 *
 * Slots are resolved once in the editor and cooked into the owning asset, so BeginPlay only transforms them
 * by the component and hands them over. Nothing traces at runtime.
 */
UCLASS(ClassGroup=Flock, meta=(BlueprintSpawnableComponent))
class FLOCK_API UFlockPerchComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFlockPerchComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	EFlockPerchSource Source = EFlockPerchSource::Box;

	/** Sockets on this mesh starting with Socket Prefix become slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Sockets", EditConditionHides))
	FName SocketPrefix = TEXT("Perch");

	/**
	 * A component reference rather than a raw pointer: the details panel can only offer a picker for
	 * FComponentReference, so a plain TObjectPtr to a sibling component is not assignable in a Blueprint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Sockets", EditConditionHides,
			UseComponentPicker, AllowedClasses="/Script/Engine.MeshComponent"))
	FComponentReference SocketMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Spline", EditConditionHides,
			UseComponentPicker, AllowedClasses="/Script/Engine.SplineComponent"))
	FComponentReference Spline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Spline", EditConditionHides, ClampMin="1.0",
			ForceUnits="cm"))
	float Spacing = 60.f;

	/** Face along the spline where it is walked, rather than keeping the component's rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Spline", EditConditionHides))
	bool bAlignToTangent = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Box", EditConditionHides))
	FVector BoxExtent = FVector(250.f, 250.f, 100.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Box", EditConditionHides, ClampMin="1.0",
			ForceUnits="cm"))
	float GridSpacing = 120.f;

	/** Drop each grid point onto whatever is beneath it. Editor only; the result is baked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock",
		meta=(EditCondition="Source == EFlockPerchSource::Box", EditConditionHides))
	bool bTraceDown = true;

	/** Scatter within each grid cell, so a box does not read as a grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PositionJitter = 0.4f;

	/** Ground rather than perch. Only changes which birds will consider the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bIsGround = false;

	/**
	 * The resolved slots, in component space. Rebuilt by the button or on edit, and cooked - so this is the
	 * data-only spot list, authored in place rather than kept somewhere else.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	TArray<FFlockAuthoredSlot> BakedSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flock")
	bool bAutoRegister = true;

	/** Resolves the picked references against the owning actor. Null when unset or mistyped. */
	USplineComponent* ResolveSpline() const;
	UMeshComponent* ResolveSocketMesh() const;

	virtual void BeginPlay() override;
	virtual void OnRegister() override;

	/**
	 * Re-derives BakedSlots from the chosen source. The only place a trace ever happens, and a no-op outside
	 * the editor.
	 *
	 * Declared outside WITH_EDITOR on purpose: a CallInEditor button is only drawn for a function that is also
	 * BlueprintCallable and not editor-only reflection, so guarding the declaration hides the button.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Flock")
	void RebuildSlots();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	void RebuildSlotsInternal();
#endif
};
