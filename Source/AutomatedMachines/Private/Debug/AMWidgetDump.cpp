#include "Debug/AMWidgetDump.h"
#include "AutomatedMachinesModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/SlateWrapperTypes.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildablePipelinePump.h"

namespace
{
	// Windows whose whole widget tree is written to the log (once per class).
	const TCHAR* const AMWD_TreeClasses[] = {
		TEXT("BPW_SignInteractWidget_C"),
	};

	// Widgets inside those windows whose variables (with values) and functions are written too:
	// the tab machinery, to learn how to add an Automation tab.
	const TCHAR* const AMWD_ReflectionClasses[] = {
		TEXT("BPW_SignInteractWidget_C"),
		TEXT("Widget_Window_DarkMode_C"),
		TEXT("Widget_TabsContainer_C"),
		TEXT("Widget_SlidingTabs_C"),
		TEXT("Widget_SlidingTabs_Button_C"),
	};

	// Also every widget in those windows whose class name contains one of these (tab and menu parts).
	const TCHAR* const AMWD_ReflectionKeywords[] = {
		TEXT("Tab"), TEXT("Overclock"), TEXT("Standby"),
	};

	constexpr int32 AMWD_MaxDepth = 40;
	constexpr int32 AMWD_MaxLinesPerDump = 8000;
	constexpr int32 AMWD_MaxValueLength = 300;

	/** Widgets already looked at, so each is handled once. */
	TSet<TWeakObjectPtr<UUserWidget>> AMWD_Dumped;

	/** Widget classes already announced with a one-line "new widget class" entry. */
	TSet<FName> AMWD_SeenClasses;

	/** Window classes whose tree was already written. */
	TSet<FName> AMWD_TreeDone;

	/** Classes whose functions were already written (variables are written per instance). */
	TSet<FName> AMWD_FunctionsDone;

	bool AMWD_IsOneOf(const FString& ClassName, const TCHAR* const* Names, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (ClassName == Names[Index])
			{
				return true;
			}
		}
		return false;
	}

	/** Also every window whose class name contains one of these (the MAM's research window, 5 Nov 2026). */
	const TCHAR* const AMWD_TreeKeywords[] = {
		TEXT("MAM"), TEXT("Research"),
	};

	bool AMWD_IsTreeClass(const FString& ClassName)
	{
		if (AMWD_IsOneOf(ClassName, AMWD_TreeClasses, UE_ARRAY_COUNT(AMWD_TreeClasses)))
		{
			return true;
		}
		for (const TCHAR* Keyword : AMWD_TreeKeywords)
		{
			if (ClassName.Contains(Keyword))
			{
				return true;
			}
		}
		return false;
	}

	bool AMWD_IsReflectionClass(const FString& ClassName)
	{
		if (AMWD_IsOneOf(ClassName, AMWD_ReflectionClasses, UE_ARRAY_COUNT(AMWD_ReflectionClasses)))
		{
			return true;
		}
		for (const TCHAR* Keyword : AMWD_ReflectionKeywords)
		{
			if (ClassName.Contains(Keyword))
			{
				return true;
			}
		}
		return false;
	}

	/** Names of the user widgets this widget sits in, outermost last, e.g. "BPW_A < BPW_GameUI". */
	FString AMWD_OuterChain(const UUserWidget* Widget)
	{
		FString Chain;
		for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		{
			if (const UUserWidget* OuterWidget = Cast<UUserWidget>(Outer))
			{
				Chain += Chain.IsEmpty() ? TEXT("") : TEXT(" < ");
				Chain += OuterWidget->GetClass()->GetName();
			}
		}
		return Chain.IsEmpty() ? TEXT("(none)") : Chain;
	}

	void AMWD_DumpWidget(UWidget* Widget, int32 Depth, int32& Lines, TArray<UUserWidget*>& OutUserWidgets)
	{
		if (!Widget || Depth > AMWD_MaxDepth || Lines >= AMWD_MaxLinesPerDump)
		{
			return;
		}
		++Lines;

		const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
		const FString SlotClass = Widget->Slot ? Widget->Slot->GetClass()->GetName() : TEXT("-");
		const FString Visibility = StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget->GetVisibility()));

		FString Extra;
		if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			Extra = FString::Printf(TEXT(" text=\"%s\""), *Text->GetText().ToString().Left(40));
		}

		UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] %s%s [%s] slot=%s vis=%s%s"),
			*Indent, *Widget->GetName(), *Widget->GetClass()->GetName(), *SlotClass, *Visibility, *Extra);

		if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			AMWD_Dumped.Add(UserWidget);
			OutUserWidgets.Add(UserWidget);
			if (UserWidget->WidgetTree && UserWidget->WidgetTree->RootWidget)
			{
				AMWD_DumpWidget(UserWidget->WidgetTree->RootWidget, Depth + 1, Lines, OutUserWidgets);
			}
		}
		else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				AMWD_DumpWidget(Panel->GetChildAt(Index), Depth + 1, Lines, OutUserWidgets);
			}
		}
	}

	/** Parameters of a function as "Type Name, out Type Name" and its return type. */
	void AMWD_DescribeParams(const UFunction* Function, FString& OutParams, FString& OutReturn)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			const FProperty* Param = *It;
			if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				OutReturn = Param->GetCPPType();
				continue;
			}
			const bool bOut = Param->HasAnyPropertyFlags(CPF_OutParm) && !Param->HasAnyPropertyFlags(CPF_ConstParm);
			OutParams += OutParams.IsEmpty() ? TEXT("") : TEXT(", ");
			OutParams += FString::Printf(TEXT("%s%s %s"), bOut ? TEXT("out ") : TEXT(""), *Param->GetCPPType(), *Param->GetName());
		}
	}

	/**
	 * Variables (with this instance's values) and functions of the widget's Blueprint classes,
	 * from its own class up to (not including) UUserWidget.
	 */
	void AMWD_DumpReflection(UUserWidget* Widget)
	{
		UClass* WidgetClass = Widget->GetClass();
		bool bFunctionsAlreadyDone = false;
		AMWD_FunctionsDone.Add(WidgetClass->GetFName(), &bFunctionsAlreadyDone);

		UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== reflection %s (%s, inside: %s) ====="),
			*WidgetClass->GetName(), *Widget->GetName(), *AMWD_OuterChain(Widget));

		for (UClass* Class = WidgetClass; Class && Class != UUserWidget::StaticClass(); Class = Class->GetSuperClass())
		{
			UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] class %s (parent %s)"),
				*Class->GetName(), Class->GetSuperClass() ? *Class->GetSuperClass()->GetName() : TEXT("-"));

			for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				FProperty* Property = *It;
				if (Property->GetName() == TEXT("UberGraphFrame"))
				{
					continue;
				}
				FString Value;
				Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Widget), nullptr, Widget, PPF_None);
				if (Value.Len() > AMWD_MaxValueLength)
				{
					Value = Value.Left(AMWD_MaxValueLength) + TEXT("...");
				}
				UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump]   var %s %s = %s"),
					*Property->GetCPPType(), *Property->GetName(), *Value);
			}

			if (bFunctionsAlreadyDone)
			{
				continue;
			}
			for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				const UFunction* Function = *It;
				if (Function->GetName().StartsWith(TEXT("ExecuteUbergraph")))
				{
					continue;
				}
				FString Params;
				FString Return = TEXT("void");
				AMWD_DescribeParams(Function, Params, Return);
				UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump]   func %s %s(%s)%s"),
					*Return, *Function->GetName(), *Params,
					Function->HasAnyFunctionFlags(FUNC_Event) ? TEXT(" [event]") : TEXT(""));
			}
		}

		UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== end reflection %s ====="), *WidgetClass->GetName());
	}

	/** The window of a power switch / priority switch, a light or light control panel, an extractor (its mInteractObject). */
	bool AMWD_IsSwitchWindow(const UUserWidget* Widget)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("mInteractObject"));
		const UObject* Object = Property ? Property->GetObjectPropertyValue_InContainer(Widget) : nullptr;
		return Object && (Object->IsA<AFGBuildableCircuitSwitch>() || Object->IsA<AFGBuildableLightSource>()
			|| Object->IsA<AFGBuildableLightsControlPanel>() || Object->IsA<AFGBuildableResourceExtractor>()
			|| Object->IsA<AFGBuildablePipelinePump>());
	}

	void AMWD_CheckWorld(UWorld* World)
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), false);

		for (UUserWidget* Widget : Widgets)
		{
			if (!Widget || AMWD_Dumped.Contains(Widget))
			{
				continue;
			}

			const FString ClassName = Widget->GetClass()->GetName();
			bool bClassAlreadySeen = false;
			AMWD_SeenClasses.Add(Widget->GetClass()->GetFName(), &bClassAlreadySeen);
			if (!bClassAlreadySeen)
			{
				UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] new widget class: %s (inside: %s)"),
					*ClassName, *AMWD_OuterChain(Widget));
			}

			const bool bSwitchWindow = AMWD_IsSwitchWindow(Widget);
			if (!AMWD_IsTreeClass(ClassName) && !bSwitchWindow)
			{
				continue;
			}
			bool bTreeAlreadyDone = false;
			AMWD_TreeDone.Add(Widget->GetClass()->GetFName(), &bTreeAlreadyDone);
			if (bTreeAlreadyDone)
			{
				AMWD_Dumped.Add(Widget);
				continue;
			}

			UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== begin %s (inside: %s) path %s ====="),
				*ClassName, *AMWD_OuterChain(Widget), *Widget->GetClass()->GetPathName());
			int32 Lines = 0;
			TArray<UUserWidget*> UserWidgets;
			AMWD_DumpWidget(Widget, 0, Lines, UserWidgets);
			UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== end %s (%d lines) ====="), *ClassName, Lines);

			for (UUserWidget* Inner : UserWidgets)
			{
				if (Inner && (AMWD_IsReflectionClass(Inner->GetClass()->GetName()) || (bSwitchWindow && Inner == Widget)))
				{
					AMWD_DumpReflection(Inner);
				}
			}
		}
	}
}

void AMWidgetDump::Install()
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
	{
		if (!GEngine || IsRunningDedicatedServer())
		{
			return true;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType == EWorldType::Game && World)
			{
				AMWD_CheckWorld(World);
			}
		}
		return true;
	}), 0.5f);
}

namespace
{
	const TCHAR* const AMWD_SoundKeywords[] = {
		TEXT("UI"), TEXT("Notif"), TEXT("Alarm"), TEXT("Alert"), TEXT("Beep"), TEXT("Ping"), TEXT("Click"),
		TEXT("Siren"), TEXT("Horn"), TEXT("Bell"), TEXT("Warning"), TEXT("Error"), TEXT("Success"), TEXT("Unlock"),
		TEXT("Complete"), TEXT("Ding"), TEXT("Chime"), TEXT("Hub"), TEXT("Milestone"), TEXT("Scanner"), TEXT("Radar"),
	};
	constexpr int32 AMWD_MaxSoundLines = 1500;
	bool AMWD_SoundsDumped = false;

	void AMWD_DumpSounds()
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/AkAudio"), TEXT("AkAudioEvent")), Assets, true);
		UE_LOG(LogAutomatedMachines, Log, TEXT("[SoundDump] %d sound events in the game"), Assets.Num());

		int32 Lines = 0;
		for (const FAssetData& Asset : Assets)
		{
			const FString Path = Asset.GetObjectPathString();
			bool bMatch = false;
			for (const TCHAR* Keyword : AMWD_SoundKeywords)
			{
				if (Asset.AssetName.ToString().Contains(Keyword, ESearchCase::IgnoreCase))
				{
					bMatch = true;
					break;
				}
			}
			if (bMatch && Lines++ < AMWD_MaxSoundLines)
			{
				UE_LOG(LogAutomatedMachines, Log, TEXT("[SoundDump] %s"), *Path);
			}
		}
		UE_LOG(LogAutomatedMachines, Log, TEXT("[SoundDump] %d matching events"), Lines);

		// How to play one: the PostEvent functions of AkGameplayStatics, with their parameters.
		if (const UClass* Statics = FindObject<UClass>(nullptr, TEXT("/Script/AkAudio.AkGameplayStatics")))
		{
			for (TFieldIterator<UFunction> It(Statics, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				const UFunction* Function = *It;
				if (Function->GetName().Contains(TEXT("PostEvent")) || Function->GetName().Contains(TEXT("PostAkEvent")))
				{
					FString Params;
					FString Return = TEXT("void");
					AMWD_DescribeParams(Function, Params, Return);
					UE_LOG(LogAutomatedMachines, Log, TEXT("[SoundDump] func %s %s(%s)"), *Return, *Function->GetName(), *Params);
				}
			}
		}
		else
		{
			UE_LOG(LogAutomatedMachines, Log, TEXT("[SoundDump] AkGameplayStatics not found"));
		}
		if (const UClass* EventClass = FindObject<UClass>(nullptr, TEXT("/Script/AkAudio.AkAudioEvent")))
		{
			for (TFieldIterator<UFunction> It(EventClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				FString Params;
				FString Return = TEXT("void");
				AMWD_DescribeParams(*It, Params, Return);
				UE_LOG(LogAutomatedMachines, Log, TEXT("[SoundDump] AkAudioEvent func %s %s(%s)"), *Return, *It->GetName(), *Params);
			}
		}
	}
}

void AMWidgetDump::InstallSoundDump()
{
	FWorldDelegates::OnWorldInitializedActors.AddLambda([](const UWorld::FActorsInitializedParams& Params)
	{
		if (!AMWD_SoundsDumped && Params.World && Params.World->IsGameWorld())
		{
			AMWD_SoundsDumped = true;
			AMWD_DumpSounds();
		}
	});
}
