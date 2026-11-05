// One row of the Automation tab's tables, with its widgets and click handlers:
//   alarm:   | ALARM NAME [PLATES BACKED UP] | MACHINE EVENT [Output full] | [X]
//   action:  | DO [Pause]                    | ON ALARM [PLATES BACKED UP]  | [X]
// Clicking a cell's button opens a list of choices under the row; pointing at a choice explains it in
// the tab's info panel. While typing an alarm name, the list suggests existing alarms.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/SlateEnums.h"
#include "Automation/AMTypes.h"
#include "AMRuleRow.generated.h"

class UAMAutomationTab;
class UAMUppercaseText;
class UTextBlock;
class UVerticalBox;
class UWidget;

/** Column widths of the two tables (the tab's header row uses them too). */
namespace AMRowLayout
{
	constexpr float AlarmName = 200.f;
	constexpr float MachineEvent = 190.f;
	constexpr float Do = 150.f;
	constexpr float OnAlarm = 180.f;
	constexpr float ValueField = 64.f;
	constexpr float Remove = 36.f;
	constexpr float Gap = 6.f;
	/** Used only if the tab buttons' font cannot be read. */
	constexpr int32 FallbackFontSize = 12;
}

/** What a list under a row offers. */
enum class EAMChoiceKind : uint8
{
	Alarm,
	MachineEvent,
	Action,
	Recipe,
	Sound,
	/** An item (Item count above / below), picked in the tab's item window. */
	Item,
};

/** Click and hover handler of one button in a row's list of choices. */
UCLASS()
class AUTOMATEDMACHINES_API UAMChoice : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<class UAMRuleRow> Row;

	EAMChoiceKind Kind = EAMChoiceKind::Alarm;

	/** The alarm name (Alarm), or the condition / action as a number. */
	FString Alarm;
	uint8 Value = 0;

	/** The recipe (Recipe). */
	UPROPERTY()
	TObjectPtr<UClass> RecipeClass;

	/** The item (Item). */
	UPROPERTY()
	TObjectPtr<UClass> ItemClass;

	UFUNCTION()
	void OnClicked();

	UFUNCTION()
	void OnHovered();
};

UCLASS()
class AUTOMATEDMACHINES_API UAMRuleRow : public UObject
{
	GENERATED_BODY()

public:
	/** Makes the row's widgets for rule Index of the tab (an alarm or an action). */
	void Build(UAMAutomationTab* InTab, bool bInIsAlarm, int32 InIndex);

	UWidget* GetRootWidget() const;

	/** A choice was clicked / pointed at in the list under the row. */
	void Choose(const UAMChoice& Choice);
	void Explain(const UAMChoice& Choice);

	UFUNCTION()
	void OnAlarmNameChanged(const FText& Text);

	UFUNCTION()
	void OnAlarmNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** Alarm row: the Machine Event cell. Action row: the Do cell. */
	UFUNCTION()
	void OnSecondCellClicked();

	/** Action row: the On Alarm cell. */
	UFUNCTION()
	void OnAlarmCellClicked();

	/** Action row with Change recipe: the recipe dropdown under the row. */
	UFUNCTION()
	void OnRecipeCellClicked();

	/** Alarm row with Item count above / below: opens the item window. */
	UFUNCTION()
	void OnItemCellClicked();

	/** Action row with Play sound: the sound dropdown under the row. */
	UFUNCTION()
	void OnSoundCellClicked();

	/** Action row with Show message: the text field under the row. */
	UFUNCTION()
	void OnMessageCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnRemoveClicked();

	/** The percentage field (for machine events / actions that need a number). */
	UFUNCTION()
	void OnValueCommitted(const FText& Text, ETextCommit::Type CommitMethod);

private:
	/** Fills the list under the row (or shows a message if empty). */
	void ShowChoices(EAMChoiceKind Kind, const TArray<FString>& Alarms, const FString& EmptyMessage);
	void HideChoices();

	/** Writes the typed alarm name into the rule and saves. */
	void CommitAlarmName(const FString& Typed);

	/** Sets the name field's text without opening the suggestions. */
	void SetAlarmNameText(const FString& Text);

	void UpdateLabels();

	/** Explains this row's current machine event / action in the info panel. */
	void ExplainCurrent();

	UPROPERTY()
	TObjectPtr<UAMAutomationTab> mTab;

	bool bIsAlarm = true;
	int32 mIndex = INDEX_NONE;

	/** True while we set the field's text ourselves (not the player typing). */
	bool bSettingText = false;

	/** What the list under the row shows right now (to close it on a second click). */
	bool bChoicesOpen = false;
	EAMChoiceKind mOpenKind = EAMChoiceKind::Alarm;

	UPROPERTY()
	TObjectPtr<UVerticalBox> mRoot;

	UPROPERTY()
	TObjectPtr<UAMUppercaseText> mAlarmNameText;

	/** Machine Event (alarm row) or Do (action row) label. */
	UPROPERTY()
	TObjectPtr<UTextBlock> mSecondLabel;

	/** The line with the percentage field, under the row (shown only when needed). */
	UPROPERTY()
	TObjectPtr<UWidget> mValueLine;

	UPROPERTY()
	TObjectPtr<UTextBlock> mValueLabel;

	/** "%" after the number field (empty for a count). */
	UPROPERTY()
	TObjectPtr<UTextBlock> mValueUnit;

	/** The line with the item dropdown, under the alarm row (Item count above / below only). */
	UPROPERTY()
	TObjectPtr<UWidget> mItemLine;

	UPROPERTY()
	TObjectPtr<class UButton> mItemButton;

	UPROPERTY()
	TObjectPtr<class UImage> mItemIcon;

	UPROPERTY()
	TObjectPtr<UTextBlock> mItemLabel;

	UPROPERTY()
	TObjectPtr<UAMUppercaseText> mValueText;

	/** The number info of this row's current machine event / action. */
	AMRules::FValueInfo GetValueInfo() const;

	/** Reads / writes this row's rule's number. */
	float GetRuleValue() const;
	void SetRuleValue(float Value);

	/** The line with the recipe dropdown, under the row (Change recipe only). */
	UPROPERTY()
	TObjectPtr<UWidget> mRecipeLine;

	UPROPERTY()
	TObjectPtr<UTextBlock> mRecipeLabel;

	/** The line with the sound dropdown, under the row (Play sound only). */
	UPROPERTY()
	TObjectPtr<UWidget> mSoundLine;

	UPROPERTY()
	TObjectPtr<UTextBlock> mSoundLabel;

	/** The line with the message text field, under the row (Show message only). */
	UPROPERTY()
	TObjectPtr<UWidget> mMessageLine;

	UPROPERTY()
	TObjectPtr<UAMUppercaseText> mMessageText;

	/** On Alarm label (action row). */
	UPROPERTY()
	TObjectPtr<UTextBlock> mAlarmLabel;

	/** The open dropdown list under the row. */
	UPROPERTY()
	TObjectPtr<UVerticalBox> mChoices;

	UPROPERTY()
	TArray<TObjectPtr<UAMChoice>> mChoiceHandlers;
};
