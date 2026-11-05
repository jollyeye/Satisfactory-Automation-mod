#include "Automation/AMClipboard.h"
#include "AutomatedMachinesModule.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Net/AMRemoteCallObject.h"
#include "UI/AMNotification.h"
#include "Buildables/FGBuildable.h"
#include "FGFactoryClipboard.h"
#include "FGPlayerState.h"
#include "FGCharacterPlayer.h"
#include "Equipment/FGBuildGunBuild.h"
#include "GameFramework/PlayerController.h"
#include "Patching/NativeHookManager.h"
#include "UObject/ObjectKey.h"

namespace
{
	/** Per player: per kind of machine. */
	TMap<FObjectKey, TMap<FObjectKey, AMClipboard::FClip>> AMCB_Copies;

	/** Per build gun (server): what its last middle click sampled. */
	struct FAMCBSample
	{
		FObjectKey Kind;
		AMClipboard::FClip Clip;
	};
	TMap<FObjectKey, FAMCBSample> AMCB_Samples;

	/** The build gun constructing right now (server, during InternalConstructHologram). */
	TWeakObjectPtr<UFGBuildGunStateBuild> AMCB_ConstructingGun;

	/** Ctrl+C reaches us twice for machines the game copies too (player state and character): once per frame. */
	uint64 AMCB_LastCopyFrame = 0;
	uint64 AMCB_LastPasteFrame = 0;
	FObjectKey AMCB_LastCopyMachine;
	FObjectKey AMCB_LastPasteMachine;

	/**
	 * The kind of a machine for copying: its own class, so only the same machine (Constructor to
	 * Constructor). The game's mapping class puts all production machines together.
	 */
	UClass* AMCB_Kind(UObject* Machine)
	{
		return Machine ? Machine->GetClass() : nullptr;
	}

	APlayerController* AMCB_Controller(AFGPlayerState* Player)
	{
		return Player ? Player->GetPlayerController() : nullptr;
	}

	FString AMCB_Describe(const AMClipboard::FClip& Clip)
	{
		return FString::Printf(TEXT("%d alarm%s, %d action%s"),
			Clip.Fires.Num(), Clip.Fires.Num() == 1 ? TEXT("") : TEXT("s"),
			Clip.Reactions.Num(), Clip.Reactions.Num() == 1 ? TEXT("") : TEXT("s"));
	}

	/** The machine's automation, if it has any. */
	bool AMCB_Read(AFGBuildable* Machine, AMClipboard::FClip& OutClip)
	{
		OutClip = AMClipboard::FClip();
		const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(Machine);
		const FAMMachineRules* Rules = Subsystem ? Subsystem->FindRules(Machine) : nullptr;
		if (!Rules || Rules->IsEmpty())
		{
			return false;
		}
		OutClip.Fires = Rules->Fires;
		OutClip.Reactions = Rules->Reactions;
		OutClip.Options = Rules->Options;
		return true;
	}
}

bool AMClipboard::GameCanCopy(UObject* Machine)
{
	return Machine && Machine->GetClass()->ImplementsInterface(UFGFactoryClipboardInterface::StaticClass())
		&& IFGFactoryClipboardInterface::Execute_CanUseFactoryClipboard(Machine);
}

void AMClipboard::Copy(AFGPlayerState* Player, UObject* Object)
{
	AFGBuildable* Machine = Cast<AFGBuildable>(Object);
	if (!Player || !Machine || !AAMAutomationSubsystem::CanAutomate(Machine))
	{
		return;
	}
	if (AMCB_LastCopyFrame == GFrameCounter && AMCB_LastCopyMachine == FObjectKey(Machine))
	{
		return;
	}
	AMCB_LastCopyFrame = GFrameCounter;
	AMCB_LastCopyMachine = FObjectKey(Machine);

	TMap<FObjectKey, FClip>& Copies = AMCB_Copies.FindOrAdd(FObjectKey(Player));
	// A machine without automation is copied too: pasting it removes the other machine's automation.
	FClip Clip;
	AMCB_Read(Machine, Clip);
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation copied from %s: %s"), *Machine->GetName(), *AMCB_Describe(Clip));
	UAMNotification::Show(AMCB_Controller(Player), TEXT("AUTOMATION COPIED"), AMCB_Describe(Clip));
	Copies.Add(FObjectKey(AMCB_Kind(Machine)), MoveTemp(Clip));
}

const AMClipboard::FClip* AMClipboard::Find(AFGPlayerState* Player, UObject* Machine)
{
	const TMap<FObjectKey, FClip>* Copies = Player ? AMCB_Copies.Find(FObjectKey(Player)) : nullptr;
	return Copies && Machine ? Copies->Find(FObjectKey(AMCB_Kind(Machine))) : nullptr;
}

bool AMClipboard::Paste(AFGPlayerState* Player, UObject* Object)
{
	AFGBuildable* Machine = Cast<AFGBuildable>(Object);
	const FClip* Clip = Find(Player, Machine);
	if (!Clip || !AAMAutomationSubsystem::CanAutomate(Machine))
	{
		return false;
	}
	if (AMCB_LastPasteFrame == GFrameCounter && AMCB_LastPasteMachine == FObjectKey(Machine))
	{
		return true;
	}
	AMCB_LastPasteFrame = GFrameCounter;
	AMCB_LastPasteMachine = FObjectKey(Machine);

	UAMRemoteCallObject::RequestSetRules(AMCB_Controller(Player), Machine, Clip->Fires, Clip->Reactions, Clip->Options);
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation pasted onto %s: %s"), *Machine->GetName(), *AMCB_Describe(*Clip));
	UAMNotification::Show(AMCB_Controller(Player), TEXT("AUTOMATION PASTED"), AMCB_Describe(*Clip));
	return true;
}

void AMClipboard::OnBuildableConstructed(AAMAutomationSubsystem* Subsystem, AFGBuildable* Buildable)
{
	UFGBuildGunStateBuild* Gun = AMCB_ConstructingGun.Get();
	const FAMCBSample* Sample = Gun ? AMCB_Samples.Find(FObjectKey(Gun)) : nullptr;
	if (!Subsystem || !Buildable || !Sample || Sample->Kind != FObjectKey(AMCB_Kind(Buildable)) || !AAMAutomationSubsystem::CanAutomate(Buildable))
	{
		return;
	}
	Subsystem->SetRules(Buildable, Sample->Clip.Fires, Sample->Clip.Reactions, Sample->Clip.Options);
	UE_LOG(LogAutomatedMachines, Log, TEXT("Built %s with the sampled automation: %s"), *Buildable->GetName(), *AMCB_Describe(Sample->Clip));
}

void AMClipboard::Install()
{
	// Ctrl+C / Ctrl+V while looking at a machine: everything (the game copies its settings, we add
	// the automation). The window's Copy / Paste Settings buttons copy only the game's settings (not
	// hooked), our Copy / Paste automation settings buttons only the automation.
	SUBSCRIBE_METHOD_AFTER(AFGCharacterPlayer::ClipboardCopy, [](AFGCharacterPlayer* Self)
	{
		if (AActor* Target = Self ? Self->GetBestUsableActor() : nullptr)
		{
			Copy(Self->GetPlayerState<AFGPlayerState>(), Target);
		}
	});
	SUBSCRIBE_METHOD_AFTER(AFGCharacterPlayer::ClipboardPaste, [](AFGCharacterPlayer* Self)
	{
		if (AActor* Target = Self ? Self->GetBestUsableActor() : nullptr)
		{
			Paste(Self->GetPlayerState<AFGPlayerState>(), Target);
		}
	});

	// Middle click: remember the sampled machine's automation for this build gun.
	SUBSCRIBE_METHOD_AFTER(UFGBuildGunStateBuild::SampleClipboardSettingsFromActor, [](UFGBuildGunStateBuild* Self, AActor* Actor)
	{
		AFGBuildable* Machine = Cast<AFGBuildable>(Actor);
		FAMCBSample Sample;
		if (!Self || !Machine || !AMCB_Read(Machine, Sample.Clip))
		{
			AMCB_Samples.Remove(FObjectKey(Self));
			return;
		}
		Sample.Kind = FObjectKey(AMCB_Kind(Machine));
		UE_LOG(LogAutomatedMachines, Log, TEXT("Sampled the automation of %s: %s"), *Machine->GetName(), *AMCB_Describe(Sample.Clip));
		AMCB_Samples.Add(FObjectKey(Self), MoveTemp(Sample));
	});

	// Building: the buildings made during this call get the sampled automation (OnBuildableConstructed).
	SUBSCRIBE_METHOD(UFGBuildGunStateBuild::InternalConstructHologram, [](auto& Scope, UFGBuildGunStateBuild* Self, FNetConstructionID ConstructionId)
	{
		AMCB_ConstructingGun = Self;
		Scope(Self, ConstructionId);
		AMCB_ConstructingGun = nullptr;
	});
}
