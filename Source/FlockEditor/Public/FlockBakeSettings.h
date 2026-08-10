// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Data/FlockBakeRecipe.h"
#include "FlockBakeSettings.generated.h"

class UFlockSpeciesData;

/**
 * The bake window's state. Config-backed on the CDO, so the window comes back as it was left.
 *
 * The recipe itself belongs to a species asset, not to the window. Pick a Species and its recipe loads;
 * bake and it is written back. Nothing is lost by moving on to a different bird.
 */
UCLASS(Config=EditorPerProjectUserSettings)
class FLOCKEDITOR_API UFlockBakeSettings : public UObject
{
	GENERATED_BODY()

public:
	UFlockBakeSettings();

	static UFlockBakeSettings* Get() { return GetMutableDefault<UFlockBakeSettings>(); }

	/**
	 * The bird being set up. Assigning one loads its recipe; baking writes the recipe back and points the
	 * species at the assets that were produced.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Species")
	TSoftObjectPtr<UFlockSpeciesData> Species;

	UPROPERTY(EditAnywhere, Config, Category="Bake", meta=(ShowOnlyInnerProperties))
	FFlockBakeRecipe Recipe;

	/** Also save every asset the bake touched. The bake itself only marks packages dirty. */
	UPROPERTY(EditAnywhere, Config, Category="Options")
	bool bSaveAfterBake = true;

	/**
	 * Write the recipe back to the species after a successful bake, and point it at the assets produced.
	 * Off means the window is a scratchpad and the species keeps whatever it had.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Options")
	bool bWriteBackToSpecies = true;

	/**
	 * Allow the bake to update material instances outside /Game. Off by default: engine plugin content has no
	 * source control to undo a bad write. Only tick this to deliberately re-bake a plugin's reference assets.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Options", AdvancedDisplay)
	bool bAllowWritingOutsideProject = false;

	/** Copies the assigned species' recipe into the window. No-op without a species. */
	bool LoadFromSpecies();

	/** Copies the window's recipe onto the assigned species and saves it. No-op without a species. */
	bool SaveToSpecies();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Fires when the recipe is swapped underneath the details view, which then has to re-read it. */
	DECLARE_MULTICAST_DELEGATE(FOnRecipeReplaced);
	FOnRecipeReplaced OnRecipeReplaced;
};
