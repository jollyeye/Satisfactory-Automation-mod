#include "Automation/AMMarkers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Automation/AMAutomationSubsystem.h"
#include "AutomatedMachinesModule.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWidgetSign.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// Made by Tools/create_assets.py.
	const TCHAR* const AMMK_GlowMaterialPath = TEXT("/AutomatedMachines/Marker/M_AM_Underglow.M_AM_Underglow");

	/** How far the glow reaches out from the machine's edge, in cm. */
	constexpr float AMMK_Margin = 120.f;

	/** The projection box: from this high above the machine's base ... */
	constexpr float AMMK_Above = 40.f;

	/** ... to this far below it, in cm (the floor may be a little lower than the machine's base). */
	constexpr float AMMK_Below = 160.f;

	/** Glows smaller than this part of the screen are not drawn (far away). */
	constexpr float AMMK_FadeScreenSize = 0.004f;

	/** Glows are checked again this often, in seconds (a machine may have been dismantled). */
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

	// The machines that should glow.
	TSet<const AActor*> Wanted;
	for (const FAMMachineRules& Entry : Subsystem->GetAllMachines())
	{
		const AActor* Machine = Entry.Machine;
		// Signs never glow: nothing may change how a sign looks except its own designs.
		if (IsValid(Machine) && !Entry.IsEmpty() && !Machine->IsA<AFGBuildableWidgetSign>())
		{
			Wanted.Add(Machine);
		}
	}

	// Remove the glows of machines gone or no longer automated.
	for (auto It = mGlows.CreateIterator(); It; ++It)
	{
		const AActor* Machine = It->Key.Get();
		UDecalComponent* Glow = It->Value.Get();
		if (!IsValid(Machine) || !Wanted.Contains(Machine) || !IsValid(Glow))
		{
			if (IsValid(Glow))
			{
				RemoveInstanceComponent(Glow);
				Glow->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}

	// Add the new ones.
	for (const AActor* Machine : Wanted)
	{
		if (!mGlows.Contains(TWeakObjectPtr<const AActor>(Machine)))
		{
			if (UDecalComponent* Glow = MakeGlow(Machine))
			{
				mGlows.Add(TWeakObjectPtr<const AActor>(Machine), Glow);
			}
		}
	}
}

UDecalComponent* AAMMarkers::MakeGlow(const AActor* Machine)
{
	// The machine's own box, in its local space (X forward, Y right, Z up).
	const FBox LocalBox = Machine->CalculateComponentsBoundingBoxInLocalSpace(true);
	if (!LocalBox.IsValid || LocalBox.GetExtent().X < 1.0 || LocalBox.GetExtent().Y < 1.0)
	{
		return nullptr;
	}
	const FVector Extent = LocalBox.GetExtent();
	const FTransform& ToWorld = Machine->GetActorTransform();
	const FVector LocalBase(LocalBox.GetCenter().X, LocalBox.GetCenter().Y, LocalBox.Min.Z);

	UDecalComponent* Glow = NewObject<UDecalComponent>(this);
	Glow->SetupAttachment(GetRootComponent());
	Glow->SetUsingAbsoluteLocation(true);
	Glow->SetUsingAbsoluteRotation(true);
	Glow->SetUsingAbsoluteScale(true);

	// A decal projects along its own X: pitched down, its X points at the floor, its Y stays the
	// machine's Y and its Z becomes the machine's X.
	const FQuat Down = ToWorld.GetRotation() * FRotator(-90.f, 0.f, 0.f).Quaternion();
	const float HalfDepth = (AMMK_Above + AMMK_Below) * 0.5f;
	const FVector Center = ToWorld.TransformPosition(LocalBase + FVector(0.f, 0.f, AMMK_Above - HalfDepth));
	Glow->SetWorldLocationAndRotation(Center, Down);
	const float HalfY = static_cast<float>(Extent.Y) + AMMK_Margin;
	const float HalfZ = static_cast<float>(Extent.X) + AMMK_Margin;
	Glow->DecalSize = FVector(HalfDepth, HalfY, HalfZ);
	Glow->SetFadeScreenSize(AMMK_FadeScreenSize);

	// The machine's footprint inside the decal, as a part of it (U along the machine's X, V along its Y).
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(mGlowMaterial, Glow);
	Material->SetVectorParameterValue(TEXT("Inner"), FLinearColor(
		static_cast<float>(Extent.X) / HalfZ, static_cast<float>(Extent.Y) / HalfY, 0.f, 0.f));
	Glow->SetDecalMaterial(Material);
	AddInstanceComponent(Glow);
	Glow->RegisterComponent();
	return Glow;
}

void AAMMarkers::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AAMAutomationSubsystem* Subsystem = mSubsystem.Get())
	{
		Subsystem->OnRulesChanged.Remove(mRulesChangedHandle);
	}
	Super::EndPlay(Reason);
}
