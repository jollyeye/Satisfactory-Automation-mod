// Rules of Automated Machines: what a machine fires, and how it reacts to events.
//
// Kept small on purpose (they are saved): an event is just its name, stored in each rule. The list
// of all events in the world is every name some rule uses, so an event no rule uses is gone by itself.
#pragma once

#include "CoreMinimal.h"
#include "AMTypes.generated.h"

class AFGBuildable;

/**
 * A status of a machine (a "machine event" in the game). A fire rule fires its event when the status
 * becomes true. New values go at the end only: the values are saved.
 */
UENUM(BlueprintType)
enum class EAMCondition : uint8
{
	OutputFull         UMETA(DisplayName = "Output full"),
	OutputNotFull      UMETA(DisplayName = "Output not full"),
	StartsProducing    UMETA(DisplayName = "Starts producing"),
	StopsProducing     UMETA(DisplayName = "Stops producing"),
	LosesPower         UMETA(DisplayName = "Loses power"),
	GetsPower          UMETA(DisplayName = "Gets power"),
	InputMissing       UMETA(DisplayName = "Input missing"),
	InputAvailable     UMETA(DisplayName = "Input available"),
	Paused             UMETA(DisplayName = "Paused"),
	Resumed            UMETA(DisplayName = "Resumed"),
	ProductivityBelow  UMETA(DisplayName = "Productivity below"),
	ProductivityAbove  UMETA(DisplayName = "Productivity above"),
	RecipeChanged      UMETA(DisplayName = "Recipe changed"),
	Count              UMETA(Hidden)
};

/** What a machine does when it hears an event. New values go at the end only: the values are saved. */
UENUM(BlueprintType)
enum class EAMAction : uint8
{
	Pause          UMETA(DisplayName = "Pause"),
	Resume         UMETA(DisplayName = "Resume"),
	TogglePause    UMETA(DisplayName = "Pause or resume"),
	SetClockSpeed  UMETA(DisplayName = "Set clock speed"),
	Count          UMETA(Hidden)
};

/** "Fire <Event> when <Condition>" */
USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMFireRule
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	FString Event;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	EAMCondition Condition = EAMCondition::OutputFull;

	/** The number of conditions that need one (productivity %), else unused. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	float Value = 0.f;
};

/** "<Action> when event <Event>" */
USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMReactionRule
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	EAMAction Action = EAMAction::Pause;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	FString Event;

	/** The number of actions that need one (clock speed %), else unused. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	float Value = 0.f;
};

/** All rules of one machine. Machines without rules have no entry at all. */
USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMMachineRules
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Automation")
	TObjectPtr<AFGBuildable> Machine = nullptr;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Automation")
	TArray<FAMFireRule> Fires;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Automation")
	TArray<FAMReactionRule> Reactions;

	bool IsEmpty() const { return Fires.Num() == 0 && Reactions.Num() == 0; }
};

namespace AMRules
{
	/** Event names are uppercase, without spaces at the ends. */
	AUTOMATEDMACHINES_API FString NormalizeEventName(const FString& Name);

	/** Text shown in the UI for a condition / an action. */
	AUTOMATEDMACHINES_API FText GetConditionText(EAMCondition Condition);
	AUTOMATEDMACHINES_API FText GetActionText(EAMAction Action);

	/** Explanations for the info panel of the Automation tab. */
	AUTOMATEDMACHINES_API FText GetConditionDescription(EAMCondition Condition);
	AUTOMATEDMACHINES_API FText GetActionDescription(EAMAction Action);

	/**
	 * Conditions / actions that need a number (a percentage). Their label for the number field, its
	 * allowed range and the value a new rule starts with.
	 */
	struct FValueInfo
	{
		bool bNeeded = false;
		const TCHAR* Label = TEXT("");
		float Min = 0.f;
		float Max = 100.f;
		float Default = 0.f;
	};
	AUTOMATEDMACHINES_API FValueInfo GetConditionValueInfo(EAMCondition Condition);
	AUTOMATEDMACHINES_API FValueInfo GetActionValueInfo(EAMAction Action);

	/** The value clamped to the allowed range (or the default if it is not set yet). */
	AUTOMATEDMACHINES_API float CleanValue(const FValueInfo& Info, float Value);
}
