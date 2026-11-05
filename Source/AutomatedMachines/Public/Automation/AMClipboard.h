// Copy and paste of automation settings (alarms, actions, options), together with the game's own
// copy / paste of machine settings:
//   - Ctrl+C / Ctrl+V while looking at a machine (everything: the game's settings and ours): hooks on
//     the character's ClipboardCopy / ClipboardPaste;
//   - the window's Copy / Paste Settings buttons: the game's settings only (not hooked);
//   - our Copy / Paste automation settings buttons on the Automation page: the automation only;
//   - middle click (sample a building with the build gun), then build: the new buildings of the same
//     kind get the sampled automation (hooks on the build gun's sample and construct).
// The clipboard is per player and per kind of machine, like the game's. Nothing is saved.
#pragma once

#include "CoreMinimal.h"
#include "Automation/AMTypes.h"

class AFGPlayerState;
class AFGBuildable;
class AAMAutomationSubsystem;

namespace AMClipboard
{
	/** Copied automation of one kind of machine. */
	struct FClip
	{
		TArray<FAMFireRule> Fires;
		TArray<FAMReactionRule> Reactions;
		FAMMachineOptions Options;
		/** Signs: the normal design and the alarm designs. */
		FAMSignDesign SignNormal;
		TArray<FAMSignDesign> SignDesigns;
	};

	/** Installs the hooks (once, at module startup, not in the editor). */
	void Install();

	/** Copies the automation of a machine for this player. A machine without automation clears the copy of its kind. */
	void Copy(AFGPlayerState* Player, UObject* Machine);

	/** Pastes this player's copy of the machine's kind onto it (nothing if there is none). Returns true if pasted. */
	bool Paste(AFGPlayerState* Player, UObject* Machine);

	/** This player's copy for the machine's kind, or null. */
	const FClip* Find(AFGPlayerState* Player, UObject* Machine);

	/** True if the game itself can copy / paste this machine's settings (it has its own buttons and Ctrl+C / Ctrl+V). */
	bool GameCanCopy(UObject* Machine);

	/** Server: a buildable was built; if it was built from a sampled machine, it gets the sampled automation. */
	void OnBuildableConstructed(AAMAutomationSubsystem* Subsystem, AFGBuildable* Buildable);
}
