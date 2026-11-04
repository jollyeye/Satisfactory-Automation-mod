// The "Automation" tab added to the game's machine window (Smelter, Constructor, ...).
//
// SML's widget blueprint hook puts this widget into the window's tab pages (HorizontalBox_1, next to
// "Select Recipe" and "Production"). When it is initialized it also adds a tab button: one more entry
// in the window frame's tab list (mTabs). Clicking that button slides the window to this page.
// The game's widgets are Blueprints, so everything is reached by name through reflection.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AMAutomationTab.generated.h"

class UTextBlock;
class UVerticalBox;

UCLASS()
class AUTOMATEDMACHINES_API UAMAutomationTab : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Bound to the window frame's OnTabButtonClicked. */
	UFUNCTION()
	void OnWindowTabButtonClicked(int32 ButtonIndex);

private:
	/** The machine window (Widget_Manufacturing_C) this page sits in. */
	UUserWidget* GetHostWindow() const;

	/** The window frame inside it (Widget_Window_DarkMode_C), which owns the tab list. */
	UObject* GetWindowFrame() const;

	/** The machine this window is for (the window's mInteractObject), e.g. a Smelter. */
	UObject* GetMachine() const;

	/** Shows the machine's name under the title. */
	void UpdateMachineName();

	/** Builds the page's widgets (placeholder content for now). */
	void BuildLayout();

	/** Adds our entry to the frame's tab list, once. Sets TabIndex. */
	void AddTabEntry();

	/** If the frame already made its tab buttons without ours, makes them again. */
	void EnsureTabButtonExists();

	/** Takes the game's font from a text in the window, so our texts look like the game's. */
	void ApplyGameFonts();

	/** Makes sure the sliding pages know where our page is, then slides to it. */
	void ShowThisTab();

	UTextBlock* MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color);

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
