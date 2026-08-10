// Copyright (c) Jared Taylor

#include "Notifies/AnimNotify_ScareFlock.h"

#include "System/FlockSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_ScareFlock)

UAnimNotify_ScareFlock::UAnimNotify_ScareFlock()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(120, 180, 90);
#endif
}

void UAnimNotify_ScareFlock::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Preview and editor worlds run notifies too, and there is nothing to alarm in either.
	const UWorld* World = MeshComp->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	UFlockSubsystem* Subsystem = UFlockSubsystem::Get(MeshComp);
	if (!Subsystem)
	{
		return;
	}

	const FVector Location = !SocketName.IsNone() && MeshComp->DoesSocketExist(SocketName)
		? MeshComp->GetSocketLocation(SocketName)
		: MeshComp->GetComponentLocation();

	Subsystem->AddScare(Location, Radius, Weight, Duration, Falloff);
}

FString UAnimNotify_ScareFlock::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Scare Flock (%.0f)"), Radius);
}
