#include "Automation/AMMarkers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Automation/AMAutomationSubsystem.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWidgetSign.h"
#include "Automation/AMLight.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Components/DecalComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "CollisionQueryParams.h"
#include "UObject/UnrealType.h"
#include "FGOptimizationSettings.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Buildables/FGBuildableTradingPost.h"
#include "Buildables/FGBuildableMAM.h"

namespace
{
	// Made by Tools/create_assets.py.
	const TCHAR* const AMMK_GlowMaterialPath = TEXT("/AutomatedMachines/Marker/M_AM_Underglow.M_AM_Underglow");
	const TCHAR* const AMMK_UpMaterialPath = TEXT("/AutomatedMachines/Marker/M_AM_Overglow.M_AM_Overglow");

	/** The glow projected up reaches this far above the machine's top, in cm (fading out on the way). */
	constexpr float AMMK_UpReach = 300.f;

	/** How far the glow reaches out from the machine's visible edge, in cm. */
	constexpr float AMMK_Margin = 45.f;

	/** How wide the fade from full light to nothing is, in cm, ending at the glow's outer edge. */
	constexpr float AMMK_FadeWidth = 150.f;

	/** The projection box around the floor height: this high above it, so the circuit lines climb a little onto the
	 * machine (the material fades them out between 40 and 110 cm) ... */
	constexpr float AMMK_Above = 130.f;

	/** ... to this far below it, in cm (slopes, uneven ground). */
	constexpr float AMMK_Below = 300.f;

	/**
	 * The floor is looked for from this far above the machine's anchor (or from its top, if lower:
	 * not on a floor above it) to this far below, in cm (a miner's anchor is underground).
	 */
	constexpr float AMMK_FloorSearchUp = 400.f;
	constexpr float AMMK_FloorSearchDown = 300.f;

	/** The floor is probed this far outside the machine's visible edge, in cm. */
	constexpr float AMMK_FloorProbeOut = 20.f;

	/** If the game's distance for medium buildings cannot be read: this far, in cm (times View Distance). */
	constexpr float AMMK_FallbackDrawDistance = 30000.f;

	/** Glows are checked again this often, in seconds (a machine may have been dismantled, the floor loaded). */
	constexpr float AMMK_RefreshInterval = 5.f;
}

AAMMarkers::AAMMarkers()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AAMMarkers::Init(AAMAutomationSubsystem* InSubsystem)
{
	mSubsystem = InSubsystem;
	if (InSubsystem)
	{
		mRulesChangedHandle = InSubsystem->OnRulesChanged.AddUObject(this, &AAMMarkers::OnRulesChanged);
	}
	mGlowMaterial = LoadObject<UMaterialInterface>(nullptr, AMMK_GlowMaterialPath);
	mUpMaterial = LoadObject<UMaterialInterface>(nullptr, AMMK_UpMaterialPath);
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
	const AAMAutomationSubsystem* Subsystem = mSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}
	if (!mGlowMaterial)
	{
		if (!bMaterialMissingLogged)
		{
			UE_LOG(LogAutomatedMachines, Warning, TEXT("Underglow material %s not found: no glow under machines"), AMMK_GlowMaterialPath);
			bMaterialMissingLogged = true;
		}
		return;
	}

	UpdateDrawDistance();

	// The machines that should glow.
	TSet<const AActor*> Wanted;
	for (const FAMMachineRules& Entry : Subsystem->GetAllMachines())
	{
		const AActor* Machine = Entry.Machine;
		// Signs and lights never glow: nothing may change how they look except their own settings.
		if (IsValid(Machine) && !Entry.IsEmpty() && !Machine->IsA<AFGBuildableWidgetSign>() && !AMLight::IsLight(Machine))
		{
			Wanted.Add(Machine);
		}
	}
	AddWorldBuildings(Wanted);

	// Remove the glows of machines gone or no longer automated.
	for (auto It = mGlows.CreateIterator(); It; ++It)
	{
		const AActor* Machine = It->Key.Get();
		UDecalComponent* Glow = It->Value.Decal.Get();
		if (!IsValid(Machine) || !Wanted.Contains(Machine) || !IsValid(Glow))
		{
			if (IsValid(Glow))
			{
				RemoveInstanceComponent(Glow);
				Glow->DestroyComponent();
			}
			if (UDecalComponent* Up = It->Value.Up.Get())
			{
				RemoveInstanceComponent(Up);
				Up->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}

	// Add the new ones; place again the ones whose floor was not found yet.
	for (const AActor* Machine : Wanted)
	{
		if (FGlow* Existing = mGlows.Find(TWeakObjectPtr<const AActor>(Machine)))
		{
			if (!Existing->bSettled)
			{
				if (UDecalComponent* Decal = Existing->Decal.Get())
				{
					Existing->bSettled = PlaceGlow(Decal, Machine, Existing->Up.Get());
				}
			}
			continue;
		}
		bool bSettled = false;
		UDecalComponent* Up = nullptr;
		if (UDecalComponent* Glow = MakeGlow(Machine, bSettled, Up))
		{
			FGlow& Added = mGlows.Add(TWeakObjectPtr<const AActor>(Machine));
			Added.Decal = Glow;
			Added.Up = Up;
			Added.bSettled = bSettled;
		}
	}
}

FBox AAMMarkers::GetVisibleBox(const AActor* Machine)
{
	FBox Box(ForceInit);
	const FTransform& ToWorld = Machine->GetActorTransform();
	// The buildings' static parts: the game draws them as instances, listed in the building's
	// instance data object (its private "Instances" array, read by reflection: copying FInstanceData
	// needs functions of the AbstractInstance module, which would have to be linked).
	const AFGBuildable* Buildable = Cast<AFGBuildable>(Machine);
	const UObject* InstanceData = Buildable ? reinterpret_cast<const UObject*>(Buildable->GetLightweightInstanceData()) : nullptr;
	const FArrayProperty* InstancesProperty = InstanceData ? FindFProperty<FArrayProperty>(InstanceData->GetClass(), TEXT("Instances")) : nullptr;
	const FStructProperty* EntryProperty = InstancesProperty ? CastField<FStructProperty>(InstancesProperty->Inner) : nullptr;
	const FObjectPropertyBase* MeshField = EntryProperty ? FindFProperty<FObjectPropertyBase>(EntryProperty->Struct, TEXT("StaticMesh")) : nullptr;
	const FStructProperty* TransformField = EntryProperty ? FindFProperty<FStructProperty>(EntryProperty->Struct, TEXT("RelativeTransform")) : nullptr;
	if (MeshField && TransformField && TransformField->Struct == TBaseStructure<FTransform>::Get())
	{
		FScriptArrayHelper Instances(InstancesProperty, InstancesProperty->ContainerPtrToValuePtr<void>(InstanceData));
		for (int32 Index = 0; Index < Instances.Num(); ++Index)
		{
			const uint8* Entry = Instances.GetRawPtr(Index);
			const UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshField->GetObjectPropertyValue_InContainer(Entry));
			if (StaticMesh)
			{
				const FTransform& Relative = *TransformField->ContainerPtrToValuePtr<FTransform>(Entry);
				Box += StaticMesh->GetBoundingBox().TransformBy(Relative);
			}
		}
	}
	// Its own visible meshes (moving parts, miners), not the clearance or collision boxes.
	TArray<UMeshComponent*> Meshes;
	Machine->GetComponents<UMeshComponent>(Meshes);
	for (const UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->IsVisible())
		{
			Box += Mesh->CalcBounds(Mesh->GetComponentTransform().GetRelativeTransform(ToWorld)).GetBox();
		}
	}
	if (!Box.IsValid || Box.GetExtent().X < 1.0 || Box.GetExtent().Y < 1.0)
	{
		// Nothing visible found: the components' box (may include the clearance).
		Box = Machine->CalculateComponentsBoundingBoxInLocalSpace(true);
	}
	return Box;
}

void AAMMarkers::AddWorldBuildings(TSet<const AActor*>& Wanted) const
{
	const AAMAutomationSubsystem* Subsystem = mSubsystem.Get();
	UWorld* World = GetWorld();
	if (!Subsystem || !World)
	{
		return;
	}
	bool bHub = Subsystem->GetWorldActions().Num() > 0;
	bool bMam = false;
	for (const FAMFireRule& Rule : Subsystem->GetWorldFires())
	{
		if (AMRules::IsMamCondition(Rule.Condition))
		{
			bMam = true;
		}
		else
		{
			bHub = true;
		}
	}
	if (bHub)
	{
		for (TActorIterator<AFGBuildableTradingPost> It(World); It; ++It)
		{
			Wanted.Add(*It);
		}
	}
	if (bMam)
	{
		for (TActorIterator<AFGBuildableMAM> It(World); It; ++It)
		{
			Wanted.Add(*It);
		}
	}
}

UDecalComponent* AAMMarkers::MakeDecal(UMaterialInterface* Material)
{
	UDecalComponent* Decal = NewObject<UDecalComponent>(this);
	Decal->SetupAttachment(GetRootComponent());
	Decal->SetUsingAbsoluteLocation(true);
	Decal->SetUsingAbsoluteRotation(true);
	Decal->SetUsingAbsoluteScale(true);
	Decal->SetDecalMaterial(UMaterialInstanceDynamic::Create(Material, Decal));
	AddInstanceComponent(Decal);
	Decal->RegisterComponent();
	return Decal;
}

UDecalComponent* AAMMarkers::MakeGlow(const AActor* Machine, bool& bOutSettled, UDecalComponent*& OutUp)
{
	bOutSettled = false;
	OutUp = nullptr;
	const FBox Box = GetVisibleBox(Machine);
	if (!Box.IsValid || Box.GetExtent().X < 1.0 || Box.GetExtent().Y < 1.0)
	{
		return nullptr;
	}
	UDecalComponent* Glow = MakeDecal(mGlowMaterial);
	OutUp = mUpMaterial ? MakeDecal(mUpMaterial) : nullptr;
	bOutSettled = PlaceGlow(Glow, Machine, OutUp);
	ApplyDrawDistance(Glow);
	ApplyDrawDistance(OutUp);
	return Glow;
}

bool AAMMarkers::PlaceGlow(UDecalComponent* Glow, const AActor* Machine, UDecalComponent* Up) const
{
	const FBox Box = GetVisibleBox(Machine);
	if (!Box.IsValid)
	{
		return false;
	}
	const FVector Extent = Box.GetExtent();
	const FVector LocalCenter(Box.GetCenter().X, Box.GetCenter().Y, 0.0);
	const FTransform& ToWorld = Machine->GetActorTransform();
	const double AnchorZ = ToWorld.GetLocation().Z;

	// The floor is the machine's anchor: where it was built, on a foundation or the ground. Only
	// extractors (miners) can have their anchor underground (on the resource node): for them the
	// ground is looked for straight down just outside the four sides (the median of what is hit:
	// one side hanging over a cliff or touching a belt does not count), and it can only lift the
	// glow above the anchor, never lower it (a foundation not loaded yet, a foundation's edge).
	const bool bSearchFloor = Machine->IsA<AFGBuildableResourceExtractor>();
	TArray<double> FloorHits;
	UWorld* World = bSearchFloor ? GetWorld() : nullptr;
	if (World)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AMUnderglowFloor), false, Machine);
		const double SearchTop = AnchorZ + FMath::Min(static_cast<double>(AMMK_FloorSearchUp), FMath::Max(Box.Max.Z, 50.0));
		const FVector Probes[] = {
			FVector(Extent.X + AMMK_FloorProbeOut, 0.0, 0.0), FVector(-Extent.X - AMMK_FloorProbeOut, 0.0, 0.0),
			FVector(0.0, Extent.Y + AMMK_FloorProbeOut, 0.0), FVector(0.0, -Extent.Y - AMMK_FloorProbeOut, 0.0) };
		for (const FVector& Probe : Probes)
		{
			const FVector Point = ToWorld.TransformPosition(LocalCenter + Probe);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, FVector(Point.X, Point.Y, SearchTop),
				FVector(Point.X, Point.Y, AnchorZ - AMMK_FloorSearchDown), ECC_Visibility, Params))
			{
				FloorHits.Add(Hit.ImpactPoint.Z);
			}
		}
	}
	double FloorZ = AnchorZ;
	const bool bFound = !bSearchFloor || FloorHits.Num() >= 2;
	if (bSearchFloor && bFound)
	{
		FloorHits.Sort();
		const int32 Middle = FloorHits.Num() / 2;
		FloorZ = FMath::Max(AnchorZ, FloorHits.Num() % 2 == 0 ? (FloorHits[Middle - 1] + FloorHits[Middle]) * 0.5 : FloorHits[Middle]);
	}

	// A decal projects along its own X: pitched down, its X points at the floor, its Y stays the
	// machine's Y and its Z becomes the machine's X.
	const FQuat Down = ToWorld.GetRotation() * FRotator(-90.f, 0.f, 0.f).Quaternion();
	const float HalfDepth = (AMMK_Above + AMMK_Below) * 0.5f;
	FVector Center = ToWorld.TransformPosition(LocalCenter);
	Center.Z = FloorZ + AMMK_Above - HalfDepth;
	Glow->SetWorldLocationAndRotation(Center, Down);
	const float HalfY = static_cast<float>(Extent.Y) + AMMK_Margin;
	const float HalfZ = static_cast<float>(Extent.X) + AMMK_Margin;
	Glow->DecalSize = FVector(HalfDepth, HalfY, HalfZ);
	Glow->MarkRenderStateDirty();

	// Where the fade starts, as a part of the way from the centre to the edge (U along the
	// machine's X, V along its Y), so it is AMMK_FadeWidth wide on any machine; and the floor height
	// the light belongs to.
	if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Glow->GetDecalMaterial()))
	{
		Material->SetVectorParameterValue(TEXT("FadeStart"), FLinearColor(
			FMath::Max(0.f, 1.f - AMMK_FadeWidth / HalfZ), FMath::Max(0.f, 1.f - AMMK_FadeWidth / HalfY), 0.f, 0.f));
		Material->SetScalarParameterValue(TEXT("BaseZ"), static_cast<float>(FloorZ));
		// The glow's half size in cm (U, V), so the circuit lines keep the same size on every machine.
		Material->SetVectorParameterValue(TEXT("HalfSize"), FLinearColor(HalfZ, HalfY, 0.f, 0.f));
	}

	// The same footprint projected up, from just below the floor to AMMK_UpReach above the machine's
	// top: only surfaces facing down take it (the material), fading out with height.
	if (Up)
	{
		const float Reach = FMath::Max(static_cast<float>(AnchorZ + Box.Max.Z - FloorZ), 100.f) + AMMK_UpReach;
		const float UpHalfDepth = (Reach + 20.f) * 0.5f;
		FVector UpCenter = ToWorld.TransformPosition(LocalCenter);
		UpCenter.Z = FloorZ - 20.f + UpHalfDepth;
		Up->SetWorldLocationAndRotation(UpCenter, ToWorld.GetRotation() * FRotator(90.f, 0.f, 0.f).Quaternion());
		Up->DecalSize = FVector(UpHalfDepth, HalfY, HalfZ);
		Up->MarkRenderStateDirty();
		if (UMaterialInstanceDynamic* UpMaterial = Cast<UMaterialInstanceDynamic>(Up->GetDecalMaterial()))
		{
			UpMaterial->SetVectorParameterValue(TEXT("FadeStart"), FLinearColor(
				FMath::Max(0.f, 1.f - AMMK_FadeWidth / HalfZ), FMath::Max(0.f, 1.f - AMMK_FadeWidth / HalfY), 0.f, 0.f));
			UpMaterial->SetScalarParameterValue(TEXT("BaseZ"), static_cast<float>(FloorZ));
			UpMaterial->SetScalarParameterValue(TEXT("Reach"), Reach);
		}
	}
	return bFound;
}

void AAMMarkers::UpdateDrawDistance()
{
	float GameDistance = 0.f;
	if (const UFGOptimizationSettings* Settings = GetDefault<UFGOptimizationSettings>())
	{
		// The game's cull distances (min, max) for medium buildings; the engine scales them by View Distance.
		GameDistance = static_cast<float>(Settings->GetCullDistanceByCategory(EDistanceCullCategory::FGDCC_BuildingMedium, 1.f).Y);
	}
	const bool bFromGame = GameDistance > 0.f;
	if (!bFromGame)
	{
		GameDistance = AMMK_FallbackDrawDistance;
	}
	float ViewDistanceScale = 1.f;
	if (const IConsoleVariable* Scale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale")))
	{
		ViewDistanceScale = FMath::Max(0.1f, Scale->GetFloat());
	}
	const float Distance = GameDistance * ViewDistanceScale;
	if (FMath::IsNearlyEqual(Distance, mDrawDistance, 1.f))
	{
		return;
	}
	mDrawDistance = Distance;
	UE_LOG(LogAutomatedMachines, Log, TEXT("Underglow drawn up to %.0f m (%s %.0f m x View Distance %.2f)"),
		Distance / 100.f, bFromGame ? TEXT("game's medium buildings") : TEXT("fallback"), GameDistance / 100.f, ViewDistanceScale);
	for (const TPair<TWeakObjectPtr<const AActor>, FGlow>& Entry : mGlows)
	{
		if (UDecalComponent* Glow = Entry.Value.Decal.Get())
		{
			ApplyDrawDistance(Glow);
		}
		ApplyDrawDistance(Entry.Value.Up.Get());
	}
}

void AAMMarkers::ApplyDrawDistance(UDecalComponent* Glow) const
{
	// A decal is drawn while it is at least FadeScreenSize of the screen; its screen size is about
	// its bounds radius / distance (90 degrees field of view), so this size stops it near mDrawDistance.
	if (Glow && mDrawDistance > 0.f)
	{
		const float Radius = static_cast<float>(Glow->DecalSize.Size());
		Glow->SetFadeScreenSize(FMath::Clamp(Radius / mDrawDistance, 0.0001f, 0.5f));
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
