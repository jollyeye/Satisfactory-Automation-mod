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

void UAMRemoteCallObject::ServerSetRules_Implementation(AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions)
{
	if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(Machine))
	{
		Subsystem->SetRules(Machine, Fires, Reactions);
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

void UAMRemoteCallObject::RequestSetRules(APlayerController* Player, AFGBuildable* Machine, const TArray<FAMFireRule>& Fires, const TArray<FAMReactionRule>& Reactions)
{
	if (!Machine)
	{
		return;
	}
	if (Machine->HasAuthority())
	{
		if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(Machine))
		{
			Subsystem->SetRules(Machine, Fires, Reactions);
		}
		return;
	}
	AFGPlayerController* FGPlayer = Cast<AFGPlayerController>(Player);
	UAMRemoteCallObject* RCO = FGPlayer ? Cast<UAMRemoteCallObject>(FGPlayer->GetRemoteCallObjectOfClass(StaticClass())) : nullptr;
	if (RCO)
	{
		RCO->ServerSetRules(Machine, Fires, Reactions);
	}
	else
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Cannot send rules to the server: no remote call object"));
	}
}
