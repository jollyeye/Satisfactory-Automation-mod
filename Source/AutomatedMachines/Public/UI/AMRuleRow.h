// One row of the Automation tab, with its widgets and click handlers:
//   fire rule:  Fire [EVENT NAME    ] when [Output full]   [X]
//   reaction:   [Pause] when event [PLATES BACKED UP]      [X]
// Under the row, a list of event names: suggestions while typing (fire rule) or all events
// (reaction, after clicking the event button).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/SlateEnums.h"
#include "AMRuleRow.generated.h"

class UAMAutomationTab;
class UEditableText;
class UTextBlock;
class UVerticalBox;
class UWrapBox;
class UWidget;

/** Click handler of one event name button in a row's list. */
UCLASS()
class AUTOMATEDMACHINES_API UAMEventChoice : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<class UAMRuleRow> Row;

	FString Event;

	UFUNCTION()
	void OnClicked();
};

UCLASS()
class AUTOMATEDMACHINES_API UAMRuleRow : public UObject
{
	GENERATED_BODY()

public:
	/** Makes the row's widgets for rule Index of the tab (a fire rule or a reaction). */
	void Build(UAMAutomationTab* InTab, bool bInIsFire, int32 InIndex);

	UWidget* GetRootWidget() const;

	/** An event name was picked from the list under the row. */
	void ChooseEvent(const FString& Event);

	UFUNCTION()
	void OnEventTextChanged(const FText& Text);

	UFUNCTION()
	void OnEventTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnCycleClicked();

	UFUNCTION()
	void OnPickEventClicked();

	UFUNCTION()
	void OnRemoveClicked();

private:
	/** Fills the list under the row with these names (or hides it if there are none). */
	void ShowChoices(const TArray<FString>& Events, const FString& EmptyMessage);
	void HideChoices();

	/** Writes the typed event name into the fire rule and saves. */
	void CommitEventName(const FString& Typed);

	void UpdateLabels();

	UPROPERTY()
	TObjectPtr<UAMAutomationTab> mTab;

	bool bIsFire = true;
	int32 mIndex = INDEX_NONE;

	/** True while we set the field's text ourselves (not the player typing). */
	bool bSettingText = false;

	/** Sets the event field's text without opening the suggestions. */
	void SetEventText(const FString& Text);

	UPROPERTY()
	TObjectPtr<UVerticalBox> mRoot;

	UPROPERTY()
	TObjectPtr<UEditableText> mEventText;

	UPROPERTY()
	TObjectPtr<UTextBlock> mCycleLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> mPickLabel;

	UPROPERTY()
	TObjectPtr<UWrapBox> mChoices;

	UPROPERTY()
	TArray<TObjectPtr<UAMEventChoice>> mChoiceHandlers;
};
