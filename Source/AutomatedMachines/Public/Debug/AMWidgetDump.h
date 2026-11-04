// Development tool: writes the widget tree of newly opened game windows to the log.
//
// The editor only has placeholder versions of the game's UI, so we cannot see in the editor how
// the Smelter / Constructor window is built. In game, this lists every widget (name, class, slot
// type) of each new top-level window, so we can pick the panel to insert the Automation section into.
// Remove before release.
#pragma once

#include "CoreMinimal.h"

namespace AMWidgetDump
{
	/** Starts a ticker that checks for new top-level widgets twice a second. */
	void Install();

	/**
	 * Once, when the first game world starts: writes the game's Wwise sound events whose names look
	 * like UI / alert sounds, and the signatures of AkGameplayStatics' PostEvent functions.
	 */
	void InstallSoundDump();
}
