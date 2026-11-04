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

	bool AMWH_Registered = false;

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

		UWidgetBlueprintHookData* Hook = NewObject<UWidgetBlueprintHookData>(GetTransientPackage(), TEXT("AM_ManufacturingAutomationTabHook"));
		Hook->AddToRoot();
		Hook->DeveloperComment = TEXT("Automated Machines: Automation tab page in the manufacturer window");
		Hook->WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(AMWH_ManufacturingWindow));
		Hook->NewWidgetClass = UAMAutomationTab::StaticClass();
		Hook->NewWidgetName = TEXT("AM_AutomationTab");
		Hook->ParentWidgetType = EWidgetBlueprintHookParentType::Direct_Any;
		Hook->ParentWidgetName = AMWH_ManufacturingPages;

		UWidgetBlueprintHookSlot_Generic* SlotSetup = NewObject<UWidgetBlueprintHookSlot_Generic>(Hook);
		SlotSetup->Size = FSlateChildSize(ESlateSizeRule::Automatic);
		SlotSetup->HorizontalAlignment = HAlign_Fill;
		SlotSetup->VerticalAlignment = VAlign_Fill;
		Hook->SlotConfiguration = SlotSetup;

		HookManager->RegisterWidgetBlueprintHook(Hook);
		UE_LOG(LogAutomatedMachines, Log, TEXT("Registered the Automation tab hook on %s"), AMWH_ManufacturingWindow);
	}
}

void AMWidgetHooks::Install()
{
	FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld*, const UWorld::InitializationValues)
	{
		AMWH_Register();
	});
}
