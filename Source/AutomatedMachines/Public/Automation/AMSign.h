// Signs: converting between the game's sign data and our saved designs, and showing a design.
#pragma once

#include "CoreMinimal.h"
#include "Automation/AMTypes.h"

class AFGBuildableWidgetSign;
struct FPrefabSignData;

namespace AMSign
{
	/** The game's sign data as one of our designs (for the alarm Event; empty: normal). */
	AUTOMATEDMACHINES_API FAMSignDesign FromSignData(const FPrefabSignData& Data, const FString& Event);

	/** One of our designs as the game's sign data. */
	AUTOMATEDMACHINES_API void ToSignData(const FAMSignDesign& Design, FPrefabSignData& OutData);

	/** Shows a design on a sign (server: the sign sends it to every player). */
	AUTOMATEDMACHINES_API void Show(AFGBuildableWidgetSign* Sign, const FAMSignDesign& Design);
}
