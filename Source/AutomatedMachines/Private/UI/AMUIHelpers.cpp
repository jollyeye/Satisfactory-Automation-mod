#include "UI/AMUIHelpers.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

// FColor values are the game's sRGB greys; FLinearColor(FColor) converts them.
const FLinearColor AMUI::Orange(FColor(250, 149, 73));
const FLinearColor AMUI::White(FColor(255, 255, 255));
const FLinearColor AMUI::Grey(FColor(170, 170, 170));
const FLinearColor AMUI::PageBackground(FColor(15, 15, 15, 240));
const FLinearColor AMUI::SidePanel(FColor(56, 57, 61, 235));
const FLinearColor AMUI::PanelBody(FColor(67, 67, 68, 230));
const FLinearColor AMUI::Field(FColor(87, 87, 87));
const FLinearColor AMUI::Button(FColor(102, 102, 103));
const FLinearColor AMUI::Option(FColor(110, 110, 111));

namespace
{
	const TCHAR* const AMUIH_DropArrowPath = TEXT("/AutomatedMachines/Icons/T_AM_DropArrow.T_AM_DropArrow");
	const TCHAR* const AMUIH_TrashPath = TEXT("/AutomatedMachines/Icons/T_AM_Trash.T_AM_Trash");

	/** Lighter / darker shade of a grey for hover / press. */
	FLinearColor AMUIH_Shade(const FLinearColor& Color, float Factor)
	{
		return FLinearColor(Color.R * Factor, Color.G * Factor, Color.B * Factor, Color.A);
	}

	FButtonStyle AMUIH_ButtonStyle(const FLinearColor& Color)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateColorBrush(Color));
		Style.SetHovered(FSlateColorBrush(AMUIH_Shade(Color, 1.6f)));
		Style.SetPressed(FSlateColorBrush(AMUIH_Shade(Color, 0.7f)));
		Style.SetDisabled(FSlateColorBrush(AMUIH_Shade(Color, 0.6f)));
		Style.SetNormalPadding(FMargin(12.f, 7.f));
		Style.SetPressedPadding(FMargin(12.f, 8.f, 12.f, 6.f));
		return Style;
	}

	UImage* AMUIH_Icon(UWidgetTree* Tree, const TCHAR* Path, float Size)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path))
		{
			Image->SetBrushFromTexture(Texture, false);
		}
		Image->SetDesiredSizeOverride(FVector2D(Size, Size));
		return Image;
	}
}

FSlateFontInfo AMUI::SizedFont(const FSlateFontInfo& Base, int32 Size)
{
	FSlateFontInfo Font = Base;
	Font.Size = Size;
	return Font;
}

UTextBlock* AMUI::MakeText(UWidgetTree* Tree, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetFont(Font);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	return TextBlock;
}

UButton* AMUI::MakeButton(UWidgetTree* Tree, const FString& Label, const FSlateFontInfo& Font, const FLinearColor& Color, UTextBlock** OutLabel)
{
	UButton* Widget = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	Widget->SetStyle(AMUIH_ButtonStyle(Color));
	UTextBlock* LabelText = MakeText(Tree, Label, Font, White);
	Widget->AddChild(LabelText);
	if (OutLabel)
	{
		*OutLabel = LabelText;
	}
	return Widget;
}

UButton* AMUI::MakeDropdown(UWidgetTree* Tree, const FSlateFontInfo& Font, UTextBlock*& OutLabel)
{
	UButton* Widget = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	Widget->SetStyle(AMUIH_ButtonStyle(Field));

	UHorizontalBox* Content = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	OutLabel = MakeText(Tree, TEXT(""), Font, White);
	if (UHorizontalBoxSlot* LabelSlot = Content->AddChildToHorizontalBox(OutLabel))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ArrowSlot = Content->AddChildToHorizontalBox(AMUIH_Icon(Tree, AMUIH_DropArrowPath, 14.f)))
	{
		ArrowSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		ArrowSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Widget->AddChild(Content)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Widget;
}

UButton* AMUI::MakeRemoveButton(UWidgetTree* Tree, const FSlateFontInfo& Font)
{
	UButton* Widget = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = AMUIH_ButtonStyle(Field);
	Style.SetNormalPadding(FMargin(8.f, 7.f));
	Style.SetPressedPadding(FMargin(8.f, 8.f, 8.f, 6.f));
	Widget->SetStyle(Style);
	USizeBox* IconSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSize->SetWidthOverride(16.f);
	IconSize->SetHeightOverride(16.f);
	IconSize->AddChild(AMUIH_Icon(Tree, AMUIH_TrashPath, 16.f));
	if (UButtonSlot* IconSlot = Cast<UButtonSlot>(Widget->AddChild(IconSize)))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}
	Widget->SetToolTipText(FText::FromString(TEXT("Remove")));
	return Widget;
}

UBorder* AMUI::MakeBox(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding)
{
	UBorder* Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(FSlateColorBrush(Color));
	Box->SetPadding(Padding);
	return Box;
}
