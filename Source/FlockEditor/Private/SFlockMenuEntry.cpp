// Copyright (c) Jared Taylor

#include "SFlockMenuEntry.h"

#include "ActorFactories/ActorFactory.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Editor.h"
#include "LevelEditorActions.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FlockEditor"

void SFlockMenuEntry::Construct(const FArguments& InArgs, UActorFactory* InFactory)
{
	Factory = InFactory;
	Style = &FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Menu.Button"));

	const float IconSize = FAppStyle::Get().GetFloat(TEXT("Menu.MenuIconSize"));

	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(this, &SFlockMenuEntry::GetBorder)
		.Cursor(EMouseCursor::GrabHand)
		.ToolTipText(InArgs._ToolTip)
		.Padding(FMargin(10.f, 3.f, 5.f, 3.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(14.f, 0.f, 10.f, 0.f)
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(IconSize)
				.HeightOverride(IconSize)
				[
					SNew(SImage)
					.Image(InArgs._Icon.GetIcon())
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Text(InArgs._Label)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Image(FAppStyle::Get().GetBrush(TEXT("Icons.DragHandle")))
			]
		]
	];
}

const FSlateBrush* SFlockMenuEntry::GetBorder() const
{
	if (bIsPressed)
	{
		return &Style->Pressed;
	}
	return IsHovered() ? &Style->Hovered : &Style->Normal;
}

FReply SFlockMenuEntry::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	bIsPressed = true;
	return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
}

FReply SFlockMenuEntry::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !Factory.IsValid())
	{
		return FReply::Unhandled();
	}

	bIsPressed = false;

	{
		// The spawn and the move are one transaction, so undo does not leave an actor at the origin.
		const FScopedTransaction Transaction(LOCTEXT("PlaceFlockActor", "Place Flock Actor"));

		const FAssetData Asset(Factory->NewActorClass.Get());
		AActor* NewActor = FLevelEditorActionCallbacks::AddActor(Factory.Get(), Asset, nullptr);

		if (NewActor && GCurrentLevelEditingViewportClient)
		{
			GEditor->MoveActorInFrontOfCamera(*NewActor,
				GCurrentLevelEditingViewportClient->GetViewLocation(),
				GCurrentLevelEditingViewportClient->GetViewRotation().Vector());
		}
	}

	if (!MouseEvent.IsControlDown())
	{
		FSlateApplication::Get().DismissAllMenus();
	}

	return FReply::Handled();
}

FReply SFlockMenuEntry::OnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	bIsPressed = false;

	if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || !Factory.IsValid())
	{
		return FReply::Handled();
	}

	const FAssetData Asset(Factory->NewActorClass.Get());

	// Content browser style drags are routed through this when something is listening, and the
	// viewport's own preview only follows the drag it started itself.
	if (FEditorDelegates::OnAssetDragStarted.IsBound())
	{
		TArray<FAssetData> Dragged;
		Dragged.Add(Asset);
		FEditorDelegates::OnAssetDragStarted.Broadcast(Dragged, Factory.Get());
		return FReply::Handled();
	}

	return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(Asset, Factory.Get()));
}

#undef LOCTEXT_NAMESPACE
