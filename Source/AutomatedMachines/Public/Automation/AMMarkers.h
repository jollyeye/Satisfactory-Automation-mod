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
	UDecalComponent* MakeGlow(const AActor* Machine, bool& bOutSettled, UDecalComponent*& OutUp);

	/** Makes one decal with this material (registered, on this actor). */
	UDecalComponent* MakeDecal(UMaterialInterface* Material);

	/** The HUB and the MAMs glow while they have alarms (their Automation tabs: world alarms, the HUB's world actions). */
	void AddWorldBuildings(TSet<const AActor*>& Wanted) const;

	/**
	 * Puts a machine's decal under its visible footprint, at the floor found around it. Returns
	 * false if the floor was not found (not loaded yet): the anchor is used meanwhile, and it is
	 * placed again on the next rebuild.
	 */
	bool PlaceGlow(UDecalComponent* Glow, const AActor* Machine, UDecalComponent* Up) const;

	/**
	 * How far the glows are drawn: the game's own distance for medium buildings (Constructors,
	 * Assemblers: its optimization settings) times the View Distance video setting, read again
	 * on every rebuild. Each decal then gets the fade screen size that matches that distance.
	 */
	void UpdateDrawDistance();
	float mDrawDistance = 0.f;
	void ApplyDrawDistance(UDecalComponent* Glow) const;

	/** The machine's visible footprint in its local space (its meshes, not its clearance box). */
	static FBox GetVisibleBox(const AActor* Machine);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> mGlowMaterial;

	/** The glow projected up from the floor, on surfaces facing down (M_AM_Overglow; optional). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> mUpMaterial;

	/** The glow of each automated machine (the decals are kept alive as this actor's instance components). */
	struct FGlow
	{
		TWeakObjectPtr<UDecalComponent> Decal;
		TWeakObjectPtr<UDecalComponent> Up;
		/** False while the floor around it was not found: placed again on the next rebuild. */
		bool bSettled = false;
	};
	TMap<TWeakObjectPtr<const AActor>, FGlow> mGlows;

	TWeakObjectPtr<AAMAutomationSubsystem> mSubsystem;
	FDelegateHandle mRulesChangedHandle;
	bool bMaterialMissingLogged = false;
	FTimerHandle mLaterRebuild;
};
