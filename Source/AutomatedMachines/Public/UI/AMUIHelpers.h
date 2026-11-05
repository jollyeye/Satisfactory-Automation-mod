// Small helpers to build our widgets in code, in the look of the game's own windows (e.g. the train
// Stop Settings): flat, square, greys, a white arrow on dropdowns, a bin icon to remove.
//
// Brushes are plain colours (no textures) except the two small icons made by Tools/create_assets.py,
// so nothing depends on engine style images that might not be in the packaged game.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UWidgetTree;
class UTextBlock;
class UButton;
class UBorder;
class UImage;

namespace AMUI
{
	// Colours sampled from the game's windows.
	extern const FLinearColor Orange;
	extern const FLinearColor White;
	extern const FLinearColor Grey;
	/** Page background, like the recipe tab's list area (dark, a little see-through). */
	extern const FLinearColor PageBackground;
	/** Side panel, like the recipe tab's info panel (lighter, a little see-through). */
	extern const FLinearColor SidePanel;
	/** List areas. */
	extern const FLinearColor PanelBody;
	/** Fields and dropdowns. */
	extern const FLinearColor Field;
	/** Buttons ("+ Add Item", "Save"). */
	extern const FLinearColor Button;
	/** Options of an open dropdown list. */
	extern const FLinearColor Option;

	/** The base font with another size. */
	FSlateFontInfo SizedFont(const FSlateFontInfo& Base, int32 Size);

	/** The base font family in its "Regular" face (lighter than the game's semi-bold labels). */
	FSlateFontInfo RegularFont(const FSlateFontInfo& Base, int32 Size);

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color);

	/** A flat button with a centred text label. OutLabel (optional) gets the label, to change it later. */
	UButton* MakeButton(UWidgetTree* Tree, const FString& Label, const FSlateFontInfo& Font, const FLinearColor& Color, UTextBlock** OutLabel = nullptr);

	/** The option switch pictures (like the game's: off = outline, on = filled; disabled = grey). */
	extern const TCHAR* const SwitchOffPath;
	extern const TCHAR* const SwitchOnPath;
	extern const TCHAR* const SwitchOffDisabledPath;
	extern const TCHAR* const SwitchOnDisabledPath;

	/** An image of one of our textures, shown at Size. */
	UImage* MakeImage(UWidgetTree* Tree, const TCHAR* Path, const FVector2D& Size);

	/** Shows another of our textures in an image. */
	void SetImage(UImage* Target, const TCHAR* Path);

	/** Gives a button of MakeButton another colour. */
	void SetButtonColor(UButton* Target, const FLinearColor& Color);

	/** A dropdown-looking button: label on the left, arrow on the right. */
	UButton* MakeDropdown(UWidgetTree* Tree, const FSlateFontInfo& Font, UTextBlock*& OutLabel);

	/** A small square button showing the bin icon. */
	UButton* MakeRemoveButton(UWidgetTree* Tree, const FSlateFontInfo& Font);

	/** A flat coloured box with padding. */
	UBorder* MakeBox(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding);

	/**
	 * The box around a text input: a little darker while the player is typing in it (it has the
	 * keyboard focus). UMG text fields tell nothing about focus, so a ticker checks the registered
	 * boxes every frame (a handful at most) and tints them; boxes that are gone drop out by themselves.
	 */
	void MarkAsInputBox(UBorder* Box);
}
