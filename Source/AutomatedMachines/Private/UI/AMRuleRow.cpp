#include "UI/AMRuleRow.h"
#include "UI/AMAutomationTab.h"
#include "UI/AMUIHelpers.h"
#include "Automation/AMTypes.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Automation/AMSound.h"
#include "FGRecipe.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "UI/AMUppercaseText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 AMRR_MaxSuggestions = 6;

	/** A fixed-width cell. */
	USizeBox* AMRR_Cell(UWidgetTree* Tree, float Width, UWidget* Content)
	{
		USizeBox* Cell = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(Width);
		Cell->AddChild(Content);
		return Cell;
	}

	/** A button of an open dropdown list: full width, label on the left (orange = the current one). */
	UButton* AMRR_OptionButton(UWidgetTree* Tree, const FString& Label, const FSlateFontInfo& Font, bool bCurrent)
	{
		UTextBlock* LabelText = nullptr;
		UButton* Button = AMUI::MakeButton(Tree, Label, Font, AMUI::Option, &LabelText);
		if (UButtonSlot* LabelSlot = LabelText ? Cast<UButtonSlot>(LabelText->Slot) : nullptr)
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Left);
		}
		if (LabelText && bCurrent)
		{
			LabelText->SetColorAndOpacity(FSlateColor(AMUI::Orange));
		}
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
	const FSlateFontInfo Font = mTab->GetRowFont();

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
	UButton* SecondButton = AMUI::MakeDropdown(Tree, Font, SecondLabel);
	mSecondLabel = SecondLabel;
	SecondButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnSecondCellClicked);

	if (bIsAlarm)
	{
		// | ALARM NAME | MACHINE EVENT |
		UBorder* Field = AMUI::MakeBox(Tree, AMUI::Field, FMargin(10.f, 7.f));
		Field->SetVerticalAlignment(VAlign_Center);
		mAlarmNameText = Tree->ConstructWidget<UAMUppercaseText>(UAMUppercaseText::StaticClass());
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
		UButton* AlarmButton = AMUI::MakeDropdown(Tree, Font, AlarmLabel);
		mAlarmLabel = AlarmLabel;
		AlarmButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnAlarmCellClicked);
		Add(AMRR_Cell(Tree, AMRowLayout::OnAlarm, AlarmButton), AMRowLayout::Gap);
	}

	UButton* RemoveButton = AMUI::MakeRemoveButton(Tree, Font);
	RemoveButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnRemoveClicked);
	Add(AMRR_Cell(Tree, AMRowLayout::Remove, RemoveButton), AMRowLayout::Gap);

	// "Productivity [ 50 ] %" / "Clock speed [ 100 ] %", under the cell it belongs to.
	{
		UHorizontalBox* ValueLine = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		mValueLine = ValueLine;
		mValueLabel = AMUI::MakeText(Tree, TEXT(""), Font, AMUI::Grey);
		if (UHorizontalBoxSlot* LabelSlot = ValueLine->AddChildToHorizontalBox(mValueLabel))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		UBorder* ValueField = AMUI::MakeBox(Tree, AMUI::Field, FMargin(8.f, 5.f));
		mValueText = Tree->ConstructWidget<UAMUppercaseText>(UAMUppercaseText::StaticClass());
		mValueText->SetFont(Font);
		mValueText->SetJustification(ETextJustify::Right);
		mValueText->OnTextCommitted.AddDynamic(this, &UAMRuleRow::OnValueCommitted);
		ValueField->AddChild(mValueText);
		if (UHorizontalBoxSlot* FieldSlot = ValueLine->AddChildToHorizontalBox(AMRR_Cell(Tree, AMRowLayout::ValueField, ValueField)))
		{
			FieldSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* UnitSlot = ValueLine->AddChildToHorizontalBox(AMUI::MakeText(Tree, TEXT("%"), Font, AMUI::Grey)))
		{
			UnitSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			UnitSlot->SetVerticalAlignment(VAlign_Center);
		}
		ValueLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* ValueSlot = mRoot->AddChildToVerticalBox(ValueLine))
		{
			ValueSlot->SetPadding(FMargin(bIsAlarm ? AMRowLayout::AlarmName + AMRowLayout::Gap : 0.f, 4.f, 0.f, 0.f));
		}
	}

	// "Recipe [Iron Plate v]" for Change recipe, under the row.
	if (!bIsAlarm)
	{
		UHorizontalBox* RecipeLine = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		mRecipeLine = RecipeLine;
		if (UHorizontalBoxSlot* LabelSlot = RecipeLine->AddChildToHorizontalBox(AMUI::MakeText(Tree, TEXT("Recipe"), Font, AMUI::Grey)))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* RecipeLabel = nullptr;
		UButton* RecipeButton = AMUI::MakeDropdown(Tree, Font, RecipeLabel);
		mRecipeLabel = RecipeLabel;
		RecipeButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnRecipeCellClicked);
		if (UHorizontalBoxSlot* ButtonSlot = RecipeLine->AddChildToHorizontalBox(
			AMRR_Cell(Tree, AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm - 60.f, RecipeButton)))
		{
			ButtonSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		RecipeLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* RecipeSlot = mRoot->AddChildToVerticalBox(RecipeLine))
		{
			RecipeSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		// "Message [ your text ]" for Show message (normal letters, not capitals).
		UHorizontalBox* MessageLine = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		mMessageLine = MessageLine;
		if (UHorizontalBoxSlot* LabelSlot = MessageLine->AddChildToHorizontalBox(AMUI::MakeText(Tree, TEXT("Message"), Font, AMUI::Grey)))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		UBorder* MessageField = AMUI::MakeBox(Tree, AMUI::Field, FMargin(8.f, 5.f));
		mMessageText = Tree->ConstructWidget<UAMUppercaseText>(UAMUppercaseText::StaticClass());
		mMessageText->SetFont(Font);
		mMessageText->SetHintText(FText::FromString(TEXT("empty: which machine rang")));
		mMessageText->OnTextCommitted.AddDynamic(this, &UAMRuleRow::OnMessageCommitted);
		MessageField->AddChild(mMessageText);
		if (UHorizontalBoxSlot* FieldSlot = MessageLine->AddChildToHorizontalBox(
			AMRR_Cell(Tree, AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm - 70.f, MessageField)))
		{
			FieldSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		MessageLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* MessageSlot = mRoot->AddChildToVerticalBox(MessageLine))
		{
			MessageSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		// "Sound [Blip v]" for Play sound.
		UHorizontalBox* SoundLine = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		mSoundLine = SoundLine;
		if (UHorizontalBoxSlot* LabelSlot = SoundLine->AddChildToHorizontalBox(AMUI::MakeText(Tree, TEXT("Sound"), Font, AMUI::Grey)))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* SoundLabel = nullptr;
		UButton* SoundButton = AMUI::MakeDropdown(Tree, Font, SoundLabel);
		mSoundLabel = SoundLabel;
		SoundButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnSoundCellClicked);
		if (UHorizontalBoxSlot* ButtonSlot = SoundLine->AddChildToHorizontalBox(AMRR_Cell(Tree, AMRowLayout::Do, SoundButton)))
		{
			ButtonSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		SoundLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* SoundSlot = mRoot->AddChildToVerticalBox(SoundLine))
		{
			SoundSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}

	mChoices = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	mChoices->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ChoicesSlot = mRoot->AddChildToVerticalBox(mChoices))
	{
		ChoicesSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 4.f));
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

	if (!bIsAlarm && mRecipeLine && mRecipeLabel)
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		const bool bNeedsRecipe = Rule && AMRules::ActionNeedsRecipe(Rule->Action);
		mRecipeLine->SetVisibility(bNeedsRecipe ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bNeedsRecipe)
		{
			mRecipeLabel->SetText(Rule->Recipe ? UFGRecipe::GetRecipeName(Rule->Recipe) : FText::FromString(TEXT("choose a recipe")));
			mRecipeLabel->SetColorAndOpacity(FSlateColor(Rule->Recipe ? AMUI::White : AMUI::Grey));
		}
	}

	if (!bIsAlarm && mSoundLine && mSoundLabel)
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		const bool bNeedsSound = Rule && AMRules::ActionNeedsSound(Rule->Action);
		mSoundLine->SetVisibility(bNeedsSound ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bNeedsSound)
		{
			mSoundLabel->SetText(FText::FromString(AMRules::GetSounds()[AMRules::GetSoundIndex(Rule->Value)].Label));
		}
	}

	if (!bIsAlarm && mMessageLine && mMessageText)
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		const bool bNeedsMessage = Rule && AMRules::ActionNeedsMessage(Rule->Action);
		mMessageLine->SetVisibility(bNeedsMessage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bNeedsMessage)
		{
			mMessageText->ShowText(Rule->Message);
		}
	}

	const AMRules::FValueInfo Info = GetValueInfo();
	if (mValueLine)
	{
		mValueLine->SetVisibility(Info.bNeeded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Info.bNeeded && mValueLabel && mValueText)
	{
		mValueLabel->SetText(FText::FromString(Info.Label));
		mValueText->ShowText(FString::Printf(TEXT("%.0f"), GetRuleValue()));
	}
}

AMRules::FValueInfo UAMRuleRow::GetValueInfo() const
{
	if (bIsAlarm)
	{
		const FAMFireRule* Rule = mTab->GetFireRule(mIndex);
		return Rule ? AMRules::GetConditionValueInfo(Rule->Condition) : AMRules::FValueInfo();
	}
	const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
	return Rule ? AMRules::GetActionValueInfo(Rule->Action) : AMRules::FValueInfo();
}

float UAMRuleRow::GetRuleValue() const
{
	if (bIsAlarm)
	{
		const FAMFireRule* Rule = mTab->GetFireRule(mIndex);
		return Rule ? Rule->Value : 0.f;
	}
	const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
	return Rule ? Rule->Value : 0.f;
}

void UAMRuleRow::SetRuleValue(float Value)
{
	if (bIsAlarm)
	{
		if (FAMFireRule* Rule = mTab->GetFireRule(mIndex))
		{
			Rule->Value = Value;
		}
	}
	else if (FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		Rule->Value = Value;
	}
}

void UAMRuleRow::OnValueCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	const AMRules::FValueInfo Info = GetValueInfo();
	if (!Info.bNeeded)
	{
		return;
	}
	const FString Typed = Text.ToString().Replace(TEXT("%"), TEXT("")).TrimStartAndEnd();
	const float NewValue = Typed.IsNumeric() ? AMRules::CleanValue(Info, FCString::Atof(*Typed)) : GetRuleValue();
	if (!FMath::IsNearlyEqual(NewValue, GetRuleValue()))
	{
		SetRuleValue(NewValue);
		mTab->OnRulesEdited();
	}
	// What is kept, e.g. 300 -> 250.
	const FString Shown = FString::Printf(TEXT("%.0f"), NewValue);
	if (UWorld* World = mTab->GetWorld())
	{
		TWeakObjectPtr<UAMUppercaseText> WeakField(mValueText);
		World->GetTimerManager().SetTimerForNextTick([WeakField, Shown]()
		{
			if (UAMUppercaseText* Field = WeakField.Get())
			{
				Field->ShowText(Shown);
			}
		});
	}
	mTab->ShowInfo(FText::FromString(Info.Label), FText::FromString(FString::Printf(
		TEXT("Set to %s%%. Allowed: %.0f%% to %.0f%%."), *Shown, Info.Min, Info.Max)));
}

void UAMRuleRow::ShowChoices(EAMChoiceKind Kind, const TArray<FString>& Alarms, const FString& EmptyMessage)
{
	mChoices->ClearChildren();
	mChoiceHandlers.Reset();
	UWidgetTree* Tree = mTab->GetTree();
	const FSlateFontInfo Font = mTab->GetRowFont();

	auto AddChoice = [this, Tree, &Font, Kind](const FString& Label, const FString& Alarm, uint8 Value, bool bCurrent)
	{
		UAMChoice* Handler = NewObject<UAMChoice>(this);
		Handler->Row = this;
		Handler->Kind = Kind;
		Handler->Alarm = Alarm;
		Handler->Value = Value;
		mChoiceHandlers.Add(Handler);

		UButton* Button = AMRR_OptionButton(Tree, Label, Font, bCurrent);
		Button->OnClicked.AddDynamic(Handler, &UAMChoice::OnClicked);
		Button->OnHovered.AddDynamic(Handler, &UAMChoice::OnHovered);
		if (UVerticalBoxSlot* OptionSlot = mChoices->AddChildToVerticalBox(Button))
		{
			OptionSlot->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
	};

	if (Kind == EAMChoiceKind::MachineEvent)
	{
		const FAMFireRule* Rule = mTab->GetFireRule(mIndex);
		for (uint8 Value = 0; Value < static_cast<uint8>(EAMCondition::Count); ++Value)
		{
			const EAMCondition Condition = static_cast<EAMCondition>(Value);
			// Only what this machine has (a miner has no input or recipe); the current one stays listed.
			if (!AMRules::IsConditionAvailable(Condition, mTab->GetMachineObject()) && !(Rule && Rule->Condition == Condition))
			{
				continue;
			}
			AddChoice(AMRules::GetConditionText(Condition).ToString(), FString(), Value, Rule && Rule->Condition == Condition);
		}
	}
	else if (Kind == EAMChoiceKind::Action)
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		for (uint8 Value = 0; Value < static_cast<uint8>(EAMAction::Count); ++Value)
		{
			const EAMAction Action = static_cast<EAMAction>(Value);
			// Machines: machine actions. HUB: world actions.
			if (AMRules::IsWorldAction(Action) != mTab->IsWorldMode())
			{
				continue;
			}
			if (!AMRules::IsActionAvailable(Action, mTab->GetMachineObject()) && !(Rule && Rule->Action == Action))
			{
				continue;
			}
			AddChoice(AMRules::GetActionText(Action).ToString(), FString(), Value, Rule && Rule->Action == Action);
		}
	}
	else if (Kind == EAMChoiceKind::Sound)
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		const int32 Current = Rule ? AMRules::GetSoundIndex(Rule->Value) : INDEX_NONE;
		const TArray<AMRules::FSound>& Sounds = AMRules::GetSounds();
		for (int32 Index = 0; Index < Sounds.Num(); ++Index)
		{
			AddChoice(Sounds[Index].Label, FString(), static_cast<uint8>(Index), Index == Current);
		}
	}
	else if (Kind == EAMChoiceKind::Recipe)
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		TArray<TSubclassOf<UFGRecipe>> Recipes;
		if (const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(mTab->GetMachineObject()))
		{
			Manufacturer->GetAvailableRecipes(Recipes);
		}
		if (Recipes.Num() == 0)
		{
			UBorder* Message = AMUI::MakeBox(Tree, AMUI::Option, FMargin(12.f, 7.f));
			Message->AddChild(AMUI::MakeText(Tree, TEXT("No recipes available for this machine."), Font, AMUI::Grey));
			mChoices->AddChildToVerticalBox(Message);
		}
		for (const TSubclassOf<UFGRecipe>& Recipe : Recipes)
		{
			AddChoice(UFGRecipe::GetRecipeName(Recipe).ToString(), FString(), 0, Rule && Rule->Recipe.Get() == Recipe.Get());
			mChoiceHandlers.Last()->RecipeClass = Recipe.Get();
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
			UBorder* Message = AMUI::MakeBox(Tree, AMUI::Option, FMargin(12.f, 7.f));
			Message->AddChild(AMUI::MakeText(Tree, EmptyMessage, Font, AMUI::Grey));
			mChoices->AddChildToVerticalBox(Message);
		}
		const FAMReactionRule* Rule = bIsAlarm ? nullptr : mTab->GetReactionRule(mIndex);
		for (const FString& Alarm : Alarms)
		{
			AddChoice(Alarm, Alarm, 0, Rule && Rule->Event == Alarm);
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
	// Capitals while typing.
	{
		TGuardValue<bool> Guard(bSettingText, true);
		mAlarmNameText->MakeUppercase();
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
		// Not now: while the field is still committing it ignores a new text. One frame later.
		SetAlarmNameText(Name);
		if (UWorld* World = mTab->GetWorld())
		{
			TWeakObjectPtr<UAMRuleRow> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis, Name]()
			{
				if (UAMRuleRow* Row = WeakThis.Get())
				{
					Row->SetAlarmNameText(Name);
				}
			});
		}
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
				const AMRules::FValueInfo Info = AMRules::GetConditionValueInfo(Condition);
				Rule->Value = Info.bNeeded ? Info.Default : 0.f;
				mTab->OnRulesEdited();
			}
		}
		break;
	case EAMChoiceKind::Sound:
		if (FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
		{
			if (AMRules::GetSoundIndex(Rule->Value) != Choice.Value)
			{
				Rule->Value = Choice.Value;
				mTab->OnRulesEdited();
			}
			// Let the player hear it.
			AMSound::PlayForPlayer(mTab->GetOwningPlayer(), Choice.Value);
		}
		break;
	case EAMChoiceKind::Recipe:
		if (FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
		{
			if (Rule->Recipe.Get() != Choice.RecipeClass)
			{
				Rule->Recipe = Choice.RecipeClass.Get();
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
				const AMRules::FValueInfo Info = AMRules::GetActionValueInfo(Action);
				Rule->Value = Info.bNeeded ? Info.Default : 0.f;
				Rule->Recipe = nullptr;
				Rule->Message.Reset();
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
	case EAMChoiceKind::Sound:
		if (AMRules::GetSounds().IsValidIndex(Choice.Value))
		{
			const AMRules::FSound& Sound = AMRules::GetSounds()[Choice.Value];
			mTab->ShowInfo(FText::FromString(Sound.Label), FText::FromString(FString::Printf(
				TEXT("%s\n\nClick it to choose it and hear it."), Sound.Description)));
		}
		break;
	case EAMChoiceKind::Recipe:
		if (Choice.RecipeClass)
		{
			const TSubclassOf<UFGRecipe> Recipe(Choice.RecipeClass.Get());
			mTab->ShowInfo(UFGRecipe::GetRecipeName(Recipe), FText::FromString(FString::Printf(
				TEXT("%s\n\nWhen the alarm rings, this machine's input and output slots are emptied (the items are destroyed) and it switches to this recipe."),
				*AMRules::DescribeRecipe(mTab, Recipe).ToString())));
		}
		break;
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

void UAMRuleRow::OnSoundCellClicked()
{
	if (bChoicesOpen && mOpenKind == EAMChoiceKind::Sound)
	{
		HideChoices();
		return;
	}
	if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		mTab->ShowInfo(AMRules::GetActionText(Rule->Action), AMRules::GetActionDescription(Rule->Action));
	}
	ShowChoices(EAMChoiceKind::Sound, {}, FString());
}

void UAMRuleRow::OnRecipeCellClicked()
{
	if (bChoicesOpen && mOpenKind == EAMChoiceKind::Recipe)
	{
		HideChoices();
		return;
	}
	if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		mTab->ShowInfo(AMRules::GetActionText(Rule->Action), AMRules::GetActionDescription(Rule->Action));
	}
	ShowChoices(EAMChoiceKind::Recipe, {}, FString());
}

void UAMRuleRow::OnMessageCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
	if (!Rule)
	{
		return;
	}
	const FString NewMessage = Text.ToString().TrimStartAndEnd().Left(AMRules::MaxMessageLength);
	if (Rule->Message != NewMessage)
	{
		Rule->Message = NewMessage;
		mTab->OnRulesEdited();
	}
	mTab->ShowInfo(AMRules::GetActionText(Rule->Action), FText::FromString(FString::Printf(
		TEXT("Every player will see:\n\n%s\n%s"), *Rule->Event, Rule->Message.IsEmpty() ? TEXT("Rung by <machine>") : *Rule->Message)));
}

void UAMRuleRow::OnRemoveClicked()
{
	// The tab rebuilds all rows, this one included.
	mTab->RemoveRule(bIsAlarm, mIndex);
}
