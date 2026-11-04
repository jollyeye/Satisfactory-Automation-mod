// Puts our widgets into the game's windows with SML's widget blueprint hooks.
#pragma once

#include "CoreMinimal.h"

namespace AMWidgetHooks
{
	/** Registers the hooks when the first world starts (the game's widget classes can be loaded then). */
	void Install();
}
