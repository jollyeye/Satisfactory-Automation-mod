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
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Resources/FGItemDescriptor.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Buildables/FGBuildableFactory.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Styling/SlateBrush.h"

namespace
{
	constexpr int32 AMRR_MaxSuggestions = 6;


	/**
	 * The name of an action on a one-line row: Pause / Resume are one "Running" with a switch (on = runs,
	 * off = standby, like the game's standby lever: 1 runs, 0 stands by), Turn on / off one "Power".
	 */
	FString AMRR_CompactName(EAMAction Action)
	{
		switch (Action)
		{
		case EAMAction::Pause:
		case EAMAction::Resume:    return TEXT("Running");
		case EAMAction::SwitchOn:
		case EAMAction::SwitchOff: return TEXT("Power");
		case EAMAction::BlockSignal:
		case EAMAction::UnblockSignal: return TEXT("Blocked");
		default:                   return AMRules::GetActionText(Action).ToString();
		}
	}

	/** The action a one-line row's switch is about (on = the first one). */
	bool AMRR_HasSwitch(EAMAction Action)
	{
		return Action == EAMAction::Pause || Action == EAMAction::Resume || Action == EAMAction::SwitchOn || Action == EAMAction::SwitchOff
			|| Action == EAMAction::BlockSignal || Action == EAMAction::UnblockSignal;
	}

	constexpr float AMRR_CompactHeight = 34.f;

	/** The recipe / sound dropdown of a one-line action row, and the room its text has (padding, arrow). */
	constexpr float AMRR_CompactDropdown = AMRowLayout::Do + 20.f;
	constexpr float AMRR_CompactDropdownText = AMRR_CompactDropdown - 10.f - 8.f - 11.f - 8.f;

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

void UAMRuleRow::BuildGroupHeader(UAMAutomationTab* InTab, const FString& InEvent, int32 InCount)
{
	mTab = InTab;
	bIsAlarm = false;
	bGroupHeader = true;
	mGroupEvent = InEvent;
	mGroupCount = InCount;
	mIndex = INDEX_NONE;

	UWidgetTree* Tree = mTab->GetTree();
	const FSlateFontInfo Font = mTab->GetRowFont();
	mRoot = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	mRoot->AddChildToVerticalBox(Line);

	// [icon ALARM v]: the alarm of all the actions under it.
	UTextBlock* AlarmLabel = nullptr;
	UButton* AlarmButton = AMUI::MakeDropdown(Tree, Font, AlarmLabel);
	mAlarmLabel = AlarmLabel;
	mAlarmButton = AlarmButton;
	AlarmButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnAlarmCellClicked);
	if (UHorizontalBox* Content = Cast<UHorizontalBox>(AlarmButton->GetChildAt(0)))
	{
		UImage* Icon = AMUI::MakeImage(Tree, TEXT("/AutomatedMachines/Icons/T_AM_TabIcon.T_AM_TabIcon"), FVector2D(18.f, 18.f));
		if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(Content->InsertChildAt(0, Icon)))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
	}
	if (UHorizontalBoxSlot* AlarmSlot = Line->AddChildToHorizontalBox(AMRR_Cell(Tree, AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm, AlarmButton)))
	{
		AlarmSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UButton* RemoveButton = AMUI::MakeRemoveButton(Tree, Font);
	RemoveButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnGroupBinClicked);
	RemoveButton->OnPressed.AddDynamic(this, &UAMRuleRow::OnGroupBinPressed);
	RemoveButton->OnReleased.AddDynamic(this, &UAMRuleRow::OnGroupBinReleased);
	if (UHorizontalBoxSlot* RemoveSlot = Line->AddChildToHorizontalBox(AMRR_Cell(Tree, AMRowLayout::Remove, RemoveButton)))
	{
		RemoveSlot->SetPadding(FMargin(AMRowLayout::Gap, 0.f, 0.f, 0.f));
		RemoveSlot->SetVerticalAlignment(VAlign_Fill);
	}
	mAlarmLabel->SetText(FText::FromString(mGroupEvent.IsEmpty() ? TEXT("choose an alarm") : mGroupEvent));
	mAlarmLabel->SetColorAndOpacity(FSlateColor(mGroupEvent.IsEmpty() ? AMUI::Grey : AMUI::White));

	// "+ action" under the group's actions (a small text button, indented like them).
	UTextBlock* AddLabel = nullptr;
	UButton* AddButton = AMUI::MakeButton(Tree, TEXT("+ action"), Font, AMUI::Button, &AddLabel);
	if (UButtonSlot* LabelSlot = AddLabel ? Cast<UButtonSlot>(AddLabel->Slot) : nullptr)
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Left);
	}
	AddButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnGroupAddClicked);
	AMUI::SetButtonSeeThrough(AddButton, AMUI::Button);
	mGroupAddButton = AddButton;
	USizeBox* AddSize = AMRR_Cell(Tree, AMRowLayout::Do, AddButton);
	UHorizontalBox* FooterLine = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* AddSlot = FooterLine->AddChildToHorizontalBox(AddSize))
	{
		AddSlot->SetPadding(FMargin(AMRowLayout::GroupIndent, 0.f, 0.f, 0.f));
	}
	mFooter = FooterLine;
}

void UAMRuleRow::OnGroupAddClicked()
{
	// Which action: the list of those this alarm can still have.
	OpenPicker(EAMChoiceKind::Action, mGroupAddButton);
}

void UAMRuleRow::BuildCompact()
{
	UWidgetTree* Tree = mTab->GetTree();
	const FSlateFontInfo Font = mTab->GetRowFont();
	const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
	const EAMAction Action = Rule ? Rule->Action : EAMAction::Pause;

	mRoot = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Outer = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* OuterSlot = mRoot->AddChildToVerticalBox(Outer))
	{
		OuterSlot->SetPadding(FMargin(AMRowLayout::GroupIndent, 0.f, 0.f, 0.f));
	}
	// [ name ........ value ] [X]: the name and value in one plain box (no background, no hover:
	// only the value, the bin and "+ action" react to the mouse), the bin next to it.
	const float BoxWidth = AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm - AMRowLayout::GroupIndent;
	UBorder* Box = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(FSlateNoResource());
	Box->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	Box->SetHorizontalAlignment(HAlign_Fill);
	Box->SetVerticalAlignment(VAlign_Fill);
	USizeBox* BoxSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BoxSize->SetWidthOverride(BoxWidth);
	BoxSize->SetHeightOverride(AMRR_CompactHeight);
	BoxSize->AddChild(Box);
	Outer->AddChildToHorizontalBox(BoxSize);
	UHorizontalBox* Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Box->AddChild(Line);

	UTextBlock* Name = AMUI::MakeText(Tree, AMRR_CompactName(Action), Font, AMUI::White);
	Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	if (UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}
	auto AddValue = [Line](UWidget* Widget, float LeftPadding, bool bFullHeight = false)
	{
		if (UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(Widget))
		{
			// A dropdown is exactly as high as the row (no margin); fields sit in the middle.
			ValueSlot->SetPadding(bFullHeight ? FMargin(LeftPadding, 0.f, 0.f, 0.f) : FMargin(LeftPadding, 3.f, 6.f, 3.f));
			ValueSlot->SetVerticalAlignment(bFullHeight ? VAlign_Fill : VAlign_Center);
		}
	};

	const AMRules::FValueInfo Info = AMRules::GetActionValueInfo(Action);
	if (AMRR_HasSwitch(Action))
	{
		// The options' switch: on = Standby / Power on.
		UButton* StateButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Clear;
		Clear.SetNormal(FSlateNoResource());
		Clear.SetHovered(FSlateNoResource());
		Clear.SetPressed(FSlateNoResource());
		Clear.SetNormalPadding(FMargin(0.f));
		Clear.SetPressedPadding(FMargin(0.f));
		StateButton->SetStyle(Clear);
		mStateImage = AMUI::MakeImage(Tree, AMUI::SwitchOffPath, FVector2D(44.f, 22.f));
		StateButton->AddChild(mStateImage);
		StateButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnStateClicked);
		AddValue(AMRR_Cell(Tree, 44.f, StateButton), 8.f);
	}
	else if (Info.bNeeded)
	{
		// [100] %
		UBorder* ValueField = AMUI::MakeBox(Tree, AMUI::Field, FMargin(8.f, 2.f));
		ValueField->SetVerticalAlignment(VAlign_Center);
		mValueText = Tree->ConstructWidget<UAMUppercaseText>(UAMUppercaseText::StaticClass());
		mValueText->SetFont(Font);
		mValueText->SetJustification(ETextJustify::Right);
		mValueText->OnTextCommitted.AddDynamic(this, &UAMRuleRow::OnValueCommitted);
		ValueField->AddChild(mValueText);
		AMUI::MarkAsInputBox(ValueField);
		AddValue(AMRR_Cell(Tree, AMRowLayout::ValueField, ValueField), 8.f);
		mValueUnit = AMUI::MakeText(Tree, Info.Unit, Font, AMUI::Grey);
		AddValue(AMRR_Cell(Tree, FMath::Max(16.f, 9.f * FCString::Strlen(Info.Unit)), mValueUnit), 6.f);
	}
	else if (AMRules::ActionNeedsRecipe(Action) || AMRules::ActionNeedsSound(Action))
	{
		UTextBlock* ValueLabel = nullptr;
		UButton* ValueButton = AMUI::MakeDropdown(Tree, Font, ValueLabel);
		{
			// As high as the row, its text in the middle (no top / bottom padding).
			FButtonStyle Style = ValueButton->GetStyle();
			Style.SetNormalPadding(FMargin(10.f, 0.f, 8.f, 0.f));
			Style.SetPressedPadding(FMargin(10.f, 1.f, 8.f, 0.f));
			ValueButton->SetStyle(Style);
			if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(ValueButton->GetChildAt(0) ? ValueButton->GetChildAt(0)->Slot : nullptr))
			{
				ContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		if (AMRules::ActionNeedsRecipe(Action))
		{
			mRecipeButton = ValueButton;
			mRecipeLabel = ValueLabel;
			ValueButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnRecipeCellClicked);
		}
		else
		{
			mSoundButton = ValueButton;
			mSoundLabel = ValueLabel;
			ValueButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnSoundCellClicked);
		}
		AddValue(AMRR_Cell(Tree, AMRR_CompactDropdown, ValueButton), 8.f, true);
	}
	else if (AMRules::ActionNeedsMessage(Action))
	{
		UBorder* MessageField = AMUI::MakeBox(Tree, AMUI::Field, FMargin(8.f, 2.f));
		MessageField->SetVerticalAlignment(VAlign_Center);
		mMessageText = Tree->ConstructWidget<UAMUppercaseText>(UAMUppercaseText::StaticClass());
		mMessageText->SetFont(Font);
		mMessageText->SetHintText(FText::FromString(TEXT("empty: which machine rang")));
		mMessageText->OnTextCommitted.AddDynamic(this, &UAMRuleRow::OnMessageCommitted);
		MessageField->AddChild(mMessageText);
		AMUI::MarkAsInputBox(MessageField);
		AddValue(AMRR_Cell(Tree, BoxWidth - 150.f, MessageField), 8.f);
	}

	UButton* RemoveButton = AMUI::MakeRemoveButton(Tree, Font);
	AMUI::SetButtonSeeThrough(RemoveButton, AMUI::Field);
	RemoveButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnRemoveClicked);
	USizeBox* RemoveSize = AMRR_Cell(Tree, AMRowLayout::Remove, RemoveButton);
	RemoveSize->SetHeightOverride(AMRR_CompactHeight);
	if (UHorizontalBoxSlot* RemoveSlot = Outer->AddChildToHorizontalBox(RemoveSize))
	{
		RemoveSlot->SetPadding(FMargin(AMRowLayout::Gap, 0.f, 0.f, 0.f));
	}
	UpdateCompact();
}

void UAMRuleRow::UpdateCompact()
{
	const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
	if (!Rule)
	{
		return;
	}
	if (mStateImage)
	{
		const bool bOn = Rule->Action == EAMAction::Resume || Rule->Action == EAMAction::SwitchOn || Rule->Action == EAMAction::BlockSignal;
		AMUI::SetImage(mStateImage, bOn ? AMUI::SwitchOnPath : AMUI::SwitchOffPath);
	}
	if (mValueText)
	{
		mValueText->ShowText(FString::Printf(TEXT("%.0f"), Rule->Value));
	}
	if (mRecipeLabel)
	{
		AMUI::SetFittedText(mRecipeLabel, Rule->Recipe ? UFGRecipe::GetRecipeName(Rule->Recipe).ToString() : FString(TEXT("choose a recipe")), AMRR_CompactDropdownText);
		mRecipeLabel->SetColorAndOpacity(FSlateColor(Rule->Recipe ? AMUI::White : AMUI::Grey));
	}
	if (mSoundLabel)
	{
		AMUI::SetFittedText(mSoundLabel, AMRules::GetSounds()[AMRules::GetSoundIndex(Rule->Value)].Label, AMRR_CompactDropdownText);
	}
	if (mMessageText)
	{
		mMessageText->ShowText(Rule->Message);
	}
}

void UAMRuleRow::OnCompactHovered()
{
	ExplainCurrent();
}

void UAMRuleRow::OnStateClicked()
{
	FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
	if (!Rule)
	{
		return;
	}
	switch (Rule->Action)
	{
	case EAMAction::Pause:     Rule->Action = EAMAction::Resume; break;
	case EAMAction::Resume:    Rule->Action = EAMAction::Pause; break;
	case EAMAction::SwitchOn:  Rule->Action = EAMAction::SwitchOff; break;
	case EAMAction::SwitchOff: Rule->Action = EAMAction::SwitchOn; break;
	case EAMAction::BlockSignal:   Rule->Action = EAMAction::UnblockSignal; break;
	case EAMAction::UnblockSignal: Rule->Action = EAMAction::BlockSignal; break;
	default: return;
	}
	if (AMRules::IsSignalAction(Rule->Action))
	{
		mTab->OnRulesEdited();
		UpdateCompact();
		mTab->ShowInfo(FText::FromString(TEXT("Blocked")), AMRules::GetActionDescription(Rule->Action));
		return;
	}
	mTab->OnRulesEdited();
	UpdateCompact();
	const bool bStandby = Rule->Action == EAMAction::Pause || Rule->Action == EAMAction::Resume;
	const bool bOn = Rule->Action == EAMAction::Resume || Rule->Action == EAMAction::SwitchOn;
	mTab->ShowInfo(FText::FromString(AMRR_CompactName(Rule->Action)), FText::FromString(bStandby
		? (bOn ? TEXT("On: when the alarm rings, the machine runs (leaves standby).") : TEXT("Off: when the alarm rings, the machine goes on standby."))
		: (bOn ? TEXT("On: when the alarm rings, the switch is turned on.") : TEXT("Off: when the alarm rings, the switch is turned off."))));
}

void UAMRuleRow::OnGroupBinPressed()
{
	// More than one action: hold the bin down (they all go). One: a click is enough.
	if (mGroupCount > 1)
	{
		mTab->StartGroupHold(mGroupEvent, mGroupCount);
	}
}

void UAMRuleRow::OnGroupBinReleased()
{
	if (mGroupCount > 1)
	{
		mTab->StopOverviewHold(false);
	}
}

void UAMRuleRow::OnGroupBinClicked()
{
	if (mGroupCount <= 1)
	{
		mTab->RemoveActionGroup(mGroupEvent);
	}
}

void UAMRuleRow::Build(UAMAutomationTab* InTab, bool bInIsAlarm, int32 InIndex, bool bInGrouped)
{
	mTab = InTab;
	bIsAlarm = bInIsAlarm;
	mIndex = InIndex;
	if (!bIsAlarm && bInGrouped)
	{
		bCompact = true;
		BuildCompact();
		return;
	}

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
	mSecondButton = SecondButton;
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
		AMUI::MarkAsInputBox(Field);
		mAlarmNameField = Field;
		Add(AMRR_Cell(Tree, AMRowLayout::AlarmName, Field), 0.f);
		Add(AMRR_Cell(Tree, AMRowLayout::MachineEvent, SecondButton), AMRowLayout::Gap);
	}
	else if (bInGrouped)
	{
		// Under its alarm's header: | DO | (the header names the alarm), moved right.
		Add(AMRR_Cell(Tree, AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm - AMRowLayout::GroupIndent, SecondButton), AMRowLayout::GroupIndent);
	}
	else
	{
		// | DO | ON ALARM |
		Add(AMRR_Cell(Tree, AMRowLayout::Do, SecondButton), 0.f);
		UTextBlock* AlarmLabel = nullptr;
		UButton* AlarmButton = AMUI::MakeDropdown(Tree, Font, AlarmLabel);
		mAlarmLabel = AlarmLabel;
		mAlarmButton = AlarmButton;
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
		AMUI::MarkAsInputBox(ValueField);
		if (UHorizontalBoxSlot* FieldSlot = ValueLine->AddChildToHorizontalBox(AMRR_Cell(Tree, AMRowLayout::ValueField, ValueField)))
		{
			FieldSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		mValueUnit = AMUI::MakeText(Tree, TEXT("%"), Font, AMUI::Grey);
		if (UHorizontalBoxSlot* UnitSlot = ValueLine->AddChildToHorizontalBox(mValueUnit))
		{
			UnitSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			UnitSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Item count above / below: "# [500]  [icon Iron Plate v]" on one line.
		if (bIsAlarm)
		{
			UTextBlock* ItemLabel = nullptr;
			mItemButton = AMUI::MakeDropdown(Tree, Font, ItemLabel);
			mItemLabel = ItemLabel;
			if (UHorizontalBox* Content = Cast<UHorizontalBox>(mItemButton->GetChildAt(0)))
			{
				mItemIcon = Tree->ConstructWidget<UImage>(UImage::StaticClass());
				mItemIcon->SetDesiredSizeOverride(FVector2D(20.f, 20.f));
				mItemIcon->SetVisibility(ESlateVisibility::Collapsed);
				if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(Content->InsertChildAt(0, mItemIcon)))
				{
					IconSlot->SetVerticalAlignment(VAlign_Center);
					IconSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				}
			}
			mItemButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnItemCellClicked);
			// The rest of the row's width: alarm name + machine event + bin, minus "# [500]".
			mItemCell = AMRR_Cell(Tree, AMRowLayout::AlarmName + AMRowLayout::MachineEvent + AMRowLayout::Remove + 2 * AMRowLayout::Gap
				- AMRowLayout::ValueField - 40.f, mItemButton);
			mItemCell->SetVisibility(ESlateVisibility::Collapsed);
			if (UHorizontalBoxSlot* ItemSlot = ValueLine->AddChildToHorizontalBox(mItemCell))
			{
				ItemSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
			}
		}
		ValueLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* ValueSlot = mRoot->AddChildToVerticalBox(ValueLine))
		{
			ValueSlot->SetPadding(FMargin(bIsAlarm ? AMRowLayout::AlarmName + AMRowLayout::Gap : (bInGrouped ? AMRowLayout::GroupIndent : 0.f), 4.f, 0.f, 0.f));
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
		mRecipeButton = RecipeButton;
		RecipeButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnRecipeCellClicked);
		if (UHorizontalBoxSlot* ButtonSlot = RecipeLine->AddChildToHorizontalBox(
			AMRR_Cell(Tree, AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm - 60.f, RecipeButton)))
		{
			ButtonSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		RecipeLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* RecipeSlot = mRoot->AddChildToVerticalBox(RecipeLine))
		{
			RecipeSlot->SetPadding(FMargin(bInGrouped ? AMRowLayout::GroupIndent : 0.f, 4.f, 0.f, 0.f));
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
		AMUI::MarkAsInputBox(MessageField);
		if (UHorizontalBoxSlot* FieldSlot = MessageLine->AddChildToHorizontalBox(
			AMRR_Cell(Tree, AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm - 70.f, MessageField)))
		{
			FieldSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		MessageLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* MessageSlot = mRoot->AddChildToVerticalBox(MessageLine))
		{
			MessageSlot->SetPadding(FMargin(bInGrouped ? AMRowLayout::GroupIndent : 0.f, 4.f, 0.f, 0.f));
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
		mSoundButton = SoundButton;
		SoundButton->OnClicked.AddDynamic(this, &UAMRuleRow::OnSoundCellClicked);
		if (UHorizontalBoxSlot* ButtonSlot = SoundLine->AddChildToHorizontalBox(AMRR_Cell(Tree, AMRowLayout::Do, SoundButton)))
		{
			ButtonSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		SoundLine->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* SoundSlot = mRoot->AddChildToVerticalBox(SoundLine))
		{
			SoundSlot->SetPadding(FMargin(bInGrouped ? AMRowLayout::GroupIndent : 0.f, 4.f, 0.f, 0.f));
		}
	}

	// Not in the row: shown floating under the name field (UAMAutomationTab::ShowFloatingList).
	mChoices = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	mChoicesFloat = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	mChoicesFloat->SetWidthOverride(AMRowLayout::AlarmName + AMRowLayout::Gap + AMRowLayout::MachineEvent);
	UBorder* ChoicesBox = AMUI::MakeBox(Tree, AMUI::SidePanel, FMargin(2.f));
	ChoicesBox->AddChild(mChoices);
	mChoicesFloat->AddChild(ChoicesBox);

	UpdateLabels();
}

void UAMRuleRow::UpdateLabels()
{
	if (bGroupHeader)
	{
		return;
	}
	if (bCompact)
	{
		UpdateCompact();
		return;
	}
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

	if (bIsAlarm && mItemCell && mItemLabel)
	{
		const FAMFireRule* Rule = mTab->GetFireRule(mIndex);
		const bool bNeedsItem = Rule && AMRules::ConditionNeedsItem(Rule->Condition);
		mItemCell->SetVisibility(bNeedsItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		// With an item, the line starts under the alarm name (room for the item's name).
		if (UVerticalBoxSlot* ValueSlot = mValueLine ? Cast<UVerticalBoxSlot>(mValueLine->Slot) : nullptr)
		{
			ValueSlot->SetPadding(FMargin(bNeedsItem ? 0.f : AMRowLayout::AlarmName + AMRowLayout::Gap, 4.f, 0.f, 0.f));
		}
		if (bNeedsItem)
		{
			mItemLabel->SetText(Rule->Item ? UFGItemDescriptor::GetItemName(Rule->Item) : FText::FromString(TEXT("choose an item")));
			mItemLabel->SetColorAndOpacity(FSlateColor(Rule->Item ? AMUI::White : AMUI::Grey));
			UTexture2D* Icon = Rule->Item ? UFGItemDescriptor::GetSmallIcon(Rule->Item) : nullptr;
			if (mItemIcon)
			{
				if (Icon)
				{
					mItemIcon->SetBrushFromTexture(Icon, false);
					mItemIcon->SetDesiredSizeOverride(FVector2D(20.f, 20.f));
				}
				mItemIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			}
		}
	}

	const AMRules::FValueInfo Info = GetValueInfo();
	if (mValueUnit)
	{
		mValueUnit->SetText(FText::FromString(Info.Unit));
	}
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
		TEXT("Set to %s%s. Allowed: %.0f%s to %.0f%s."), *Shown, Info.Unit, Info.Min, Info.Unit, Info.Max, Info.Unit)));
}

void UAMRuleRow::ShowChoices(EAMChoiceKind Kind, const TArray<FString>& Alarms, const FString& EmptyMessage)
{
	if (!mChoices)
	{
		return;
	}
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
			if (!mTab->IsConditionAvailableHere(Condition) && !(Rule && Rule->Condition == Condition))
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
	mTab->ShowFloatingList(mChoicesFloat, mAlarmNameField ? mAlarmNameField.Get() : mRoot.Get());
}

void UAMRuleRow::HideChoices()
{
	// Alarm group headers have no suggestion list (it is made only for alarm / action rows).
	if (mChoices)
	{
		mChoices->ClearChildren();
	}
	mChoiceHandlers.Reset();
	if (mChoicesFloat)
	{
		mTab->HideFloatingList(mChoicesFloat);
	}
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
		else if (bGroupHeader)
		{
			// All the group's actions move to the chosen alarm (rows made again on the next tick).
			mTab->RenameActionGroup(mGroupEvent, Choice.Alarm);
			HideChoices();
			mTab->ClosePicker(true);
			return;
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
				// Above <-> below keeps the item; anything else clears it.
				if (!AMRules::ConditionNeedsItem(Condition))
				{
					Rule->Item = nullptr;
				}
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
	case EAMChoiceKind::Item:
		if (FAMFireRule* Rule = mTab->GetFireRule(mIndex))
		{
			if (Rule->Item.Get() != Choice.ItemClass)
			{
				Rule->Item = Choice.ItemClass.Get();
				mTab->OnRulesEdited();
			}
		}
		break;
	case EAMChoiceKind::Action:
		if (bGroupHeader)
		{
			// "+ action": this action, added under the alarm.
			HideChoices();
			mTab->ClosePicker(true);
			mTab->AddActionToGroup(mGroupEvent, static_cast<EAMAction>(Choice.Value));
			return;
		}
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
	mTab->ClosePicker(true);
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
	case EAMChoiceKind::Item:
		if (Choice.ItemClass)
		{
			const TSubclassOf<UFGItemDescriptor> Item(Choice.ItemClass.Get());
			const int32 Count = AAMAutomationSubsystem::CountItem(Cast<AFGBuildableFactory>(mTab->GetMachineObject()), Item);
			mTab->ShowInfo(UFGItemDescriptor::GetItemName(Item), FText::FromString(FString::Printf(
				TEXT("This container holds %d now.\n\nClick it to count this item."), Count)));
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
		mTab->ShowInfo(bCompact ? FText::FromString(AMRR_CompactName(Rule->Action)) : AMRules::GetActionText(Rule->Action), AMRules::GetActionDescription(Rule->Action));
	}
}

void UAMRuleRow::OpenPicker(EAMChoiceKind Kind, UWidget* Anchor)
{
	HideChoices();
	TArray<FAMPickerOption> Options;
	auto Add = [this, Kind, &Options](const FString& Label, uint8 Value, bool bCurrent) -> UAMChoice*
	{
		UAMChoice* Handler = NewObject<UAMChoice>(this);
		Handler->Row = this;
		Handler->Kind = Kind;
		Handler->Value = Value;
		FAMPickerOption& Option = Options.AddDefaulted_GetRef();
		Option.Label = Label;
		Option.bCurrent = bCurrent;
		Option.Handler = Handler;
		return Handler;
	};
	FString EmptyMessage = TEXT("Nothing to choose here.");
	float MinWidth = 220.f;

	switch (Kind)
	{
	case EAMChoiceKind::MachineEvent:
	{
		const FAMFireRule* Rule = mTab->GetFireRule(mIndex);
		for (uint8 Value = 0; Value < static_cast<uint8>(EAMCondition::Count); ++Value)
		{
			const EAMCondition Condition = static_cast<EAMCondition>(Value);
			const bool bCurrent = Rule && Rule->Condition == Condition;
			if (mTab->IsConditionAvailableHere(Condition) || bCurrent)
			{
				Add(AMRules::GetConditionText(Condition).ToString(), Value, bCurrent);
			}
		}
		break;
	}
	case EAMChoiceKind::Action:
	{
		if (bGroupHeader)
		{
			// "+ action": the actions this alarm can still have, Pause / Resume as one "Standby"
			// and Turn on / off as one "Power" (their switch says which).
			for (uint8 Value = 0; Value < static_cast<uint8>(EAMAction::Count); ++Value)
			{
				const EAMAction Action = static_cast<EAMAction>(Value);
				if (Action == EAMAction::Resume || Action == EAMAction::SwitchOff || Action == EAMAction::UnblockSignal)
				{
					continue;
				}
				if (mTab->IsActionAvailableHere(Action) && !mTab->ActionClashesWithAlarm(mGroupEvent, Action))
				{
					Add(AMRR_CompactName(Action), Value, false);
				}
			}
			EmptyMessage = TEXT("Every action is already used for this alarm.");
			break;
		}
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		for (uint8 Value = 0; Value < static_cast<uint8>(EAMAction::Count); ++Value)
		{
			const EAMAction Action = static_cast<EAMAction>(Value);
			const bool bCurrent = Rule && Rule->Action == Action;
			// Machines: machine actions. HUB: world actions. Not one that clashes with another action
			// of the same alarm (two messages, Pause next to Resume).
			if (!bCurrent && mTab->ActionClashesInGroup(mIndex, Action))
			{
				continue;
			}
			if (AMRules::IsWorldAction(Action) == mTab->IsWorldMode() && (AMRules::IsActionAvailable(Action, mTab->GetMachineObject()) || bCurrent))
			{
				Add(AMRules::GetActionText(Action).ToString(), Value, bCurrent);
			}
		}
		break;
	}
	case EAMChoiceKind::Alarm:
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		const FString Current = bGroupHeader ? mGroupEvent : (Rule ? Rule->Event : FString());
		for (const FString& Alarm : mTab->GetEventNames())
		{
			// A header: not an alarm that already has its own group here (they would mix).
			if (bGroupHeader && Alarm != Current && mTab->HasActionGroup(Alarm))
			{
				continue;
			}
			Add(Alarm, 0, Alarm == Current)->Alarm = Alarm;
		}
		EmptyMessage = TEXT("No alarms yet: give a machine an alarm first.");
		MinWidth = 260.f;
		break;
	}
	case EAMChoiceKind::Recipe:
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		TArray<TSubclassOf<UFGRecipe>> Recipes;
		if (const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(mTab->GetMachineObject()))
		{
			Manufacturer->GetAvailableRecipes(Recipes);
		}
		for (const TSubclassOf<UFGRecipe>& Recipe : Recipes)
		{
			Add(UFGRecipe::GetRecipeName(Recipe).ToString(), 0, Rule && Rule->Recipe.Get() == Recipe.Get())->RecipeClass = Recipe.Get();
			// The icon of what it makes (its first product).
			const TArray<FItemAmount> Products = UFGRecipe::GetProducts(Recipe);
			if (Products.Num() > 0)
			{
				Options.Last().Icon = UFGItemDescriptor::GetSmallIcon(Products[0].ItemClass);
			}
		}
		EmptyMessage = TEXT("No recipes available for this machine.");
		MinWidth = 280.f;
		break;
	}
	case EAMChoiceKind::Sound:
	{
		const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex);
		const int32 Current = Rule ? AMRules::GetSoundIndex(Rule->Value) : INDEX_NONE;
		const TArray<AMRules::FSound>& Sounds = AMRules::GetSounds();
		for (int32 Index = 0; Index < Sounds.Num(); ++Index)
		{
			Add(Sounds[Index].Label, static_cast<uint8>(Index), Index == Current);
		}
		break;
	}
	case EAMChoiceKind::Item:
	{
		const FAMFireRule* Rule = mTab->GetFireRule(mIndex);
		for (UClass* Item : mTab->GetPickableItems())
		{
			const TSubclassOf<UFGItemDescriptor> ItemClass(Item);
			UAMChoice* Handler = Add(UFGItemDescriptor::GetItemName(ItemClass).ToString(), 0, Rule && Rule->Item.Get() == Item);
			Handler->ItemClass = Item;
			Options.Last().Icon = UFGItemDescriptor::GetSmallIcon(ItemClass);
		}
		EmptyMessage = TEXT("No items known yet.");
		MinWidth = 300.f;
		break;
	}
	}
	mTab->OpenPicker(this, Anchor, MoveTemp(Options), EmptyMessage, MinWidth);
}

void UAMRuleRow::OnSecondCellClicked()
{
	ExplainCurrent();
	OpenPicker(bIsAlarm ? EAMChoiceKind::MachineEvent : EAMChoiceKind::Action, mSecondButton);
}

void UAMRuleRow::OnAlarmCellClicked()
{
	if (bGroupHeader)
	{
		if (!mGroupEvent.IsEmpty())
		{
			mTab->ShowAlarmInfo(mGroupEvent);
		}
	}
	else if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		mTab->ShowAlarmInfo(Rule->Event);
	}
	OpenPicker(EAMChoiceKind::Alarm, mAlarmButton);
}

void UAMRuleRow::OnItemCellClicked()
{
	if (const FAMFireRule* Rule = mTab->GetFireRule(mIndex))
	{
		mTab->ShowInfo(AMRules::GetConditionText(Rule->Condition), AMRules::GetConditionDescription(Rule->Condition));
	}
	OpenPicker(EAMChoiceKind::Item, mItemButton);
}

void UAMRuleRow::OnSoundCellClicked()
{
	if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		mTab->ShowInfo(AMRules::GetActionText(Rule->Action), AMRules::GetActionDescription(Rule->Action));
	}
	OpenPicker(EAMChoiceKind::Sound, mSoundButton);
}

void UAMRuleRow::OnRecipeCellClicked()
{
	if (const FAMReactionRule* Rule = mTab->GetReactionRule(mIndex))
	{
		mTab->ShowInfo(AMRules::GetActionText(Rule->Action), AMRules::GetActionDescription(Rule->Action));
	}
	OpenPicker(EAMChoiceKind::Recipe, mRecipeButton);
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
