#include "Net/AMRemoteCallObject.h"
#include "AutomatedMachinesModule.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Patching/NativeHookManager.h"

void UAMRemoteCallObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAMRemoteCallObject, mDummy);
}

void UAMRemoteCallObject::ServerSetRules_Implementation(AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions, const FAMMachineOptions& Options)
{
	if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(Machine))
	{
		Subsystem->SetRules(Machine, Fires, Reactions, Options);
	}
}

void UAMRemoteCallObject::ServerSetWorldActions_Implementation(const TArray<FAMReactionRule>& Actions)
{
	if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(GetOuter()))
	{
		Subsystem->SetWorldActions(Actions);
	}
}

void UAMRemoteCallObject::RequestSetWorldActions(APlayerController* Player, const TArray<FAMReactionRule>& Actions)
{
	if (!Player)
	{
		return;
	}
	if (Player->HasAuthority())
	{
		if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(Player))
		{
			Subsystem->SetWorldActions(Actions);
		}
		return;
	}
	AFGPlayerController* FGPlayer = Cast<AFGPlayerController>(Player);
	UAMRemoteCallObject* RCO = FGPlayer ? Cast<UAMRemoteCallObject>(FGPlayer->GetRemoteCallObjectOfClass(StaticClass())) : nullptr;
	if (RCO)
	{
		RCO->ServerSetWorldActions(Actions);
	}
	else
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Cannot send world actions to the server: no remote call object"));
	}
}

void UAMRemoteCallObject::InstallRegistration()
{
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGPlayerController::BeginPlay, GetMutableDefault<AFGPlayerController>(),
		[](AFGPlayerController* PC)
		{
			if (PC && PC->HasAuthority() && !PC->GetRemoteCallObjectOfClass(UAMRemoteCallObject::StaticClass()))
			{
				PC->RegisterRemoteCallObjectClass(UAMRemoteCallObject::StaticClass());
			}
		});
}

void UAMRemoteCallObject::RequestSetRules(APlayerController* Player, AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions, const FAMMachineOptions& Options)
{
	if (!Machine)
	{
		return;
	}
	if (Machine->HasAuthority())
	{
		if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(Machine))
		{
			Subsystem->SetRules(Machine, Fires, Reactions, Options);
		}
		return;
	}
	AFGPlayerController* FGPlayer = Cast<AFGPlayerController>(Player);
	UAMRemoteCallObject* RCO = FGPlayer ? Cast<UAMRemoteCallObject>(FGPlayer->GetRemoteCallObjectOfClass(StaticClass())) : nullptr;
	if (RCO)
	{
		RCO->ServerSetRules(Machine, Fires, Reactions, Options);
	}
	else
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Cannot send rules to the server: no remote call object"));
	}
}
