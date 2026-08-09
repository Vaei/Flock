// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

/**
 * The bake window's contents: a details view over UFlockBakeSettings plus the two actions.
 * A details view rather than hand-built Slate, so asset pickers, array editing and reset-to-default all
 * come for free and any field added later needs no widget work.
 */
class SFlockBakeWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlockBakeWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SFlockBakeWindow() override;

	/** Opens the window, or focuses it if it is already up. */
	static void Open();

private:
	FReply OnLoadClicked();
	FReply OnSaveClicked();
	FReply OnPrepareClicked();
	FReply OnBakeClicked();

	bool HasSpecies() const;

	bool IsBakeEnabled() const;
	FText GetBakeTooltip() const;

	TSharedPtr<IDetailsView> DetailsView;

	/** The recipe can be replaced underneath the view, which then has to re-read it. */
	FDelegateHandle RecipeReplacedHandle;

	static TWeakPtr<SWindow> WindowInstance;
};
