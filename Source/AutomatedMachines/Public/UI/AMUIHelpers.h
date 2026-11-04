// Small helpers to build our widgets in code, in one consistent look.
//
// Buttons and fields are drawn with plain colour brushes (no textures), so they never depend on
// engine style images that might not be in the packaged game.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UWidgetTree;
class UTextBlock;
class UButton;
class UBorder;

namespace AMUI
{
	extern const FLinearColor Orange;
	extern const FLinearColor White;
	extern const FLinearColor Grey;
	extern const FLinearColor PageBackground;
	extern const FLinearColor FieldBackground;

	/** The base font with another size. */
	FSlateFontInfo SizedFont(const FSlateFontInfo& Base, int32 Size);

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color);

	/** A flat button with a text label. OutLabel (optional) gets the label, to change it later. */
	UButton* MakeButton(UWidgetTree* Tree, const FString& Label, const FSlateFontInfo& Font, bool bAccent = false, UTextBlock** OutLabel = nullptr);

	/** A flat coloured box with padding. */
	UBorder* MakeBox(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding);
}
