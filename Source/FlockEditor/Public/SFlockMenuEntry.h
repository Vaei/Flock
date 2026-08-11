// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UActorFactory;

/**
 * A menu row that can be dragged into the level, the way the Place Actors panel's rows can.
 *
 * A plain menu entry only clicks. Dragging needs a widget that starts a drag operation the level
 * viewport understands, and the panel's own row widget is private to the PlacementMode module.
 */
class SFlockMenuEntry : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlockMenuEntry) {}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, ToolTip)
		SLATE_ARGUMENT(FSlateIcon, Icon)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UActorFactory* InFactory);

	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
	const FSlateBrush* GetBorder() const;

	/** What the drop spawns, and what its properties are set from. */
	TWeakObjectPtr<UActorFactory> Factory;

	const FButtonStyle* Style = nullptr;
	bool bIsPressed = false;
};
