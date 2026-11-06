// Lets clients in multiplayer ask the server to change a machine's rules (only the server may).
#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "Automation/AMTypes.h"
#include "AMRemoteCallObject.generated.h"

class AFGBuildable;

UCLASS()
class AUTOMATEDMACHINES_API UAMRemoteCallObject : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void ServerSetRules(AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions, const FAMMachineOptions& Options);

	UFUNCTION(Server, Reliable)
	void ServerSetWorldRules(const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Actions, bool bMam);

	UFUNCTION(Server, Reliable)
	void ServerClearAlarm(const FString& Event, bool bRingers, AFGBuildable* OnlyMachine, bool bOnlyHub, bool bMam);

	/** Removes an alarm's ringers or listeners from the whole world: directly on the server, through the server on a client. */
	static void RequestClearAlarm(APlayerController* Player, const FString& Event, bool bRingers, AFGBuildable* OnlyMachine = nullptr, bool bOnlyHub = false, bool bMam = false);

	UFUNCTION(Server, Reliable)
	void ServerSetSignDesigns(AFGBuildable* Sign, const FAMSignDesign& Normal, const TArray<FAMSignDesign>& Designs);

	/** Changes a sign's designs: directly on the server, through the server on a client. */
	static void RequestSetSignDesigns(APlayerController* Player, AFGBuildable* Sign, const FAMSignDesign& Normal, const TArray<FAMSignDesign>& Designs);

	UFUNCTION(Server, Reliable)
	void ServerSetLightSettings(AFGBuildable* Light, const FAMLightSetting& Normal, const TArray<FAMLightSetting>& Settings);

	/** A light's settings (lights, light control panels): on the server directly, else through the RCO. */
	static void RequestSetLightSettings(APlayerController* Player, AFGBuildable* Light, const FAMLightSetting& Normal, const TArray<FAMLightSetting>& Settings);

	UFUNCTION(Server, Reliable)
	void ServerPreviewLight(AFGBuildable* Light, const FAMLightSetting& Setting);

	/** Gives a light some settings now (its window shows another row of the list). */
	static void RequestPreviewLight(APlayerController* Player, AFGBuildable* Light, const FAMLightSetting& Setting);

	/** Every player gets this object (made on the server when the player controller starts). */
	static void InstallRegistration();

	/**
	 * Changes a machine's rules: directly on the server (single player, host), through the server
	 * on a client.
	 */
	static void RequestSetRules(APlayerController* Player, AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions, const FAMMachineOptions& Options);

	/** Changes the world actions (HUB terminal): directly on the server, through the server on a client. */
	static void RequestSetWorldRules(APlayerController* Player, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Actions, bool bMam = false);

private:
	// An RCO needs at least one replicated property for its RPCs to work.
	UPROPERTY(Replicated)
	bool mDummy = true;
};
