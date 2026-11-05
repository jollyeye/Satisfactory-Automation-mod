// Locating a machine from the HUB's Alarms Overview, like the resource scanner shows ore nodes, but
// without a scan (we know where the machine is):
//   - the scanner's ping sound;
//   - the machine's icon on the compass and on the map (a local actor representation: only the
//     player who asked sees it), with its name;
//   - an expanding ring effect on the machine, again every few seconds;
//   - all of it until the player gets close to the machine (or after a while, or when asked again).
// Local only: never replicated, never saved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGActorRepresentationInterface.h"
#include "AMLocator.generated.h"

class UTexture2D;
class UFGActorRepresentation;

/** The point on the compass and map: a local actor at the machine, implementing the game's representation interface. */
UCLASS(NotPlaceable, Transient)
class AUTOMATEDMACHINES_API AAMLocateMarker : public AActor, public IFGActorRepresentationInterface
{
	GENERATED_BODY()

public:
	AAMLocateMarker();

	void Setup(UTexture2D* InTexture, const FText& InName);

	// Begin IFGActorRepresentationInterface
	UFUNCTION() virtual bool AddAsRepresentation() override;
	UFUNCTION() virtual bool UpdateRepresentation() override;
	UFUNCTION() virtual bool RemoveAsRepresentation() override;
	UFUNCTION() virtual bool IsActorStatic() override { return true; }
	UFUNCTION() virtual FVector GetRealActorLocation() override { return GetActorLocation(); }
	UFUNCTION() virtual FRotator GetRealActorRotation() override { return FRotator::ZeroRotator; }
	UFUNCTION() virtual UTexture2D* GetActorRepresentationTexture() override { return mTexture; }
	UFUNCTION() virtual UMaterialInterface* GetActorRepresentationCompassMaterial() override { return nullptr; }
	UFUNCTION() virtual FText GetActorRepresentationText() override { return mName; }
	UFUNCTION() virtual void SetActorRepresentationText(const FText& NewText) override { mName = NewText; }
	UFUNCTION() virtual FLinearColor GetActorRepresentationColor() override;
	UFUNCTION() virtual void SetActorRepresentationColor(FLinearColor NewColor) override {}
	UFUNCTION() virtual ERepresentationType GetActorRepresentationType() override { return ERepresentationType::RT_Resource; }
	UFUNCTION() virtual bool GetActorShouldShowInCompass() override { return true; }
	UFUNCTION() virtual bool GetActorShouldShowOnMap() override { return true; }
	UFUNCTION() virtual EFogOfWarRevealType GetActorFogOfWarRevealType() override { return EFogOfWarRevealType::FOWRT_None; }
	UFUNCTION() virtual float GetActorFogOfWarRevealRadius() override { return 0.f; }
	UFUNCTION() virtual ECompassViewDistance GetActorCompassViewDistance() override { return ECompassViewDistance::CVD_Always; }
	UFUNCTION() virtual void SetActorCompassViewDistance(ECompassViewDistance CompassViewDistance) override {}
	// End IFGActorRepresentationInterface

private:
	UPROPERTY()
	TObjectPtr<UTexture2D> mTexture;

	FText mName;

	UPROPERTY()
	TObjectPtr<UFGActorRepresentation> mRepresentation;
};

UCLASS(NotPlaceable, Transient)
class AUTOMATEDMACHINES_API AAMLocator : public AActor
{
	GENERATED_BODY()

public:
	AAMLocator();

	/** The locator of this world (made the first time). */
	static AAMLocator* Get(UWorld* World);

	/** Starts locating Target for the local player, or stops if it is being located. Returns true if it started. */
	bool Toggle(AActor* Target, UTexture2D* Icon, const FString& Name);

	bool IsLocating(const AActor* Target) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FLocated
	{
		TWeakObjectPtr<AActor> Target;
		TWeakObjectPtr<AAMLocateMarker> Marker;
		FString Name;
		double Start = 0.0;
		double NextPulse = 0.0;
		float Radius = 0.f;
	};
	TArray<FLocated> mLocated;
	FTimerHandle mTimer;

	/** Four times a second: arrived, gone or too long: stop; else the ring effect again when due. */
	void Check();

	void Stop(int32 Index, const TCHAR* Why);

	/** The expanding ring at the target (the resource scanner's effect, found in the asset registry once). */
	void Pulse(const AActor* Target, float Radius);
};
