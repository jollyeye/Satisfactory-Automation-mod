// Railway block / path signals: red or green, a train in their block, and our "Blocked" switch.
//
// The game cannot hold a signal red by itself. A blocked signal makes its block look busy to trains
// entering through it (FFGRailroadSignalBlock::HasOverlappingTrains / HasOverlappingApprovedReservations
// answer "yes"), so they wait at it like at any red signal, and the signal shows red (GetAspectFor).
// Trains already in or committed to the block are not stopped. Nothing is saved in the game's own
// data: without the mod, every signal works normally again.
#pragma once

#include "CoreMinimal.h"

class AFGBuildableRailroadSignal;

namespace AMSignal
{
	/** Installs the hooks above (once). */
	void InstallHooks();

	/** Our switch (server): a blocked signal stays red for trains entering through it. Redraws it. */
	void SetBlocked(AFGBuildableRailroadSignal* Signal, bool bBlocked);
	bool IsBlocked(const AFGBuildableRailroadSignal* Signal);

	/** Red (Stop) / green (Clear or Dock); a train in the block it watches. */
	bool IsRed(const AFGBuildableRailroadSignal* Signal);
	bool IsGreen(const AFGBuildableRailroadSignal* Signal);
	bool IsBlockOccupied(AFGBuildableRailroadSignal* Signal);
}
