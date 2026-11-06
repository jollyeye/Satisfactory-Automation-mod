// Lights and light control panels: their settings (colour slot, intensity, night mode, on / standby)
// as our saved light settings, and applying them.
#pragma once

#include "CoreMinimal.h"
#include "Automation/AMTypes.h"

class AFGBuildable;

namespace AMLight
{
	/** A light (street light, ceiling light, flood light...) or a light control panel. */
	AUTOMATEDMACHINES_API bool IsLight(const UObject* Object);

	/** What the building has now (replicated: works on clients too), for the alarm Event. */
	AUTOMATEDMACHINES_API FAMLightSetting Read(const AFGBuildable* Light, const FString& Event);

	/** Applies settings (server only). A control panel passes them on to its lights. */
	AUTOMATEDMACHINES_API void Apply(AFGBuildable* Light, const FAMLightSetting& Setting);
}
