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

namespace
{
	// Widget classes whose name contains one of these get their whole tree written to the log.
	const TCHAR* const AMWD_Keywords[] = {
		TEXT("Manufactur"), TEXT("Smelter"), TEXT("Constructor"), TEXT("Interact"), TEXT("Factory"), TEXT("Production"),
	};

	constexpr int32 AMWD_MaxDepth = 40;
	constexpr int32 AMWD_MaxLinesPerDump = 3000;

	/** Widgets already written (as a window or inside one), so each is written once. */
	TSet<TWeakObjectPtr<UUserWidget>> AMWD_Dumped;

	/** Widget classes already announced with a one-line "new widget class" entry. */
	TSet<FName> AMWD_SeenClasses;

	bool AMWD_MatchesKeyword(const FString& ClassName)
	{
		for (const TCHAR* Keyword : AMWD_Keywords)
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

	void AMWD_DumpWidget(UWidget* Widget, int32 Depth, int32& Lines)
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
			if (UserWidget->WidgetTree && UserWidget->WidgetTree->RootWidget)
			{
				AMWD_DumpWidget(UserWidget->WidgetTree->RootWidget, Depth + 1, Lines);
			}
		}
		else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				AMWD_DumpWidget(Panel->GetChildAt(Index), Depth + 1, Lines);
			}
		}
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

			if (!AMWD_MatchesKeyword(ClassName))
			{
				continue;
			}

			UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== begin %s (inside: %s) ====="),
				*ClassName, *AMWD_OuterChain(Widget));
			int32 Lines = 0;
			AMWD_DumpWidget(Widget, 0, Lines);
			UE_LOG(LogAutomatedMachines, Log, TEXT("[WidgetDump] ===== end %s (%d lines) ====="), *ClassName, Lines);
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
