// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Shared details customization for Flock types. Promotes the "Flock" category to Important so it sits
 * above the inherited actor and component categories instead of below them.
 *
 * Register any Flock class against this, or derive from it when a class needs more:
 *   PropertyModule.RegisterCustomClassLayout(UMyFlockThing::StaticClass()->GetFName(),
 *       FOnGetDetailCustomizationInstance::CreateStatic(&FFlockDetails::MakeInstance));
 */
class FLOCKEDITOR_API FFlockDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	/**
	 * Promotes a category, optionally under a different display name. Only touches categories that exist,
	 * so a class without one does not gain an empty header.
	 */
	static void PromoteCategory(IDetailLayoutBuilder& DetailBuilder, FName Category,
		const FText& DisplayName = FText::GetEmpty());
};

/** Presents AFlockVATPreview's "Preview" categories as "Flock". */
class FLOCKEDITOR_API FFlockVATPreviewDetails : public FFlockDetails
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
