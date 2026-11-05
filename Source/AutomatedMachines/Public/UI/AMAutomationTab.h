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

/** A row's buttons in the HUB's Automation Overview: the alarm's name, and its two bins. */
UCLASS()
class AUTOMATEDMACHINES_API UAMOverviewHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<class UAMAutomationTab> Tab;

	FString Event;
	bool bRingers = true;

	UFUNCTION()
	void OnBinClicked();

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

	/** The info panel on the right of the page. */
	void ShowInfo(const FText& Title, const FText& Body);

	/** Info about an alarm: who rings it and who acts on it (or how alarm names work, if empty). */
	void ShowAlarmInfo(const FString& Alarm);

	/** What the page is about (shown until something is picked). */
	void ShowDefaultInfo();

	/** The machine this window is for (e.g. a Smelter). */
	UObject* GetMachineObject() const { return GetMachine(); }

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

	/** Overview: a bin was clicked (once: asks to click again; twice within a few seconds: clears). */
	void OnOverviewBin(const FString& Event, bool bRingers);
	void ExplainOverviewBin(const FString& Event, bool bRingers);

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
	 * The HUB's second page, "Automation Overview" (this class, named AM_OverviewTab by its hook):
	 * a table of every alarm in the world with who rings it and who listens.
	 */
	bool bOverviewMode = false;

	/** Our tab's title: "Automation" or "Automation Overview". */
	const TCHAR* GetTabTitle() const;

	/** Our tab's index in the window's tab list, by title (two pages of ours in one window can move it). */
	void RefreshTabIndex();

	/** Overview: the rows, made again whenever any rules change. */
	void RebuildOverview();

	UPROPERTY()
	TObjectPtr<UVerticalBox> mOverviewRows;

	UPROPERTY()
	TArray<TObjectPtr<UAMOverviewHandler>> mOverviewHandlers;

	/** Overview: the bin clicked once, waiting for a second click. */
	FString mPendingClearEvent;
	bool bPendingClearRingers = false;
	double mPendingClearTime = 0.0;

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
