#include "Automation/AMAutomationSubsystem.h"
#include "Automation/AMMarkers.h"
#include "FGSchematicManager.h"
#include "FGResearchManager.h"
#include "FGSchematic.h"
#include "EngineUtils.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePriorityPowerSwitch.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "FGPipeSubsystem.h"
#include "Buildables/FGBuildablePipeReservoir.h"
#include "Buildables/FGBuildableResourceSink.h"
#include "FGResourceSinkSubsystem.h"
#include "Buildables/FGBuildableRailroadSignal.h"
#include "Automation/AMSignal.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Buildables/FGBuildableTrainPlatformCargo.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "FGTrainStationIdentifier.h"
#include "FGTextRenderer.h"
#include "FGTrain.h"
#include "FGRailroadVehicle.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerCircuit.h"
#include "FGPowerInfoComponent.h"
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
#include "Automation/AMSign.h"
#include "Automation/AMLight.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildableWidgetSign.h"
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
	AMSignal::InstallHooks();
}

void AAMAutomationSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAMAutomationSubsystem, mMachines);
	DOREPLIFETIME(AAMAutomationSubsystem, mWorldActions);
	DOREPLIFETIME(AAMAutomationSubsystem, mWorldFires);
}

void AAMAutomationSubsystem::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		BindWorldEvents();
	}
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

void AAMAutomationSubsystem::PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
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
	// Signals our Block action held red.
	for (const FAMMachineRules& Entry : mMachines)
	{
		if (Entry.bSignalBlocked)
		{
			AMSignal::SetBlocked(Cast<AFGBuildableRailroadSignal>(Entry.Machine), true);
		}
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation subsystem loaded: %d automated machines, %d events"),
		mMachines.Num(), GetAllEventNames().Num());
}

void AAMAutomationSubsystem::SetSignDesigns(AFGBuildableWidgetSign* Sign, const FAMSignDesign& Normal, const TArray<FAMSignDesign>& Designs)
{
	if (!HasAuthority() || !Sign)
	{
		return;
	}
	int32 Index = mMachines.IndexOfByPredicate([Sign](const FAMMachineRules& Entry) { return Entry.Machine == Sign; });
	if (Index == INDEX_NONE)
	{
		if (Designs.Num() == 0)
		{
			return;
		}
		FAMMachineRules& NewEntry = mMachines.AddDefaulted_GetRef();
		NewEntry.Machine = Sign;
		Index = mMachines.Num() - 1;
	}
	FAMMachineRules& Entry = mMachines[Index];
	Entry.SignNormal = Normal;
	Entry.SignNormal.Event.Reset();
	Entry.SignDesigns.Reset();
	for (const FAMSignDesign& Design : Designs)
	{
		FAMSignDesign Clean = Design;
		Clean.Event = AMRules::NormalizeEventName(Design.Event);
		if (!Clean.Event.IsEmpty() && !Entry.SignDesigns.ContainsByPredicate([&Clean](const FAMSignDesign& D) { return D.Event == Clean.Event; }))
		{
			Entry.SignDesigns.Add(Clean);
		}
	}
	if (!Entry.SignDesigns.ContainsByPredicate([&Entry](const FAMSignDesign& D) { return D.Event == Entry.SignShowing; }))
	{
		Entry.SignShowing.Reset();
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("%s: %d sign designs, showing %s"), *AMAS_Describe(Sign), Entry.SignDesigns.Num(),
		Entry.SignShowing.IsEmpty() ? TEXT("the normal design") : *Entry.SignShowing);

	const bool bNoDesigns = Entry.SignDesigns.Num() == 0;
	if (bNoDesigns)
	{
		// Back to a plain sign: its normal design, no entry.
		AMSign::Show(Sign, Normal);
		if (Entry.IsEmpty())
		{
			mMachines.RemoveAt(Index);
		}
	}
	else
	{
		// After the game's own save of the open window (which saves the tab it was on).
		TWeakObjectPtr<AFGBuildableWidgetSign> WeakSign(Sign);
		FTimerHandle Later;
		GetWorldTimerManager().SetTimer(Later, FTimerDelegate::CreateWeakLambda(this, [this, WeakSign]()
		{
			ShowCurrentSignDesign(WeakSign.Get());
		}), 1.f, false);
	}
	ForceNetUpdate();
	OnRulesChanged.Broadcast();
}

void AAMAutomationSubsystem::SetLightSettings(AFGBuildable* Light, const FAMLightSetting& Normal, const TArray<FAMLightSetting>& Settings)
{
	if (!HasAuthority() || !AMLight::IsLight(Light))
	{
		return;
	}
	int32 Index = mMachines.IndexOfByPredicate([Light](const FAMMachineRules& Entry) { return Entry.Machine == Light; });
	if (Index == INDEX_NONE)
	{
		if (Settings.Num() == 0)
		{
			return;
		}
		FAMMachineRules& NewEntry = mMachines.AddDefaulted_GetRef();
		NewEntry.Machine = Light;
		Index = mMachines.Num() - 1;
	}
	FAMMachineRules& Entry = mMachines[Index];
	Entry.LightNormal = Normal;
	Entry.LightNormal.Event.Reset();
	Entry.LightSettings.Reset();
	for (const FAMLightSetting& Setting : Settings)
	{
		FAMLightSetting Clean = Setting;
		Clean.Event = AMRules::NormalizeEventName(Setting.Event);
		if (!Clean.Event.IsEmpty() && !Entry.LightSettings.ContainsByPredicate([&Clean](const FAMLightSetting& S) { return S.Event == Clean.Event; }))
		{
			Entry.LightSettings.Add(Clean);
		}
	}
	if (!Entry.LightSettings.ContainsByPredicate([&Entry](const FAMLightSetting& S) { return S.Event == Entry.LightShowing; }))
	{
		Entry.LightShowing.Reset();
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("%s: %d light settings, showing %s"), *AMAS_Describe(Light), Entry.LightSettings.Num(),
		Entry.LightShowing.IsEmpty() ? TEXT("the normal settings") : *Entry.LightShowing);
	if (Entry.LightSettings.Num() == 0)
	{
		AMLight::Apply(Light, Normal);
		if (Entry.IsEmpty())
		{
			mMachines.RemoveAt(Index);
		}
	}
	else
	{
		TWeakObjectPtr<AFGBuildable> WeakLight(Light);
		FTimerHandle Later;
		GetWorldTimerManager().SetTimer(Later, FTimerDelegate::CreateWeakLambda(this, [this, WeakLight]()
		{
			ShowCurrentLightSetting(WeakLight.Get());
		}), 1.f, false);
	}
	ForceNetUpdate();
	OnRulesChanged.Broadcast();
}

void AAMAutomationSubsystem::PreviewLight(AFGBuildable* Light, const FAMLightSetting& Setting)
{
	if (HasAuthority() && AMLight::IsLight(Light))
	{
		AMLight::Apply(Light, Setting);
	}
}

void AAMAutomationSubsystem::ShowCurrentLightSetting(AFGBuildable* Light)
{
	const FAMMachineRules* Rules = FindRules(Light);
	if (!Rules || Rules->LightSettings.Num() == 0)
	{
		return;
	}
	const FAMLightSetting* Setting = Rules->LightSettings.FindByPredicate([Rules](const FAMLightSetting& S) { return S.Event == Rules->LightShowing; });
	AMLight::Apply(Light, Setting ? *Setting : Rules->LightNormal);
}

void AAMAutomationSubsystem::ClearAlarm(const FString& Event, bool bRingers, const AActor* OnlyMachine, bool bOnlyHub, bool bMam)
{
	if (!HasAuthority() || Event.IsEmpty())
	{
		return;
	}
	int32 Removed = 0;
	TArray<AFGBuildableWidgetSign*> SignsToRefresh;
	TArray<AFGBuildable*> LightsToRefresh;
	for (FAMMachineRules& Entry : mMachines)
	{
		if (bOnlyHub || (OnlyMachine && Entry.Machine.Get() != OnlyMachine))
		{
			continue;
		}
		if (bRingers)
		{
			const int32 Count = Entry.Fires.RemoveAll([&Event](const FAMFireRule& Rule) { return Rule.Event == Event; });
			if (Count > 0)
			{
				Removed += Count;
				// Their conditions start again.
				mLastConditionValues.Remove(TWeakObjectPtr<AFGBuildable>(Entry.Machine.Get()));
			}
		}
		else
		{
			const int32 RemovedReactions = Entry.Reactions.RemoveAll([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; });
			Removed += RemovedReactions;
			if (RemovedReactions > 0)
			{
				ReleaseActionStates(Entry);
			}
			const int32 Designs = Entry.SignDesigns.RemoveAll([&Event](const FAMSignDesign& Design) { return Design.Event == Event; });
			if (Designs > 0)
			{
				Removed += Designs;
				if (Entry.SignShowing == Event)
				{
					Entry.SignShowing.Reset();
				}
				if (AFGBuildableWidgetSign* Sign = Cast<AFGBuildableWidgetSign>(Entry.Machine))
				{
					SignsToRefresh.Add(Sign);
					if (Entry.SignDesigns.Num() == 0)
					{
						// A plain sign again: its normal design.
						AMSign::Show(Sign, Entry.SignNormal);
					}
				}
			}
			const int32 Lights = Entry.LightSettings.RemoveAll([&Event](const FAMLightSetting& Setting) { return Setting.Event == Event; });
			if (Lights > 0)
			{
				Removed += Lights;
				if (Entry.LightShowing == Event)
				{
					Entry.LightShowing.Reset();
				}
				if (Entry.Machine)
				{
					if (Entry.LightSettings.Num() == 0)
					{
						AMLight::Apply(Entry.Machine, Entry.LightNormal);
					}
					else
					{
						LightsToRefresh.Add(Entry.Machine);
					}
				}
			}
		}
	}
	if (bRingers && !OnlyMachine)
	{
		// All of them, or (one row of the overview) only the HUB's or only the MAM's.
		Removed += mWorldFires.RemoveAll([&Event, bOnlyHub, bMam](const FAMFireRule& Rule)
		{
			return Rule.Event == Event && (!bOnlyHub || AMRules::IsMamCondition(Rule.Condition) == bMam);
		});
	}
	if (!bRingers && !OnlyMachine && !(bOnlyHub && bMam))
	{
		Removed += mWorldActions.RemoveAll([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; });
	}
	for (AFGBuildableWidgetSign* Sign : SignsToRefresh)
	{
		ShowCurrentSignDesign(Sign);
	}
	for (AFGBuildable* Light : LightsToRefresh)
	{
		ShowCurrentLightSetting(Light);
	}
	RemoveInvalidEntries();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Alarm %s cleared (%s): %d rows removed"), *Event, bRingers ? TEXT("ringers") : TEXT("listeners"), Removed);
	ForceNetUpdate();
	OnRulesChanged.Broadcast();
}

void AAMAutomationSubsystem::ShowCurrentSignDesign(AFGBuildableWidgetSign* Sign)
{
	const FAMMachineRules* Rules = FindRules(Sign);
	if (!Rules || Rules->SignDesigns.Num() == 0)
	{
		return;
	}
	const FAMSignDesign* Design = Rules->SignDesigns.FindByPredicate([Rules](const FAMSignDesign& D) { return D.Event == Rules->SignShowing; });
	AMSign::Show(Sign, Design ? *Design : Rules->SignNormal);
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
	if (const FAMMachineRules* Existing = FindRules(Machine))
	{
		// Sign designs and light settings are set on their own (SetSignDesigns, SetLightSettings);
		// a signal's block and a station's display text are states the actions left.
		NewRules.SignDesigns = Existing->SignDesigns;
		NewRules.SignNormal = Existing->SignNormal;
		NewRules.SignShowing = Existing->SignShowing;
		NewRules.LightSettings = Existing->LightSettings;
		NewRules.LightNormal = Existing->LightNormal;
		NewRules.LightShowing = Existing->LightShowing;
		NewRules.bSignalBlocked = Existing->bSignalBlocked;
		NewRules.StationTextShown = Existing->StationTextShown;
	}
	NewRules.Options.bPassWrongItems = Options.bPassWrongItems && AMRules::HasPassWrongItems(Machine);
	NewRules.Options.bPassExtraItems = Options.bPassExtraItems && AMRules::HasPassWrongItems(Machine);
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

	ReleaseActionStates(NewRules);

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

	UE_LOG(LogAutomatedMachines, Log, TEXT("Rules of %s: %d fire rules, %d reactions, pass wrong items %s, extra items %s"),
		*AMAS_Describe(Machine), NewRules.Fires.Num(), NewRules.Reactions.Num(), NewRules.Options.bPassWrongItems ? TEXT("on") : TEXT("off"),
		NewRules.Options.bPassExtraItems ? TEXT("on") : TEXT("off"));

	ForceNetUpdate();
	OnRulesChanged.Broadcast();
}

void AAMAutomationSubsystem::ReleaseActionStates(FAMMachineRules& Entry)
{
	// A signal with no Blocked action any more works normally again.
	if (Entry.bSignalBlocked && !Entry.Reactions.ContainsByPredicate([](const FAMReactionRule& Rule) { return AMRules::IsSignalAction(Rule.Action); }))
	{
		Entry.bSignalBlocked = false;
		AMSignal::SetBlocked(Cast<AFGBuildableRailroadSignal>(Entry.Machine), false);
		UE_LOG(LogAutomatedMachines, Log, TEXT("%s unblocked (no Blocked action left)"), *AMAS_Describe(Entry.Machine));
	}
	// A station with no Display text action any more shows its name.
	if (!Entry.StationTextShown.IsEmpty() && !Entry.Reactions.ContainsByPredicate([](const FAMReactionRule& Rule) { return Rule.Action == EAMAction::StationText; }))
	{
		Entry.StationTextShown.Reset();
	}
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

void AAMAutomationSubsystem::SetWorldRules(const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Actions, bool bMam)
{
	if (!HasAuthority())
	{
		return;
	}
	// Only this tab's world alarms are replaced (the HUB's or the MAM's).
	mWorldFires.RemoveAll([bMam](const FAMFireRule& Rule) { return AMRules::IsMamCondition(Rule.Condition) == bMam; });
	for (const FAMFireRule& Rule : Fires)
	{
		FAMFireRule Clean = Rule;
		Clean.Event = AMRules::NormalizeEventName(Rule.Event);
		Clean.Item = nullptr;
		if (!Clean.Event.IsEmpty() && AMRules::IsWorldCondition(Clean.Condition) && AMRules::IsMamCondition(Clean.Condition) == bMam)
		{
			mWorldFires.Add(Clean);
		}
	}
	if (bMam)
	{
		// The MAM's tab has no actions: the world actions stay as they are.
		UE_LOG(LogAutomatedMachines, Log, TEXT("World alarms: %d (MAM tab)"), mWorldFires.Num());
		ForceNetUpdate();
		OnRulesChanged.Broadcast();
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
	UE_LOG(LogAutomatedMachines, Log, TEXT("World alarms: %d, world actions: %d"), mWorldFires.Num(), mWorldActions.Num());
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
	for (const FAMFireRule& Rule : mWorldFires)
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
		for (const FAMSignDesign& Design : Entry.SignDesigns)
		{
			Names.Add(Design.Event);
		}
		for (const FAMLightSetting& Setting : Entry.LightSettings)
		{
			Names.Add(Setting.Event);
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
		if (Entry.Reactions.ContainsByPredicate([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; })
			|| Entry.SignDesigns.ContainsByPredicate([&Event](const FAMSignDesign& Design) { return Design.Event == Event; })
			|| Entry.LightSettings.ContainsByPredicate([&Event](const FAMLightSetting& Setting) { return Setting.Event == Event; }))
		{
			++OutActors;
		}
	}
	// The HUB listens, and rings world events, too.
	if (mWorldActions.ContainsByPredicate([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; }))
	{
		++OutActors;
	}
	if (mWorldFires.ContainsByPredicate([&Event](const FAMFireRule& Rule) { return Rule.Event == Event; }))
	{
		++OutRingers;
	}
}

bool AAMAutomationSubsystem::CanAutomate(const AActor* Machine)
{
	// Production machines, and extractors (miners; also water and oil extractors, resource well
	// extractors: they are the same class).
	// Storage containers too (not the Dimensional Depot uploader: its own window, later).
	// Fuel generators: Biomass Burner, Coal, Fuel, Nuclear (one class; their windows come one by one).
	// Power switches and priority power switches (the priority switch is a subclass).
	return Machine && (Machine->IsA<AFGBuildableManufacturer>() || Machine->IsA<AFGBuildableResourceExtractor>()
		|| (Machine->IsA<AFGBuildableStorage>() && !Machine->IsA<AFGCentralStorageContainer>())
		|| Machine->IsA<AFGBuildableGeneratorFuel>() || Machine->IsA<AFGBuildableWidgetSign>()
		|| Machine->IsA<AFGBuildableCircuitSwitch>() || AMLight::IsLight(Machine)
		|| AMRules::IsValve(Machine) || Machine->IsA<AFGBuildablePipeReservoir>() || Machine->IsA<AFGBuildableResourceSink>()
		|| Machine->IsA<AFGBuildableRailroadSignal>() || Machine->IsA<AFGBuildableRailroadStation>()
		|| Machine->IsA<AFGBuildableTrainPlatformCargo>() || Machine->IsA<AFGBuildableDockingStation>());
}

bool AAMAutomationSubsystem::RemoveInvalidEntries()
{
	const int32 Removed = mMachines.RemoveAll([](const FAMMachineRules& Entry)
	{
		const bool bRemove = !IsValid(Entry.Machine) || Entry.IsEmpty();
		if (bRemove && Entry.bSignalBlocked)
		{
			// A signal without automation is never held red.
			AMSignal::SetBlocked(Cast<AFGBuildableRailroadSignal>(Entry.Machine), false);
		}

		return bRemove;
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

void AAMAutomationSubsystem::UpdateStationDisplays()
{
	// The text on each station's display (its UFGTextRenderer): ours, or its name again. Only the
	// picture: re-written now and then (the station's blueprint writes its name when it changes).
	TSet<TWeakObjectPtr<AActor>> Seen;
	for (const FAMMachineRules& Entry : mMachines)
	{
		AFGBuildableRailroadStation* Station = Cast<AFGBuildableRailroadStation>(Entry.Machine);
		if (!Station || Entry.StationTextShown.IsEmpty())
		{
			continue;
		}
		Seen.Add(Station);
		TArray<UFGTextRenderer*> Displays;
		Station->GetComponents<UFGTextRenderer>(Displays);
		for (UFGTextRenderer* Display : Displays)
		{
			// UpdateText is private in C++ but a Blueprint function: called by name.
			if (UFunction* Update = Display->FindFunction(TEXT("UpdateText")))
			{
				struct
				{
					int32 ElementID = 0;
					FString Text;
				} Params;
				Params.Text = Entry.StationTextShown;
				Display->ProcessEvent(Update, &Params);
			}
		}
		if (!mStationDisplays.Contains(Station))
		{
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s: %d display(s) show %s"), *AMAS_Describe(Station), Displays.Num(), *Entry.StationTextShown);
		}
		mStationDisplays.Add(Station);
	}
	// Stations whose text went: their name again (their blueprint draws it).
	for (auto It = mStationDisplays.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(*It))
		{
			if (AFGBuildableRailroadStation* Station = Cast<AFGBuildableRailroadStation>(It->Get()))
			{
				Station->OnNameChanged();
			}
			It.RemoveCurrent();
		}
	}
}

void AAMAutomationSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsRunningDedicatedServer())
	{
		mTimeSinceDisplays += DeltaSeconds;
		if (mTimeSinceDisplays >= 1.f)
		{
			mTimeSinceDisplays = 0.f;
			UpdateStationDisplays();
		}
	}
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
		AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(Entry.Machine);
		if (Factory && Entry.Options.bPassWrongItems)
		{
			TakeWrongItem(Factory);
		}
		// Both share the one waiting item per machine: a wrong item, if any, goes first.
		if (Factory && Entry.Options.bPassExtraItems)
		{
			TakeExtraItem(Factory);
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
		const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Entry.Machine);
		AFGBuildableRailroadSignal* Signal = Cast<AFGBuildableRailroadSignal>(Entry.Machine);
		const AFGBuildable* Machine = Entry.Machine;
		if ((!Factory && !Switch && !Signal) || Entry.Fires.Num() == 0)
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
			bool bNow = false;
			if (Signal)
			{
				switch (Rule.Condition)
				{
				case EAMCondition::SignalRed:    bNow = AMSignal::IsRed(Signal); break;
				case EAMCondition::SignalGreen:  bNow = AMSignal::IsGreen(Signal); break;
				case EAMCondition::TrainInBlock: bNow = AMSignal::IsBlockOccupied(Signal); break;
				case EAMCondition::BlockEmpty:   bNow = !AMSignal::IsBlockOccupied(Signal); break;
				default: break;
				}
			}
			else
			{
				bNow = Switch ? IsSwitchConditionTrue(Switch, Rule) : IsConditionTrue(Factory, Rule, bRecipeChanged);
			}
			const bool bBecameTrue = bNow && LastValues[Index] != 1;
			LastValues[Index] = bNow ? 1 : 0;
			if (bBecameTrue)
			{
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s fires %s (%s)"), *AMAS_Describe(Machine), *Rule.Event,
					*AMRules::GetConditionText(Rule.Condition).ToString());
				EventsToFire.Emplace(Rule.Event, Machine);
			}
		}
	}
	// Fire after the loop: actions never change the rules, but this keeps the loop simple and safe.
	for (const TPair<FString, const AActor*>& Fired : EventsToFire)
	{
		FireEvent(Fired.Key, Fired.Value);
	}
}

void AAMAutomationSubsystem::FireEvent(const FString& Event, const AActor* Ringer, const FString& RungBy)
{
	if (!HasAuthority())
	{
		return;
	}
	// Who listens (in the log: an alarm that rings and changes nothing is easy to spot).
	int32 Listeners = 0;
	FString ListenerText;
	for (const FAMMachineRules& Entry : mMachines)
	{
		for (const FAMReactionRule& Rule : Entry.Reactions)
		{
			if (Rule.Event == Event)
			{
				++Listeners;
				ListenerText += FString::Printf(TEXT(" %s:%s"), *GetNameSafe(Entry.Machine), *AMRules::GetActionText(Rule.Action).ToString());
			}
		}
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Alarm %s rings: %d machine action(s)%s"), *Event, Listeners, *ListenerText);
	for (const FAMReactionRule& Rule : mWorldActions)
	{
		if (Rule.Event == Event)
		{
			ApplyWorldAction(Rule, Ringer, RungBy);
		}
	}
	for (FAMMachineRules& Entry : mMachines)
	{
		// Signs: show this alarm's design.
		if (AFGBuildableWidgetSign* Sign = Cast<AFGBuildableWidgetSign>(Entry.Machine))
		{
			if (const FAMSignDesign* Design = Entry.SignDesigns.FindByPredicate([&Event](const FAMSignDesign& D) { return D.Event == Event; }))
			{
				Entry.SignShowing = Event;
				AMSign::Show(Sign, *Design);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s shows its %s design"), *AMAS_Describe(Sign), *Event);
			}
			continue;
		}
		// Lights: after this loop (panels first, then single lights).
		if (AMLight::IsLight(Entry.Machine))
		{
			continue;
		}
		// Railway signals: Block / Unblock (kept in the entry, saved).
		if (AFGBuildableRailroadSignal* Signal = Cast<AFGBuildableRailroadSignal>(Entry.Machine))
		{
			for (const FAMReactionRule& Rule : Entry.Reactions)
			{
				if (Rule.Event == Event && AMRules::IsSignalAction(Rule.Action))
				{
					Entry.bSignalBlocked = Rule.Action == EAMAction::BlockSignal;
					AMSignal::SetBlocked(Signal, Entry.bSignalBlocked);
					UE_LOG(LogAutomatedMachines, Log, TEXT("%s %s"), *AMAS_Describe(Signal), Entry.bSignalBlocked ? TEXT("blocked") : TEXT("unblocked"));
				}
			}
			continue;
		}
		// Power switches.
		if (AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Entry.Machine))
		{
			for (const FAMReactionRule& Rule : Entry.Reactions)
			{
				if (Rule.Event == Event)
				{
					ApplySwitchAction(Switch, Rule);
				}
			}
			continue;
		}
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

	// Lights: control panels first, then single lights, so a light's own settings for an alarm win
	// over its panel's (the panel passes its settings on to all its lights).
	for (const bool bPanels : { true, false })
	{
		for (FAMMachineRules& Entry : mMachines)
		{
			if (!AMLight::IsLight(Entry.Machine) || Entry.Machine->IsA<AFGBuildableLightsControlPanel>() != bPanels)
			{
				continue;
			}
			if (const FAMLightSetting* Setting = Entry.LightSettings.FindByPredicate([&Event](const FAMLightSetting& S) { return S.Event == Event; }))
			{
				Entry.LightShowing = Event;
				AMLight::Apply(Entry.Machine, *Setting);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s gets its %s settings"), *AMAS_Describe(Entry.Machine), *Event);
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
	case EAMCondition::PlatformBusy:
	case EAMCondition::PlatformIdle:
		if (const AFGBuildableTrainPlatformCargo* Platform = Cast<AFGBuildableTrainPlatformCargo>(Factory))
		{
			// Busy: a docked train being loaded / unloaded (not yet complete).
			const ETrainPlatformDockingStatus Status = Platform->GetDockingStatus();
			const bool bBusy = Platform->GetDockedActor() && Status != ETrainPlatformDockingStatus::ETPDS_None
				&& Status != ETrainPlatformDockingStatus::ETPDS_Complete;
			return Rule.Condition == EAMCondition::PlatformBusy ? bBusy : !bBusy;
		}
		if (const AFGBuildableDockingStation* TruckStation = Cast<AFGBuildableDockingStation>(Factory))
		{
			const bool bBusy = TruckStation->GetDockedActor() && TruckStation->IsLoadUnloading();
			return Rule.Condition == EAMCondition::PlatformBusy ? bBusy : !bBusy;
		}
		return false;
	case EAMCondition::VehicleArrives:
	case EAMCondition::VehicleLeaves:
		if (const AFGBuildableDockingStation* TruckStation = Cast<AFGBuildableDockingStation>(Factory))
		{
			const bool bDocked = TruckStation->GetDockedActor() != nullptr;
			return Rule.Condition == EAMCondition::VehicleArrives ? bDocked : !bDocked;
		}
		return false;
	case EAMCondition::TrainArrives:
	case EAMCondition::TrainLeaves:
	case EAMCondition::TransferDone:
		if (const AFGBuildableRailroadStation* Station = Cast<AFGBuildableRailroadStation>(Factory))
		{
			// Docked: a train docked (IsDocked) at this station (its mDockedAtStation, read by name: private).
			// The station's own docking status and docked vehicle stayed empty (tested 7 Nov 2026).
			static const FObjectPropertyBase* DockedAtProperty = FindFProperty<FObjectPropertyBase>(AFGTrain::StaticClass(), TEXT("mDockedAtStation"));
			static const FArrayProperty* PlatformsProperty = FindFProperty<FArrayProperty>(AFGBuildableRailroadStation::StaticClass(), TEXT("mDockedPlatformList"));
			bool bDocked = false;
			for (TActorIterator<AFGTrain> It(Station->GetWorld()); It && DockedAtProperty && !bDocked; ++It)
			{
				bDocked = It->IsDocked() && DockedAtProperty->GetObjectPropertyValue_InContainer(*It) == Station;
			}
			// A station without freight platforms: trains only stop there (no docking). A train
			// standing (under 1 m/s) on the station counts too.
			if (!bDocked)
			{
				const FBox Area = Station->GetComponentsBoundingBox().ExpandBy(200.f);
				for (TActorIterator<AFGRailroadVehicle> It(Station->GetWorld()); It && !bDocked; ++It)
				{
					bDocked = Area.IsInside(It->GetActorLocation()) && It->GetVelocity().Size() < 100.f;
				}
			}
			switch (Rule.Condition)
			{
			case EAMCondition::TrainArrives: return bDocked;
			case EAMCondition::TrainLeaves:  return !bDocked;
			default:
			{
				// Done: docked, and every platform it docked has finished (Complete).
				if (!bDocked || !PlatformsProperty)
				{
					return false;
				}
				FScriptArrayHelper Platforms(PlatformsProperty, PlatformsProperty->ContainerPtrToValuePtr<void>(Station));
				const FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(PlatformsProperty->Inner);
				int32 Done = 0;
				int32 Count = 0;
				for (int32 Index = 0; Inner && Index < Platforms.Num(); ++Index)
				{
					if (const AFGBuildableTrainPlatform* Platform = Cast<AFGBuildableTrainPlatform>(Inner->GetObjectPropertyValue(Platforms.GetRawPtr(Index))))
					{
						if (Platform == Station)
						{
							continue;
						}
						++Count;
						Done += Platform->GetDockingStatus() == ETrainPlatformDockingStatus::ETPDS_Complete ? 1 : 0;
					}
				}
				return Count > 0 && Done == Count;
			}
			}
		}
		return false;
	case EAMCondition::CouponsAbove:
	case EAMCondition::CouponsBelow:
		if (const AFGResourceSinkSubsystem* Sinks = AFGResourceSinkSubsystem::Get(const_cast<AFGBuildableFactory*>(Factory)))
		{
			const float Coupons = static_cast<float>(Sinks->GetNumCoupons());
			return Rule.Condition == EAMCondition::CouponsAbove ? Coupons > Rule.Value : Coupons < Rule.Value;
		}
		return false;
	case EAMCondition::FlowAbove:
	case EAMCondition::FlowBelow:
		if (const AFGBuildablePipelinePump* Pump = Cast<AFGBuildablePipelinePump>(Factory))
		{
			// m3/s in the game, m3/min in its window and in our alarms.
			const float Flow = FMath::Abs(Pump->GetIndicatorFlow()) * 60.f;
			return Rule.Condition == EAMCondition::FlowAbove ? Flow > Rule.Value : Flow < Rule.Value;
		}
		return false;
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
	case EAMCondition::FuelEmpty:         return IsOutOfFuel(Factory);
	case EAMCondition::FuelAvailable:     return !IsOutOfFuel(Factory);
	case EAMCondition::FuelBelow:         return CountFuel(Factory) < Rule.Value;
	case EAMCondition::LoadAbove:         return GetLoad(Factory) * 100.f > Rule.Value;
	case EAMCondition::LoadBelow:         return GetLoad(Factory) * 100.f < Rule.Value;
	case EAMCondition::FuseBlown:         return IsFuseBlown(Factory);
	case EAMCondition::FuseReset:         return !IsFuseBlown(Factory);
	case EAMCondition::MilestoneCompleted: return false;
	case EAMCondition::ResearchCompleted: return false;
	case EAMCondition::SwitchedOn:        return false;
	case EAMCondition::SwitchedOff:       return false;
	default:                              return false;
	}
}

namespace
{
	/** The contents of a storage container or a freight platform (items or fluid). */
	const UFGInventoryComponent* AMAS_StorageInventory(const AFGBuildableFactory* Factory)
	{
		if (const AFGBuildableStorage* Storage = Cast<AFGBuildableStorage>(Factory))
		{
			return const_cast<AFGBuildableStorage*>(Storage)->GetStorageInventory();
		}
		if (const AFGBuildableTrainPlatformCargo* Platform = Cast<AFGBuildableTrainPlatformCargo>(Factory))
		{
			return Platform->GetInventory();
		}
		if (const AFGBuildableDockingStation* TruckStation = Cast<AFGBuildableDockingStation>(Factory))
		{
			return TruckStation->GetInventory();
		}
		return nullptr;
	}
}

float AAMAutomationSubsystem::GetFillLevel(const AFGBuildableFactory* Factory)
{
	// Fluid buffers: their fluid / their capacity (it can go a little over: kept at 1).
	if (const AFGBuildablePipeReservoir* Buffer = Cast<AFGBuildablePipeReservoir>(Factory))
	{
		const float Max = Buffer->GetFluidContentMax();
		return Max > 0.f ? FMath::Clamp(Buffer->GetFluidContent() / Max, 0.f, 1.f) : 0.f;
	}
	// 0..1: each slot counts as (items / stack size); a container of 48 slots with 24 full stacks is 0.5.
	const UFGInventoryComponent* Inventory = AMAS_StorageInventory(Factory);
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

bool AAMAutomationSubsystem::IsOutOfFuel(const AFGBuildableFactory* Factory)
{
	if (Cast<AFGBuildableDockingStation>(Factory))
	{
		return CountFuel(Factory) <= 0;
	}
	const AFGBuildableGeneratorFuel* Generator = Cast<AFGBuildableGeneratorFuel>(Factory);
	return Generator && !Generator->HasFuel();
}

int32 AAMAutomationSubsystem::CountFuel(const AFGBuildableFactory* Factory)
{
	// Truck stations: every item in the fuel slot(s).
	if (const AFGBuildableDockingStation* TruckStation = Cast<AFGBuildableDockingStation>(Factory))
	{
		const UFGInventoryComponent* StationFuel = TruckStation->GetFuelInventory();
		int32 Count = 0;
		for (int32 SlotIndex = 0; StationFuel && SlotIndex < StationFuel->GetSizeLinear(); ++SlotIndex)
		{
			FInventoryStack Stack;
			if (StationFuel->GetStackFromIndex(SlotIndex, Stack) && Stack.HasItems())
			{
				Count += Stack.NumItems;
			}
		}
		return Count;
	}
	const AFGBuildableGeneratorFuel* Generator = Cast<AFGBuildableGeneratorFuel>(Factory);
	const UFGInventoryComponent* Fuel = Generator ? Generator->GetFuelInventory() : nullptr;
	FInventoryStack Stack;
	if (Fuel && Fuel->GetStackFromIndex(Generator->GetFuelInventoryIndex(), Stack) && Stack.HasItems())
	{
		return Stack.NumItems;
	}
	return 0;
}

float AAMAutomationSubsystem::GetLoad(const AFGBuildableFactory* Factory)
{
	const AFGBuildableGenerator* Generator = Cast<AFGBuildableGenerator>(Factory);
	return Generator ? Generator->GetLoadPercentage() : 0.f;
}

bool AAMAutomationSubsystem::IsFuseBlown(const AFGBuildableFactory* Factory)
{
	const UFGPowerInfoComponent* Power = Factory ? Factory->GetPowerInfo() : nullptr;
	return Power && Power->IsFuseTriggered();
}

int32 AAMAutomationSubsystem::CountItem(const AFGBuildableFactory* Factory, TSubclassOf<UFGItemDescriptor> Item)
{
	const UFGInventoryComponent* Inventory = AMAS_StorageInventory(Factory);
	return Inventory && Item ? Inventory->GetNumItems(Item) : 0;
}

bool AAMAutomationSubsystem::IsBeltBlocked(const AFGBuildableFactory* Factory)
{
	// The item waiting at the end of an input belt has no room in the container.
	const UFGInventoryComponent* Inventory = AMAS_StorageInventory(Factory);
	if (!Inventory)
	{
		return false;
	}
	for (const UFGFactoryConnectionComponent* Connection : Factory->GetConnectionComponents())
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
	// A generator: its fuel slot has no room for one more of the fuel it holds.
	if (const AFGBuildableGeneratorFuel* Generator = Cast<AFGBuildableGeneratorFuel>(Factory))
	{
		const UFGInventoryComponent* Fuel = Generator->GetFuelInventory();
		FInventoryStack Stack;
		return Fuel && Fuel->GetStackFromIndex(Generator->GetFuelInventoryIndex(), Stack) && Stack.HasItems()
			&& !Fuel->HasEnoughSpaceForItem(FInventoryItem(Stack.Item.GetItemClass()));
	}
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
	// What the machine takes: its recipe's ingredients, or a generator's fuels.
	const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory);
	const AFGBuildableGeneratorFuel* Generator = Cast<AFGBuildableGeneratorFuel>(Factory);
	TArray<const UClass*> Ingredients;
	if (Manufacturer && Manufacturer->GetCurrentRecipe())
	{
		for (const FItemAmount& Ingredient : UFGRecipe::GetIngredients(Manufacturer, Manufacturer->GetCurrentRecipe()))
		{
			Ingredients.Add(Ingredient.ItemClass.Get());
		}
	}
	else if (!Generator)
	{
		return false;
	}
	auto Accepts = [&Ingredients, Generator](const UClass* Item)
	{
		if (Generator)
		{
			const TSubclassOf<UFGItemDescriptor> ItemClass(const_cast<UClass*>(Item));
			return Generator->IsValidFuel(ItemClass) || Generator->IsValidSupplementalResource(ItemClass);
		}
		return Ingredients.Contains(Item);
	};
	const AFGBuildableFactory* Machine = Factory;

	// Belts: the item waiting at the end of each input belt.
	for (UFGFactoryConnectionComponent* Connection : Machine->GetConnectionComponents())
	{
		if (!Connection || Connection->GetDirection() != EFactoryConnectionDirection::FCD_INPUT || !Connection->IsConnected())
		{
			continue;
		}
		UFGFactoryConnectionComponent* BeltEnd = Connection->GetConnection();
		TArray<FInventoryItem> Waiting;
		if (BeltEnd && BeltEnd->Factory_PeekOutput(Waiting) && Waiting.Num() > 0 && !Accepts(Waiting[0].GetItemClass().Get()))
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
	Machine->GetComponents<UFGPipeConnectionComponent>(Pipes);
	for (const UFGPipeConnectionComponent* Pipe : Pipes)
	{
		if (Pipe && Pipe->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER && Pipe->IsConnected())
		{
			const UClass* Fluid = Pipe->GetFluidDescriptor().Get();
			if (Fluid && !Accepts(Fluid))
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

bool AAMAutomationSubsystem::FindExtraInput(const AFGBuildableFactory* Factory, UFGFactoryConnectionComponent** OutBelt, FInventoryItem* OutItem)
{
	const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Factory);
	const UFGInventoryComponent* Input = Manufacturer ? Manufacturer->GetInputInventory() : nullptr;
	if (!Manufacturer || !Manufacturer->GetCurrentRecipe() || !Input)
	{
		return false;
	}
	TArray<const UClass*> Ingredients;
	for (const FItemAmount& Ingredient : UFGRecipe::GetIngredients(Manufacturer, Manufacturer->GetCurrentRecipe()))
	{
		Ingredients.Add(Ingredient.ItemClass.Get());
	}
	for (UFGFactoryConnectionComponent* Connection : Manufacturer->GetConnectionComponents())
	{
		if (!Connection || Connection->GetDirection() != EFactoryConnectionDirection::FCD_INPUT || !Connection->IsConnected())
		{
			continue;
		}
		UFGFactoryConnectionComponent* BeltEnd = Connection->GetConnection();
		TArray<FInventoryItem> Waiting;
		if (!BeltEnd || !BeltEnd->Factory_PeekOutput(Waiting) || Waiting.Num() == 0)
		{
			continue;
		}
		const TSubclassOf<UFGItemDescriptor> ItemClass = Waiting[0].GetItemClass();
		// A right item whose slot has no room for one more (wrong items are the other option's).
		if (Ingredients.Contains(ItemClass.Get()) && !Input->HasEnoughSpaceForStack(FInventoryStack(1, ItemClass)))
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
	return false;
}

void AAMAutomationSubsystem::TakeExtraItem(AFGBuildableFactory* Factory)
{
	{
		FScopeLock HeldScope(&AMAS_HeldLock);
		if (AMAS_Held.Contains(Factory))
		{
			return;
		}
	}
	UFGFactoryConnectionComponent* BeltEnd = nullptr;
	FInventoryItem Extra;
	if (!FindExtraInput(Factory, &BeltEnd, &Extra) || !BeltEnd || !AMAS_HasOutputBelt(Factory))
	{
		return;
	}
	FInventoryItem Grabbed;
	float OffsetBeyond = 0.f;
	if (BeltEnd->Factory_GrabOutput(Grabbed, OffsetBeyond, Extra.GetItemClass()))
	{
		AMAS_Hold(Factory, Grabbed);
		UE_LOG(LogAutomatedMachines, Verbose, TEXT("%s passes on %s (its slot is full)"), *AMAS_Describe(Factory), *GetNameSafe(Grabbed.GetItemClass().Get()));
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
	case EAMAction::SetFlowLimit:
		if (AFGBuildablePipelinePump* Pump = Cast<AFGBuildablePipelinePump>(Factory))
		{
			const float Limit = FMath::Max(0.f, Rule.Value) / 60.f;
			if (!FMath::IsNearlyEqual(Pump->GetUserFlowLimit(), Limit, 0.0001f))
			{
				Pump->SetUserFlowLimit(Limit);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s flow limit %.1f m3/min"), *AMAS_Describe(Factory), Rule.Value);
			}
		}
		break;
	case EAMAction::StationText:
	case EAMAction::StationName:
		if (Factory->IsA<AFGBuildableRailroadStation>())
		{
			if (FAMMachineRules* Entry = mMachines.FindByPredicate([Factory](const FAMMachineRules& E) { return E.Machine == Factory; }))
			{
				// The displays follow (UpdateStationDisplays, on every machine).
				Entry->StationTextShown = Rule.Action == EAMAction::StationText ? Rule.Message.TrimStartAndEnd() : FString();
				ForceNetUpdate();
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s display: %s"), *AMAS_Describe(Factory), Entry->StationTextShown.IsEmpty() ? TEXT("its name") : *Entry->StationTextShown);
			}
		}
		break;
	case EAMAction::FlushBuffer:
		if (AFGPipeSubsystem* Pipes = Factory->IsA<AFGBuildablePipeReservoir>() ? AFGPipeSubsystem::GetPipeSubsystem(Factory) : nullptr)
		{
			Pipes->FlushIntegrant(Factory);
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s: flushed"), *AMAS_Describe(Factory));
		}
		break;
	case EAMAction::FlushNetwork:
		if (AFGPipeSubsystem* Pipes = (AMRules::IsValve(Factory) || Factory->IsA<AFGBuildablePipeReservoir>()) ? AFGPipeSubsystem::GetPipeSubsystem(Factory) : nullptr)
		{
			Pipes->FlushPipeNetworkFromIntegrant(Factory);
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s: pipe network flushed"), *AMAS_Describe(Factory));
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
		if (AFGBuildableGeneratorFuel* Generator = Cast<AFGBuildableGeneratorFuel>(Factory))
		{
			if (Rule.Action == EAMAction::DeleteInput)
			{
				const int32 Slots = DeleteAllItems(Generator->GetFuelInventory());
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s: fuel deleted (%d slots had items)"), *AMAS_Describe(Factory), Slots);
			}
			break;
		}
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
	case EAMAction::OverflowOn:
	case EAMAction::OverflowOff:
		if (FAMMachineRules* Entry = mMachines.FindByPredicate([Factory](const FAMMachineRules& E) { return E.Machine == Factory; }))
		{
			const bool bOn = Rule.Action == EAMAction::OverflowOn;
			if (Entry->Options.bPassWrongItems != bOn || Entry->Options.bPassExtraItems != bOn)
			{
				Entry->Options.bPassWrongItems = bOn;
				Entry->Options.bPassExtraItems = bOn;
				ForceNetUpdate();
				OnRulesChanged.Broadcast();
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s: output overflow %s"), *AMAS_Describe(Factory), bOn ? TEXT("on") : TEXT("off"));
			}
		}
		break;
	case EAMAction::LoadMode:
	case EAMAction::UnloadMode:
		if (AFGBuildableTrainPlatformCargo* Platform = Cast<AFGBuildableTrainPlatformCargo>(Factory))
		{
			const bool bLoad = Rule.Action == EAMAction::LoadMode;
			if (Platform->GetIsInLoadMode() != bLoad)
			{
				Platform->SetIsInLoadMode(bLoad);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s: %s mode"), *AMAS_Describe(Factory), bLoad ? TEXT("load") : TEXT("unload"));
			}
		}
		else if (AFGBuildableDockingStation* TruckStation = Cast<AFGBuildableDockingStation>(Factory))
		{
			const bool bLoad = Rule.Action == EAMAction::LoadMode;
			if (TruckStation->GetIsInLoadMode() != bLoad)
			{
				TruckStation->SetIsInLoadMode(bLoad);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s: %s mode"), *AMAS_Describe(Factory), bLoad ? TEXT("load") : TEXT("unload"));
			}
		}
		break;
	case EAMAction::ReplaceFuelOn:
	case EAMAction::ReplaceFuelOff:
		if (AFGBuildableDockingStation* TruckStation = Cast<AFGBuildableDockingStation>(Factory))
		{
			const bool bOn = Rule.Action == EAMAction::ReplaceFuelOn;
			if (TruckStation->GetIsForceVehicleFuelType() != bOn)
			{
				TruckStation->SetForceVehicleFuelType(bOn);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s: replace differing fuel %s"), *AMAS_Describe(Factory), bOn ? TEXT("on") : TEXT("off"));
			}
		}
		break;
	case EAMAction::DeleteContents:
		if (UFGInventoryComponent* Contents = const_cast<UFGInventoryComponent*>(AMAS_StorageInventory(Factory)))
		{
			const int32 Slots = DeleteAllItems(Contents);
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

namespace
{
	/**
	 * The power used (bConsumed) or that can be made (capacity) by the buildings on one side of a switch, in MW; false if that
	 * side is not connected. Summed over the side's own buildings, not taken from the circuit's
	 * numbers: while the switch is on, both sides' circuits show the numbers of the joined grid.
	 * The circuit's list of buildings is private: read by name (a UPROPERTY). Kept for the rest of
	 * the check (several alarms of one switch ask for the same side).
	 */
	bool AMAS_GetGridPower(const AFGBuildableCircuitSwitch* Switch, int32 Side, bool bConsumed, float& OutPower)
	{
		AFGCircuitSubsystem* Circuits = AFGCircuitSubsystem::GetCircuitSubsystem(const_cast<AFGBuildableCircuitSwitch*>(Switch));
		const int32 CircuitID = Side == 0 ? Switch->GetCircuitID0() : Switch->GetCircuitID1();
		const UFGPowerCircuit* Circuit = Circuits && CircuitID != INDEX_NONE ? Circuits->FindCircuit<UFGPowerCircuit>(CircuitID) : nullptr;
		static const FArrayProperty* InfosProperty = FindFProperty<FArrayProperty>(UFGPowerCircuit::StaticClass(), TEXT("mPowerInfos"));
		if (!Circuit || !InfosProperty)
		{
			return false;
		}

		struct FSums
		{
			double Time = -1.0;
			float Used = 0.f;
			float Made = 0.f;
		};
		static TMap<TWeakObjectPtr<const UFGPowerCircuit>, FSums> Cache;
		const UWorld* World = Switch->GetWorld();
		const double Now = World ? World->GetTimeSeconds() : 0.0;
		FSums& Sums = Cache.FindOrAdd(Circuit);
		if (Sums.Time != Now)
		{
			Sums.Time = Now;
			Sums.Used = 0.f;
			Sums.Made = 0.f;
			FScriptArrayHelper Infos(InfosProperty, InfosProperty->ContainerPtrToValuePtr<void>(Circuit));
			const FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(InfosProperty->Inner);
			for (int32 Index = 0; Inner && Index < Infos.Num(); ++Index)
			{
				const UFGPowerInfoComponent* Info = Cast<UFGPowerInfoComponent>(Inner->GetObjectPropertyValue(Infos.GetRawPtr(Index)));
				if (Info)
				{
					Sums.Used += Info->GetActualConsumption();
					// What the generators can make (the graph's capacity), not what they make now:
					// generators make only what is used, so that would just repeat the usage.
					Sums.Made += Info->GetBaseProduction() + Info->GetDynamicProductionCapacity();
				}
			}
		}
		OutPower = bConsumed ? Sums.Used : Sums.Made;
		if (Cache.Num() > 256)
		{
			for (auto It = Cache.CreateIterator(); It; ++It)
			{
				if (!It->Key.IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
		return true;
	}

	/**
	 * Side A has more than side B: by a small margin (0.1 MW, or 1% of the larger), so two nearly
	 * equal grids do not ring the pair of alarms over and over.
	 */
	bool AMAS_GridHasMore(const AFGBuildableCircuitSwitch* Switch, int32 Side, bool bConsumed)
	{
			float Mine = 0.f;
		float Other = 0.f;
		if (!AMAS_GetGridPower(Switch, Side, bConsumed, Mine) || !AMAS_GetGridPower(Switch, 1 - Side, bConsumed, Other))
		{
			return false;
		}
		const float Margin = FMath::Max(0.1f, 0.01f * FMath::Max(Mine, Other));
		return Mine > Other + Margin;
	}
}

bool AAMAutomationSubsystem::IsSwitchConditionTrue(const AFGBuildableCircuitSwitch* Switch, const FAMFireRule& Rule)
{
	switch (Rule.Condition)
	{
	case EAMCondition::SwitchedOn:     return Switch->IsSwitchOn();
	case EAMCondition::SwitchedOff:    return !Switch->IsSwitchOn();
	case EAMCondition::GridAUsesMore:  return AMAS_GridHasMore(Switch, 0, true);
	case EAMCondition::GridBUsesMore:  return AMAS_GridHasMore(Switch, 1, true);
	case EAMCondition::GridAMakesMore: return AMAS_GridHasMore(Switch, 0, false);
	case EAMCondition::GridBMakesMore: return AMAS_GridHasMore(Switch, 1, false);
	default:                           return false;
	}
}

void AAMAutomationSubsystem::ApplySwitchAction(AFGBuildableCircuitSwitch* Switch, const FAMReactionRule& Rule)
{
	switch (Rule.Action)
	{
	case EAMAction::SwitchOn:
	case EAMAction::SwitchOff:
	case EAMAction::ToggleSwitch:
	{
		const bool bOn = Rule.Action == EAMAction::SwitchOn ? true : Rule.Action == EAMAction::SwitchOff ? false : !Switch->IsSwitchOn();
		if (Switch->IsSwitchOn() != bOn)
		{
			Switch->SetSwitchOn(bOn);
			UE_LOG(LogAutomatedMachines, Log, TEXT("%s turned %s"), *AMAS_Describe(Switch), bOn ? TEXT("on") : TEXT("off"));
		}
		break;
	}
	case EAMAction::SetPriority:
		if (AFGBuildablePriorityPowerSwitch* Priority = Cast<AFGBuildablePriorityPowerSwitch>(Switch))
		{
			const int32 Target = FMath::Clamp(FMath::RoundToInt(Rule.Value), 0, 8);
			if (Priority->GetPriority() != Target)
			{
				Priority->SetPriority(Target);
				UE_LOG(LogAutomatedMachines, Log, TEXT("%s priority %d"), *AMAS_Describe(Switch), Target);
			}
		}
		break;
	default:
		break;
	}
}

void AAMAutomationSubsystem::ApplyWorldAction(const FAMReactionRule& Rule, const AActor* Ringer, const FString& RungBy)
{
	switch (Rule.Action)
	{
	case EAMAction::ShowMessage:
	{
		// Title: the alarm. Text: the player's own, or the machine that rang it.
		FString Body = Rule.Message;
		if (Body.IsEmpty())
		{
			Body = !RungBy.IsEmpty() ? FString::Printf(TEXT("Rung by %s"), *RungBy)
				: Ringer ? FString::Printf(TEXT("Rung by %s"), *GetMachineName(Ringer)) : FString(TEXT("Rang"));
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
	const FString Name = NameProperty ? NameProperty->GetPropertyValue_InContainer(Machine).ToString() : AMAS_Describe(Machine);
	// Power switches have a name the player gave them (their tag): "Power Switch MAIN BASE".
	if (const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Machine))
	{
		if (Switch->HasBuildingTag_Implementation())
		{
			const FString Tag = Switch->GetBuildingTag_Implementation().TrimStartAndEnd();
			if (!Tag.IsEmpty())
			{
				return FString::Printf(TEXT("%s %s"), *Name, *Tag);
			}
		}
	}
	return Name;
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

void AAMAutomationSubsystem::BindWorldEvents()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!bMilestonesBound)
	{
		if (AFGSchematicManager* Schematics = AFGSchematicManager::Get(World))
		{
			Schematics->PurchasedSchematicDelegate.AddUniqueDynamic(this, &AAMAutomationSubsystem::OnSchematicPurchased);
			bMilestonesBound = true;
			UE_LOG(LogAutomatedMachines, Log, TEXT("World events: listening for milestones"));
		}
	}
	if (!bResearchBound)
	{
		if (AFGResearchManager* Research = AFGResearchManager::Get(World))
		{
			Research->ResearchCompletedDelegate.AddUniqueDynamic(this, &AAMAutomationSubsystem::OnResearchCompleted);
			bResearchBound = true;
			UE_LOG(LogAutomatedMachines, Log, TEXT("World events: listening for MAM research"));
		}
	}
	if (!bMilestonesBound || !bResearchBound)
	{
		// Not there yet (early in the load): again in a second.
		World->GetTimerManager().SetTimer(mBindWorldEventsTimer, this, &AAMAutomationSubsystem::BindWorldEvents, 1.f, false);
	}
}

void AAMAutomationSubsystem::OnResearchCompleted(TSubclassOf<UFGSchematic> Schematic)
{
	if (!HasAuthority() || !Schematic)
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		if (World->GetTimeSeconds() < 10.0)
		{
			return;
		}
	}
	FireWorldEvent(EAMCondition::ResearchCompleted, UFGSchematic::GetSchematicDisplayName(Schematic).ToString());
}

void AAMAutomationSubsystem::OnSchematicPurchased(TSubclassOf<UFGSchematic> Schematic)
{
	if (!HasAuthority() || !Schematic || UFGSchematic::GetType(Schematic) != ESchematicType::EST_Milestone)
	{
		return;
	}
	// Not the milestones the game gives back while a save loads.
	if (const UWorld* World = GetWorld())
	{
		if (World->GetTimeSeconds() < 10.0)
		{
			return;
		}
	}
	FireWorldEvent(EAMCondition::MilestoneCompleted, UFGSchematic::GetSchematicDisplayName(Schematic).ToString());
}

void AAMAutomationSubsystem::FireWorldEvent(EAMCondition Condition, const FString& Detail)
{
	// The world rings them, not a building: "Rung by the MAM (Caterium Research)".
	const FString RungBy = FString::Printf(TEXT("the %s (%s)"), AMRules::IsMamCondition(Condition) ? TEXT("MAM") : TEXT("HUB"), *Detail);
	TArray<FString> Events;
	for (const FAMFireRule& Rule : mWorldFires)
	{
		if (Rule.Condition == Condition)
		{
			Events.AddUnique(Rule.Event);
		}
	}
	for (const FString& Event : Events)
	{
		UE_LOG(LogAutomatedMachines, Log, TEXT("World event %s (%s): rings %s"), *AMRules::GetConditionText(Condition).ToString(), *Detail, *Event);
		FireEvent(Event, nullptr, RungBy);
	}
}
