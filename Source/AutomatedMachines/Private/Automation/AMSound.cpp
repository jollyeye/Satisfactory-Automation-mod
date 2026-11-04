#include "Automation/AMSound.h"
#include "Automation/AMTypes.h"
#include "AutomatedMachinesModule.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

void AMSound::PlayForPlayer(APlayerController* Player, int32 Index)
{
	const TArray<AMRules::FSound>& Sounds = AMRules::GetSounds();
	if (!Player || !Sounds.IsValidIndex(Index))
	{
		return;
	}
	UObject* Event = LoadObject<UObject>(nullptr, Sounds[Index].EventPath);
	UFunction* Post = Event ? Event->FindFunction(TEXT("PostOnActor")) : nullptr;
	if (!Post)
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Sound %s cannot be played (%s)"), Sounds[Index].Label, Event ? TEXT("no PostOnActor") : TEXT("event not found"));
		return;
	}
	AActor* Listener = Player->GetPawn() ? static_cast<AActor*>(Player->GetPawn()) : static_cast<AActor*>(Player);

	// PostOnActor(AActor* Actor, FOnAkPostEventCallback delegate, int32 CallbackMask, bool bStopWhenAttachedObjectDestroyed)
	TArray<uint8> Params;
	Params.SetNumZeroed(FMath::Max<int32>(Post->ParmsSize, 1));
	for (TFieldIterator<FProperty> It(Post); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params.GetData());
		if (const FObjectPropertyBase* ActorParam = CastField<FObjectPropertyBase>(*It))
		{
			if (ActorParam->PropertyClass && Listener->IsA(ActorParam->PropertyClass))
			{
				ActorParam->SetObjectPropertyValue_InContainer(Params.GetData(), Listener);
			}
		}
	}
	Event->ProcessEvent(Post, Params.GetData());
	for (TFieldIterator<FProperty> It(Post); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params.GetData());
	}
}

void AMSound::PlayForLocalPlayers(UWorld* World, int32 Index)
{
	if (!World || IsRunningDedicatedServer())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (Player && Player->IsLocalController())
		{
			PlayForPlayer(Player, Index);
		}
	}
}
