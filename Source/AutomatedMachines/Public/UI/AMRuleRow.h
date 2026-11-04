// One row of the Automation tab's tables, with its widgets and click handlers:
//   alarm:   | ALARM NAME [PLATES BACKED UP] | MACHINE EVENT [Output full] | [X]
//   action:  | DO [Pause]                    | ON ALARM [PLATES BACKED UP]  | [X]
// Clicking a cell's button opens a list of choices under the row; pointing at a choice explains it in
// the tab's info panel. While typing an alarm name, the list suggests existing alarms.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/SlateEnums.h"
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
	constexpr float Do = 130.f;
	constexpr float OnAlarm = 200.f;
	constexpr float Remove = 36.f;
	constexpr float Gap = 6.f;
	constexpr int32 FontSize = 15;
}

/** What a list under a row offers. */
enum class EAMChoiceKind : uint8
{
	Alarm,
	MachineEvent,
	Action,
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

	UFUNCTION()
	void OnRemoveClicked();

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

	/** On Alarm label (action row). */
	UPROPERTY()
	TObjectPtr<UTextBlock> mAlarmLabel;

	/** The open dropdown list under the row. */
	UPROPERTY()
	TObjectPtr<UVerticalBox> mChoices;

	UPROPERTY()
	TArray<TObjectPtr<UAMChoice>> mChoiceHandlers;
};
