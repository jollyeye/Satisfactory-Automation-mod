#include "UI/AMWidgetHooks.h"
#include "AutomatedMachinesModule.h"
#include "UI/AMAutomationTab.h"
#include "UI/AMSignTabs.h"
#include "Patching/WidgetBlueprintHookManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

namespace
{
	/** The window of the Smelter, Constructor, Assembler, ... (one class for all manufacturers). */
	const TCHAR* const AMWH_ManufacturingWindow = TEXT("/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Manufacturing.Widget_Manufacturing_C");

	/** Its tab pages, side by side: "Select Recipe", "Production", and then ours. */
	const TCHAR* const AMWH_ManufacturingPages = TEXT("HorizontalBox_1");

	/** The HUB terminal and its tab pages ("Select Milestone", "Active Milestone", and then ours). */
	const TCHAR* const AMWH_HubWindow = TEXT("/Game/FactoryGame/Buildable/Factory/TradingPost/UI/BPW_Hub.BPW_Hub_C");
	const TCHAR* const AMWH_HubPages = TEXT("HorizontalBox_25");

	// MAM: the same sliding pages (dump 5 Nov 2026: pages MolecularAnalysisMachine and HardDriveLibrary
	// in HorizontalBox_0). The window's path is looked up by its name, BPW_MAM.
	const TCHAR* const AMWH_MamWindowName = TEXT("BPW_MAM");
	const TCHAR* const AMWH_MamPages = TEXT("HorizontalBox_0");

	/** The class path of a game widget blueprint, by its asset name (e.g. ".../BPW_MAM.BPW_MAM_C"). */
	FString AMWH_FindWidgetPath(const TCHAR* AssetName)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		for (const TCHAR* Folder : { TEXT("/Game/FactoryGame/Buildable/Factory/Mam"), TEXT("/Game/FactoryGame/Interface"), TEXT("/Game/FactoryGame") })
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPath(FName(Folder), Assets, true);
			for (const FAssetData& Asset : Assets)
			{
				if (Asset.AssetName == FName(AssetName))
				{
					return FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), AssetName);
				}
			}
		}
		return FString();
	}

	// Miners (Mk1-3): a window with one page (no tab pages). Our widget goes into that page first,
	// then moves itself next to it (UAMAutomationTab::SetupSinglePage).
	const TCHAR* const AMWH_ExtractorWindow = TEXT("/Game/FactoryGame/Interface/UI/InGame/Widget_ResourceExtractorUI.Widget_ResourceExtractorUI_C");
	const TCHAR* const AMWH_ExtractorPage = TEXT("CanvasPanel_3");

	// Storage containers (Mk1, Mk2): also a one-page window. mInventoryVertBox is a variable of the
	// window, inside its page (from the window's asset; to confirm with the dump).
	const TCHAR* const AMWH_StorageWindow = TEXT("/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Storage.Widget_Storage_C");
	const TCHAR* const AMWH_StoragePage = TEXT("mInventoryVertBox");

	// Fuel generators (Biomass Burner, ...): a one-page window too. Its page panel has no stable
	// name, so the hook goes into the panel holding the standby button (Indirect_Child: the
	// named widget's parent), a variable every machine window with a standby switch has.
	const TCHAR* const AMWH_GeneratorWindow = TEXT("/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Generator.Widget_Generator_C");
	const TCHAR* const AMWH_StandbyButton = TEXT("Widget_StandbyButton");

	// Signs: the Sign Designer gets design tabs (UAMSignTabs), not an Automation page. Hooked into
	// the page panel holding its Revert Settings button (mSaveButton).
	const TCHAR* const AMWH_SignWindow = TEXT("/Game/FactoryGame/Interface/UI/InGame/Signs/BPW_SignInteractWidget.BPW_SignInteractWidget_C");
	const TCHAR* const AMWH_SignPageChild = TEXT("mSaveButton");

	bool AMWH_Registered = false;

	/** Adds UAMAutomationTab as the last page of a window's tab pages. */
	void AMWH_HookPages(UWidgetBlueprintHookManager* HookManager, const TCHAR* Window, const TCHAR* Pages, const TCHAR* HookName, bool bSlotSetup = true,
		EWidgetBlueprintHookParentType ParentType = EWidgetBlueprintHookParentType::Direct_Any, UClass* WidgetClass = nullptr,
		const TCHAR* WidgetName = TEXT("AM_AutomationTab"))
	{
		UWidgetBlueprintHookData* Hook = NewObject<UWidgetBlueprintHookData>(GetTransientPackage(), HookName);
		Hook->AddToRoot();
		Hook->DeveloperComment = TEXT("Automated Machines: Automation tab page");
		Hook->WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(Window));
		Hook->NewWidgetClass = WidgetClass ? WidgetClass : UAMAutomationTab::StaticClass();
		Hook->NewWidgetName = WidgetName;
		Hook->ParentWidgetType = ParentType;
		Hook->ParentWidgetName = Pages;

		// SML 3.12 bug: the generic slot setup crashes on a VerticalBox parent (its VerticalBox branch
		// calls SetSize on a null HorizontalBoxSlot). One-page windows move our widget anyway, so
		// their hooks go without a slot setup.
		if (bSlotSetup)
		{
			UWidgetBlueprintHookSlot_Generic* SlotSetup = NewObject<UWidgetBlueprintHookSlot_Generic>(Hook);
			SlotSetup->Size = FSlateChildSize(ESlateSizeRule::Automatic);
			SlotSetup->HorizontalAlignment = HAlign_Fill;
			SlotSetup->VerticalAlignment = VAlign_Fill;
			Hook->SlotConfiguration = SlotSetup;
		}

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
		// A second page in the HUB: every alarm of the world (UAMAutomationTab in overview mode, by its name).
		AMWH_HookPages(HookManager, AMWH_HubWindow, AMWH_HubPages, TEXT("AM_HubOverviewTabHook"), true,
			EWidgetBlueprintHookParentType::Direct_Any, nullptr, TEXT("AM_OverviewTab"));
		const FString MamWindow = AMWH_FindWidgetPath(AMWH_MamWindowName);
		UE_LOG(LogAutomatedMachines, Log, TEXT("MAM window: %s"), MamWindow.IsEmpty() ? TEXT("not found") : *MamWindow);
		if (!MamWindow.IsEmpty())
		{
			AMWH_HookPages(HookManager, *MamWindow, AMWH_MamPages, TEXT("AM_MamAutomationTabHook"));
		}
		AMWH_HookPages(HookManager, AMWH_ExtractorWindow, AMWH_ExtractorPage, TEXT("AM_ExtractorAutomationTabHook"));
		AMWH_HookPages(HookManager, AMWH_StorageWindow, AMWH_StoragePage, TEXT("AM_StorageAutomationTabHook"), false);
		AMWH_HookPages(HookManager, AMWH_GeneratorWindow, AMWH_StandbyButton, TEXT("AM_GeneratorAutomationTabHook"), false,
			EWidgetBlueprintHookParentType::Indirect_Child);
		AMWH_HookPages(HookManager, AMWH_SignWindow, AMWH_SignPageChild, TEXT("AM_SignDesignTabsHook"), false,
			EWidgetBlueprintHookParentType::Indirect_Child, UAMSignTabs::StaticClass());
	}
}

void AMWidgetHooks::Install()
{
	FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld*, const UWorld::InitializationValues)
	{
		AMWH_Register();
	});
}
