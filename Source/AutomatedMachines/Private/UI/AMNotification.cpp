#include "UI/AMNotification.h"
#include "AutomatedMachinesModule.h"
#include "UI/AMUIHelpers.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FGHUD.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/FGGameUI.h"

namespace
{
	const TCHAR* const AMNT_IconPath = TEXT("/AutomatedMachines/Icons/T_AM_TabIcon.T_AM_TabIcon");

	/** Seconds a notification stays. */
	constexpr float AMNT_Lifetime = 7.f;

	constexpr int32 AMNT_TitleSize = 15;
	constexpr int32 AMNT_BodySize = 19;

	/** A text block of the game's HUD, for the game's font. */
	bool AMNT_FindGameFont(const UUserWidget* GameUI, FSlateFontInfo& OutFont)
	{
		if (!GameUI || !GameUI->WidgetTree)
		{
			return false;
		}
		TArray<UWidget*> Widgets;
		GameUI->WidgetTree->GetAllWidgets(Widgets);
		for (const UWidget* Widget : Widgets)
		{
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				OutFont = Text->GetFont();
				return true;
			}
		}
		return false;
	}
}

void UAMNotification::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// [ icon | ALARM NAME     ]
	// [      | the text       ]
	UBorder* Box = AMUI::MakeBox(WidgetTree, AMUI::PageBackground, FMargin(14.f, 10.f, 18.f, 10.f));
	WidgetTree->RootWidget = Box;
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Box->AddChild(Line);

	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconSize->SetWidthOverride(34.f);
	IconSize->SetHeightOverride(34.f);
	UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AMNT_IconPath))
	{
		Icon->SetBrushFromTexture(Texture, false);
	}
	Icon->SetColorAndOpacity(AMUI::Orange);
	IconSize->AddChild(Icon);
	if (UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(IconSize))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* TextsSlot = Line->AddChildToHorizontalBox(Texts))
	{
		TextsSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
		TextsSlot->SetVerticalAlignment(VAlign_Center);
	}
	mTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	mTitle->SetColorAndOpacity(FSlateColor(AMUI::Orange));
	Texts->AddChildToVerticalBox(mTitle);
	mBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	mBody->SetColorAndOpacity(FSlateColor(AMUI::White));
	mBody->SetAutoWrapText(true);
	if (UVerticalBoxSlot* BodySlot = Texts->AddChildToVerticalBox(mBody))
	{
		BodySlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}
}

void UAMNotification::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UAMNotification::Expire), AMNT_Lifetime, false);
	}
}

void UAMNotification::Expire()
{
	RemoveFromParent();
}

void UAMNotification::SetContent(const FString& Title, const FString& Body, const FSlateFontInfo& GameFont)
{
	if (mTitle && mBody)
	{
		mTitle->SetFont(AMUI::SizedFont(GameFont, AMNT_TitleSize));
		mTitle->SetText(FText::FromString(Title));
		mBody->SetFont(AMUI::RegularFont(GameFont, AMNT_BodySize));
		mBody->SetText(FText::FromString(Body));
	}
}

void UAMNotification::Show(APlayerController* Player, const FString& Title, const FString& Body)
{
	const AFGHUD* HUD = Player ? Cast<AFGHUD>(Player->GetHUD()) : nullptr;
	UFGGameUI* GameUI = HUD ? HUD->GetGameUI() : nullptr;
	if (!GameUI)
	{
		return;
	}
	FSlateFontInfo GameFont = GetDefault<UTextBlock>()->GetFont();
	AMNT_FindGameFont(GameUI, GameFont);

	UAMNotification* Notification = CreateWidget<UAMNotification>(Player, UAMNotification::StaticClass());
	if (!Notification)
	{
		GameUI->ShowTextNotification(FText::FromString(Title + TEXT(": ") + Body));
		return;
	}
	Notification->SetContent(Title, Body, GameFont);
	GameUI->PushNotificationWidget(Notification);

	// If the game did not put it anywhere, show the plain text notification instead.
	if (UWorld* World = Player->GetWorld())
	{
		TWeakObjectPtr<UAMNotification> WeakNotification(Notification);
		TWeakObjectPtr<UFGGameUI> WeakGameUI(GameUI);
		const FString Fallback = Title + TEXT(": ") + Body;
		World->GetTimerManager().SetTimerForNextTick([WeakNotification, WeakGameUI, Fallback]()
		{
			const UAMNotification* Shown = WeakNotification.Get();
			const bool bOnScreen = Shown && (Shown->GetParent() || Shown->IsInViewport());
			UE_LOG(LogAutomatedMachines, Log, TEXT("Notification %s"), bOnScreen ? TEXT("shown in the game's list") : TEXT("not taken: plain text instead"));
			if (!bOnScreen && WeakGameUI.IsValid())
			{
				WeakGameUI->ShowTextNotification(FText::FromString(Fallback));
			}
		});
	}
}
