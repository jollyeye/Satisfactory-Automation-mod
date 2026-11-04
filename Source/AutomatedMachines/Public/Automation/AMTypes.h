// Rules of Automated Machines: what a machine fires, and how it reacts to events.
//
// Kept small on purpose (they are saved): an event is just its name, stored in each rule. The list
// of all events in the world is every name some rule uses, so an event no rule uses is gone by itself.
#pragma once

#include "CoreMinimal.h"
#include "AMTypes.generated.h"

class AFGBuildable;

/** A status of a machine. A fire rule fires its event when the status becomes true. */
UENUM(BlueprintType)
enum class EAMCondition : uint8
{
	OutputFull       UMETA(DisplayName = "Output full"),
	OutputNotFull    UMETA(DisplayName = "Output not full"),
	StartsProducing  UMETA(DisplayName = "Starts producing"),
	StopsProducing   UMETA(DisplayName = "Stops producing"),
	Count            UMETA(Hidden)
};

/** What a machine does when it hears an event. */
UENUM(BlueprintType)
enum class EAMAction : uint8
{
	Pause   UMETA(DisplayName = "Pause"),
	Resume  UMETA(DisplayName = "Resume"),
	Count   UMETA(Hidden)
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
}
