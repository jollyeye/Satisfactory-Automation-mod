#include "UI/AMRuleRow.h"
#include "UI/AMAutomationTab.h"
#include "UI/AMUIHelpers.h"
#include "Automation/AMTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
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
	constexpr float AMRR_RemoveWidth = 34.f;

	/** A fixed-width cell. */
	USizeBox* AMRR_Cell(UWidgetTree* Tree, float Width, UWidget* Content)
	{
		USizeBox* Cell = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(Width);
		Cell->AddChild(Content);
		return Cell;
	}

	/** A flat button whose label sits on the left, like a table cell. */
	UButton* AMRR_CellButton(UWidgetTree* Tree, const FSlateFontInfo& Font, UTextBlock*& OutLabel)
	{
		UTextBlock* Label = nullptr;
		UButton* Button = AMUI::MakeButton(Tree, TEXT(""), Font, false, &Label);
		if (UButtonSlot* LabelSlot = Label ? Cast<UButtonSlot>(Label->Slot) : nullptr)
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Left);
		}
		OutLabel = Label;
		return Button;
	}
}

void UAMChoice::OnClicked()
{
	if (Row)
	{
		Row->Choose(*this);
	}
}

void UAMChoice::OnHovered()
{
	if (Row)
	{
		Row->Explain(*this);
	}
}

UWidget* UAMRuleRow::GetRootWidget() const
{
	return mRoot;
}

void UAMRuleRow::Build(UAMAutomationTab* InTab, bool bInIsAlarm, int32 InIndex)
{
	mTab = InTab;
	bIsAlarm = bInIsAlarm;
	mIndex = InIndex;

	UWidgetTree* Tree = mTab->GetTree();
	const FSlateFontInfo Font = AMUI::SizedFont(mTab->GetGameFont(), AMRowLayout::FontSize);

	mRoot = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	mRoot->AddChildToVerticalBox(Line);

	auto Add = [Line](UWidget* Widget, float LeftPadding)
	{
		if (UHorizontalBoxSlot* LineSlot = Line->AddChildToHorizontalBox(Widget))
		{
			LineSlot->SetPadding(FMargin(LeftPadding, 0.f, 0.f, 0.f));
			LineSlot->SetVerticalAlignment(VAlign_Fill);
		}
	};

	UTextBlock* SecondLabel = nullptr;
	UButton* SecondButton = AMRR_CellButton(Tree, Font, SecondLabel);
	mSecondLabel = SecondLabel;
	SecondButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnSecondCellClicked);

	if (bIsAlarm)
	{
		// | ALARM NAME | MACHINE EVENT |
		UBorder* Field = AMUI::MakeBox(Tree, AMUI::FieldBackground, FMargin(8.f, 5.f));
		mAlarmNameText = Tree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
		mAlarmNameText->SetFont(Font);
		mAlarmNameText->SetHintText(FText::FromString(TEXT("ALARM NAME")));
		mAlarmNameText->OnTextChanged.AddDynamic(this, &UAMRuleRow::OnAlarmNameChanged);
		mAlarmNameText->OnTextCommitted.AddDynamic(this, &UAMRuleRow::OnAlarmNameCommitted);
		Field->AddChild(mAlarmNameText);
		Add(AMRR_Cell(Tree, AMRowLayout::AlarmName, Field), 0.f);
		Add(AMRR_Cell(Tree, AMRowLayout::MachineEvent, SecondButton), AMRowLayout::Gap);
	}
	else
	{
		// | DO | ON ALARM |
		Add(AMRR_Cell(Tree, AMRowLayout::Do, SecondButton), 0.f);
		UTextBlock* AlarmLabel = nullptr;
		UButton* AlarmButton = AMRR_CellButton(Tree, Font, AlarmLabel);
		mAlarmLabel = AlarmLabel;
		AlarmButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnAlarmCellClicked);
		Add(AMRR_Cell(Tree, AMRowLayout::OnAlarm, AlarmButton), AMRowLayout::Gap);
	}

	UButton* RemoveButton = AMUI::MakeButton(Tree, TEXT("X"), Font, false, nullptr);
	RemoveButton->SetToolTipText(FText::FromString(TEXT("Remove")));
	RemoveButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnRemoveClicked);
	Add(AMRR_Cell(Tree, AMRR_RemoveWidth, RemoveButton), AMRowLayout::Gap);

	mChoices = Tree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	mChoices->SetInnerSlotPadding(FVector2D(6.f, 6.f));
	mChoices->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ChoicesSlot = mRoot->AddChildToVerticalBox(mChoices))
	{
		ChoicesSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
	}

	UpdateLabels();
}

void UAMRuleRow::UpdateLabels()
{
	if (bIsAlarm)
	{
		if (const FAMFireRule* Rule = mTab->GetFireRule(mIndex))
		{
			SetAlarmNameText(Rule->Event);
			if (mSecondLabel)
			{
				mSecondLabel->SetText(AMRules::GetConditionText(Rule->Condition));
			}
		}
	}
	else if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		if (mSecondLabel)
		{
			mSecondLabel->SetText(AMRules::GetActionText(Rule->Action));
		}
		if (mAlarmLabel)
		{
			mAlarmLabel->SetText(FText::FromString(Rule->Event.IsEmpty() ? TEXT("choose an alarm") : Rule->Event));
			mAlarmLabel->SetColorAndOpacity(FSlateColor(Rule->Event.IsEmpty() ? AMUI::Grey : AMUI::White));
		}
	}
}

void UAMRuleRow::ShowChoices(EAMChoiceKind Kind, const TArray<FString>& Alarms, const FString& EmptyMessage)
{
	mChoices->ClearChildren();
	mChoiceHandlers.Reset();
	UWidgetTree* Tree = mTab->GetTree();
	const FSlateFontInfo Font = AMUI::SizedFont(mTab->GetGameFont(), AMRowLayout::FontSize - 1);

	auto AddChoice = [this, Tree, &Font, Kind](const FString& Label, const FString& Alarm, uint8 Value, bool bCurrent)
	{
		UAMChoice* Handler = NewObject<UAMChoice>(this);
		Handler->Row = this;
		Handler->Kind = Kind;
		Handler->Alarm = Alarm;
		Handler->Value = Value;
		mChoiceHandlers.Add(Handler);

		UButton* Button = AMUI::MakeButton(Tree, Label, Font, bCurrent, nullptr);
		Button->OnClicked.AddDynamic(Handler, &UAMChoice::OnClicked);
		Button->OnHovered.AddDynamic(Handler, &UAMChoice::OnHovered);
		mChoices->AddChildToWrapBox(Button);
	};

	if (Kind == EAMChoiceKind::MachineEvent)
	{
		const FAMFireRule* Rule = mTab->GetFireRule(mIndex);
		for (uint8 Value = 0; Value < static_cast<uint8>(EAMCondition::Count); ++Value)
		{
			const EAMCondition Condition = static_cast<EAMCondition>(Value);
			AddChoice(AMRules::GetConditionText(Condition).ToString(), FString(), Value, Rule && Rule->Condition == Condition);
		}
	}
	else if (Kind == EAMChoiceKind::Action)
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		for (uint8 Value = 0; Value < static_cast<uint8>(EAMAction::Count); ++Value)
		{
			const EAMAction Action = static_cast<EAMAction>(Value);
			AddChoice(AMRules::GetActionText(Action).ToString(), FString(), Value, Rule && Rule->Action == Action);
		}
	}
	else
	{
		if (Alarms.Num() == 0)
		{
			if (EmptyMessage.IsEmpty())
			{
				HideChoices();
				return;
			}
			mChoices->AddChildToWrapBox(AMUI::MakeText(Tree, EmptyMessage, Font, AMUI::Grey));
		}
		for (const FString& Alarm : Alarms)
		{
			AddChoice(Alarm, Alarm, 0, false);
		}
	}

	bChoicesOpen = true;
	mOpenKind = Kind;
	mChoices->SetVisibility(ESlateVisibility::Visible);
}

void UAMRuleRow::HideChoices()
{
	mChoices->ClearChildren();
	mChoiceHandlers.Reset();
	mChoices->SetVisibility(ESlateVisibility::Collapsed);
	bChoicesOpen = false;
}

void UAMRuleRow::SetAlarmNameText(const FString& Text)
{
	if (mAlarmNameText)
	{
		TGuardValue<bool> Guard(bSettingText, true);
		mAlarmNameText->SetText(FText::FromString(Text));
	}
}

void UAMRuleRow::OnAlarmNameChanged(const FText& Text)
{
	if (bSettingText)
	{
		return;
	}
	// Suggestions: existing alarms containing what was typed (any case), the closest first.
	const FString Typed = AMRules::NormalizeEventName(Text.ToString());
	mTab->ShowAlarmInfo(Typed);
	if (Typed.IsEmpty())
	{
		HideChoices();
		return;
	}
	TArray<FString> StartsWith;
	TArray<FString> Contains;
	for (const FString& Alarm : mTab->GetEventNames())
	{
		if (Alarm == Typed)
		{
			continue;
		}
		if (Alarm.StartsWith(Typed))
		{
			StartsWith.Add(Alarm);
		}
		else if (Alarm.Contains(Typed))
		{
			Contains.Add(Alarm);
		}
	}
	StartsWith.Append(Contains);
	if (StartsWith.Num() > AMRR_MaxSuggestions)
	{
		StartsWith.SetNum(AMRR_MaxSuggestions);
	}
	ShowChoices(EAMChoiceKind::Alarm, StartsWith, FString());
}

void UAMRuleRow::OnAlarmNameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	CommitAlarmName(Text.ToString());
	// Keep the suggestions when focus moved away: the player may be clicking one of them right now.
	if (CommitMethod != ETextCommit::OnUserMovedFocus && mOpenKind == EAMChoiceKind::Alarm)
	{
		HideChoices();
	}
}

void UAMRuleRow::CommitAlarmName(const FString& Typed)
{
	FAMFireRule* Rule = mTab->GetFireRule(mIndex);
	if (!Rule)
	{
		return;
	}
	const FString Name = AMRules::NormalizeEventName(Typed);
	if (mAlarmNameText && mAlarmNameText->GetText().ToString() != Name)
	{
		SetAlarmNameText(Name);
	}
	if (Rule->Event != Name)
	{
		Rule->Event = Name;
		mTab->OnRulesEdited();
	}
}

void UAMRuleRow::Choose(const UAMChoice& Choice)
{
	switch (Choice.Kind)
	{
	case EAMChoiceKind::Alarm:
		if (bIsAlarm)
		{
			CommitAlarmName(Choice.Alarm);
		}
		else if (FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
		{
			if (Rule->Event != Choice.Alarm)
			{
				Rule->Event = Choice.Alarm;
				mTab->OnRulesEdited();
			}
		}
		break;
	case EAMChoiceKind::MachineEvent:
		if (FAMFireRule* Rule = mTab->GetFireRule(mIndex))
		{
			const EAMCondition Condition = static_cast<EAMCondition>(Choice.Value);
			if (Rule->Condition != Condition)
			{
				Rule->Condition = Condition;
				mTab->OnRulesEdited();
			}
		}
		break;
	case EAMChoiceKind::Action:
		if (FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
		{
			const EAMAction Action = static_cast<EAMAction>(Choice.Value);
			if (Rule->Action != Action)
			{
				Rule->Action = Action;
				mTab->OnRulesEdited();
			}
		}
		break;
	}
	UpdateLabels();
	HideChoices();
	Explain(Choice);
}

void UAMRuleRow::Explain(const UAMChoice& Choice)
{
	switch (Choice.Kind)
	{
	case EAMChoiceKind::Alarm:
		mTab->ShowAlarmInfo(Choice.Alarm);
		break;
	case EAMChoiceKind::MachineEvent:
	{
		const EAMCondition Condition = static_cast<EAMCondition>(Choice.Value);
		mTab->ShowInfo(AMRules::GetConditionText(Condition), AMRules::GetConditionDescription(Condition));
		break;
	}
	case EAMChoiceKind::Action:
	{
		const EAMAction Action = static_cast<EAMAction>(Choice.Value);
		mTab->ShowInfo(AMRules::GetActionText(Action), AMRules::GetActionDescription(Action));
		break;
	}
	}
}

void UAMRuleRow::ExplainCurrent()
{
	if (bIsAlarm)
	{
		if (const FAMFireRule* Rule = mTab->GetFireRule(mIndex))
		{
			mTab->ShowInfo(AMRules::GetConditionText(Rule->Condition), AMRules::GetConditionDescription(Rule->Condition));
		}
	}
	else if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		mTab->ShowInfo(AMRules::GetActionText(Rule->Action), AMRules::GetActionDescription(Rule->Action));
	}
}

void UAMRuleRow::OnSecondCellClicked()
{
	const EAMChoiceKind Kind = bIsAlarm ? EAMChoiceKind::MachineEvent : EAMChoiceKind::Action;
	if (bChoicesOpen && mOpenKind == Kind)
	{
		HideChoices();
		return;
	}
	ExplainCurrent();
	ShowChoices(Kind, {}, FString());
}

void UAMRuleRow::OnAlarmCellClicked()
{
	if (bChoicesOpen && mOpenKind == EAMChoiceKind::Alarm)
	{
		HideChoices();
		return;
	}
	if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		mTab->ShowAlarmInfo(Rule->Event);
	}
	ShowChoices(EAMChoiceKind::Alarm, mTab->GetEventNames(), TEXT("No alarms yet: give a machine an alarm first."));
}

void UAMRuleRow::OnRemoveClicked()
{
	// The tab rebuilds all rows, this one included.
	mTab->RemoveRule(bIsAlarm, mIndex);
}
