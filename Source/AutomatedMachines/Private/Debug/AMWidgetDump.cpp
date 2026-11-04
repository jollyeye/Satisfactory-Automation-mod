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

namespace
{
	// Windows whose whole widget tree is written to the log (once per class).
	const TCHAR* const AMWD_TreeClasses[] = {
		TEXT("Widget_Manufacturing_C"),
	};

	// Widgets inside those windows whose variables (with values) and functions are written too:
	// the tab machinery, to learn how to add an Automation tab.
	const TCHAR* const AMWD_ReflectionClasses[] = {
		TEXT("Widget_Manufacturing_C"),
		TEXT("Widget_Window_DarkMode_C"),
		TEXT("Widget_TabsContainer_C"),
		TEXT("Widget_SlidingTabs_C"),
		TEXT("Widget_SlidingTabs_Button_C"),
	};

	constexpr int32 AMWD_MaxDepth = 40;
	constexpr int32 AMWD_MaxLinesPerDump = 3000;
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

	bool AMWD_IsTreeClass(const FString& ClassName)
	{
		return AMWD_IsOneOf(ClassName, AMWD_TreeClasses, UE_ARRAY_COUNT(AMWD_TreeClasses));
	}

	bool AMWD_IsReflectionClass(const FString& ClassName)
	{
		return AMWD_IsOneOf(ClassName, AMWD_ReflectionClasses, UE_ARRAY_COUNT(AMWD_ReflectionClasses));
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

			if (!AMWD_IsTreeClass(ClassName))
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

			UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== begin %s (inside: %s) ====="),
				*ClassName, *AMWD_OuterChain(Widget));
			int32 Lines = 0;
			TArray<UUserWidget*> UserWidgets;
			AMWD_DumpWidget(Widget, 0, Lines, UserWidgets);
			UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== end %s (%d lines) ====="), *ClassName, Lines);

			for (UUserWidget* Inner : UserWidgets)
			{
				if (Inner && AMWD_IsReflectionClass(Inner->GetClass()->GetName()))
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
