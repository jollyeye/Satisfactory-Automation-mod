#include "UI/AMUIHelpers.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"

const FLinearColor AMUI::Orange(0.98f, 0.55f, 0.13f, 1.f);
const FLinearColor AMUI::White(1.f, 1.f, 1.f, 1.f);
const FLinearColor AMUI::Grey(0.6f, 0.6f, 0.6f, 1.f);
const FLinearColor AMUI::PageBackground(0.018f, 0.018f, 0.02f, 1.f);
const FLinearColor AMUI::FieldBackground(0.06f, 0.06f, 0.065f, 1.f);

namespace
{
	const FLinearColor AMUIH_Button(0.11f, 0.11f, 0.115f, 1.f);
	const FLinearColor AMUIH_ButtonHovered(0.2f, 0.2f, 0.21f, 1.f);
	const FLinearColor AMUIH_ButtonPressed(0.07f, 0.07f, 0.075f, 1.f);
	const FLinearColor AMUIH_Accent(0.55f, 0.28f, 0.05f, 1.f);
	const FLinearColor AMUIH_AccentHovered(0.75f, 0.4f, 0.08f, 1.f);
	const FLinearColor AMUIH_AccentPressed(0.4f, 0.2f, 0.04f, 1.f);
	constexpr float AMUIH_Radius = 3.f;

	FButtonStyle AMUIH_ButtonStyle(bool bAccent)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateRoundedBoxBrush(bAccent ? AMUIH_Accent : AMUIH_Button, AMUIH_Radius));
		Style.SetHovered(FSlateRoundedBoxBrush(bAccent ? AMUIH_AccentHovered : AMUIH_ButtonHovered, AMUIH_Radius));
		Style.SetPressed(FSlateRoundedBoxBrush(bAccent ? AMUIH_AccentPressed : AMUIH_ButtonPressed, AMUIH_Radius));
		Style.SetDisabled(FSlateRoundedBoxBrush(AMUIH_ButtonPressed, AMUIH_Radius));
		Style.SetNormalPadding(FMargin(10.f, 4.f));
		Style.SetPressedPadding(FMargin(10.f, 5.f, 10.f, 3.f));
		return Style;
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

UButton* AMUI::MakeButton(UWidgetTree* Tree, const FString& Label, const FSlateFontInfo& Font, bool bAccent, UTextBlock** OutLabel)
{
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(AMUIH_ButtonStyle(bAccent));
	UTextBlock* LabelText = MakeText(Tree, Label, Font, White);
	Button->AddChild(LabelText);
	if (OutLabel)
	{
		*OutLabel = LabelText;
	}
	return Button;
}

UBorder* AMUI::MakeBox(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding)
{
	UBorder* Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(FSlateRoundedBoxBrush(Color, AMUIH_Radius));
	Box->SetPadding(Padding);
	return Box;
}
