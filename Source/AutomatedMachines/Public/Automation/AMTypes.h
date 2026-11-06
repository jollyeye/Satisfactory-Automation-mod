// Rules of Automated Machines: what a machine fires, and how it reacts to events.
//
// Kept small on purpose (they are saved): an event is just its name, stored in each rule. The list
// of all events in the world is every name some rule uses, so an event no rule uses is gone by itself.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPath.h"
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
	StorageFull        UMETA(DisplayName = "Full"),
	StorageNotFull     UMETA(DisplayName = "Not full"),
	StorageEmpty       UMETA(DisplayName = "Empty"),
	StorageNotEmpty    UMETA(DisplayName = "Not empty"),
	FillAbove          UMETA(DisplayName = "Filled above"),
	FillBelow          UMETA(DisplayName = "Filled below"),
	ItemCountAbove     UMETA(DisplayName = "Item count above"),
	ItemCountBelow     UMETA(DisplayName = "Item count below"),
	FuelEmpty          UMETA(DisplayName = "Out of fuel"),
	FuelAvailable      UMETA(DisplayName = "Has fuel"),
	FuelBelow          UMETA(DisplayName = "Fuel below"),
	LoadAbove          UMETA(DisplayName = "Load above"),
	LoadBelow          UMETA(DisplayName = "Load below"),
	FuseBlown          UMETA(DisplayName = "Fuse blown"),
	FuseReset          UMETA(DisplayName = "Fuse reset"),
	// World events: rung by the HUB (its Automation tab), not by machines.
	MilestoneCompleted UMETA(DisplayName = "Milestone completed"),
	// World events of the MAM (its Automation tab).
	ResearchCompleted  UMETA(DisplayName = "Research completed"),
	// Power switches and priority power switches.
	SwitchedOn         UMETA(DisplayName = "Switched on"),
	SwitchedOff        UMETA(DisplayName = "Switched off"),
	// The two power grids of a switch (Power Grid A and B of its window) compared.
	GridAUsesMore      UMETA(DisplayName = "A usage > B usage"),
	GridBUsesMore      UMETA(DisplayName = "A usage < B usage"),
	GridAMakesMore     UMETA(DisplayName = "A power > B power"),
	GridBMakesMore     UMETA(DisplayName = "A power < B power"),
	// Pipeline pumps and valves: the fluid flowing through them (m3/min).
	FlowAbove          UMETA(DisplayName = "Flow above"),
	FlowBelow          UMETA(DisplayName = "Flow below"),
	// AWESOME Sinks: the coupons waiting to be printed (the same for every sink: a world number).
	CouponsAbove       UMETA(DisplayName = "Coupons above"),
	CouponsBelow       UMETA(DisplayName = "Coupons below"),
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
	DeleteContents UMETA(DisplayName = "Delete contents"),
	// Power switches and priority power switches (Set priority: priority switches only).
	SwitchOn       UMETA(DisplayName = "Turn on"),
	SwitchOff      UMETA(DisplayName = "Turn off"),
	ToggleSwitch   UMETA(DisplayName = "Turn on or off"),
	SetPriority    UMETA(DisplayName = "Set priority"),
	// Pipeline pumps and valves: the flow limit of their window (m3/min).
	SetFlowLimit   UMETA(DisplayName = "Set flow limit"),
	// Valves: empties the whole pipe network it is part of (like the pipes' Flush button).
	FlushNetwork   UMETA(DisplayName = "Flush pipe network"),
	// Fluid buffers: empties the buffer only (its network keeps its fluid).
	FlushBuffer    UMETA(DisplayName = "Flush buffer"),
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

	/** The item of conditions that count one (Item count above / below), else null. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	TSubclassOf<UFGItemDescriptor> Item;
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

	/** The number of actions that need one (clock speed %, priority), else unused. */
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

	/**
	 * A right item (an ingredient) blocking an input belt because its input slot is full is taken off
	 * the belt and put on the output belt the same way: the input belt keeps moving.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Automation")
	bool bPassExtraItems = false;

	bool IsDefault() const { return !bPassWrongItems && !bPassExtraItems; }
	bool operator==(const FAMMachineOptions& Other) const { return bPassWrongItems == Other.bPassWrongItems && bPassExtraItems == Other.bPassExtraItems; }
};

/** One text or icon of a sign design (maps cannot be replicated or sent in RPCs, so key / value pairs). */
USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMSignText
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString Key;

	UPROPERTY(SaveGame)
	FString Value;
};

USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMSignIcon
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString Key;

	UPROPERTY(SaveGame)
	int32 Id = 0;
};

/**
 * A sign's design for an alarm (the game's sign data, in fields our save keeps): when the alarm
 * rings, the sign shows it. Event empty: the sign's normal design.
 */
USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMSignDesign
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString Event;

	UPROPERTY(SaveGame)
	FSoftClassPath Layout;

	UPROPERTY(SaveGame)
	TArray<FAMSignText> Texts;

	UPROPERTY(SaveGame)
	TArray<FAMSignIcon> Icons;

	UPROPERTY(SaveGame)
	FLinearColor Foreground = FLinearColor::Black;

	UPROPERTY(SaveGame)
	FLinearColor Background = FLinearColor::Black;

	UPROPERTY(SaveGame)
	FLinearColor Auxiliary = FLinearColor::Black;

	UPROPERTY(SaveGame)
	float Emissive = 0.f;

	UPROPERTY(SaveGame)
	float Glossiness = 0.f;

	UPROPERTY(SaveGame)
	TSubclassOf<UObject> SignType;
};

/**
 * A light's settings for an alarm (lights and light control panels, the game's light window): when
 * the alarm rings, the light switches to them. Event empty: the light's normal settings.
 */
USTRUCT(BlueprintType)
struct AUTOMATEDMACHINES_API FAMLightSetting
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString Event;

	/** Which of the game's light colour slots (0..6). */
	UPROPERTY(SaveGame)
	int32 ColorSlot = 0;

	/** 0.1 .. 1, like the game's slider. */
	UPROPERTY(SaveGame)
	float Intensity = 1.f;

	/** Night mode: lit only at night. */
	UPROPERTY(SaveGame)
	bool bNightMode = false;

	/** On (not in standby). */
	UPROPERTY(SaveGame)
	bool bEnabled = true;
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

	/** Signs: a design per alarm, the normal design, and the alarm whose design shows now (empty: normal). */
	UPROPERTY(SaveGame)
	TArray<FAMSignDesign> SignDesigns;

	UPROPERTY(SaveGame)
	FAMSignDesign SignNormal;

	UPROPERTY(SaveGame)
	FString SignShowing;

	/** Lights: settings per alarm, the normal settings, and the alarm whose settings show now (empty: normal). */
	UPROPERTY(SaveGame)
	TArray<FAMLightSetting> LightSettings;

	UPROPERTY(SaveGame)
	FAMLightSetting LightNormal;

	UPROPERTY(SaveGame)
	FString LightShowing;

	bool IsEmpty() const { return Fires.Num() == 0 && Reactions.Num() == 0 && Options.IsDefault() && SignDesigns.Num() == 0 && LightSettings.Num() == 0; }
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
		/** Shown after the number field ("%", or nothing for a count). */
		const TCHAR* Unit = TEXT("%");
	};
	AUTOMATEDMACHINES_API FValueInfo GetConditionValueInfo(EAMCondition Condition);
	AUTOMATEDMACHINES_API FValueInfo GetActionValueInfo(EAMAction Action);

	/**
	 * Machine events / actions this machine has (Machine null: all of them). Production machines
	 * have all; miners have no input and no recipe.
	 */
	AUTOMATEDMACHINES_API bool IsConditionAvailable(EAMCondition Condition, const UObject* Machine);
	AUTOMATEDMACHINES_API bool IsActionAvailable(EAMAction Action, const UObject* Machine);

	/** Events / actions of power switches (Switched on / off; Turn on / off, Set priority). */
	AUTOMATEDMACHINES_API bool IsSwitchCondition(EAMCondition Condition);

	/** A valve: a pipeline pump without power (pumps themselves are not automated). */
	AUTOMATEDMACHINES_API bool IsValve(const UObject* Machine);

	/**
	 * Two actions that make no sense together under one alarm: the same action twice (two messages),
	 * or two of a kind (Pause / Resume / Pause or resume; Turn on / Turn off / Turn on or off).
	 */
	AUTOMATEDMACHINES_API bool ActionsConflict(EAMAction A, EAMAction B);
	AUTOMATEDMACHINES_API bool IsSwitchAction(EAMAction Action);

	/** True if the machine has the Options section's "Pass wrong items through" (it has input belts). */
	AUTOMATEDMACHINES_API bool HasPassWrongItems(const UObject* Machine);

	/** Machine events that count one item (Item count above / below): the row gets an item picker. */
	AUTOMATEDMACHINES_API bool ConditionNeedsItem(EAMCondition Condition);

	/** Actions that need a recipe (Change recipe). */
	AUTOMATEDMACHINES_API bool ActionNeedsRecipe(EAMAction Action);

	/** Actions that have a text (Show message). */
	AUTOMATEDMACHINES_API bool ActionNeedsMessage(EAMAction Action);

	/**
	 * World actions do nothing to a machine (Show message): they are set in the HUB terminal's
	 * Automation tab, not on machines.
	 */
	AUTOMATEDMACHINES_API bool IsWorldAction(EAMAction Action);

	/** A world event: only the HUB rings it (e.g. a milestone is completed); machines never offer it. */
	AUTOMATEDMACHINES_API bool IsWorldCondition(EAMCondition Condition);

	/** A world event of the MAM (its tab), not of the HUB. */
	AUTOMATEDMACHINES_API bool IsMamCondition(EAMCondition Condition);

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
