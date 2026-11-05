// Alarm designs in the Sign Designer: one tab per design in the window's tab bar.
//
//   [ Sign Designer ][ PLATES BACKED UP x ][ PLATES FLOWING x ][ + Alarm ]
//
// "Sign Designer" is the sign's normal design; each other tab is the design the sign shows when its
// alarm rings. The game's own designer below edits whichever tab is selected: switching tabs keeps
// the edited design and loads the next one into it ("Set Sign Data"). "+ Alarm" picks an alarm (or a
// new name) and makes a design for it, starting as a copy of the one being edited.
// The designs are sent to the server when they change and when the window closes; the server then
// shows on the sign the design it should show (its last rung alarm's, or the normal one).
//
// This widget is hooked into the Sign Designer's page (collapsed); it only shows itself as the
// alarm picker floating over the page.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Automation/AMTypes.h"
#include "AMSignTabs.generated.h"

class UAMSignTabs;
class UCanvasPanel;
class UVerticalBox;
class UEditableText;

/** Click handler of one tab button, or of one tab's bin. */
UCLASS()
class AUTOMATEDMACHINES_API UAMSignTabHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UAMSignTabs> Owner;

	int32 Index = 0;

	/** The alarm of a choice in the picker. */
	FString Alarm;

	UFUNCTION()
	void OnTabClicked();

	UFUNCTION()
	void OnRemoveClicked();

	UFUNCTION()
	void OnAlarmChosen();
};

UCLASS()
class AUTOMATEDMACHINES_API UAMSignTabs : public UUserWidget
{
	GENERATED_BODY()

public:
	void SelectTab(int32 Index);
	void RemoveDesign(int32 Index);
	void AddDesign(const FString& Alarm);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnPickerBackgroundClicked();

	UFUNCTION()
	void OnPickerSearchChanged(const FText& Text);

	UFUNCTION()
	void OnPickerSearchCommitted(const FText& Text, ETextCommit::Type CommitMethod);

private:
	/** On the tick after construct (retried while the window has not filled its tab list yet). */
	void Setup();

	/** Tab list = normal + one per design + "+ Alarm"; makes the buttons again. */
	void RebuildTabs();

	UUserWidget* GetHostWindow() const;
	UObject* GetWindowFrame() const;
	class AFGBuildableWidgetSign* GetSign() const;

	/** The design being edited in the window, and loading one into it. */
	bool ReadEditor(FAMSignDesign& OutDesign, const FString& Event) const;
	void LoadEditor(const FAMSignDesign& Design);

	/** Keeps the edited design in its tab's slot. */
	void CaptureEditor();

	/** Sends the designs to the server. */
	void SendDesigns();

	FAMSignDesign& GetSlot(int32 Index);

	void OpenPicker(UWidget* Anchor);
	void ClosePicker();
	void FillPicker(const FString& Filter);

	FSlateFontInfo GetFont() const;

	FAMSignDesign mNormal;
	TArray<FAMSignDesign> mDesigns;

	/** 0 = normal, 1.. = mDesigns[Index - 1]. */
	int32 mSelected = 0;

	/** The window's first tab entry (its title), copied for every tab. */
	TArray<uint8> mFirstEntry;
	FText mWindowTitle;

	bool bSetUp = false;
	bool bHadDesigns = false;
	int32 mSetupTries = 0;

	UPROPERTY()
	TArray<TObjectPtr<UAMSignTabHandler>> mHandlers;

	UPROPERTY()
	TArray<TObjectPtr<UAMSignTabHandler>> mPickerHandlers;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> mLayer;

	UPROPERTY()
	TObjectPtr<UVerticalBox> mPickerList;

	UPROPERTY()
	TObjectPtr<UEditableText> mPickerSearch;

	TArray<FString> mPickerAlarms;
};
