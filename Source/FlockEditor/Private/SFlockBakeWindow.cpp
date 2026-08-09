// Copyright (c) Jared Taylor. All Rights Reserved

#include "SFlockBakeWindow.h"

#include "FlockBake.h"
#include "FlockBakeSettings.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FlockBakeWindow"

TWeakPtr<SWindow> SFlockBakeWindow::WindowInstance;

void SFlockBakeWindow::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bShowOptions = false;
	Args.bHideSelectionTip = true;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropertyModule.CreateDetailView(Args);
	DetailsView->SetObject(UFlockBakeSettings::Get());

	// Assigning a species swaps the whole recipe out from under the view.
	RecipeReplacedHandle = UFlockBakeSettings::Get()->OnRecipeReplaced.AddLambda([this]()
	{
		if (DetailsView.IsValid())
		{
			DetailsView->ForceRefresh();
		}
	});

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			DetailsView.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
			.Padding(8.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SFlockBakeWindow::GetBakeTooltip)
					.AutoWrapText(true)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Load", "Load"))
					.ToolTipText(LOCTEXT("LoadTip", "Replace the recipe below with the assigned species' own."))
					.IsEnabled(this, &SFlockBakeWindow::HasSpecies)
					.OnClicked(this, &SFlockBakeWindow::OnLoadClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Save", "Save To Species"))
					.ToolTipText(LOCTEXT("SaveTip",
						"Write the recipe below onto the assigned species, without baking."))
					.IsEnabled(this, &SFlockBakeWindow::HasSpecies)
					.OnClicked(this, &SFlockBakeWindow::OnSaveClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Prepare", "Prepare Asset Set"))
					.ToolTipText(LOCTEXT("PrepareTip",
						"Creates the static mesh, textures and data asset for the source skeletal mesh, then "
						"assigns the data asset above. Run this once per bird, then Bake."))
					.OnClicked(this, &SFlockBakeWindow::OnPrepareClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Bake", "Bake"))
					.ToolTipText(LOCTEXT("BakeTip",
						"Bakes the assigned data assets and pushes their parameters onto the listed material "
						"instances."))
					.IsEnabled(this, &SFlockBakeWindow::IsBakeEnabled)
					.OnClicked(this, &SFlockBakeWindow::OnBakeClicked)
				]
			]
		]
	];
}

void SFlockBakeWindow::Open()
{
	if (const TSharedPtr<SWindow> Existing = WindowInstance.Pin())
	{
		Existing->BringToFront();
		return;
	}

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Flock - Bake Animation Textures"))
		.ClientSize(FVector2D(620.f, 720.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SFlockBakeWindow)
		];

	FSlateApplication::Get().AddWindow(Window);
	WindowInstance = Window;
}

SFlockBakeWindow::~SFlockBakeWindow()
{
	if (RecipeReplacedHandle.IsValid())
	{
		UFlockBakeSettings::Get()->OnRecipeReplaced.Remove(RecipeReplacedHandle);
	}
}

bool SFlockBakeWindow::HasSpecies() const
{
	return !UFlockBakeSettings::Get()->Species.IsNull();
}

FReply SFlockBakeWindow::OnLoadClicked()
{
	if (UFlockBakeSettings::Get()->LoadFromSpecies())
	{
		DetailsView->ForceRefresh();

		FNotificationInfo Info(LOCTEXT("Loaded", "Loaded the recipe from the species."));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	return FReply::Handled();
}

FReply SFlockBakeWindow::OnSaveClicked()
{
	if (UFlockBakeSettings::Get()->SaveToSpecies())
	{
		FNotificationInfo Info(LOCTEXT("Saved", "Wrote the recipe to the species."));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	return FReply::Handled();
}

FReply SFlockBakeWindow::OnPrepareClicked()
{
	UFlockBakeSettings* Settings = UFlockBakeSettings::Get();

	FText Error;
	if (!FFlockBake::PrepareAssets(*Settings, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error);
		return FReply::Handled();
	}

	// PrepareAssets reassigns the data asset, so the view has to re-read it.
	DetailsView->ForceRefresh();

	FNotificationInfo Info(LOCTEXT("Prepared", "Asset set prepared. Check the clip list, then Bake."));
	Info.ExpireDuration = 6.f;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

FReply SFlockBakeWindow::OnBakeClicked()
{
	const UFlockBakeSettings* Settings = UFlockBakeSettings::Get();

	FText Error;
	if (!FFlockBake::Bake(*Settings, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error);
		return FReply::Handled();
	}

	FNotificationInfo Info(LOCTEXT("Baked", "Bake complete."));
	Info.ExpireDuration = 6.f;
	FSlateNotificationManager::Get().AddNotification(Info);

	return FReply::Handled();
}

bool SFlockBakeWindow::IsBakeEnabled() const
{
	FText Reason;
	return FFlockBake::CanBake(*UFlockBakeSettings::Get(), Reason);
}

FText SFlockBakeWindow::GetBakeTooltip() const
{
	FText Reason;
	FFlockBake::CanBake(*UFlockBakeSettings::Get(), Reason);
	return Reason;
}

#undef LOCTEXT_NAMESPACE
