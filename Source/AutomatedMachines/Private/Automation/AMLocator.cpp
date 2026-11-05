#include "Automation/AMLocator.h"
#include "AutomatedMachinesModule.h"
#include "Automation/AMSound.h"
#include "FGActorRepresentationManager.h"
#include "FGActorRepresentation.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UnrealType.h"

namespace
{
	/** The cyan of the underglow and the build hologram. */
	const FLinearColor AMLC_Colour(0.12f, 0.7f, 1.f, 1.f);

	/** Closer than this to the machine (plus its size), in cm: arrived, the markers go. */
	constexpr float AMLC_ArriveDistance = 1500.f;

	/** At the latest after this long, in seconds. */
	constexpr double AMLC_MaxSeconds = 15.0 * 60.0;

	/** The ring effect again every this many seconds. */
	constexpr double AMLC_PulseSeconds = 4.0;

	/** The Ping sound of AMRules::GetSounds (the resource scanner's ping). */
	constexpr int32 AMLC_PingSound = 1;

	TWeakObjectPtr<AAMLocator> AMLC_Instance;

	/** The ring effect asset: looked for once (Niagara or Cascade, the resource scanner's). */
	bool bAMLC_EffectSearched = false;
	TWeakObjectPtr<UObject> AMLC_Effect;

	bool AMLC_IsScannerName(const FString& Path)
	{
		return Path.Contains(TEXT("Scan"), ESearchCase::IgnoreCase) || Path.Contains(TEXT("Ping"), ESearchCase::IgnoreCase);
	}

	/** Finds the effect, logging every candidate ([EffectDump] lines: to pick the exact asset later). */
	UObject* AMLC_FindEffect()
	{
		if (bAMLC_EffectSearched)
		{
			return AMLC_Effect.Get();
		}
		bAMLC_EffectSearched = true;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FString Best;
		int32 BestScore = -1;
		const TPair<const TCHAR*, const TCHAR*> Classes[] = {
			{ TEXT("/Script/Niagara"), TEXT("NiagaraSystem") }, { TEXT("/Script/Engine"), TEXT("ParticleSystem") } };
		for (const TPair<const TCHAR*, const TCHAR*>& Class : Classes)
		{
			const TCHAR* ClassPath = Class.Value;
			TArray<FAssetData> Assets;
			Registry.GetAssetsByClass(FTopLevelAssetPath(FName(Class.Key), FName(Class.Value)), Assets, true);
			for (const FAssetData& Asset : Assets)
			{
				const FString Path = Asset.GetObjectPathString();
				if (!AMLC_IsScannerName(Path))
				{
					continue;
				}
				UE_LOG(LogAutomatedMachines, Log, TEXT("[EffectDump] %s"), *Path);
				// The resource scanner's own effects first, then those about a node / ping / pulse.
				int32 Score = 0;
				Score += Path.Contains(TEXT("ResourceScanner"), ESearchCase::IgnoreCase) ? 4 : 0;
				Score += Path.Contains(TEXT("Scanner"), ESearchCase::IgnoreCase) ? 2 : 0;
				Score += (Path.Contains(TEXT("Node"), ESearchCase::IgnoreCase) || Path.Contains(TEXT("Ping"), ESearchCase::IgnoreCase)
					|| Path.Contains(TEXT("Pulse"), ESearchCase::IgnoreCase)) ? 1 : 0;
				Score += FCString::Strstr(ClassPath, TEXT("Niagara")) ? 1 : 0;
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = Path;
				}
			}
		}
		if (!Best.IsEmpty())
		{
			AMLC_Effect = LoadObject<UObject>(nullptr, *Best);
		}
		UE_LOG(LogAutomatedMachines, Log, TEXT("Locate effect: %s"), AMLC_Effect.IsValid() ? *Best : TEXT("none found"));
		return AMLC_Effect.Get();
	}

	/** UNiagaraFunctionLibrary::SpawnSystemAtLocation, called by name (no build dependency on Niagara). */
	void AMLC_SpawnNiagara(UWorld* World, UObject* System, const FVector& Location)
	{
		UClass* Library = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraFunctionLibrary"));
		UFunction* Function = Library ? Library->FindFunctionByName(TEXT("SpawnSystemAtLocation")) : nullptr;
		if (!Function)
		{
			return;
		}
		TArray<uint8> Params;
		Params.SetNumZeroed(Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params.GetData());
			const FString Name = It->GetName();
			if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(*It))
			{
				if (Name == TEXT("WorldContextObject"))
				{
					Object->SetObjectPropertyValue_InContainer(Params.GetData(), World);
				}
				else if (Name == TEXT("SystemTemplate"))
				{
					Object->SetObjectPropertyValue_InContainer(Params.GetData(), System);
				}
			}
			else if (FStructProperty* Struct = CastField<FStructProperty>(*It))
			{
				if (Name == TEXT("Location") && Struct->Struct == TBaseStructure<FVector>::Get())
				{
					*Struct->ContainerPtrToValuePtr<FVector>(Params.GetData()) = Location;
				}
				else if (Name == TEXT("Scale") && Struct->Struct == TBaseStructure<FVector>::Get())
				{
					*Struct->ContainerPtrToValuePtr<FVector>(Params.GetData()) = FVector::OneVector;
				}
			}
			else if (FBoolProperty* Bool = CastField<FBoolProperty>(*It))
			{
				Bool->SetPropertyValue_InContainer(Params.GetData(), Name == TEXT("bAutoDestroy") || Name == TEXT("bAutoActivate"));
			}
		}
		Library->GetDefaultObject()->ProcessEvent(Function, Params.GetData());
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params.GetData());
		}
	}
}

// ----- Marker -------------------------------------------------------------------------------------

AAMLocateMarker::AAMLocateMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AAMLocateMarker::Setup(UTexture2D* InTexture, const FText& InName)
{
	mTexture = InTexture;
	mName = InName;
}

FLinearColor AAMLocateMarker::GetActorRepresentationColor()
{
	return AMLC_Colour;
}

bool AAMLocateMarker::AddAsRepresentation()
{
	if (AFGActorRepresentationManager* Manager = AFGActorRepresentationManager::Get(GetWorld()))
	{
		// Local: only this player's compass and map.
		mRepresentation = Manager->CreateAndAddNewRepresentation(this, true);
	}
	return mRepresentation != nullptr;
}

bool AAMLocateMarker::UpdateRepresentation()
{
	AFGActorRepresentationManager* Manager = AFGActorRepresentationManager::Get(GetWorld());
	return Manager && mRepresentation && Manager->UpdateRepresentation(mRepresentation);
}

bool AAMLocateMarker::RemoveAsRepresentation()
{
	AFGActorRepresentationManager* Manager = AFGActorRepresentationManager::Get(GetWorld());
	if (Manager && mRepresentation)
	{
		Manager->RemoveRepresentation(mRepresentation);
		mRepresentation = nullptr;
		return true;
	}
	return false;
}

// ----- Locator ------------------------------------------------------------------------------------

AAMLocator::AAMLocator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AAMLocator* AAMLocator::Get(UWorld* World)
{
	if (AAMLocator* Existing = AMLC_Instance.Get())
	{
		if (Existing->GetWorld() == World)
		{
			return Existing;
		}
	}
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAMLocator* Locator = World->SpawnActor<AAMLocator>(Params);
	AMLC_Instance = Locator;
	return Locator;
}

bool AAMLocator::IsLocating(const AActor* Target) const
{
	return mLocated.ContainsByPredicate([Target](const FLocated& Entry) { return Entry.Target.Get() == Target; });
}

bool AAMLocator::Toggle(AActor* Target, UTexture2D* Icon, const FString& Name)
{
	UWorld* World = GetWorld();
	if (!Target || !World)
	{
		return false;
	}
	const int32 Existing = mLocated.IndexOfByPredicate([Target](const FLocated& Entry) { return Entry.Target.Get() == Target; });
	if (Existing != INDEX_NONE)
	{
		Stop(Existing, TEXT("asked again"));
		return false;
	}
	FVector Origin;
	FVector Extent;
	Target->GetActorBounds(true, Origin, Extent);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAMLocateMarker* Marker = World->SpawnActor<AAMLocateMarker>(Origin, FRotator::ZeroRotator, Params);
	if (Marker)
	{
		Marker->Setup(Icon, FText::FromString(Name));
		if (!Marker->AddAsRepresentation())
		{
			UE_LOG(LogAutomatedMachines, Warning, TEXT("Locate: no compass / map representation for %s"), *Name);
		}
	}
	FLocated& Added = mLocated.AddDefaulted_GetRef();
	Added.Target = Target;
	Added.Marker = Marker;
	Added.Name = Name;
	Added.Start = FPlatformTime::Seconds();
	Added.NextPulse = Added.Start;
	Added.Radius = static_cast<float>(Extent.Size2D());

	if (APlayerController* Player = World->GetFirstPlayerController())
	{
		AMSound::PlayForPlayer(Player, AMLC_PingSound);
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Locate: %s at %s"), *Name, *Origin.ToCompactString());
	Check();
	if (!World->GetTimerManager().IsTimerActive(mTimer))
	{
		World->GetTimerManager().SetTimer(mTimer, this, &AAMLocator::Check, 0.25f, true);
	}
	return true;
}

void AAMLocator::Check()
{
	UWorld* World = GetWorld();
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const double Now = FPlatformTime::Seconds();
	for (int32 Index = mLocated.Num() - 1; Index >= 0; --Index)
	{
		FLocated& Entry = mLocated[Index];
		const AActor* Target = Entry.Target.Get();
		if (!Target)
		{
			Stop(Index, TEXT("gone"));
			continue;
		}
		if (Pawn && FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) < AMLC_ArriveDistance + Entry.Radius)
		{
			Stop(Index, TEXT("arrived"));
			continue;
		}
		if (Now - Entry.Start > AMLC_MaxSeconds)
		{
			Stop(Index, TEXT("time"));
			continue;
		}
		if (Now >= Entry.NextPulse)
		{
			Entry.NextPulse = Now + AMLC_PulseSeconds;
			Pulse(Target, Entry.Radius);
		}
	}
	if (mLocated.Num() == 0 && World)
	{
		World->GetTimerManager().ClearTimer(mTimer);
	}
}

void AAMLocator::Stop(int32 Index, const TCHAR* Why)
{
	if (!mLocated.IsValidIndex(Index))
	{
		return;
	}
	if (AAMLocateMarker* Marker = mLocated[Index].Marker.Get())
	{
		Marker->RemoveAsRepresentation();
		Marker->Destroy();
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Locate: %s done (%s)"), *mLocated[Index].Name, Why);
	mLocated.RemoveAt(Index);
}

void AAMLocator::Pulse(const AActor* Target, float Radius)
{
	UWorld* World = GetWorld();
	UObject* Effect = AMLC_FindEffect();
	if (!World || !Target || !Effect)
	{
		return;
	}
	FVector Origin;
	FVector Extent;
	Target->GetActorBounds(true, Origin, Extent);
	if (UParticleSystem* Cascade = Cast<UParticleSystem>(Effect))
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, Cascade, Origin, FRotator::ZeroRotator, FVector::OneVector, true);
	}
	else
	{
		AMLC_SpawnNiagara(World, Effect, Origin);
	}
}

void AAMLocator::EndPlay(const EEndPlayReason::Type Reason)
{
	for (int32 Index = mLocated.Num() - 1; Index >= 0; --Index)
	{
		Stop(Index, TEXT("world ends"));
	}
	Super::EndPlay(Reason);
}
