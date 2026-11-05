#include "UI/AMHoldBin.h"
#include "UI/AMUIHelpers.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace
{
	const TCHAR* const AMHB_TrashPath = TEXT("/AutomatedMachines/Icons/T_AM_TrashLarge.T_AM_TrashLarge");
	constexpr float AMHB_Size = 140.f;

	UImage* AMHB_Bin(UWidgetTree* Tree, float Alpha)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AMHB_TrashPath))
		{
			Image->SetBrushFromTexture(Texture, false);
		}
		FSlateBrush Brush = Image->GetBrush();
		Brush.ImageSize = FVector2D(AMHB_Size, AMHB_Size);
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Alpha));
		return Image;
	}
}

void UAMHoldBin::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	// Full screen, the bin in the middle.
	UOverlay* Screen = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Screen;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UVerticalBox* Middle = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UOverlaySlot* MiddleSlot = Screen->AddChildToOverlay(Middle))
	{
		MiddleSlot->SetHorizontalAlignment(HAlign_Center);
		MiddleSlot->SetVerticalAlignment(VAlign_Center);
	}

	// The icon: the faint bin, and over it the solid bin seen through a box rising from the bottom.
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSize->SetWidthOverride(AMHB_Size);
	IconSize->SetHeightOverride(AMHB_Size);
	UOverlay* Icon = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	IconSize->AddChild(Icon);
	if (UVerticalBoxSlot* IconSlot = Middle->AddChildToVerticalBox(IconSize))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
	}
	Icon->AddChildToOverlay(AMHB_Bin(WidgetTree, 0.22f));

	UCanvasPanel* FillArea = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	if (UOverlaySlot* FillSlot = Icon->AddChildToOverlay(FillArea))
	{
		FillSlot->SetHorizontalAlignment(HAlign_Fill);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
	}
	mClip = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	mClip->SetClipping(EWidgetClipping::ClipToBounds);
	mClipSlot = FillArea->AddChildToCanvas(mClip);
	UCanvasPanel* Inside = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	mClip->AddChild(Inside);
	mFilled = AMHB_Bin(WidgetTree, 1.f);
	mFilledSlot = Inside->AddChildToCanvas(mFilled);

	mLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = mLabel->GetFont();
	Font.Size = 20;
	mLabel->SetFont(Font);
	mLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	mLabel->SetShadowOffset(FVector2D(1.f, 1.f));
	mLabel->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	if (UVerticalBoxSlot* LabelSlot = Middle->AddChildToVerticalBox(mLabel))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	}
	SetProgress(0.f);
}

void UAMHoldBin::SetProgress(float Progress)
{
	const float Fill = FMath::Clamp(Progress, 0.f, 1.f);
	const float Height = AMHB_Size * Fill;
	if (mClipSlot && mClip)
	{
		// The box sits on the bottom and grows upward; the solid bin inside stays where the faint one is.
		mClip->SetWidthOverride(AMHB_Size);
		mClip->SetHeightOverride(FMath::Max(Height, 0.01f));
		mClipSlot->SetPosition(FVector2D(0.f, AMHB_Size - Height));
		mClipSlot->SetSize(FVector2D(AMHB_Size, FMath::Max(Height, 0.01f)));
	}
	if (mFilledSlot && mFilled)
	{
		mFilledSlot->SetPosition(FVector2D(0.f, Height - AMHB_Size));
		mFilledSlot->SetSize(FVector2D(AMHB_Size, AMHB_Size));
		// From transparent white to solid white while it fills.
		mFilled->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.45f + 0.55f * Fill));
	}
}

void UAMHoldBin::SetLabel(const FString& Text)
{
	if (mLabel)
	{
		mLabel->SetText(FText::FromString(Text));
	}
}
