// Plays one of the Play sound sounds (AMRules::GetSounds) for local players.
//
// The game uses Wwise. To avoid a build dependency on the Wwise plugin, the sound event is loaded by
// path and played through its own UFUNCTION PostOnActor, called by name.
#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

namespace AMSound
{
	/** Plays sound Index (of AMRules::GetSounds) on this player. */
	AUTOMATEDMACHINES_API void PlayForPlayer(APlayerController* Player, int32 Index);

	/** Plays it for every local player of this game instance. */
	AUTOMATEDMACHINES_API void PlayForLocalPlayers(UWorld* World, int32 Index);
}
