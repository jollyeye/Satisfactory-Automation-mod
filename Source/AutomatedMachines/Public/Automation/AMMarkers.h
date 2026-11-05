// The Wi-Fi markers above automated machines: one instanced mesh for all of them.
//
// Cheap on purpose: one actor and one instanced mesh component for the whole world (about one draw
// call however many machines are automated). The mesh is a flat square; its material turns it to the
// camera in the shader, so nothing moves on the CPU. Never saved: rebuilt from the rules on load.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AMMarkers.generated.h"

class AAMAutomationSubsystem;
class UInstancedStaticMeshComponent;

UCLASS(NotPlaceable, Transient)
class AUTOMATEDMACHINES_API AAMMarkers : public AActor
{
	GENERATED_BODY()

public:
	AAMMarkers();

	/** Follows the rules of this subsystem: one marker per machine with rules. */
	void Init(AAMAutomationSubsystem* InSubsystem);

	/** Places the markers again. */
	void Rebuild();

	/**
	 * Rules changed: places the markers now and once more a second later. A machine that was just
	 * built (middle click) has no size yet at the moment it gets its rules, so its marker would sit
	 * inside it.
	 */
	void OnRulesChanged();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> mInstances;

	TWeakObjectPtr<AAMAutomationSubsystem> mSubsystem;
	FDelegateHandle mRulesChangedHandle;
	bool bMeshMissingLogged = false;

	/**
	 * Machines with something on top (stacked): their marker floats in front of the side the local
	 * player looks at, updated while playing (the box is the machine's own, in its local space).
	 */
	struct FStackedMarker
	{
		TWeakObjectPtr<const AActor> Machine;
		int32 Instance = INDEX_NONE;
		FBox LocalBox;
		int32 Face = INDEX_NONE;
	};
	TArray<FStackedMarker> mStacked;

	/** Moves a stacked machine's marker to the face towards the camera (if that face changed). */
	void PlaceStacked(FStackedMarker& Marker, const FVector& CameraLocation, bool bForce);
	FTimerHandle mLaterRebuild;
};
