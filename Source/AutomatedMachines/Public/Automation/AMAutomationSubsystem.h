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
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;

	/** The rules of a machine, or null if it has none. */
	const FAMMachineRules* FindRules(const AFGBuildable* Machine) const;

	/** Replaces a machine's rules (server only). Empty event names are dropped, names are made uppercase. */
	void SetRules(AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions);

	/** Every event name in use, sorted. */
	TArray<FString> GetAllEventNames() const;

	/** How many machines ring this event, and how many act on it. */
	void CountEventUses(const FString& Event, int32& OutRingers, int32& OutActors) const;

	/** Every machine with rules. */
	const TArray<FAMMachineRules>& GetAllMachines() const { return mMachines; }

	/** Fires an event now: every machine that reacts to it acts (server only). */
	void FireEvent(const FString& Event);

	/** True if this machine type can be automated (for now: Smelter, Constructor and other manufacturers). */
	static bool CanAutomate(const AActor* Machine);

	/** Broadcast whenever any machine's rules change (on the server and, through replication, on clients). */
	FSimpleMulticastDelegate OnRulesChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Machines();

private:
	/** Removes entries of dismantled machines and entries without rules. Returns true if any were removed. */
	bool RemoveInvalidEntries();

	/** Checks every fire rule; fires the events whose condition just became true. */
	void EvaluateFireRules();

	/** bRecipeChanged: the machine's recipe changed since the last check. */
	static bool IsConditionTrue(const AFGBuildableFactory* Factory, const FAMFireRule& Rule, bool bRecipeChanged);
	static bool IsOutputFull(const AFGBuildableFactory* Factory);
	static bool IsInputMissing(const AFGBuildableFactory* Factory);
	static bool IsInputFull(const AFGBuildableFactory* Factory);
	void ApplyAction(AFGBuildableFactory* Factory, const FAMReactionRule& Rule);

	/** Shows a message on the screen of every player (runs on the server and on every client). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowMessage(const FString& Message);

	/** The machine's name, e.g. "Constructor". */
	static FString GetMachineName(const AActor* Machine);

	/** Destroys every item in an inventory, slot by slot (the slots keep their settings). Returns how many slots had items. */
	static int32 DeleteAllItems(class UFGInventoryComponent* Inventory);

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Machines)
	TArray<FAMMachineRules> mMachines;

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
