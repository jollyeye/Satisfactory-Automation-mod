// Rules of Automated Machines: what a machine fires, and how it reacts to events.
//
// Kept small on purpose (they are saved): an event is just its name, stored in each rule. The list
// of all events in the world is every name some rule uses, so an event no rule uses is gone by itself.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "AMTypes.generated.h"

class AFGBuildable;
class UFGRecipe;
class UFGItemDescriptor;

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
	InputFull          UMETA(DisplayName = "Input full"),
	InputNotFull       UMETA(DisplayName = "Input not full"),
	WrongInput         UMETA(DisplayName = "Wrong input"),
	InputCorrect       UMETA(DisplayName = "Input correct"),
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
	DeleteOutput   UMETA(DisplayName = "Delete output"),
	DeleteInput    UMETA(DisplayName = "Delete input"),
	ChangeRecipe   UMETA(DisplayName = "Change recipe"),
	ShowMessage    UMETA(DisplayName = "Show message"),
	PlaySound      UMETA(DisplayName = "Play sound"),
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

	/** The recipe of "Change recipe", else unused. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	TSubclassOf<UFGRecipe> Recipe;

	/** The text of "Show message" (empty: machine and alarm), else unused. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	FString Message;
};

/** A wrong item taken off a machine's input belt, waiting to leave on its output belt (saved). */
USTRUCT()
struct AUTOMATEDMACHINES_API FAMHeldItem
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TObjectPtr<AFGBuildable> Machine;

	UPROPERTY(SaveGame)
	TSubclassOf<UFGItemDescriptor> Item;
};

/** Switches of a machine (the Options section of its Automation tab). */
USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMMachineOptions
{
	GENERATED_BODY()

	/**
	 * A wrong item blocking an input belt is taken off the belt and put in the empty output slot,
	 * so the output belt carries it away and the input belt moves again.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	bool bPassWrongItems = false;

	bool IsDefault() const { return !bPassWrongItems; }
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

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Automation")
	FAMMachineOptions Options;

	bool IsEmpty() const { return Fires.Num() == 0 && Reactions.Num() == 0 && Options.IsDefault(); }
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

	/**
	 * Machine events / actions this machine has (Machine null: all of them). Production machines
	 * have all; miners have no input and no recipe.
	 */
	AUTOMATEDMACHINES_API bool IsConditionAvailable(EAMCondition Condition, const UObject* Machine);
	AUTOMATEDMACHINES_API bool IsActionAvailable(EAMAction Action, const UObject* Machine);

	/** True if the machine has the Options section's "Pass wrong items through" (it has input belts). */
	AUTOMATEDMACHINES_API bool HasPassWrongItems(const UObject* Machine);

	/** Actions that need a recipe (Change recipe). */
	AUTOMATEDMACHINES_API bool ActionNeedsRecipe(EAMAction Action);

	/** Actions that have a text (Show message). */
	AUTOMATEDMACHINES_API bool ActionNeedsMessage(EAMAction Action);

	/**
	 * World actions do nothing to a machine (Show message): they are set in the HUB terminal's
	 * Automation tab, not on machines.
	 */
	AUTOMATEDMACHINES_API bool IsWorldAction(EAMAction Action);

	/** Longest message text kept. */
	constexpr int32 MaxMessageLength = 120;

	/** Actions that play one of the sounds below (Play sound); the sound's number is the rule's Value. */
	AUTOMATEDMACHINES_API bool ActionNeedsSound(EAMAction Action);

	/** A few short sounds of the game for Play sound (Wwise events). */
	struct FSound
	{
		const TCHAR* Label;
		const TCHAR* EventPath;
		const TCHAR* Description;
	};
	AUTOMATEDMACHINES_API const TArray<FSound>& GetSounds();

	/** The sound of a rule's value (clamped to the list). */
	AUTOMATEDMACHINES_API int32 GetSoundIndex(float Value);

	/** "2 Iron Ingot -> 1 Iron Plate" (fluids in m3). */
	AUTOMATEDMACHINES_API FText DescribeRecipe(const UObject* WorldContext, TSubclassOf<UFGRecipe> Recipe);

	/** The value clamped to the allowed range (or the default if it is not set yet). */
	AUTOMATEDMACHINES_API float CleanValue(const FValueInfo& Info, float Value);
}
