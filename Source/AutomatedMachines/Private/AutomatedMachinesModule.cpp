#include "AutomatedMachinesModule.h"
#include "Debug/AMWidgetDump.h"
#include "UI/AMWidgetHooks.h"

DEFINE_LOG_CATEGORY(LogAutomatedMachines);

void FAutomatedMachinesModule::StartupModule()
{
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automated Machines module started"));

#if !WITH_EDITOR
	// Automation tab in the machine window (Smelter, Constructor, ...).
	AMWidgetHooks::Install();

	// Development only: writes the widget tree of open machine windows to the log, so we can find
	// where the Automation section can be inserted. Removed before release.
	AMWidgetDump::Install();
#endif
}

IMPLEMENT_GAME_MODULE(FAutomatedMachinesModule, AutomatedMachines);
