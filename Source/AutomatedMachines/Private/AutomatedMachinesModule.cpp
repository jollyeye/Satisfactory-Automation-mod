#include "AutomatedMachinesModule.h"
#include "Debug/AMWidgetDump.h"
#include "UI/AMWidgetHooks.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Net/AMRemoteCallObject.h"
#include "Automation/AMClipboard.h"

DEFINE_LOG_CATEGORY(LogAutomatedMachines);

void FAutomatedMachinesModule::StartupModule()
{
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automated Machines module started"));

#if !WITH_EDITOR
	// The save object with every machine's rules, and the engine that runs them.
	AAMAutomationSubsystem::InstallRegistration();

	// Multiplayer: clients send rule changes to the server through this.
	UAMRemoteCallObject::InstallRegistration();

	// Automation tab in the machine window (Smelter, Constructor, ...).
	AMWidgetHooks::Install();

	// Copy / paste of automation with the game's copy / paste and middle click.
	AMClipboard::Install();

	// Development only: writes the widget tree of open machine windows to the log, so we can find
	// where the Automation section can be inserted. Removed before release.
	AMWidgetDump::Install();
	// Lists the game's UI / alert sounds (used to pick the Play sound sounds); off, not needed now.
	// AMWidgetDump::InstallSoundDump();
#endif
}

IMPLEMENT_GAME_MODULE(FAutomatedMachinesModule, AutomatedMachines);
