// The mod's one save object: every machine's rules, and the engine that runs them.
//
// Uninstall-safe: the game's machines are never changed. All our data is in this actor; if the mod
// is removed, the game drops this actor from the save and every machine loads as normal.
//
// Runs on the server (it is replicated so clients can show the rules). A few times a second it
// checks each machine's fire rules; when a condition becomes true it fires the event, and every
// machine with a reaction to that event acts at once.
#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSaveInterface.h"
#include "Automation/AMTypes.h"
#include "AMAutomationSubsystem.generated.h"

class AFGBuildable;
class AFGBuildableFactory;

UCLASS()
class AUTOMATEDMACHINES_API AAMAutomationSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AAMAutomationSubsystem();

	/** The subsystem of this world, or null (e.g. not spawned yet). */
	static AAMAutomationSubsystem* Get(const UObject* WorldContext);

	/** Registers the subsystem class with SML in every game world (call once at module startup). */
	static void InstallRegistration();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// IFGSaveInterface
	virtual bool ShouldSave_Implementation() const override { return true; }
	virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;

	/** The rules of a machine, or null if it has none. */
	const FAMMachineRules* FindRules(const AFGBuildable* Machine) const;

	/** Replaces a machine's rules and options (server only). Empty event names are dropped, names are made uppercase. */
	void SetRules(AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions, const FAMMachineOptions& Options);

	/** Every event name in use, sorted. */
	TArray<FString> GetAllEventNames() const;

	/** How many machines ring this event, and how many act on it. */
	void CountEventUses(const FString& Event, int32& OutRingers, int32& OutActors) const;

	/** Every machine with rules. */
	const TArray<FAMMachineRules>& GetAllMachines() const { return mMachines; }

	/** Fires an event now: every machine that reacts to it acts, and the world actions run (server only). */
	void FireEvent(const FString& Event, const AActor* Ringer = nullptr);

	/** Actions of the whole world (the HUB terminal's Automation tab): they need no machine. */
	const TArray<FAMReactionRule>& GetWorldActions() const { return mWorldActions; }

	/** Alarms the world rings (the HUB terminal's Automation tab): world events, e.g. a milestone completed. */
	const TArray<FAMFireRule>& GetWorldFires() const { return mWorldFires; }

	/**
	 * Replaces a sign's designs (server only): its normal design and one per alarm. The sign then
	 * shows the design it should (a second later, after the game's own save of the open window).
	 * No designs: the sign gets its normal design back and its entry goes.
	 */
	void SetSignDesigns(class AFGBuildableWidgetSign* Sign, const FAMSignDesign& Normal, const TArray<FAMSignDesign>& Designs);

	/**
	 * Removes an alarm (server only): bRingers = the machines' alarm rows with this name; else every
	 * action with it (machines, the HUB) and every sign design for it. OnlyMachine: from that one
	 * machine only; bOnlyHub: from the HUB's actions only (the overview's per-machine bins).
	 */
		void ClearAlarm(const FString& Event, bool bRingers, const AActor* OnlyMachine = nullptr, bool bOnlyHub = false, bool bMam = false);

	/** The HUB's or the MAM's building (rings their world events; found by class name). */
	const AActor* FindWorldBuilding(bool bMam) const;

	/** Replaces the world actions (server only). Only world actions are kept. */
	/**
	 * The world's alarms and actions from one tab: the HUB's (bMam false: its world alarms and the
	 * world actions) or the MAM's (bMam true: its world alarms only). The other tab's alarms are kept.
	 */
	void SetWorldRules(const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Actions, bool bMam = false);

	/** True if this machine type can be automated (for now: Smelter, Constructor and other manufacturers). */
	static bool CanAutomate(const AActor* Machine);

	/** Broadcast whenever any machine's rules change (on the server and, through replication, on clients). */
	FSimpleMulticastDelegate OnRulesChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_Machines();

	/** Server: every new buildable (for middle-click sampled automation, AMClipboard). */
	UFUNCTION()
	void OnBuildableConstructed(AFGBuildable* Buildable);

private:
	/** Shows on a sign the design it should show now (its alarm's, or the normal one). */
	void ShowCurrentSignDesign(class AFGBuildableWidgetSign* Sign);

	/** Removes entries of dismantled machines and entries without rules. Returns true if any were removed. */
	bool RemoveInvalidEntries();

	/** Checks every fire rule; fires the events whose condition just became true. */
	void EvaluateFireRules();

	/** bRecipeChanged: the machine's recipe changed since the last check. */
	static bool IsConditionTrue(const AFGBuildableFactory* Factory, const FAMFireRule& Rule, bool bRecipeChanged);
	static bool IsOutputFull(const AFGBuildableFactory* Factory);
	static bool IsInputMissing(const AFGBuildableFactory* Factory);
	static bool IsInputFull(const AFGBuildableFactory* Factory);

	/** How full a storage container is, 0..1 (0 for other machines). */
	static float GetFillLevel(const AFGBuildableFactory* Factory);

	/** Fuel generators: no fuel left, fuel items in the slot, load 0..1, the grid's fuse blown. */
	static bool IsOutOfFuel(const AFGBuildableFactory* Factory);
	static int32 CountFuel(const AFGBuildableFactory* Factory);
	static float GetLoad(const AFGBuildableFactory* Factory);
	static bool IsFuseBlown(const AFGBuildableFactory* Factory);

	/** A storage container: an input belt has an item waiting that does not fit. */
	static bool IsBeltBlocked(const AFGBuildableFactory* Factory);

public:
	/** How many of an item a storage container holds (0 for other machines). */
	static int32 CountItem(const AFGBuildableFactory* Factory, TSubclassOf<class UFGItemDescriptor> Item);

private:

	/**
	 * A wrong item at the end of an input belt (OutBelt: that belt's end, OutItem: the item), or a
	 * wrong fluid in an input pipe (OutBelt null). False if the machine has no recipe.
	 */
	static bool FindWrongInput(const AFGBuildableFactory* Factory, class UFGFactoryConnectionComponent** OutBelt, struct FInventoryItem* OutItem);

	/**
	 * "Pass wrong items through": takes a wrong item off an input belt; it waits (not in any slot)
	 * until the output belt finds the machine has nothing of its own to give, then goes out on it.
	 */
	static void TakeWrongItem(AFGBuildableFactory* Factory);

	/** Lets output belts take a machine's waiting wrong item (hooks on the game's belt connections). */
	static void InstallOutputHooks();

	/** The waiting wrong items, only while saving and loading. */
	UPROPERTY(SaveGame)
	TArray<FAMHeldItem> mHeldItems;
	void ApplyAction(AFGBuildableFactory* Factory, const FAMReactionRule& Rule);

	/** Shows a message on the screen of every player (runs on the server and on every client). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowMessage(const FString& Title, const FString& Body);

	/** Plays a sound of AMRules::GetSounds for every player (runs on the server and on every client). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySound(uint8 SoundIndex);

	/** The machine's name, e.g. "Constructor". */
	static FString GetMachineName(const AActor* Machine);

	/** Destroys every item in an inventory, slot by slot (the slots keep their settings). Returns how many slots had items. */
	static int32 DeleteAllItems(class UFGInventoryComponent* Inventory);

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Machines)
	TArray<FAMMachineRules> mMachines;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Machines)
	TArray<FAMReactionRule> mWorldActions;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Machines)
	TArray<FAMFireRule> mWorldFires;

	/** Server: the game's schematic manager tells us about milestones (bound once it exists). */
	void BindWorldEvents();
	FTimerHandle mBindWorldEventsTimer;

	UFUNCTION()
	void OnSchematicPurchased(TSubclassOf<class UFGSchematic> Schematic);

	UFUNCTION()
	void OnResearchCompleted(TSubclassOf<class UFGSchematic> Schematic);

	bool bMilestonesBound = false;
	bool bResearchBound = false;

	/** Rings the world alarms with this world event (rung by the HUB). */
	void FireWorldEvent(EAMCondition Condition, const FString& Detail);

	/** Runs one world action; Ringer is the machine that rang the alarm. */
	void ApplyWorldAction(const FAMReactionRule& Rule, const AActor* Ringer);

	/** Older saves had world actions on machines: move them to the world actions. */
	void MoveWorldActionsFromMachines();

	/** A cleaned copy of a rule (name in capitals, value in range, unused fields cleared). */
	static FAMReactionRule CleanReaction(const FAMReactionRule& Rule);

	/**
	 * Last known value of each fire rule's condition, per machine (not saved: after loading, a
	 * condition that is already true fires once, so reacting machines catch up).
	 * -1 = unknown, 0 = false, 1 = true.
	 */
	TMap<TWeakObjectPtr<AFGBuildable>, TArray<int8>> mLastConditionValues;

	/** Recipe of each machine at the last check (for "Recipe changed"; not saved). */
	TMap<TWeakObjectPtr<AFGBuildable>, TWeakObjectPtr<UClass>> mLastRecipes;

	float mTimeSinceEvaluation = 0.f;

	/** The markers above automated machines (local, never saved; not on a dedicated server). */
	UPROPERTY(Transient)
	TObjectPtr<class AAMMarkers> mMarkers;
};
