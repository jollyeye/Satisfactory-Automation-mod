#include "Automation/AMAutomationSubsystem.h"
#include "Automation/AMMarkers.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGCentralStorageContainer.h"
#include "Resources/FGExtractableResourceInterface.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "Subsystem/SubsystemActorManager.h"
#include "FGHUD.h"
#include "UI/FGGameUI.h"
#include "UI/AMNotification.h"
#include "Automation/AMSound.h"
#include "Automation/AMClipboard.h"
#include "FGBuildableSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "Patching/NativeHookManager.h"
#include "Misc/ScopeLock.h"
#include <atomic>

namespace
{
	/** How often fire rules are checked, in seconds. */
	constexpr float AMAS_EvaluationInterval = 0.25f;

	FString AMAS_Describe(const AActor* Actor)
	{
		return Actor ? Actor->GetName() : TEXT("(none)");
	}

	// "Pass wrong items through": the wrong item each machine took off its input belt, waiting to
	// go out on its output belt. Belts may take items from worker threads, hence the lock; the count
	// lets every other grab in the factory skip the lock.
	struct FAMASHeld
	{
		TWeakObjectPtr<const AFGBuildable> Machine;
		FInventoryItem Item;
	};
	FCriticalSection AMAS_HeldLock;
	TMap<const AFGBuildable*, FAMASHeld> AMAS_Held;
	std::atomic<int32> AMAS_HeldCount{0};

	void AMAS_Hold(const AFGBuildable* Machine, const FInventoryItem& Item)
	{
		FScopeLock HeldScope(&AMAS_HeldLock);
		AMAS_Held.Add(Machine, FAMASHeld{Machine, Item});
		AMAS_HeldCount = AMAS_Held.Num();
	}

	/** The waiting item of a machine, if it matches Type (null = any); bRemove: it leaves. */
	bool AMAS_GiveHeld(const AFGBuildable* Machine, TSubclassOf<UFGItemDescriptor> Type, bool bRemove, FInventoryItem& OutItem)
	{
		FScopeLock HeldScope(&AMAS_HeldLock);
		const FAMASHeld* Held = AMAS_Held.Find(Machine);
		if (!Held || (Type.Get() && Held->Item.GetItemClass().Get() != Type.Get()))
		{
			return false;
		}
		OutItem = Held->Item;
		if (bRemove)
		{
			AMAS_Held.Remove(Machine);
			AMAS_HeldCount = AMAS_Held.Num();
		}
		return true;
	}

	bool AMAS_HasOutputBelt(const AFGBuildableFactory* Factory)
	{
		for (const UFGFactoryConnectionComponent* Connection : Factory->GetConnectionComponents())
		{
			if (Connection && Connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT && Connection->IsConnected())
			{
				return true;
			}
		}
		return false;
	}
}

AAMAutomationSubsystem::AAMAutomationSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AAMAutomationSubsystem* AAMAutomationSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>())
	{
		if (AAMAutomationSubsystem* Subsystem = Manager->GetSubsystemActor<AAMAutomationSubsystem>())
		{
			return Subsystem;
		}
	}
	// Fallback (e.g. a replicated copy on a client the manager has not picked up yet).
	for (TActorIterator<AAMAutomationSubsystem> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AAMAutomationSubsystem::InstallRegistration()
{
	// Same moment SML registers subsystems of mods' game world modules: the save is loaded by then, so
	// our saved actor is found and reused instead of a new one being spawned.
	FWorldDelegates::OnWorldInitializedActors.AddLambda([](const UWorld::FActorsInitializedParams& Params)
	{
		UWorld* World = Params.World;
		if (!World || !World->IsGameWorld())
		{
			return;
		}
		if (USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>())
		{
			Manager->RegisterSubsystemActor(AAMAutomationSubsystem::StaticClass());
		}
	});
	InstallOutputHooks();
}

void AAMAutomationSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAMAutomationSubsystem, mMachines);
	DOREPLIFETIME(AAMAutomationSubsystem, mWorldActions);
}

void AAMAutomationSubsystem::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation subsystem started (%d automated machines)"), mMachines.Num());

	if (HasAuthority())
	{
		if (AFGBuildableSubsystem* Buildables = AFGBuildableSubsystem::Get(this))
		{
			Buildables->BuildableConstructedGlobalDelegate.AddDynamic(this, &AAMAutomationSubsystem::OnBuildableConstructed);
		}
	}

	if (!IsRunningDedicatedServer() && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		mMarkers = GetWorld()->SpawnActor<AAMMarkers>(Params);
		if (mMarkers)
		{
			mMarkers->Init(this);
		}
	}
}

void AAMAutomationSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	{
		FScopeLock HeldScope(&AMAS_HeldLock);
		AMAS_Held.Reset();
		AMAS_HeldCount = 0;
	}
	Super::EndPlay(EndPlayReason);
}

void AAMAutomationSubsystem::PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	// The waiting wrong items are saved, so none is lost.
	mHeldItems.Reset();
	FScopeLock HeldScope(&AMAS_HeldLock);
	for (const TPair<const AFGBuildable*, FAMASHeld>& Pair : AMAS_Held)
	{
		if (AFGBuildable* Machine = const_cast<AFGBuildable*>(Pair.Value.Machine.Get()))
		{
			FAMHeldItem& Saved = mHeldItems.AddDefaulted_GetRef();
			Saved.Machine = Machine;
			Saved.Item = Pair.Value.Item.GetItemClass();
		}
	}
}

void AAMAutomationSubsystem::PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	for (const FAMHeldItem& Saved : mHeldItems)
	{
		if (IsValid(Saved.Machine) && Saved.Item)
		{
			AMAS_Hold(Saved.Machine, FInventoryItem(Saved.Item));
		}
	}
	mHeldItems.Reset();
	MoveWorldActionsFromMachines();
	RemoveInvalidEntries();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation subsystem loaded: %d automated machines, %d events"),
		mMachines.Num(), GetAllEventNames().Num());
}

void AAMAutomationSubsystem::OnBuildableConstructed(AFGBuildable* Buildable)
{
	AMClipboard::OnBuildableConstructed(this, Buildable);
}

void AAMAutomationSubsystem::OnRep_Machines()
{
	OnRulesChanged.Broadcast();
}

const FAMMachineRules* AAMAutomationSubsystem::FindRules(const AFGBuildable* Machine) const
{
	if (!Machine)
	{
		return nullptr;
	}
	return mMachines.FindByPredicate([Machine](const FAMMachineRules& Entry) { return Entry.Machine == Machine; });
}

void AAMAutomationSubsystem::SetRules(AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions, const FAMMachineOptions& Options)
{
	if (!HasAuthority() || !CanAutomate(Machine))
	{
		return;
	}

	FAMMachineRules NewRules;
	NewRules.Machine = Machine;
	NewRules.Options = Options;
	NewRules.Options.bPassWrongItems = Options.bPassWrongItems && AMRules::HasPassWrongItems(Machine);
	for (const FAMFireRule& Rule : Fires)
	{
		FAMFireRule Clean = Rule;
		Clean.Event = AMRules::NormalizeEventName(Rule.Event);
		Clean.Value = AMRules::CleanValue(AMRules::GetConditionValueInfo(Rule.Condition), Rule.Value);
		if (!AMRules::ConditionNeedsItem(Clean.Condition))
		{
			Clean.Item = nullptr;
		}
		if (!Clean.Event.IsEmpty() && Clean.Condition < EAMCondition::Count && AMRules::IsConditionAvailable(Clean.Condition, Machine))
		{
			NewRules.Fires.Add(Clean);
		}
	}
	for (const FAMReactionRule& Rule : Reactions)
	{
		const FAMReactionRule Clean = CleanReaction(Rule);
		// World actions belong to the HUB, not to machines.
		if (!Clean.Event.IsEmpty() && Clean.Action < EAMAction::Count && !AMRules::IsWorldAction(Clean.Action) && AMRules::IsActionAvailable(Clean.Action, Machine))
		{
			NewRules.Reactions.Add(Clean);
		}
	}

	const int32 Index = mMachines.IndexOfByPredicate([Machine](const FAMMachineRules& Entry) { return Entry.Machine == Machine; });

	// Did the alarms change? (Editing only actions or options must not ring the alarms again.)
	bool bFiresChanged = Index == INDEX_NONE || mMachines[Index].Fires.Num() != NewRules.Fires.Num();
	for (int32 RuleIndex = 0; !bFiresChanged && RuleIndex < NewRules.Fires.Num(); ++RuleIndex)
	{
		const FAMFireRule& Old = mMachines[Index].Fires[RuleIndex];
		const FAMFireRule& New = NewRules.Fires[RuleIndex];
		bFiresChanged = Old.Event != New.Event || Old.Condition != New.Condition || Old.Value != New.Value || Old.Item.Get() != New.Item.Get();
	}

	if (NewRules.IsEmpty())
	{
		if (Index != INDEX_NONE)
		{
			mMachines.RemoveAt(Index);
		}
	}
	else if (Index != INDEX_NONE)
	{
		mMachines[Index] = NewRules;
	}
	else
	{
		mMachines.Add(NewRules);
	}

	// Conditions of new alarms start unknown: one that is already true rings at the next check.
	if (bFiresChanged)
	{
		mLastConditionValues.Remove(TWeakObjectPtr<AFGBuildable>(Machine));
	}

	UE_LOG(LogAutomatedMachines, Log, TEXT("Rules of %s: %d fire rules, %d reactions, pass wrong items %s"),
		*AMAS_Describe(Machine), NewRules.Fires.Num(), NewRules.Reactions.Num(), Options.bPassWrongItems ? TEXT("on") : TEXT("off"));

	ForceNetUpdate();
	OnRulesChanged.Broadcast();
}

FAMReactionRule AAMAutomationSubsystem::CleanReaction(const FAMReactionRule& Rule)
{
	FAMReactionRule Clean = Rule;
	Clean.Event = AMRules::NormalizeEventName(Rule.Event);
	Clean.Value = AMRules::ActionNeedsSound(Rule.Action)
		? static_cast<float>(AMRules::GetSoundIndex(Rule.Value))
		: AMRules::CleanValue(AMRules::GetActionValueInfo(Rule.Action), Rule.Value);
	if (!AMRules::ActionNeedsRecipe(Rule.Action))
	{
		Clean.Recipe = nullptr;
	}
	Clean.Message = AMRules::ActionNeedsMessage(Rule.Action) ? Rule.Message.TrimStartAndEnd().Left(AMRules::MaxMessageLength) : FString();
	return Clean;
}

void AAMAutomationSubsystem::SetWorldActions(const TArray<FAMReactionRule>& Actions)
{
	if (!HasAuthority())
	{
		return;
	}
	mWorldActions.Reset();
	for (const FAMReactionRule& Rule : Actions)
	{
		const FAMReactionRule Clean = CleanReaction(Rule);
		if (!Clean.Event.IsEmpty() && AMRules::IsWorldAction(Clean.Action))
		{
			mWorldActions.Add(Clean);
		}
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("World actions: %d"), mWorldActions.Num());
	ForceNetUpdate();
	OnRulesChanged.Broadcast();
}

void AAMAutomationSubsystem::MoveWorldActionsFromMachines()
{
	int32 Moved = 0;
	for (FAMMachineRules& Entry : mMachines)
	{
		for (int32 Index = Entry.Reactions.Num() - 1; Index >= 0; --Index)
		{
			const FAMReactionRule& Rule = Entry.Reactions[Index];
			if (!AMRules::IsWorldAction(Rule.Action))
			{
				continue;
			}
			const bool bAlready = mWorldActions.ContainsByPredicate([&Rule](const FAMReactionRule& Other)
			{
				return Other.Action == Rule.Action && Other.Event == Rule.Event && Other.Message == Rule.Message;
			});
			if (!bAlready)
			{
				mWorldActions.Add(Rule);
			}
			Entry.Reactions.RemoveAt(Index);
			++Moved;
		}
	}
	if (Moved > 0)
	{
		UE_LOG(LogAutomatedMachines, Log, TEXT("Moved %d world actions from machines to the HUB"), Moved);
	}
}

TArray<FString> AAMAutomationSubsystem::GetAllEventNames() const
{
	TSet<FString> Names;
	for (const FAMReactionRule& Rule : mWorldActions)
	{
		Names.Add(Rule.Event);
	}
	for (const FAMMachineRules& Entry : mMachines)
	{
		for (const FAMFireRule& Rule : Entry.Fires)
		{
			Names.Add(Rule.Event);
		}
		for (const FAMReactionRule& Rule : Entry.Reactions)
		{
			Names.Add(Rule.Event);
		}
	}
	TArray<FString> Result = Names.Array();
	Result.Sort();
	return Result;
}

void AAMAutomationSubsystem::CountEventUses(const FString& Event, int32& OutRingers, int32& OutActors) const
{
	OutRingers = 0;
	OutActors = 0;
	for (const FAMMachineRules& Entry : mMachines)
	{
		if (Entry.Fires.ContainsByPredicate([&Event](const FAMFireRule& Rule) { return Rule.Event == Event; }))
		{
			++OutRingers;
		}
		if (Entry.Reactions.ContainsByPredicate([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; }))
		{
			++OutActors;
		}
	}
}

bool AAMAutomationSubsystem::CanAutomate(const AActor* Machine)
{
	// Production machines, and extractors (miners; also water and oil extractors, resource well
	// extractors: they are the same class).
	// Storage containers too (not the Dimensional Depot uploader: its own window, later).
	return Machine && (Machine->IsA<AFGBuildableManufacturer>() || Machine->IsA<AFGBuildableResourceExtractor>()
		|| (Machine->IsA<AFGBuildableStorage>() && !Machine->IsA<AFGCentralStorageContainer>()));
}

bool AAMAutomationSubsystem::RemoveInvalidEntries()
{
	const int32 Removed = mMachines.RemoveAll([](const FAMMachineRules& Entry)
	{
		return !IsValid(Entry.Machine) || Entry.IsEmpty();
	});
	for (auto It = mLastConditionValues.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = mLastRecipes.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	return Removed > 0;
}

void AAMAutomationSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	mTimeSinceEvaluation += DeltaSeconds;
	if (mTimeSinceEvaluation < AMAS_EvaluationInterval)
	{
		return;
	}
	mTimeSinceEvaluation = 0.f;

	// Dismantled machines: forget their rules.
	if (RemoveInvalidEntries())
	{
		ForceNetUpdate();
		OnRulesChanged.Broadcast();
	}
	EvaluateFireRules();

	// Options.
	for (const FAMMachineRules& Entry : mMachines)
	{
		if (Entry.Options.bPassWrongItems)
		{
			if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(Entry.Machine))
			{
				TakeWrongItem(Factory);
			}
		}
	}
	// Waiting items of dismantled machines are gone with them.
	{
		FScopeLock HeldScope(&AMAS_HeldLock);
		for (auto It = AMAS_Held.CreateIterator(); It; ++It)
		{
			if (!It.Value().Machine.IsValid())
			{
				It.RemoveCurrent();
			}
		}
		AMAS_HeldCount = AMAS_Held.Num();
	}
}

void AAMAutomationSubsystem::EvaluateFireRules()
{
	TArray<TPair<FString, const AActor*>> EventsToFire;
	for (const FAMMachineRules& Entry : mMachines)
	{
		const AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(Entry.Machine);
		if (!Factory || Entry.Fires.Num() == 0)
		{
			continue;
		}
		const TWeakObjectPtr<AFGBuildable> MachineKey(Entry.Machine.Get());
		TArray<int8>& LastValues = mLastConditionValues.FindOrAdd(MachineKey);
		if (LastValues.Num() != Entry.Fires.Num())
		{
			LastValues.Init(-1, Entry.Fires.Num());
		}

		// Recipe changed: compared with the last check (the first check only remembers it).
		bool bRecipeChanged = false;
		if (const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory))
		{
			UClass* Recipe = Manufacturer->GetCurrentRecipe().Get();
			if (TWeakObjectPtr<UClass>* LastRecipe = mLastRecipes.Find(MachineKey))
			{
				bRecipeChanged = LastRecipe->Get() != Recipe;
				*LastRecipe = Recipe;
			}
			else
			{
				mLastRecipes.Add(MachineKey, Recipe);
			}
		}

		for (int32 Index = 0; Index < Entry.Fires.Num(); ++Index)
		{
			const FAMFireRule& Rule = Entry.Fires[Index];
			const bool bNow = IsConditionTrue(Factory, Rule, bRecipeChanged);
			const bool bBecameTrue = bNow && LastValues[Index] != 1;
			LastValues[Index] = bNow ? 1 : 0;
			if (bBecameTrue)
			{
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s fires %s (%s)"), *AMAS_Describe(Factory), *Rule.Event,
					*AMRules::GetConditionText(Rule.Condition).ToString());
				EventsToFire.Emplace(Rule.Event, Factory);
			}
		}
	}
	// Fire after the loop: actions never change the rules, but this keeps the loop simple and safe.
	for (const TPair<FString, const AActor*>& Fired : EventsToFire)
	{
		FireEvent(Fired.Key, Fired.Value);
	}
}

void AAMAutomationSubsystem::FireEvent(const FString& Event, const AActor* Ringer)
{
	if (!HasAuthority())
	{
		return;
	}
	for (const FAMReactionRule& Rule : mWorldActions)
	{
		if (Rule.Event == Event)
		{
			ApplyWorldAction(Rule, Ringer);
		}
	}
	for (const FAMMachineRules& Entry : mMachines)
	{
		AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(Entry.Machine);
		if (!Factory)
		{
			continue;
		}
		for (const FAMReactionRule& Rule : Entry.Reactions)
		{
			if (Rule.Event == Event)
			{
				ApplyAction(Factory, Rule);
			}
		}
	}
}

bool AAMAutomationSubsystem::IsConditionTrue(const AFGBuildableFactory* Factory, const FAMFireRule& Rule, bool bRecipeChanged)
{
	switch (Rule.Condition)
	{
	case EAMCondition::OutputFull:        return IsOutputFull(Factory);
	case EAMCondition::OutputNotFull:     return !IsOutputFull(Factory);
	case EAMCondition::StartsProducing:   return Factory->IsProducing();
	case EAMCondition::StopsProducing:    return !Factory->IsProducing();
	case EAMCondition::LosesPower:        return !Factory->HasPower();
	case EAMCondition::GetsPower:         return Factory->HasPower();
	case EAMCondition::InputMissing:      return IsInputMissing(Factory);
	case EAMCondition::InputAvailable:    return !IsInputMissing(Factory);
	case EAMCondition::Paused:            return Factory->IsProductionPaused();
	case EAMCondition::Resumed:           return !Factory->IsProductionPaused();
	case EAMCondition::ProductivityBelow: return Factory->GetProductivity() * 100.f < Rule.Value;
	case EAMCondition::ProductivityAbove: return Factory->GetProductivity() * 100.f > Rule.Value;
	case EAMCondition::RecipeChanged:     return bRecipeChanged;
	case EAMCondition::InputFull:         return IsInputFull(Factory);
	case EAMCondition::InputNotFull:      return !IsInputFull(Factory);
	case EAMCondition::WrongInput:        return FindWrongInput(Factory, nullptr, nullptr);
	case EAMCondition::InputCorrect:      return !FindWrongInput(Factory, nullptr, nullptr);
	case EAMCondition::StorageFull:       return GetFillLevel(Factory) >= 0.9999f || IsBeltBlocked(Factory);
	case EAMCondition::StorageNotFull:    return GetFillLevel(Factory) < 0.9999f && !IsBeltBlocked(Factory);
	case EAMCondition::StorageEmpty:      return GetFillLevel(Factory) <= 0.f;
	case EAMCondition::StorageNotEmpty:   return GetFillLevel(Factory) > 0.f;
	case EAMCondition::FillAbove:         return GetFillLevel(Factory) * 100.f > Rule.Value;
	case EAMCondition::FillBelow:         return GetFillLevel(Factory) * 100.f < Rule.Value;
	case EAMCondition::ItemCountAbove:    return Rule.Item && CountItem(Factory, Rule.Item) > Rule.Value;
	case EAMCondition::ItemCountBelow:    return Rule.Item && CountItem(Factory, Rule.Item) < Rule.Value;
	default:                              return false;
	}
}

float AAMAutomationSubsystem::GetFillLevel(const AFGBuildableFactory* Factory)
{
	// 0..1: each slot counts as (items / stack size); a container of 48 slots with 24 full stacks is 0.5.
	AFGBuildableStorage* Storage = const_cast<AFGBuildableStorage*>(Cast<AFGBuildableStorage>(Factory));
	const UFGInventoryComponent* Inventory = Storage ? Storage->GetStorageInventory() : nullptr;
	if (!Inventory || Inventory->GetSizeLinear() <= 0)
	{
		return 0.f;
	}
	double Filled = 0.0;
	for (int32 SlotIndex = 0; SlotIndex < Inventory->GetSizeLinear(); ++SlotIndex)
	{
		FInventoryStack Stack;
		if (Inventory->GetStackFromIndex(SlotIndex, Stack) && Stack.HasItems())
		{
			const int32 SlotSize = Inventory->GetSlotSize(SlotIndex, Stack.Item.GetItemClass());
			Filled += SlotSize > 0 ? FMath::Min(1.0, static_cast<double>(Stack.NumItems) / SlotSize) : 1.0;
		}
	}
	return static_cast<float>(Filled / Inventory->GetSizeLinear());
}

int32 AAMAutomationSubsystem::CountItem(const AFGBuildableFactory* Factory, TSubclassOf<UFGItemDescriptor> Item)
{
	AFGBuildableStorage* Storage = const_cast<AFGBuildableStorage*>(Cast<AFGBuildableStorage>(Factory));
	const UFGInventoryComponent* Inventory = Storage ? Storage->GetStorageInventory() : nullptr;
	return Inventory && Item ? Inventory->GetNumItems(Item) : 0;
}

bool AAMAutomationSubsystem::IsBeltBlocked(const AFGBuildableFactory* Factory)
{
	// The item waiting at the end of an input belt has no room in the container.
	AFGBuildableStorage* Storage = const_cast<AFGBuildableStorage*>(Cast<AFGBuildableStorage>(Factory));
	const UFGInventoryComponent* Inventory = Storage ? Storage->GetStorageInventory() : nullptr;
	if (!Inventory)
	{
		return false;
	}
	for (const UFGFactoryConnectionComponent* Connection : Storage->GetConnectionComponents())
	{
		if (!Connection || Connection->GetDirection() != EFactoryConnectionDirection::FCD_INPUT || !Connection->IsConnected())
		{
			continue;
		}
		const UFGFactoryConnectionComponent* BeltEnd = Connection->GetConnection();
		TArray<FInventoryItem> Waiting;
		if (BeltEnd && BeltEnd->Factory_PeekOutput(Waiting) && Waiting.Num() > 0 && !Inventory->HasEnoughSpaceForItem(Waiting[0]))
		{
			return true;
		}
	}
	return false;
}

bool AAMAutomationSubsystem::IsInputFull(const AFGBuildableFactory* Factory)
{
	// Full = some ingredient's input slot has no room for even one more.
	const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory);
	if (!Manufacturer || !Manufacturer->GetCurrentRecipe() || !Manufacturer->GetInputInventory())
	{
		return false;
	}
	for (const FItemAmount& Ingredient : UFGRecipe::GetIngredients(Manufacturer, Manufacturer->GetCurrentRecipe()))
	{
		if (!Manufacturer->GetInputInventory()->HasEnoughSpaceForStack(FInventoryStack(1, Ingredient.ItemClass)))
		{
			return true;
		}
	}
	return false;
}

bool AAMAutomationSubsystem::FindWrongInput(const AFGBuildableFactory* Factory, UFGFactoryConnectionComponent** OutBelt, FInventoryItem* OutItem)
{
	const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory);
	if (!Manufacturer || !Manufacturer->GetCurrentRecipe())
	{
		return false;
	}
	TArray<const UClass*> Ingredients;
	for (const FItemAmount& Ingredient : UFGRecipe::GetIngredients(Manufacturer, Manufacturer->GetCurrentRecipe()))
	{
		Ingredients.Add(Ingredient.ItemClass.Get());
	}

	// Belts: the item waiting at the end of each input belt.
	for (UFGFactoryConnectionComponent* Connection : Manufacturer->GetConnectionComponents())
	{
		if (!Connection || Connection->GetDirection() != EFactoryConnectionDirection::FCD_INPUT || !Connection->IsConnected())
		{
			continue;
		}
		UFGFactoryConnectionComponent* BeltEnd = Connection->GetConnection();
		TArray<FInventoryItem> Waiting;
		if (BeltEnd && BeltEnd->Factory_PeekOutput(Waiting) && Waiting.Num() > 0 && !Ingredients.Contains(static_cast<const UClass*>(Waiting[0].GetItemClass().Get())))
		{
			if (OutBelt)
			{
				*OutBelt = BeltEnd;
			}
			if (OutItem)
			{
				*OutItem = Waiting[0];
			}
			return true;
		}
	}

	// Pipes: the fluid in each input pipe.
	TArray<UFGPipeConnectionComponent*> Pipes;
	Manufacturer->GetComponents<UFGPipeConnectionComponent>(Pipes);
	for (const UFGPipeConnectionComponent* Pipe : Pipes)
	{
		if (Pipe && Pipe->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER && Pipe->IsConnected())
		{
			const UClass* Fluid = Pipe->GetFluidDescriptor().Get();
			if (Fluid && !Ingredients.Contains(Fluid))
			{
				if (OutBelt)
				{
					*OutBelt = nullptr;
				}
				return true;
			}
		}
	}
	return false;
}

void AAMAutomationSubsystem::TakeWrongItem(AFGBuildableFactory* Factory)
{
	// One wrong item per machine waits at a time; it leaves on the output belt first.
	{
		FScopeLock HeldScope(&AMAS_HeldLock);
		if (AMAS_Held.Contains(Factory))
		{
			return;
		}
	}
	UFGFactoryConnectionComponent* BeltEnd = nullptr;
	FInventoryItem Wrong;
	if (!FindWrongInput(Factory, &BeltEnd, &Wrong) || !BeltEnd || !AMAS_HasOutputBelt(Factory))
	{
		return;
	}
	FInventoryItem Grabbed;
	float OffsetBeyond = 0.f;
	if (BeltEnd->Factory_GrabOutput(Grabbed, OffsetBeyond, Wrong.GetItemClass()))
	{
		AMAS_Hold(Factory, Grabbed);
		UE_LOG(LogAutomatedMachines, Verbose, TEXT("%s took %s off its input belt"), *AMAS_Describe(Factory), *GetNameSafe(Grabbed.GetItemClass().Get()));
	}
}

void AAMAutomationSubsystem::InstallOutputHooks()
{
	// The machine's own products go first: a wrong item is only given when the machine has nothing.
	SUBSCRIBE_METHOD(UFGFactoryConnectionComponent::Factory_GrabOutput,
		[](auto& Scope, UFGFactoryConnectionComponent* Self, FInventoryItem& OutItem, float& OutOffsetBeyond, TSubclassOf<UFGItemDescriptor> Type)
		{
			const bool bResult = Scope(Self, OutItem, OutOffsetBeyond, Type);
			if (!bResult && AMAS_HeldCount.load() > 0 && Self && Self->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
			{
				if (AMAS_GiveHeld(Self->GetOuterBuildable(), Type, true, OutItem))
				{
					OutOffsetBeyond = 0.f;
					Scope.Override(true);
				}
			}
		});
	SUBSCRIBE_METHOD(UFGFactoryConnectionComponent::Factory_PeekOutput,
		[](auto& Scope, const UFGFactoryConnectionComponent* Self, TArray<FInventoryItem>& OutItems, TSubclassOf<UFGItemDescriptor> Type)
		{
			const bool bResult = Scope(Self, OutItems, Type);
			if (!bResult && AMAS_HeldCount.load() > 0 && Self && Self->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
			{
				FInventoryItem Item;
				if (AMAS_GiveHeld(Self->GetOuterBuildable(), Type, false, Item))
				{
					OutItems.Add(Item);
					Scope.Override(true);
				}
			}
		});
}

bool AAMAutomationSubsystem::IsInputMissing(const AFGBuildableFactory* Factory)
{
	// Missing = the input slots do not have one batch of every ingredient of the recipe.
	const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory);
	if (!Manufacturer || !Manufacturer->GetCurrentRecipe() || !Manufacturer->GetInputInventory())
	{
		return false;
	}
	for (const FItemAmount& Ingredient : UFGRecipe::GetIngredients(Manufacturer, Manufacturer->GetCurrentRecipe()))
	{
		if (!Manufacturer->GetInputInventory()->HasItems(Ingredient.ItemClass, Ingredient.Amount))
		{
			return true;
		}
	}
	return false;
}

bool AAMAutomationSubsystem::IsOutputFull(const AFGBuildableFactory* Factory)
{
	// Full = the output has no room for one more production cycle (the machine would stop for that).
	if (const AFGBuildableResourceExtractor* Extractor = Cast<AFGBuildableResourceExtractor>(Factory))
	{
		// A miner: room for one more cycle of what it mines.
		const TScriptInterface<IFGExtractableResourceInterface> Resource = Extractor->GetExtractableResource();
		const IFGExtractableResourceInterface* ResourceInterface = Resource.GetInterface();
		UFGInventoryComponent* ExtractorOutput = Extractor->GetOutputInventory();
		if (!ResourceInterface || !ExtractorOutput || !ResourceInterface->GetResourceClass())
		{
			return false;
		}
		const FInventoryStack Cycle(FMath::Max(1, Extractor->GetNumExtractedItemsPerCycle()), ResourceInterface->GetResourceClass());
		return !ExtractorOutput->HasEnoughSpaceForStack(Cycle);
	}
	const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory);
	if (!Manufacturer || !Manufacturer->GetCurrentRecipe() || !Manufacturer->GetOutputInventory())
	{
		return false;
	}
	TArray<FInventoryStack> Products;
	for (const FItemAmount& Product : UFGRecipe::GetProducts(Manufacturer->GetCurrentRecipe()))
	{
		Products.Emplace(Product.Amount, Product.ItemClass);
	}
	return Products.Num() > 0 && !Manufacturer->GetOutputInventory()->HasEnoughSpaceForStacks(Products);
}

void AAMAutomationSubsystem::ApplyAction(AFGBuildableFactory* Factory, const FAMReactionRule& Rule)
{
	switch (Rule.Action)
	{
	case EAMAction::Pause:
		if (!Factory->IsProductionPaused())
		{
			Factory->SetIsProductionPaused(true);
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s paused"), *AMAS_Describe(Factory));
		}
		break;
	case EAMAction::Resume:
		if (Factory->IsProductionPaused())
		{
			Factory->SetIsProductionPaused(false);
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s resumed"), *AMAS_Describe(Factory));
		}
		break;
	case EAMAction::TogglePause:
		Factory->SetIsProductionPaused(!Factory->IsProductionPaused());
		UE_LOG(LogAutomatedMachines, Log, TEXT("%s %s"), *AMAS_Describe(Factory), Factory->IsProductionPaused() ? TEXT("paused") : TEXT("resumed"));
		break;
	case EAMAction::SetClockSpeed:
		if (Factory->GetCanChangePotential())
		{
			// Within what the machine allows (above 100% only with power shards in it).
			const float Target = FMath::Clamp(Rule.Value / 100.f, Factory->GetCurrentMinPotential(), Factory->GetCurrentMaxPotential());
			if (!FMath::IsNearlyEqual(Factory->GetPendingPotential(), Target, 0.0001f))
			{
				Factory->SetPendingPotential(Target);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s clock speed %.1f%%"), *AMAS_Describe(Factory), Target * 100.f);
			}
		}
		break;
	case EAMAction::DeleteOutput:
		if (AFGBuildableResourceExtractor* Extractor = Cast<AFGBuildableResourceExtractor>(Factory))
		{
			const int32 Slots = DeleteAllItems(Extractor->GetOutputInventory());
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s: output deleted (%d slots had items)"), *AMAS_Describe(Factory), Slots);
			break;
		}
		// A production machine: below.
		[[fallthrough]];
	case EAMAction::DeleteInput:
		if (AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory))
		{
			const bool bOutput = Rule.Action == EAMAction::DeleteOutput;
			const int32 Slots = DeleteAllItems(bOutput ? Manufacturer->GetOutputInventory() : Manufacturer->GetInputInventory());
			if (Slots > 0)
			{
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s: %s deleted (%d slots)"), *AMAS_Describe(Factory), bOutput ? TEXT("output") : TEXT("input"), Slots);
			}
		}
		break;
	case EAMAction::DeleteContents:
		if (AFGBuildableStorage* Storage = Cast<AFGBuildableStorage>(Factory))
		{
			const int32 Slots = DeleteAllItems(Storage->GetStorageInventory());
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s: contents deleted (%d slots had items)"), *AMAS_Describe(Factory), Slots);
		}
		break;
	case EAMAction::ChangeRecipe:
		if (AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory))
		{
			if (!Rule.Recipe || Manufacturer->GetCurrentRecipe().Get() == Rule.Recipe.Get())
			{
				break;
			}
			// Only recipes this machine can make and the player has unlocked.
			TArray<TSubclassOf<UFGRecipe>> Available;
			Manufacturer->GetAvailableRecipes(Available);
			if (!Available.ContainsByPredicate([&Rule](const TSubclassOf<UFGRecipe>& Recipe) { return Recipe.Get() == Rule.Recipe.Get(); }))
			{
				UE_LOG(LogAutomatedMachines, Warning, TEXT("%s cannot make %s: recipe not changed"), *AMAS_Describe(Factory), *Rule.Recipe->GetName());
				break;
			}
			// The game wants empty slots for a new recipe.
			DeleteAllItems(Manufacturer->GetInputInventory());
			DeleteAllItems(Manufacturer->GetOutputInventory());
			Manufacturer->SetRecipe(Rule.Recipe);
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s recipe changed to %s"), *AMAS_Describe(Factory), *Rule.Recipe->GetName());
		}
		break;
	default:
		// World actions (Show message) are not run on machines.
		break;
	}
}

void AAMAutomationSubsystem::ApplyWorldAction(const FAMReactionRule& Rule, const AActor* Ringer)
{
	switch (Rule.Action)
	{
	case EAMAction::ShowMessage:
	{
		// Title: the alarm. Text: the player's own, or the machine that rang it.
		FString Body = Rule.Message;
		if (Body.IsEmpty())
		{
			Body = Ringer ? FString::Printf(TEXT("Rung by %s"), *GetMachineName(Ringer)) : FString(TEXT("Rang"));
		}
		MulticastShowMessage(Rule.Event, Body);
		break;
	}
	case EAMAction::PlaySound:
		MulticastPlaySound(static_cast<uint8>(AMRules::GetSoundIndex(Rule.Value)));
		break;
	default:
		break;
	}
}

FString AAMAutomationSubsystem::GetMachineName(const AActor* Machine)
{
	// AFGBuildable::mDisplayName is protected: read it by name.
	const FTextProperty* NameProperty = Machine ? FindFProperty<FTextProperty>(Machine->GetClass(), TEXT("mDisplayName")) : nullptr;
	return NameProperty ? NameProperty->GetPropertyValue_InContainer(Machine).ToString() : AMAS_Describe(Machine);
}

void AAMAutomationSubsystem::MulticastPlaySound_Implementation(uint8 SoundIndex)
{
	AMSound::PlayForLocalPlayers(GetWorld(), SoundIndex);
}

void AAMAutomationSubsystem::MulticastShowMessage_Implementation(const FString& Title, const FString& Body)
{
	if (IsRunningDedicatedServer() || !GetWorld())
	{
		return;
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Message: %s: %s"), *Title, *Body);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (Player && Player->IsLocalController())
		{
			UAMNotification::Show(Player, Title, Body);
		}
	}
}

int32 AAMAutomationSubsystem::DeleteAllItems(UFGInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return 0;
	}
	int32 Slots = 0;
	for (int32 Index = 0; Index < Inventory->GetSizeLinear(); ++Index)
	{
		if (!Inventory->IsIndexEmpty(Index))
		{
			Inventory->RemoveAllFromIndex(Index);
			++Slots;
		}
	}
	return Slots;
}
