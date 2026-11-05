// The glow under automated machines, like a tuned car's underglow: light projected onto the floor
// around the machine's base (a decal, so it lies on whatever is below: foundations, ground, the
// machine underneath in a stack). Cyan like the build hologram, emissive so it blooms.
//
//            ______________
//           |   machine    |
//      ~~~~~|______________|~~~~~      <- brightest at the machine's edge,
//    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~       fading out over AMMK_Margin
//
// One decal component per machine, all on one local actor (never replicated, never saved: made
// again from the rules on load). Signs get none: nothing may change how a sign looks.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AMMarkers.generated.h"

class AAMAutomationSubsystem;
class UDecalComponent;
class UMaterialInterface;

UCLASS(NotPlaceable, Transient)
class AUTOMATEDMACHINES_API AAMMarkers : public AActor
{
	GENERATED_BODY()

public:
	AAMMarkers();

	/** Follows the rules of this subsystem: a glow under every machine with rules. */
	void Init(AAMAutomationSubsystem* InSubsystem);

	/** Adds the glows of new automated machines, removes the ones no longer automated or gone. */
	void Rebuild();

	/**
	 * Rules changed: now and once more a second later. A machine that was just built (middle click)
	 * has no size yet at the moment it gets its rules.
	 */
	void OnRulesChanged();

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Makes the decal of one machine (null if it has no size yet). */
	UDecalComponent* MakeGlow(const AActor* Machine);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> mGlowMaterial;

	/** The glow of each automated machine (the decals are kept alive as this actor's instance components). */
	TMap<TWeakObjectPtr<const AActor>, TWeakObjectPtr<UDecalComponent>> mGlows;

	TWeakObjectPtr<AAMAutomationSubsystem> mSubsystem;
	FDelegateHandle mRulesChangedHandle;
	bool bMaterialMissingLogged = false;
	FTimerHandle mLaterRebuild;
};
