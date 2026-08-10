// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Pawn.h"
#include "FlockDeveloper.generated.h"

class UFlockSpeciesData;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Flock"))
class FLOCK_API UFlockDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(EditAnywhere, Config, Category="Flock")
	bool bEnableFlock = true;

	/** Used by any flock that does not name its own species. */
	UPROPERTY(EditAnywhere, Config, Category="Flock")
	TSoftObjectPtr<UFlockSpeciesData> DefaultSpecies;

	/**
	 * Instances per ISM component before splitting into another. Bounds the cost of one instance-buffer
	 * upload and keeps each component's bounds tight enough to frustum cull usefully.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Flock|Performance", meta=(ClampMin="1"))
	int32 MaxInstancesPerComponent = 256;

	/** Hard ceiling across every flock in the world. Spawns past it are refused and logged. */
	UPROPERTY(EditAnywhere, Config, Category="Flock|Performance", meta=(ClampMin="1"))
	int32 MaxBirdsTotal = 2000;

	/**
	 * Every spawned actor of these classes becomes a disturbance source, with no per-actor setup. Defaults
	 * to Pawn, so every character alarms birds out of the box. Add a UFlockDisturbanceComponent instead when
	 * something needs its own weight.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Flock|Disturbance", meta=(AllowAbstract))
	TArray<TSoftClassPtr<AActor>> AutoRegisterDisturbanceClasses = { APawn::StaticClass() };

	UPROPERTY(EditAnywhere, Config, Category="Flock|Disturbance", meta=(ClampMin="0.0"))
	float AutoSourceThreatWeight = 1.f;

	UPROPERTY(EditAnywhere, Config, Category="Flock|Disturbance", meta=(ClampMin="0.0", ForceUnits="cm"))
	float AutoSourceRadius = 1200.f;

	/** Beyond each of these a bird drops a tier. Screen radius can promote it back regardless. */
	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="1.0", ForceUnits="cm"))
	float NearDistance = 2000.f;

	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="1.0", ForceUnits="cm"))
	float MidDistance = 5000.f;

	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="1.0", ForceUnits="cm"))
	float FarDistance = 12000.f;

	/** Demote at the threshold times this, so a bird sitting on a boundary is not re-tiered every frame. */
	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="1.0", ClampMax="2.0"))
	float LODHysteresis = 1.15f;

	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="0.0", ForceUnits="s"))
	float LODDwellTime = 0.5f;

	/**
	 * Mid and Far birds only run their expensive work every Nth frame, with the rest of that frame's motion
	 * folded into a scaled delta so they still move at the right speed.
	 *
	 * The phase comes from each bird's seed, so the work is spread across frames rather than every Far bird
	 * spiking on the same one.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="1", ClampMax="16"))
	int32 MidFrameDivisor = 2;

	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="1", ClampMax="16"))
	int32 FarFrameDivisor = 6;

	/** How often tiers are re-evaluated. A tier change moves an archetype, so this is not per frame. */
	UPROPERTY(EditAnywhere, Config, Category="Flock|LOD", meta=(ClampMin="1.0"))
	float LODRateHz = 4.f;

	/** Spatialise takeoff and landing to the individual bird, on top of the per-flock bed. */
	UPROPERTY(EditAnywhere, Config, Category="Flock|Audio")
	bool bEnableOneShotAudio = true;

	/** How many one-shots can overlap. A cascade past this drops the surplus rather than queueing it. */
	UPROPERTY(EditAnywhere, Config, Category="Flock|Audio", meta=(ClampMin="0", ClampMax="32"))
	int32 OneShotPoolSize = 8;

	static const UFlockDeveloperSettings& Get() { return *GetDefault<UFlockDeveloperSettings>(); }
};
