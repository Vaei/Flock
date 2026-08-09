// Copyright (c) Jared Taylor. All Rights Reserved

#include "Mass/FlockEntityFactory.h"

#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"
#include "Mass/FlockFragments.h"
#include "Mass/FlockTagFragments.h"

namespace UE::Flock
{
	namespace Private
	{
		/** Grounded and Alert are both on the ground, so they share a tag. */
		static bool IsGrounded(EFlockBirdState State)
		{
			return State == EFlockBirdState::Grounded || State == EFlockBirdState::Alert
				|| State == EFlockBirdState::Perched;
		}
	}

	void SetBirdState(FMassExecutionContext& Context, const FMassEntityHandle Entity,
		FFlockStateFragment& State, EFlockBirdState NewState)
	{
		const EFlockBirdState OldState = State.State;
		if (OldState == NewState)
		{
			return;
		}

		State.State = NewState;
		State.StateTime = 0.f;

		// Only touch a tag when the group actually changes, so a Grounded to Alert move costs no archetype
		// change at all.
		const bool bWasGrounded = Private::IsGrounded(OldState);
		const bool bIsGrounded = Private::IsGrounded(NewState);

		if (bWasGrounded != bIsGrounded)
		{
			if (bIsGrounded)
			{
				Context.Defer().AddTag<FFlockGroundedTag>(Entity);
			}
			else
			{
				Context.Defer().RemoveTag<FFlockGroundedTag>(Entity);
			}
		}

		switch (OldState)
		{
		case EFlockBirdState::TakingOff: Context.Defer().RemoveTag<FFlockTakingOffTag>(Entity); break;
		case EFlockBirdState::Flying:    Context.Defer().RemoveTag<FFlockFlyingTag>(Entity); break;
		case EFlockBirdState::Landing:   Context.Defer().RemoveTag<FFlockLandingTag>(Entity); break;
		default: break;
		}

		switch (NewState)
		{
		case EFlockBirdState::TakingOff: Context.Defer().AddTag<FFlockTakingOffTag>(Entity); break;
		case EFlockBirdState::Flying:    Context.Defer().AddTag<FFlockFlyingTag>(Entity); break;
		case EFlockBirdState::Landing:   Context.Defer().AddTag<FFlockLandingTag>(Entity); break;
		default: break;
		}
	}
}
