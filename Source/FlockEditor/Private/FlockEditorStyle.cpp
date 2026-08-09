// Copyright (c) Jared Taylor. All Rights Reserved

#include "FlockEditorStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FFlockEditorStyle::StyleSet;

FName FFlockEditorStyle::GetStyleSetName()
{
	static const FName StyleName(TEXT("FlockEditorStyle"));
	return StyleName;
}

void FFlockEditorStyle::Register()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Flock"));
	if (!Plugin.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
	StyleSet->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));

	// 16 square is what a toolbar entry draws at; anything larger is downsampled every frame.
	StyleSet->Set(GetMenuIconName(),
		new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icon64"), TEXT(".png")), FVector2D(16.f, 16.f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FFlockEditorStyle::Unregister()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}
