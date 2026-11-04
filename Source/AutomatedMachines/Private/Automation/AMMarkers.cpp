#include "Automation/AMMarkers.h"
#include "Automation/AMAutomationSubsystem.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// Made by Tools/create_assets.py.
	const TCHAR* const AMMK_MeshPath = TEXT("/AutomatedMachines/Marker/SM_AM_Marker.SM_AM_Marker");
	const TCHAR* const AMMK_MaterialPath = TEXT("/AutomatedMachines/Marker/M_AM_Marker.M_AM_Marker");

	/** Height of the marker's centre above the top of the machine, in cm. */
	constexpr float AMMK_HeightAboveMachine = 150.f;

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
		mRulesChangedHandle = InSubsystem->OnRulesChanged.AddUObject(this, &AAMMarkers::Rebuild);
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
		const FVector Location(Origin.X, Origin.Y, Origin.Z + Extent.Z + AMMK_HeightAboveMachine);
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
