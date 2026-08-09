// Copyright (c) Jared Taylor. All Rights Reserved

#include "FlockDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "FlockDetails"

TSharedRef<IDetailCustomization> FFlockDetails::MakeInstance()
{
	return MakeShared<FFlockDetails>();
}

void FFlockDetails::PromoteCategory(IDetailLayoutBuilder& DetailBuilder, FName Category,
	const FText& DisplayName)
{
	TArray<FName> Existing;
	DetailBuilder.GetCategoryNames(Existing);
	if (!Existing.Contains(Category))
	{
		return;
	}

	DetailBuilder.EditCategory(Category, DisplayName, ECategoryPriority::Important);
}

void FFlockDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PromoteCategory(DetailBuilder, TEXT("Flock"));
}

TSharedRef<IDetailCustomization> FFlockVATPreviewDetails::MakeInstance()
{
	return MakeShared<FFlockVATPreviewDetails>();
}

void FFlockVATPreviewDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Display name only; the category path the properties declare is unchanged, so the nested group has to
	// be renamed on its own.
	PromoteCategory(DetailBuilder, TEXT("Preview"), LOCTEXT("FlockCategory", "Flock"));
	PromoteCategory(DetailBuilder, TEXT("Preview|Grid"), LOCTEXT("FlockGridCategory", "Flock|Grid"));

	FFlockDetails::CustomizeDetails(DetailBuilder);
}

#undef LOCTEXT_NAMESPACE
