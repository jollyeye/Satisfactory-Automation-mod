#include "Automation/AMAutomationSubsystem.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
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
		if (!Clean.Event.IsEmpty() && Clean.Condition < EAMCondition::Count)
		{
			NewRules.Fires.Add(Clean);
		}
	}
	for (const FAMReactionRule& Rule : Reactions)
	{
		FAMReactionRule Clean = Rule;
		Clean.Event = AMRules::NormalizeEventName(Rule.Event);
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
		TArray<int8>& LastValues = mLastConditionValues.FindOrAdd(TWeakObjectPtr<AFGBuildable>(Entry.Machine.Get()));
		if (LastValues.Num() != Entry.Fires.Num())
		{
			LastValues.Init(-1, Entry.Fires.Num());
		}
		for (int32 Index = 0; Index < Entry.Fires.Num(); ++Index)
		{
			const FAMFireRule& Rule = Entry.Fires[Index];
			const bool bNow = IsConditionTrue(Factory, Rule.Condition);
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
				ApplyAction(Factory, Rule.Action);
			}
		}
	}
}

bool AAMAutomationSubsystem::IsConditionTrue(const AFGBuildableFactory* Factory, EAMCondition Condition)
{
	switch (Condition)
	{
	case EAMCondition::OutputFull:      return IsOutputFull(Factory);
	case EAMCondition::OutputNotFull:   return !IsOutputFull(Factory);
	case EAMCondition::StartsProducing: return Factory->IsProducing();
	case EAMCondition::StopsProducing:  return !Factory->IsProducing();
	default:                            return false;
	}
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

void AAMAutomationSubsystem::ApplyAction(AFGBuildableFactory* Factory, EAMAction Action)
{
	switch (Action)
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
	default:
		break;
	}
}
