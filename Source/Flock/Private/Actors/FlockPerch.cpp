// Copyright (c) Jared Taylor

#include "Actors/FlockPerch.h"

#include "Components/BillboardComponent.h"
#include "Components/FlockPerchComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

AFlockPerch::AFlockPerch()
{
	PrimaryActorTick.bCanEverTick = false;

	Perch = CreateDefaultSubobject<UFlockPerchComponent>(TEXT("Perch"));
	SetRootComponent(Perch);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> IconTexture(
			TEXT("/Flock/T_FlockVolumeIcon"));

		Sprite->Sprite = IconTexture.Get();
		Sprite->SetupAttachment(Perch);

		// Constant on screen however far away it is, which is the point of having it.
		Sprite->bIsScreenSizeScaled = true;
		Sprite->ScreenSize = 0.0025f * 2.f;
		Sprite->SetHiddenInGame(true);
		Sprite->SetIsVisualizationComponent(true);
	}
#endif
}

#if WITH_EDITOR
void AFlockPerch::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// A Box source traces down onto whatever is below, so where it was dropped is the only place its slots
	// mean anything.
	if (bFinished && Perch)
	{
		Perch->RebuildSlots();
	}
}
#endif
