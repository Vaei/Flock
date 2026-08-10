// Copyright (c) Jared Taylor

#include "Actors/FlockVolume.h"

#include "Data/FlockSpeciesData.h"
#include "FlockDeveloper.h"
#include "FlockLog.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Debug/FlockVolumeEditorVisualizer.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "System/FlockSubsystem.h"

AFlockVolume::AFlockVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(250.f, 250.f, 250.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetHiddenInGame(true);
	SetRootComponent(Bounds);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> IconTexture(
			TEXT("/Flock/T_FlockVolumeIcon"));

		Sprite->Sprite = IconTexture.Get();
		Sprite->SetupAttachment(Bounds);

		// Constant on screen however far away it is, which is the point of having it.
		Sprite->bIsScreenSizeScaled = true;
		Sprite->ScreenSize = 0.0025f * 2.f;
		Sprite->SetHiddenInGame(true);
		Sprite->SetIsVisualizationComponent(true);
	}

	EditorVisualizer = CreateEditorOnlyDefaultSubobject<UFlockVolumeEditorVisualizer>(
		TEXT("EditorVisualizer"));
	if (EditorVisualizer)
	{
		EditorVisualizer->SetIsVisualizationComponent(true);
	}
#endif
}

void AFlockVolume::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnFlock();
	}
}

void AFlockVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnFlock();
	Super::EndPlay(EndPlayReason);
}

void AFlockVolume::SpawnFlock()
{
	if (FlockIndex != INDEX_NONE)
	{
		return;
	}

	// Birds are cosmetic and simulate per client, so a dedicated server spawns nothing at all: no entities,
	// no render actor, no instanced meshes. Checked here rather than left to the subsystem being absent, so
	// it is silent rather than a warning per volume.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UFlockSubsystem* Subsystem = UFlockSubsystem::Get(this);
	if (!Subsystem)
	{
		UE_LOG(LogFlock, Warning, TEXT("%s: no flock subsystem in this world."), *GetName());
		return;
	}

	UFlockSpeciesData* Resolved = Species;
	if (!Resolved)
	{
		Resolved = UFlockDeveloperSettings::Get().DefaultSpecies.LoadSynchronous();
	}

	if (!Resolved)
	{
		UE_LOG(LogFlock, Warning,
			TEXT("%s has no species, and no default is set in project settings."), *GetName());
		return;
	}

	FFlockSpawnParams Params;
	Params.Species = Resolved;
	Params.Count = SpawnCount;
	Params.Origin = GetActorLocation();
	Params.OrbitPreference = OrbitPreference;
	Params.bSnapToGround = bSnapToGround;
	Params.GroundTraceChannel = GroundTraceChannel;
	Params.GroundOffset = GroundOffset;
	Params.MinSpawnSpacing = MinSpawnSpacing;
	Params.MinGroundNormalZ = MinGroundNormalZ;
	Params.HeadroomRadius = HeadroomRadius;

	// The full extent: X and Y scatter the birds, and Z is the span the ground trace searches through.
	Params.Extent = Bounds->GetScaledBoxExtent();

	FlockIndex = Subsystem->CreateFlock(Params);
}

void AFlockVolume::DespawnFlock()
{
	if (FlockIndex == INDEX_NONE)
	{
		return;
	}

	if (UFlockSubsystem* Subsystem = UFlockSubsystem::Get(this))
	{
		Subsystem->DestroyFlock(FlockIndex);
	}

	FlockIndex = INDEX_NONE;
}
