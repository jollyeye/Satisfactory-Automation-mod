// Our pop-up for "Show message": the Wi-Fi icon, the alarm name and the text, in the game's own
// notification list (UFGGameUI::PushNotificationWidget), so it stacks with the game's notifications.
#pragma once

#include "CoreMinimal.h"
#include "FGPushNotificationWidget.h"
#include "AMNotification.generated.h"

class UTextBlock;
class APlayerController;

UCLASS()
class AUTOMATEDMACHINES_API UAMNotification : public UFGPushNotificationWidget
{
	GENERATED_BODY()

public:
	/**
	 * Shows a notification to a local player. Falls back to the game's plain text notification if
	 * the game does not take our widget.
	 */
	static void Show(APlayerController* Player, const FString& Title, const FString& Body);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	void SetContent(const FString& Title, const FString& Body, const struct FSlateFontInfo& GameFont);

	/** Removes the notification after it was shown long enough. */
	void Expire();

	UPROPERTY()
	TObjectPtr<UTextBlock> mTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> mBody;
};
