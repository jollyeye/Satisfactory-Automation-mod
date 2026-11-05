#include "Automation/AMMarkers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Automation/AMAutomationSubsystem.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "CollisionQueryParams.h"

namespace
{
	// Made by Tools/create_assets.py.
	const TCHAR* const AMMK_MeshPath = TEXT("/AutomatedMachines/Marker/SM_AM_Marker.SM_AM_Marker");
	const TCHAR* const AMMK_MaterialPath = TEXT("/AutomatedMachines/Marker/M_AM_Marker.M_AM_Marker");

	/** Height of the marker's centre above the top of the machine, in cm. */
	constexpr float AMMK_HeightAboveMachine = 150.f;

	/** Something closer than this above the machine (a stacked container) moves the marker to its front, in cm. */
	constexpr float AMMK_StackCheckHeight = 120.f;

	/** How far in front of the machine's front face the marker floats then, in cm. */
	constexpr float AMMK_FrontDistance = 60.f;

	/** Markers are placed again this often, in seconds (something may have been stacked on a machine). */
	constexpr float AMMK_RefreshInterval = 5.f;

	/** Markers further away than this are not drawn, in cm. */
	constexpr int32 AMMK_CullDistance = 30000;
}

AAMMarkers::AAMMarkers()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	mInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Markers"));
	mInstances->SetupAttachment(Root);
	mInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mInstances->SetGenerateOverlapEvents(false);
	mInstances->SetCastShadow(false);
	mInstances->SetCanEverAffectNavigation(false);
	mInstances->SetCullDistances(0, AMMK_CullDistance);
	// The material turns the square to the camera, so it reaches outside the mesh's own bounds.
	mInstances->BoundsScale = 3.f;
}

void AAMMarkers::Init(AAMAutomationSubsystem* InSubsystem)
{
	mSubsystem = InSubsystem;
	if (InSubsystem)
	{
		mRulesChangedHandle = InSubsystem->OnRulesChanged.AddUObject(this, &AAMMarkers::OnRulesChanged);
	}

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, AMMK_MeshPath);
	if (Mesh)
	{
		mInstances->SetStaticMesh(Mesh);
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, AMMK_MaterialPath))
		{
			mInstances->SetMaterial(0, Material);
		}
	}
	Rebuild();
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Refresh;
		World->GetTimerManager().SetTimer(Refresh, this, &AAMMarkers::Rebuild, AMMK_RefreshInterval, true);
	}
}

void AAMMarkers::OnRulesChanged()
{
	Rebuild();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(mLaterRebuild, this, &AAMMarkers::Rebuild, 1.f, false);
	}
}

void AAMMarkers::Rebuild()
{
	mInstances->ClearInstances();
	const AAMAutomationSubsystem* Subsystem = mSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}
	if (!mInstances->GetStaticMesh())
	{
		if (!bMeshMissingLogged)
		{
			UE_LOG(LogAutomatedMachines, Warning, TEXT("Marker mesh %s not found: no markers"), AMMK_MeshPath);
			bMeshMissingLogged = true;
		}
		return;
	}

	for (const FAMMachineRules& Entry : Subsystem->GetAllMachines())
	{
		const AActor* Machine = Entry.Machine;
		if (!IsValid(Machine) || Entry.IsEmpty())
		{
			continue;
		}
		FVector Origin;
		FVector Extent;
		Machine->GetActorBounds(true, Origin, Extent);
		const float Top = static_cast<float>(Origin.Z + Extent.Z);
		FVector Location(Origin.X, Origin.Y, Top + AMMK_HeightAboveMachine);

		// Something stacked on it (containers, signs on a wall): in front of the machine instead,
		// halfway up, so each machine of a stack shows its own marker.
		if (UWorld* World = GetWorld())
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(AMMarkerStack), false, Machine);
			FHitResult Hit;
			const FVector Start(Origin.X, Origin.Y, Top + 5.f);
			const FVector End(Origin.X, Origin.Y, Top + AMMK_StackCheckHeight);
			if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetActor() != Machine)
			{
				const FVector Forward = Machine->GetActorForwardVector().GetSafeNormal2D();
				const float Depth = static_cast<float>(FMath::Abs(Forward.X) * Extent.X + FMath::Abs(Forward.Y) * Extent.Y);
				Location = FVector(Origin.X, Origin.Y, Origin.Z) + Forward * (Depth + AMMK_FrontDistance);
			}
		}
		mInstances->AddInstance(FTransform(Location), true);
	}
}

void AAMMarkers::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AAMAutomationSubsystem* Subsystem = mSubsystem.Get())
	{
		Subsystem->OnRulesChanged.Remove(mRulesChangedHandle);
	}
	Super::EndPlay(Reason);
}
