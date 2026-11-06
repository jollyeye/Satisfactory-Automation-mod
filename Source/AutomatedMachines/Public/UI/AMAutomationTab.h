// The "Automation" tab added to the game's machine window (Smelter, Constructor, ...).
//
// SML's widget blueprint hook puts this widget into the window's tab pages (HorizontalBox_1, next to
// "Select Recipe" and "Production"). When it is initialized it also adds a tab button: one more entry
// in the window frame's tab list (mTabs). Clicking that button slides the window to this page.
// The game's widgets are Blueprints, so everything is reached by name through reflection.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Automation/AMTypes.h"
#include "AMAutomationTab.generated.h"

class UTextBlock;
class UVerticalBox;
class UWidgetTree;
class UAMRuleRow;
class UAMChoice;
class UTexture2D;
class AFGBuildable;

/** One choice of a dropdown window (UAMAutomationTab::OpenPicker). */
struct FAMPickerOption
{
	FString Label;
	/** Shown in front of the label (items), or none. */
	UTexture2D* Icon = nullptr;
	/** The current choice: shown in orange. */
	bool bCurrent = false;
	/** Its click and hover handler (kept alive by the tab while the window is open). */
	UAMChoice* Handler = nullptr;
};

/** A row's buttons in the HUB's Alarms Overview: the alarm's name, and its two bins. */
UCLASS()
class AUTOMATEDMACHINES_API UAMOverviewHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<class UAMAutomationTab> Tab;

	FString Event;
	bool bRingers = true;

	/** A row of an alarm's machine list: its machine (or the HUB); its bin removes the alarm from it only. */
	TWeakObjectPtr<AFGBuildable> Machine;
	bool bHub = false;
	bool bMam = false;
	bool bOne = false;
	FString TargetName;

	UFUNCTION()
	void OnBinPressed();

	/** A count cell (Rung by / Listening): opens or closes its machine list. */
	UFUNCTION()
	void OnCountClicked();

	UFUNCTION()
	void OnCountHovered();

	/** A machine row's locate button. */
	UFUNCTION()
	void OnLocateClicked();

	UFUNCTION()
	void OnLocateHovered();

	/** A machine row's bin: removes at once (one machine only: no hold). */
	UFUNCTION()
	void OnSingleBinClicked();

	UFUNCTION()
	void OnBinReleased();

	UFUNCTION()
	void OnBinHovered();

	UFUNCTION()
	void OnNameHovered();
};

UCLASS()
class AUTOMATEDMACHINES_API UAMAutomationTab : public UUserWidget
{
	GENERATED_BODY()

public:
	// Used by the rows (UAMRuleRow).
	UWidgetTree* GetTree() const { return WidgetTree; }

	/**
	 * A list floating on the popup layer right under the anchor (e.g. alarm name suggestions under
	 * the name field), without the click-catching background of the dropdown window: the field keeps
	 * the focus and everything else stays clickable. Hide removes it again.
	 */
	void ShowFloatingList(UWidget* List, UWidget* Anchor);
	void HideFloatingList(UWidget* List);
	FSlateFontInfo GetGameFont() const;

	/** The font of the window's tab buttons ("Select Recipe"...): used for the rows, so they match. */
	FSlateFontInfo GetRowFont() const;
	FAMFireRule* GetFireRule(int32 Index) { return mFires.IsValidIndex(Index) ? &mFires[Index] : nullptr; }
	FAMReactionRule* GetReactionRule(int32 Index) { return mReactions.IsValidIndex(Index) ? &mReactions[Index] : nullptr; }

	/** Every event name: those in the world plus those typed here and not saved yet. */
	TArray<FString> GetEventNames() const;

	/** A row changed a rule: send this machine's rules to the server. */
	void OnRulesEdited();

	/** Removes a rule, saves, and rebuilds the rows (on the next frame). */
	void RemoveRule(bool bFire, int32 Index);

	/**
	 * Actions grouped by alarm: one more action for the alarm (the first one that does not clash
	 * with the alarm's others), all of an alarm's actions moved to another alarm, or removed.
	 */
	void AddActionToGroup(const FString& Event, EAMAction Action = EAMAction::Count);
	void RenameActionGroup(const FString& OldEvent, const FString& NewEvent);
	void RemoveActionGroup(const FString& Event);
	bool HasActionGroup(const FString& Event) const;

	/** Would this action clash with another action of the same alarm as action Index (AMRules::ActionsConflict)? */
	bool ActionClashesInGroup(int32 Index, EAMAction Action) const;

	/** The same for a new action of the alarm Event (none of its actions excluded). */
	bool ActionClashesWithAlarm(const FString& Event, EAMAction Action) const;

	/** Can this page have this action (machines: machine actions; the HUB: world actions)? */
	bool IsActionAvailableHere(EAMAction Action) const;

	/** The bin of an alarm with several actions is held down (like the overview's bins). */
	void StartGroupHold(const FString& Event, int32 Count);

	/** The info panel on the right of the page. */
	void ShowInfo(const FText& Title, const FText& Body);

	/** Info about an alarm: who rings it and who acts on it (or how alarm names work, if empty). */
	void ShowAlarmInfo(const FString& Alarm);

	/** What the page is about (shown until something is picked). */
	void ShowDefaultInfo();

	/** The machine this window is for (e.g. a Smelter). */
	UObject* GetMachineObject() const { return GetMachine(); }

	/** Can an alarm row here use this event? The HUB: world events only; a machine: its own events. */
	bool IsConditionAvailableHere(EAMCondition Condition) const;

	/**
	 * A dropdown window floating over the page: a search field exactly over the clicked dropdown
	 * (Anchor), with the choices under it in a scrolling list. Typing filters the list; Enter picks
	 * the first one; a click outside closes it.
	 */
	void OpenPicker(UAMRuleRow* Row, UWidget* Anchor, TArray<FAMPickerOption>&& Options, const FString& EmptyMessage, float MinWidth);

	/** Closes the dropdown window (on the next tick when bDeferred: safe from inside its own click). */
	void ClosePicker(bool bDeferred = false);

	/** Items the player knows: this container's contents first, then items of unlocked recipes, by name; solids only. */
	TArray<UClass*> GetPickableItems() const;

	/**
	 * True in the HUB terminal: the page has only actions, and only world actions (they need no
	 * machine), saved for the whole world instead of for a machine.
	 */
	bool IsWorldMode() const { return bWorldMode; }

	/** Overview: a bin is held down (2.5 s with the big bin filling up in the middle of the screen: clears), or let go. */
	void StartOverviewHold(const FString& Event, bool bRingers, AFGBuildable* OnlyMachine = nullptr, bool bOnlyHub = false, bool bOne = false, const FString& TargetName = FString());
	void StopOverviewHold(bool bDone);
	void ExplainOverviewBin(const FString& Event, bool bRingers, bool bOne = false, const FString& TargetName = FString());

	/** Overview: a count cell clicked (its list opens; clicked again, closes; another one, swaps). */
	void OnOverviewCountClicked(const FString& Event, bool bRingers);
	void ExplainOverviewCount(const FString& Event, bool bRingers);

	/** Overview: a machine's locate button (finding it in the world comes next). */
	void OnOverviewLocate(AFGBuildable* Machine, bool bHub, const FString& TargetName);
	void ExplainOverviewLocate(const FString& TargetName);

	/** Overview: removes the alarm from one machine (or the HUB) at once. */
	void RemoveFromOne(const FString& Event, bool bRingers, AFGBuildable* Machine, bool bHub, bool bMam, const FString& TargetName);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Bound to the window frame's OnTabButtonClicked. */
	UFUNCTION()
	void OnWindowTabButtonClicked(int32 ButtonIndex);

	/** Bound to the sliding pages' OnActiveTabChanged. */
	UFUNCTION()
	void OnActiveTabChanged(UWidget* PreviouslyActiveWidget, UWidget* ActiveWidget);

	UFUNCTION()
	void OnAddFireClicked();

	UFUNCTION()
	void OnAddReactionClicked();

	UFUNCTION()
	void OnPassWrongClicked();

	UFUNCTION()
	void OnPassExtraClicked();

	/** Our own Copy / Paste automation settings buttons (machines without the game's). */
	UFUNCTION()
	void OnCopyAutomationClicked();

	UFUNCTION()
	void OnPasteAutomationClicked();

	UFUNCTION()
	void OnCopyPasteHovered();

	/** A click outside the dropdown window closes it. */
	UFUNCTION()
	void OnPopupBackgroundClicked();

	UFUNCTION()
	void OnPickerSearchChanged(const FText& Text);

	UFUNCTION()
	void OnPickerSearchCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/**
	 * One-page windows: bound to the plain button inside each tab button (mButton), because neither
	 * the frame nor the tab button passes the click on there.
	 */
	UFUNCTION()
	void OnMachineTabPressed();

	UFUNCTION()
	void OnAutomationTabPressed();

	/** One-page windows: shows page Index (0 = the machine's) and moves the highlight. */
	void SelectSingleTab(int32 Index);

	UFUNCTION()
	void OnPassWrongHovered();

	UFUNCTION()
	void OnPassExtraHovered();

private:
	/** The machine window (Widget_Manufacturing_C) this page sits in. */
	UUserWidget* GetHostWindow() const;

	/** The window frame inside it (Widget_Window_DarkMode_C), which owns the tab list. */
	UObject* GetWindowFrame() const;

	/** The machine this window is for (the window's mInteractObject), e.g. a Smelter. */
	UObject* GetMachine() const;

	/** Shows the machine's name under the title. */
	void UpdateMachineName();

	/** Builds the page's fixed widgets (titles, sections, add buttons). */
	void BuildLayout();

	/** Reads this machine's rules from the subsystem, once the machine is known. */
	void LoadRules();

	/** Makes the rows again from mFires / mReactions. */
	void RebuildRows();

	/** Remembers a text of ours, so ApplyGameFonts gives it the game font at this size. */
	void TrackText(UTextBlock* Text, int32 FontSize);

	/** Adds our entry to the frame's tab list, once. Sets TabIndex. */
	void AddTabEntry();

	/** If the frame already made its tab buttons without ours, makes them again. */
	void EnsureTabButtonExists();

	/** Takes the game's font from a text in the window, so our texts look like the game's. */
	void ApplyGameFonts();

	/** Makes sure the sliding pages know where our page is, then slides to it. */
	void ShowThisTab();

	/** Our page has its own info panel, so the window's inventory panel is hidden on it. */
	void HideInventory();

	/** Shows or hides the window's inventory panel. */
	void SetInventoryShown(bool bShow);

	/**
	 * One-page windows (miners): we move next to the window body (in the frame's row, beside the
	 * inventory) and add our tab; the tab buttons show the body or us. The machine's page itself is
	 * never moved (moving it rebuilt its widgets, which then showed their designer defaults).
	 * On the tick after construct, when the window has filled its tab list.
	 */
	void SetupSinglePage();

	/** Makes the tab buttons again from the frame's tab list (InitTabs, else the tabs container). */
	void RegenerateTabButtons();

	/** Gives our tab button its title and icon (one-tab windows give every button the window's). */
	void SetOurTabButtonLook();

	/** One-page windows: listens to every tab button's OnClicked. */
	void BindSingleTabButtons();

	/** One-page windows: shows our page (true) or the machine's own page (false). */
	void ShowSinglePage(bool bAutomation);

	/** True in windows with one page and no sliding pages (miners). */
	bool bSinglePageMode = false;

	/** SetupSinglePage tries again on the next tick while the window's tab list is still empty. */
	int32 mSetupTries = 0;

	/** One-page windows: the window body (holding the machine's page), shown or hidden. */
	UPROPERTY()
	TObjectPtr<UWidget> mMachinePage;

	ESlateVisibility mMachinePageVisibility = ESlateVisibility::SelfHitTestInvisible;

	/** One-page windows: the machine page's height, measured while it shows (ours gets the same). */
	float mMachinePageHeight = 0.f;

	UTextBlock* MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color);

	/** Shows the options' state (On / Off) on their buttons. */
	void RefreshOptions();

	/** Info panel text for "Pass wrong items through". */
	void ShowPassWrongInfo();
	void ShowPassExtraInfo();

	/** This machine's rules as edited here (sent to the server on every change). */
	TArray<FAMFireRule> mFires;
	TArray<FAMReactionRule> mReactions;
	FAMMachineOptions mOptions;
	bool bRulesLoaded = false;

	UPROPERTY()
	TObjectPtr<class UButton> mPassWrongButton;

	/** The Options section (title and box): hidden on machines without options (miners). */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> mOptionsWidgets;

	/** The Copy / Paste automation settings buttons (shown when the game has no copy for this machine). */
	UPROPERTY()
	TObjectPtr<UWidget> mClipboardRow;

	/** Rules changed in the world (a paste, another player): read them again if they differ from ours. */
	void OnWorldRulesChanged();
	FDelegateHandle mRulesChangedHandle;

	/** The switch picture on the option's row. */
	UPROPERTY()
	TObjectPtr<class UImage> mPassWrongSwitch;

	/** "Pass extra items through": its row and switch picture. */
	UPROPERTY()
	TObjectPtr<class UButton> mPassExtraButton;
	UPROPERTY()
	TObjectPtr<class UImage> mPassExtraSwitch;

	FSlateFontInfo mGameFont;
	bool bHasGameFont = false;

	UPROPERTY()
	TArray<TObjectPtr<UAMRuleRow>> mRows;

	UPROPERTY()
	TObjectPtr<UVerticalBox> mFireRowsBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> mReactionRowsBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> mNoticeText;

	/**
	 * Power switches: a warning while a cable elsewhere joins Power Grid A and B into one grid (both
	 * sides on the same circuit). Checked every second while the tab is open.
	 */
	UPROPERTY()
	TObjectPtr<UTextBlock> mGridWarningText;
	FTimerHandle mGridCheckTimer;
	void CheckJoinedGrids();

	/** Labels of the "+ Add" buttons (they get the row font). */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mAddLabels;

	UPROPERTY()
	TObjectPtr<UTextBlock> mInfoTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> mInfoBody;

	bool bBoundToPages = false;

	bool bWorldMode = false;

	/**
	 * The HUB's second page, "Alarms Overview" (this class, named AM_OverviewTab by its hook):
	 * a table of every alarm in the world with who rings it and who listens.
	 */
	bool bOverviewMode = false;

	/** The MAM's tab (a world tab like the HUB's): its world events' alarms only, no actions. */
	bool bMamMode = false;

	/**
	 * Sliding pages (production window, HUB): our page as high as the game's tallest page, so our
	 * lists scroll instead of making the window (and with it every tab) taller.
	 */
	void UpdatePageHeight();

	/** After "+ Add alarm" / "+ Add action": the list scrolls to the new row (its last). */
	void ScrollToEndLater(UWidget* Rows);

	/** Our tab's title: "Automation" or "Alarms Overview". */
	const TCHAR* GetTabTitle() const;

	/** Our tab's index in the window's tab list, by title (two pages of ours in one window can move it). */
	void RefreshTabIndex();

	/** Overview: the rows, made again whenever any rules change. */
	void RebuildOverview();

	UPROPERTY()
	TObjectPtr<UVerticalBox> mOverviewRows;

	UPROPERTY()
	TArray<TObjectPtr<UAMOverviewHandler>> mOverviewHandlers;

	/** Overview: the bin being held down, since when, and the big bin shown meanwhile. */
	FString mHoldEvent;
	bool bHoldRingers = false;
	bool bHolding = false;
	double mHoldStart = 0.0;
	FTimerHandle mHoldTimer;
	void TickOverviewHold();

	UPROPERTY()
	TObjectPtr<class UAMHoldBin> mHoldBin;

	/** Rules changed while a bin was held: the rows are made again when it is let go. */
	bool bOverviewDirty = false;

	/** The bin held is an alarm's group of actions on this page (removed here, not in the world). */
	bool bHoldGroup = false;

	/** The bin held is a machine row's: only that machine (or the HUB). */
	TWeakObjectPtr<AFGBuildable> mHoldMachine;
	bool bHoldHub = false;
	bool bHoldOne = false;
	FString mHoldTargetName;

	/**
	 * Overview: the open machine list (one at a time), under its alarm's row: the machines ringing
	 * it (Rung by) or listening to it (Listening), nearest first. Its rows are made in batches (a
	 * few frames for thousands of machines) behind a progress bar.
	 */
	FString mExpandedEvent;
	bool bExpandedRingers = true;
	struct FAMOverviewEntry
	{
		TWeakObjectPtr<AFGBuildable> Machine;
		/** The HUB or the MAM (a world entry: not located). */
		bool bHub = false;
		bool bMam = false;
		FString Name;
		TWeakObjectPtr<UTexture2D> Icon;
		float Distance = 0.f;
	};
	TArray<FAMOverviewEntry> mExpandedEntries;
	int32 mExpandedBuilt = 0;
	int32 mExpandGeneration = 0;

	/** The Machines region on the right of the alarms (the open list, or how to open one). */
	UPROPERTY()
	TObjectPtr<UVerticalBox> mMachinesBox;

	/** Fills the Machines region: the open list, or the message saying how to open one. */
	void RebuildMachinesRegion();

	UPROPERTY()
	TObjectPtr<UVerticalBox> mExpandedRows;

	UPROPERTY()
	TObjectPtr<class UProgressBar> mExpandedProgress;

	UPROPERTY()
	TObjectPtr<UTextBlock> mExpandedProgressText;

	UPROPERTY()
	TObjectPtr<UWidget> mExpandedLoading;

	/** Adds the open list to the Machines region (collects its machines, then makes the first rows). */
	void AddExpandedList();

	/** Makes the next batch of the open list's rows (again next tick until all are made). */
	void BuildExpandedRows();

	/** Index of our tab button and page; INDEX_NONE until the tab entry is added. */
	int32 TabIndex = INDEX_NONE;

	bool bBoundToFrame = false;

	UPROPERTY()
	TObjectPtr<UVerticalBox> mContentBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> mMachineText;

	/** Over the whole page: the item window and a see-through background that closes it. */
	UPROPERTY()
	TObjectPtr<class UOverlay> mRootOverlay;

	UPROPERTY()
	TObjectPtr<class UCanvasPanel> mPopupLayer;

	UPROPERTY()
	TObjectPtr<UWidget> mPopup;

	UPROPERTY()
	TArray<TObjectPtr<UAMChoice>> mPopupHandlers;

	/** The open dropdown window's choices, its list and search field. */
	TArray<FAMPickerOption> mPickerOptions;

	UPROPERTY()
	TObjectPtr<UVerticalBox> mPickerList;

	UPROPERTY()
	TObjectPtr<class UEditableText> mPickerSearch;

	FString mPickerEmptyMessage;

	/** Shows the choices whose label contains Filter (any case). */
	void FillPickerList(const FString& Filter);

	/** Our texts with the font size each should get (fonts are copied from the game in ApplyGameFonts). */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mTexts;

	TArray<int32> mTextSizes;
};
