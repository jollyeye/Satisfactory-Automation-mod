#include "UI/AMRuleRow.h"
#include "UI/AMAutomationTab.h"
#include "UI/AMUIHelpers.h"
#include "Automation/AMTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

namespace
{
	constexpr int32 AMRR_MaxSuggestions = 6;
	constexpr int32 AMRR_FontSize = 14;
	constexpr float AMRR_EventFieldWidth = 260.f;
}

void UAMEventChoice::OnClicked()
{
	if (Row)
	{
		Row->ChooseEvent(Event);
	}
}

UWidget* UAMRuleRow::GetRootWidget() const
{
	return mRoot;
}

void UAMRuleRow::Build(UAMAutomationTab* InTab, bool bInIsFire, int32 InIndex)
{
	mTab = InTab;
	bIsFire = bInIsFire;
	mIndex = InIndex;

	UWidgetTree* Tree = mTab->GetTree();
	const FSlateFontInfo Font = AMUI::SizedFont(mTab->GetGameFont(), AMRR_FontSize);

	mRoot = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	mRoot->AddChildToVerticalBox(Line);

	auto Add = [Line](UWidget* Widget, float LeftPadding)
	{
		if (UHorizontalBoxSlot* LineSlot = Line->AddChildToHorizontalBox(Widget))
		{
			LineSlot->SetPadding(FMargin(LeftPadding, 0.f, 0.f, 0.f));
			LineSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	UButton* CycleButton = AMUI::MakeButton(Tree, TEXT(""), Font, false, nullptr);
	mCycleLabel = Cast<UTextBlock>(CycleButton->GetChildAt(0));
	CycleButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnCycleClicked);

	if (bIsFire)
	{
		// Fire [EVENT NAME] when [condition]
		Add(AMUI::MakeText(Tree, TEXT("Ring"), Font, AMUI::Grey), 0.f);

		UBorder* Field = AMUI::MakeBox(Tree, AMUI::FieldBackground, FMargin(8.f, 5.f));
		USizeBox* FieldSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FieldSize->SetWidthOverride(AMRR_EventFieldWidth);
		FieldSize->AddChild(Field);
		mEventText = Tree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
		mEventText->SetFont(Font);
		mEventText->SetHintText(FText::FromString(TEXT("ALARM NAME")));
		mEventText->OnTextChanged.AddDynamic(this, &UAMRuleRow::OnEventTextChanged);
		mEventText->OnTextCommitted.AddDynamic(this, &UAMRuleRow::OnEventTextCommitted);
		Field->AddChild(mEventText);
		Add(FieldSize, 10.f);

		Add(AMUI::MakeText(Tree, TEXT("when"), Font, AMUI::Grey), 10.f);
		Add(CycleButton, 10.f);
	}
	else
	{
		// [action] when event [EVENT]
		Add(CycleButton, 0.f);
		Add(AMUI::MakeText(Tree, TEXT("when alarm"), Font, AMUI::Grey), 10.f);
		UButton* PickButton = AMUI::MakeButton(Tree, TEXT(""), Font, false, nullptr);
		mPickLabel = Cast<UTextBlock>(PickButton->GetChildAt(0));
		PickButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnPickEventClicked);
		Add(PickButton, 10.f);
	}

	UButton* RemoveButton = AMUI::MakeButton(Tree, TEXT("X"), Font, false, nullptr);
	RemoveButton->SetToolTipText(FText::FromString(TEXT("Remove")));
	RemoveButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnRemoveClicked);
	Add(RemoveButton, 14.f);

	mChoices = Tree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	mChoices->SetInnerSlotPadding(FVector2D(6.f, 6.f));
	mChoices->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ChoicesSlot = mRoot->AddChildToVerticalBox(mChoices))
	{
		ChoicesSlot->SetPadding(FMargin(bIsFire ? 44.f : 0.f, 6.f, 0.f, 0.f));
	}

	UpdateLabels();
}

void UAMRuleRow::UpdateLabels()
{
	if (bIsFire)
	{
		if (const FAMFireRule* Rule = mTab->GetFireRule(mIndex))
		{
			SetEventText(Rule->Event);
			if (mCycleLabel)
			{
				mCycleLabel->SetText(AMRules::GetConditionText(Rule->Condition));
			}
		}
	}
	else if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		if (mCycleLabel)
		{
			mCycleLabel->SetText(AMRules::GetActionText(Rule->Action));
		}
		if (mPickLabel)
		{
			mPickLabel->SetText(FText::FromString(Rule->Event.IsEmpty() ? TEXT("choose an alarm") : Rule->Event));
			mPickLabel->SetColorAndOpacity(FSlateColor(Rule->Event.IsEmpty() ? AMUI::Grey : AMUI::White));
		}
	}
}

void UAMRuleRow::ShowChoices(const TArray<FString>& Events, const FString& EmptyMessage)
{
	mChoices->ClearChildren();
	mChoiceHandlers.Reset();
	UWidgetTree* Tree = mTab->GetTree();
	const FSlateFontInfo Font = AMUI::SizedFont(mTab->GetGameFont(), AMRR_FontSize - 1);

	if (Events.Num() == 0)
	{
		if (EmptyMessage.IsEmpty())
		{
			HideChoices();
			return;
		}
		mChoices->AddChildToWrapBox(AMUI::MakeText(Tree, EmptyMessage, Font, AMUI::Grey));
	}
	for (const FString& Event : Events)
	{
		UAMEventChoice* Handler = NewObject<UAMEventChoice>(this);
		Handler->Row = this;
		Handler->Event = Event;
		mChoiceHandlers.Add(Handler);

		UButton* Button = AMUI::MakeButton(Tree, Event, Font, true, nullptr);
		Button->OnClicked.AddDynamic(Handler, &UAMEventChoice::OnClicked);
		mChoices->AddChildToWrapBox(Button);
	}
	mChoices->SetVisibility(ESlateVisibility::Visible);
}

void UAMRuleRow::HideChoices()
{
	mChoices->ClearChildren();
	mChoiceHandlers.Reset();
	mChoices->SetVisibility(ESlateVisibility::Collapsed);
}

void UAMRuleRow::SetEventText(const FString& Text)
{
	if (mEventText)
	{
		TGuardValue<bool> Guard(bSettingText, true);
		mEventText->SetText(FText::FromString(Text));
	}
}

void UAMRuleRow::OnEventTextChanged(const FText& Text)
{
	if (bSettingText)
	{
		return;
	}
	// Suggestions: existing events containing what was typed (any case), the closest first.
	const FString Typed = AMRules::NormalizeEventName(Text.ToString());
	if (Typed.IsEmpty())
	{
		HideChoices();
		return;
	}
	TArray<FString> StartsWith;
	TArray<FString> Contains;
	for (const FString& Event : mTab->GetEventNames())
	{
		if (Event == Typed)
		{
			continue;
		}
		if (Event.StartsWith(Typed))
		{
			StartsWith.Add(Event);
		}
		else if (Event.Contains(Typed))
		{
			Contains.Add(Event);
		}
	}
	StartsWith.Append(Contains);
	if (StartsWith.Num() > AMRR_MaxSuggestions)
	{
		StartsWith.SetNum(AMRR_MaxSuggestions);
	}
	ShowChoices(StartsWith, FString());
}

void UAMRuleRow::OnEventTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	CommitEventName(Text.ToString());
	// Keep the suggestions when focus moved away: the player may be clicking one of them right now.
	if (CommitMethod != ETextCommit::OnUserMovedFocus)
	{
		HideChoices();
	}
}

void UAMRuleRow::CommitEventName(const FString& Typed)
{
	FAMFireRule* Rule = mTab->GetFireRule(mIndex);
	if (!Rule)
	{
		return;
	}
	const FString Name = AMRules::NormalizeEventName(Typed);
	if (mEventText && mEventText->GetText().ToString() != Name)
	{
		SetEventText(Name);
	}
	if (Rule->Event != Name)
	{
		Rule->Event = Name;
		mTab->OnRulesEdited();
	}
}

void UAMRuleRow::ChooseEvent(const FString& Event)
{
	if (bIsFire)
	{
		CommitEventName(Event);
	}
	else if (FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		if (Rule->Event != Event)
		{
			Rule->Event = Event;
			mTab->OnRulesEdited();
		}
		UpdateLabels();
	}
	HideChoices();
}

void UAMRuleRow::OnCycleClicked()
{
	if (bIsFire)
	{
		if (FAMFireRule* Rule = mTab->GetFireRule(mIndex))
		{
			const uint8 Next = (static_cast<uint8>(Rule->Condition) + 1) % static_cast<uint8>(EAMCondition::Count);
			Rule->Condition = static_cast<EAMCondition>(Next);
		}
	}
	else if (FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		const uint8 Next = (static_cast<uint8>(Rule->Action) + 1) % static_cast<uint8>(EAMAction::Count);
		Rule->Action = static_cast<EAMAction>(Next);
	}
	UpdateLabels();
	mTab->OnRulesEdited();
}

void UAMRuleRow::OnPickEventClicked()
{
	if (mChoices->GetVisibility() == ESlateVisibility::Visible)
	{
		HideChoices();
		return;
	}
	ShowChoices(mTab->GetEventNames(), TEXT("No alarms yet: give a machine an alarm first."));
}

void UAMRuleRow::OnRemoveClicked()
{
	// The tab rebuilds all rows, this one included.
	mTab->RemoveRule(bIsFire, mIndex);
}
