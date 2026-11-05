// The big bin in the middle of the screen while a bin button is held down (HUB Alarms Overview):
// a faint bin that fills up from the bottom with solid white; when it is full, the removal happens.
//
//      .----.
//     | .... |   faint (transparent white)
//     | #### |   filled part, rising with the hold (2.5 s)
//      '----'
//   Hold to remove FUEL1
//
// Added to the viewport (above everything, not clickable) while held, removed when let go.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AMHoldBin.generated.h"

class UImage;
class USizeBox;
class UCanvasPanelSlot;
class UTextBlock;

UCLASS()
class AUTOMATEDMACHINES_API UAMHoldBin : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 0 = empty (just pressed), 1 = full. */
	void SetProgress(float Progress);

	void SetLabel(const FString& Text);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UImage> mFilled;

	/** The clipping box over the filled bin: its height is the filled part. */
	UPROPERTY()
	TObjectPtr<USizeBox> mClip;

	UPROPERTY()
	TObjectPtr<UTextBlock> mLabel;

	UCanvasPanelSlot* mClipSlot = nullptr;
	UCanvasPanelSlot* mFilledSlot = nullptr;
};
