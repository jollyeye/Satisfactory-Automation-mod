// Alarm designs in the Sign Designer: one tab per design in the window's tab bar.
//
//   [ Default ][ PLATES BACKED UP x ][ PLATES FLOWING x ][ + AutoDesign ]
//
// "Default" is the sign's normal design; each other tab is the design the sign shows when its
// alarm rings. The game's own designer below edits whichever tab is selected: switching tabs keeps
// the edited design and loads the next one into it ("Set Sign Data"). "+ AutoDesign" picks an alarm (or a
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

	/**
	 * The game's Copy Settings of a sign (window button or Ctrl+C) copies the design stored on the
	 * sign, which is the Default one. While this sign's designer is open, the copy gets the design
	 * open in the editor instead (whichever tab is selected, with its unsaved changes): copied into
	 * the clipboard object's mPrefabSignData. Returns true if it did.
	 */
	static bool CopyOpenEditor(const class AFGBuildableWidgetSign* Sign, UObject* ClipboardSettings);

	/**
	 * After the game's factory clipboard copied / pasted an object (AFGPlayerState::CopyFactoryClipboard
	 * / PasteFactoryClipboard: every copy and paste goes through them). A sign whose designer is open:
	 * the copy gets the editor's design (CopyOpenEditor); both are logged with their texts, and after
	 * a paste the editor's texts a moment later (to find where a text goes missing).
	 */
	static void OnGameClipboardCopied(UObject* Object, UObject* PlayerState);
	static void OnGameClipboardPasted(UObject* Object, UObject* PlayerState);

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

public:
	/** Our tab row: Normal + one per design + "+ AutoDesign" (made again on every change). */
	void RebuildTabs();

private:

	/**
	 * While the window is open (4 times a second): the window is reused for other signs and may
	 * make its tab buttons again after us (then our clicks were bound to buttons that are gone).
	 * Another sign: set up again for it. New buttons: make ours again.
	 */
	void Watch();

	/** The window's tab list back to its own single tab (when it closes). */
	void RestoreWindowTabs();

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

	/** The sign the tabs were set up for, and the tab buttons our clicks are bound to. */
	TWeakObjectPtr<class AFGBuildableWidgetSign> mSetupSign;
	TArray<TWeakObjectPtr<UWidget>> mBoundButtons;
	FTimerHandle mWatchTimer;
	bool bHadDesigns = false;
	int32 mSetupTries = 0;

	UPROPERTY()
	TArray<TObjectPtr<UAMSignTabHandler>> mHandlers;

	/**
	 * Our own row of tabs in the window's title bar, and its "+ AutoDesign" button (where the picker
	 * opens): [Default][ alarm tabs, scrolling sideways ][+ AutoDesign], filling the title bar's width.
	 */
	UPROPERTY()
	TObjectPtr<class UHorizontalBox> mTabRow;

	/** The alarm tabs' scroll box (its offset is kept when the row is made again). */
	UPROPERTY()
	TObjectPtr<class UScrollBox> mTabScroll;
	float mTabScrollOffset = 0.f;

	UPROPERTY()
	TObjectPtr<class UButton> mAddButton;

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
