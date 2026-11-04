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

UCLASS()
class AUTOMATEDMACHINES_API UAMAutomationTab : public UUserWidget
{
	GENERATED_BODY()

public:
	// Used by the rows (UAMRuleRow).
	UWidgetTree* GetTree() const { return WidgetTree; }
	FSlateFontInfo GetGameFont() const;
	FAMFireRule* GetFireRule(int32 Index) { return mFires.IsValidIndex(Index) ? &mFires[Index] : nullptr; }
	FAMReactionRule* GetReactionRule(int32 Index) { return mReactions.IsValidIndex(Index) ? &mReactions[Index] : nullptr; }

	/** Every event name: those in the world plus those typed here and not saved yet. */
	TArray<FString> GetEventNames() const;

	/** A row changed a rule: send this machine's rules to the server. */
	void OnRulesEdited();

	/** Removes a rule, saves, and rebuilds the rows (on the next frame). */
	void RemoveRule(bool bFire, int32 Index);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Bound to the window frame's OnTabButtonClicked. */
	UFUNCTION()
	void OnWindowTabButtonClicked(int32 ButtonIndex);

	UFUNCTION()
	void OnAddFireClicked();

	UFUNCTION()
	void OnAddReactionClicked();

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

	UTextBlock* MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color);

	/** This machine's rules as edited here (sent to the server on every change). */
	TArray<FAMFireRule> mFires;
	TArray<FAMReactionRule> mReactions;
	bool bRulesLoaded = false;

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

	/** Index of our tab button and page; INDEX_NONE until the tab entry is added. */
	int32 TabIndex = INDEX_NONE;

	bool bBoundToFrame = false;

	UPROPERTY()
	TObjectPtr<UVerticalBox> mContentBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> mMachineText;

	/** Our texts with the font size each should get (fonts are copied from the game in ApplyGameFonts). */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mTexts;

	TArray<int32> mTextSizes;
};
