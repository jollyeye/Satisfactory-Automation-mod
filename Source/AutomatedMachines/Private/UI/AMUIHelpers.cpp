#include "UI/AMUIHelpers.h"
#include "Containers/Ticker.h"
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
#include "Styling/SlateBrush.h"

// FColor values are the game's sRGB greys; FLinearColor(FColor) converts them.
const FLinearColor AMUI::Orange(FColor(250, 149, 73));
const FLinearColor AMUI::White(FColor(255, 255, 255));
const FLinearColor AMUI::Grey(FColor(170, 170, 170));
const FLinearColor AMUI::PageBackground(FColor(15, 15, 15, 240));
const FLinearColor AMUI::SidePanel(FColor(40, 41, 44, 235));
const FLinearColor AMUI::PanelBody(FColor(67, 67, 68, 230));
const FLinearColor AMUI::Field(FColor(87, 87, 87));
const FLinearColor AMUI::Button(FColor(102, 102, 103));
const FLinearColor AMUI::Option(FColor(110, 110, 111));

const TCHAR* const AMUI::SwitchOffPath = TEXT("/AutomatedMachines/Icons/T_AM_SwitchOff.T_AM_SwitchOff");
const TCHAR* const AMUI::SwitchOnPath = TEXT("/AutomatedMachines/Icons/T_AM_SwitchOn.T_AM_SwitchOn");
const TCHAR* const AMUI::SwitchOffDisabledPath = TEXT("/AutomatedMachines/Icons/T_AM_SwitchOffDisabled.T_AM_SwitchOffDisabled");
const TCHAR* const AMUI::SwitchOnDisabledPath = TEXT("/AutomatedMachines/Icons/T_AM_SwitchOnDisabled.T_AM_SwitchOnDisabled");

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

FSlateFontInfo AMUI::RegularFont(const FSlateFontInfo& Base, int32 Size)
{
	FSlateFontInfo Font = SizedFont(Base, Size);
	Font.TypefaceFontName = TEXT("Regular");
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

UImage* AMUI::MakeImage(UWidgetTree* Tree, const TCHAR* Path, const FVector2D& Size)
{
	UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	SetImage(Image, Path);
	// The brush keeps this size when the texture is changed (SetImage), so wide pictures stay wide.
	FSlateBrush Brush = Image->GetBrush();
	Brush.ImageSize = Size;
	Image->SetBrush(Brush);
	Image->SetDesiredSizeOverride(Size);
	return Image;
}

void AMUI::SetImage(UImage* Target, const TCHAR* Path)
{
	if (Target)
	{
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path))
		{
			Target->SetBrushFromTexture(Texture, false);
		}
	}
}

void AMUI::SetButtonColor(UButton* Target, const FLinearColor& Color)
{
	if (Target)
	{
		Target->SetStyle(AMUIH_ButtonStyle(Color));
	}
}

void AMUI::SetButtonSeeThrough(UButton* Target, const FLinearColor& Color)
{
	if (!Target)
	{
		return;
	}
	FButtonStyle Style = Target->GetStyle();
	Style.SetNormal(FSlateNoResource());
	Style.SetHovered(FSlateColorBrush(AMUIH_Shade(Color, 1.6f)));
	Style.SetPressed(FSlateColorBrush(AMUIH_Shade(Color, 0.7f)));
	Target->SetStyle(Style);
}

UButton* AMUI::MakeDropdown(UWidgetTree* Tree, const FSlateFontInfo& Font, UTextBlock*& OutLabel)
{
	UButton* Widget = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	Widget->SetStyle(AMUIH_ButtonStyle(Field));

	UHorizontalBox* Content = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	OutLabel = MakeText(Tree, TEXT(""), Font, White);
	// A long choice ends in "..." before the arrow instead of running over it.
	OutLabel->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	if (UHorizontalBoxSlot* LabelSlot = Content->AddChildToHorizontalBox(OutLabel))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ArrowSlot = Content->AddChildToHorizontalBox(AMUIH_Icon(Tree, AMUIH_DropArrowPath, 11.f)))
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
	Style.SetNormalPadding(FMargin(6.f, 6.f));
	Style.SetPressedPadding(FMargin(6.f, 7.f, 6.f, 5.f));
	Widget->SetStyle(Style);
	USizeBox* IconSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSize->SetWidthOverride(21.f);
	IconSize->SetHeightOverride(21.f);
	IconSize->AddChild(AMUIH_Icon(Tree, AMUIH_TrashPath, 21.f));
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

namespace
{
	/**
	 * How dark an input box gets while typing in it (its colour times this): the field grey 87
	 * becomes about 26, darker than the side panel (40), the darkest thing of ours, but not black.
	 */
	constexpr float AMUIH_TypingShade = 0.3f;

	struct FAMUIHInputBox
	{
		TWeakObjectPtr<UBorder> Box;
		bool bDark = false;
	};
	TArray<FAMUIHInputBox> AMUIH_InputBoxes;
	FTSTicker::FDelegateHandle AMUIH_InputTicker;

	bool AMUIH_TickInputBoxes(float DeltaTime)
	{
		for (int32 Index = AMUIH_InputBoxes.Num() - 1; Index >= 0; --Index)
		{
			FAMUIHInputBox& Entry = AMUIH_InputBoxes[Index];
			UBorder* Box = Entry.Box.Get();
			if (!Box)
			{
				AMUIH_InputBoxes.RemoveAtSwap(Index);
				continue;
			}
			const bool bTyping = Box->HasFocusedDescendants();
			if (bTyping != Entry.bDark)
			{
				Entry.bDark = bTyping;
				Box->SetBrushColor(bTyping ? FLinearColor(AMUIH_TypingShade, AMUIH_TypingShade, AMUIH_TypingShade, 1.f) : FLinearColor::White);
			}
		}
		return true;
	}
}

void AMUI::MarkAsInputBox(UBorder* Box)
{
	if (!Box)
	{
		return;
	}
	AMUIH_InputBoxes.Add({Box, false});
	if (!AMUIH_InputTicker.IsValid())
	{
		AMUIH_InputTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&AMUIH_TickInputBoxes), 0.f);
	}
}
