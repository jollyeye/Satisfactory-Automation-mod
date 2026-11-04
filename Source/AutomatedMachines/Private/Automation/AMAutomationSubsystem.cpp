#include "Automation/AMAutomationSubsystem.h"
#include "Automation/AMMarkers.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "Subsystem/SubsystemActorManager.h"
#include "FGHUD.h"
#include "UI/FGGameUI.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"

namespace
{
	/** How often fire rules are checked, in seconds. */
	constexpr float AMAS_EvaluationInterval = 0.25f;

	FString AMAS_Describe(const AActor* Actor)
	{
		return Actor ? Actor->GetName() : TEXT("(none)");
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
}

void AAMAutomationSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAMAutomationSubsystem, mMachines);
}

void AAMAutomationSubsystem::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation subsystem started (%d automated machines)"), mMachines.Num());

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

void AAMAutomationSubsystem::PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	RemoveInvalidEntries();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation subsystem loaded: %d automated machines, %d events"),
		mMachines.Num(), GetAllEventNames().Num());
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

void AAMAutomationSubsystem::SetRules(AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions)
{
	if (!HasAuthority() || !CanAutomate(Machine))
	{
		return;
	}

	FAMMachineRules NewRules;
	NewRules.Machine = Machine;
	for (const FAMFireRule& Rule : Fires)
	{
		FAMFireRule Clean = Rule;
		Clean.Event = AMRules::NormalizeEventName(Rule.Event);
		Clean.Value = AMRules::CleanValue(AMRules::GetConditionValueInfo(Rule.Condition), Rule.Value);
		if (!Clean.Event.IsEmpty() && Clean.Condition < EAMCondition::Count)
		{
			NewRules.Fires.Add(Clean);
		}
	}
	for (const FAMReactionRule& Rule : Reactions)
	{
		FAMReactionRule Clean = Rule;
		Clean.Event = AMRules::NormalizeEventName(Rule.Event);
		Clean.Value = AMRules::CleanValue(AMRules::GetActionValueInfo(Rule.Action), Rule.Value);
		if (!AMRules::ActionNeedsRecipe(Rule.Action))
		{
			Clean.Recipe = nullptr;
		}
		Clean.Message = AMRules::ActionNeedsMessage(Rule.Action) ? Rule.Message.TrimStartAndEnd().Left(AMRules::MaxMessageLength) : FString();
		if (!Clean.Event.IsEmpty() && Clean.Action < EAMAction::Count)
		{
			NewRules.Reactions.Add(Clean);
		}
	}

	const int32 Index = mMachines.IndexOfByPredicate([Machine](const FAMMachineRules& Entry) { return Entry.Machine == Machine; });
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

	// Conditions of the new rules start unknown: one that is already true fires at the next check.
	mLastConditionValues.Remove(TWeakObjectPtr<AFGBuildable>(Machine));

	UE_LOG(LogAutomatedMachines, Log, TEXT("Rules of %s: %d fire rules, %d reactions"),
		*AMAS_Describe(Machine), NewRules.Fires.Num(), NewRules.Reactions.Num());

	ForceNetUpdate();
	OnRulesChanged.Broadcast();
}

TArray<FString> AAMAutomationSubsystem::GetAllEventNames() const
{
	TSet<FString> Names;
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
	return Machine && Machine->IsA<AFGBuildableManufacturer>();
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
}

void AAMAutomationSubsystem::EvaluateFireRules()
{
	TArray<FString> EventsToFire;
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
				EventsToFire.Add(Rule.Event);
			}
		}
	}
	// Fire after the loop: actions never change the rules, but this keeps the loop simple and safe.
	for (const FString& Event : EventsToFire)
	{
		FireEvent(Event);
	}
}

void AAMAutomationSubsystem::FireEvent(const FString& Event)
{
	if (!HasAuthority())
	{
		return;
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
	default:                              return false;
	}
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
	case EAMAction::ShowMessage:
	{
		const FString Message = Rule.Message.IsEmpty()
			? FString::Printf(TEXT("%s: %s"), *GetMachineName(Factory), *Rule.Event)
			: Rule.Message;
		MulticastShowMessage(Message);
		break;
	}
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

void AAMAutomationSubsystem::MulticastShowMessage_Implementation(const FString& Message)
{
	if (IsRunningDedicatedServer() || !GetWorld())
	{
		return;
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Message: %s"), *Message);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Player = It->Get();
		if (!Player || !Player->IsLocalController())
		{
			continue;
		}
		const AFGHUD* HUD = Cast<AFGHUD>(Player->GetHUD());
		if (UFGGameUI* GameUI = HUD ? HUD->GetGameUI() : nullptr)
		{
			GameUI->ShowTextNotification(FText::FromString(Message));
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
