#include "UI/AMWidgetHooks.h"
#include "AutomatedMachinesModule.h"
#include "UI/AMAutomationTab.h"
#include "Patching/WidgetBlueprintHookManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Package.h"

namespace
{
	/** The window of the Smelter, Constructor, Assembler, ... (one class for all manufacturers). */
	const TCHAR* const AMWH_ManufacturingWindow = TEXT("/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Manufacturing.Widget_Manufacturing_C");

	/** Its tab pages, side by side: "Select Recipe", "Production", and then ours. */
	const TCHAR* const AMWH_ManufacturingPages = TEXT("HorizontalBox_1");

	/** The HUB terminal and its tab pages ("Select Milestone", "Active Milestone", and then ours). */
	const TCHAR* const AMWH_HubWindow = TEXT("/Game/FactoryGame/Buildable/Factory/TradingPost/UI/BPW_Hub.BPW_Hub_C");
	const TCHAR* const AMWH_HubPages = TEXT("HorizontalBox_25");

	// Miners (Mk1-3): a window with one page (no tab pages). Our widget goes into that page first,
	// then moves itself next to it (UAMAutomationTab::SetupSinglePage).
	const TCHAR* const AMWH_ExtractorWindow = TEXT("/Game/FactoryGame/Interface/UI/InGame/Widget_ResourceExtractorUI.Widget_ResourceExtractorUI_C");
	const TCHAR* const AMWH_ExtractorPage = TEXT("CanvasPanel_3");

	// Storage containers (Mk1, Mk2): also a one-page window. mInventoryVertBox is a variable of the
	// window, inside its page (from the window's asset; to confirm with the dump).
	const TCHAR* const AMWH_StorageWindow = TEXT("/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Storage.Widget_Storage_C");
	const TCHAR* const AMWH_StoragePage = TEXT("mInventoryVertBox");

	bool AMWH_Registered = false;

	/** Adds UAMAutomationTab as the last page of a window's tab pages. */
	void AMWH_HookPages(UWidgetBlueprintHookManager* HookManager, const TCHAR* Window, const TCHAR* Pages, const TCHAR* HookName)
	{
		UWidgetBlueprintHookData* Hook = NewObject<UWidgetBlueprintHookData>(GetTransientPackage(), HookName);
		Hook->AddToRoot();
		Hook->DeveloperComment = TEXT("Automated Machines: Automation tab page");
		Hook->WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(Window));
		Hook->NewWidgetClass = UAMAutomationTab::StaticClass();
		Hook->NewWidgetName = TEXT("AM_AutomationTab");
		Hook->ParentWidgetType = EWidgetBlueprintHookParentType::Direct_Any;
		Hook->ParentWidgetName = Pages;

		UWidgetBlueprintHookSlot_Generic* SlotSetup = NewObject<UWidgetBlueprintHookSlot_Generic>(Hook);
		SlotSetup->Size = FSlateChildSize(ESlateSizeRule::Automatic);
		SlotSetup->HorizontalAlignment = HAlign_Fill;
		SlotSetup->VerticalAlignment = VAlign_Fill;
		Hook->SlotConfiguration = SlotSetup;

		HookManager->RegisterWidgetBlueprintHook(Hook);
		UE_LOG(LogAutomatedMachines, Log, TEXT("Registered the Automation tab hook on %s"), Window);
	}

	void AMWH_Register()
	{
		if (AMWH_Registered || !GEngine)
		{
			return;
		}
		UWidgetBlueprintHookManager* HookManager = GEngine->GetEngineSubsystem<UWidgetBlueprintHookManager>();
		if (!HookManager)
		{
			return;
		}
		AMWH_Registered = true;
		AMWH_HookPages(HookManager, AMWH_ManufacturingWindow, AMWH_ManufacturingPages, TEXT("AM_ManufacturingAutomationTabHook"));
		AMWH_HookPages(HookManager, AMWH_HubWindow, AMWH_HubPages, TEXT("AM_HubAutomationTabHook"));
		AMWH_HookPages(HookManager, AMWH_ExtractorWindow, AMWH_ExtractorPage, TEXT("AM_ExtractorAutomationTabHook"));
		AMWH_HookPages(HookManager, AMWH_StorageWindow, AMWH_StoragePage, TEXT("AM_StorageAutomationTabHook"));
	}
}

void AMWidgetHooks::Install()
{
	FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld*, const UWorld::InitializationValues)
	{
		AMWH_Register();
	});
}
