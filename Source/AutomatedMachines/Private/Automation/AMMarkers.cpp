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
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
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
	mStacked.Reset();
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
				FStackedMarker& Stacked = mStacked.AddDefaulted_GetRef();
				Stacked.Machine = Machine;
				Stacked.LocalBox = Machine->CalculateComponentsBoundingBoxInLocalSpace(true);
				Stacked.Instance = mInstances->AddInstance(FTransform(Location), true);
				FVector CameraLocation = Location;
				if (const APlayerController* Player = World->GetFirstPlayerController())
				{
					if (Player->PlayerCameraManager)
					{
						CameraLocation = Player->PlayerCameraManager->GetCameraLocation();
					}
				}
				PlaceStacked(Stacked, CameraLocation, true);
				continue;
			}
		}
		mInstances->AddInstance(FTransform(Location), true);
	}
}

void AAMMarkers::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (mStacked.Num() == 0)
	{
		return;
	}
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Player || !Player->PlayerCameraManager)
	{
		return;
	}
	const FVector CameraLocation = Player->PlayerCameraManager->GetCameraLocation();
	for (FStackedMarker& Marker : mStacked)
	{
		PlaceStacked(Marker, CameraLocation, false);
	}
}

void AAMMarkers::PlaceStacked(FStackedMarker& Marker, const FVector& CameraLocation, bool bForce)
{
	const AActor* Machine = Marker.Machine.Get();
	if (!Machine || Marker.Instance == INDEX_NONE || !Marker.LocalBox.IsValid)
	{
		return;
	}
	// The side (+X, -X, +Y, -Y of the machine) the camera is most in front of.
	const FTransform& ToWorld = Machine->GetActorTransform();
	const FVector Center = Marker.LocalBox.GetCenter();
	const FVector Extent = Marker.LocalBox.GetExtent();
	const FVector ToCamera = ToWorld.InverseTransformPosition(CameraLocation) - Center;
	const double RatioX = Extent.X > 1.0 ? FMath::Abs(ToCamera.X) / Extent.X : 0.0;
	const double RatioY = Extent.Y > 1.0 ? FMath::Abs(ToCamera.Y) / Extent.Y : 0.0;
	const int32 Face = RatioX >= RatioY ? (ToCamera.X >= 0.0 ? 0 : 1) : (ToCamera.Y >= 0.0 ? 2 : 3);
	if (!bForce && Face == Marker.Face)
	{
		return;
	}
	Marker.Face = Face;
	FVector Local = Center;
	switch (Face)
	{
	case 0: Local.X += Extent.X + AMMK_FrontDistance; break;
	case 1: Local.X -= Extent.X + AMMK_FrontDistance; break;
	case 2: Local.Y += Extent.Y + AMMK_FrontDistance; break;
	default: Local.Y -= Extent.Y + AMMK_FrontDistance; break;
	}
	mInstances->UpdateInstanceTransform(Marker.Instance, FTransform(ToWorld.TransformPosition(Local)), true, true);
}

void AAMMarkers::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AAMAutomationSubsystem* Subsystem = mSubsystem.Get())
	{
		Subsystem->OnRulesChanged.Remove(mRulesChangedHandle);
	}
	Super::EndPlay(Reason);
}
