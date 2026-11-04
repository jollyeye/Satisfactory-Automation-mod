#include "UI/AMAutomationTab.h"
#include "AutomatedMachinesModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName AMAT_FrameProperty(TEXT("Widget_Window_DarkMode"));
	const FName AMAT_SlidingTabsProperty(TEXT("Widget_SlidingTabs"));
	const FName AMAT_TabsProperty(TEXT("mTabs"));
	const FName AMAT_TabClickedProperty(TEXT("OnTabButtonClicked"));
	const FName AMAT_TitleProperty(TEXT("mTitleText"));
	const FName AMAT_TabsContainerProperty(TEXT("Widget_TabsContainer"));
	const FName AMAT_ButtonsPanelProperty(TEXT("mContainer"));
	const FName AMAT_IndexOffsetProperty(TEXT("mIndexOffset"));
	const FName AMAT_ActiveIndexProperty(TEXT("mActiveIndex"));
	const FName AMAT_PagesProperty(TEXT("mHorizontalBox"));
	const FName AMAT_FontSourceWidget(TEXT("mRecipeName"));

	const TCHAR* const AMAT_TabTitle = TEXT("Automation");

	const FLinearColor AMAT_White(1.f, 1.f, 1.f, 1.f);
	const FLinearColor AMAT_Grey(0.6f, 0.6f, 0.6f, 1.f);
	const FLinearColor AMAT_Orange(0.98f, 0.55f, 0.13f, 1.f);

	/** An object variable of a Blueprint object, by name. */
	UObject* AMAT_GetObject(const UObject* Owner, FName Name)
	{
		if (!Owner)
		{
			return nullptr;
		}
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
	}

	bool AMAT_GetInt(const UObject* Owner, FName Name, int32& OutValue)
	{
		const FIntProperty* Property = Owner ? FindFProperty<FIntProperty>(Owner->GetClass(), Name) : nullptr;
		if (!Property)
		{
			return false;
		}
		OutValue = Property->GetPropertyValue_InContainer(Owner);
		return true;
	}

	/** The text variable inside a tab entry struct (the game names it "Text_<number>_<guid>"). */
	FTextProperty* AMAT_FindTextField(const UScriptStruct* Struct)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (FTextProperty* TextProperty = CastField<FTextProperty>(*It))
			{
				return TextProperty;
			}
		}
		return nullptr;
	}

	/**
	 * Calls a Blueprint function by name. Fill sets the parameters (they start zeroed and initialized).
	 * Returns false if the function does not exist.
	 */
	bool AMAT_Call(UObject* Target, FName FunctionName, TFunctionRef<void(UFunction*, uint8*)> Fill)
	{
		UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
		if (!Function)
		{
			UE_LOG(LogAutomatedMachines, Warning, TEXT("Automation tab: function %s not found on %s"),
				*FunctionName.ToString(), Target ? *Target->GetClass()->GetName() : TEXT("null"));
			return false;
		}
		TArray<uint8> Params;
		Params.SetNumZeroed(FMath::Max<int32>(Function->ParmsSize, 1));
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params.GetData());
		}
		Fill(Function, Params.GetData());
		Target->ProcessEvent(Function, Params.GetData());
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params.GetData());
		}
		return true;
	}

	bool AMAT_CallNoParams(UObject* Target, FName FunctionName)
	{
		return AMAT_Call(Target, FunctionName, [](UFunction*, uint8*) {});
	}

	/** Sliding pages: offset of each page index (TMap<int32, double> mIndexOffset). */
	struct FAMATIndexOffsets
	{
		FMapProperty* Property = nullptr;
		void* Map = nullptr;

		explicit FAMATIndexOffsets(UObject* SlidingTabs)
		{
			Property = SlidingTabs ? FindFProperty<FMapProperty>(SlidingTabs->GetClass(), AMAT_IndexOffsetProperty) : nullptr;
			if (Property && CastField<FIntProperty>(Property->KeyProp) && CastField<FDoubleProperty>(Property->ValueProp))
			{
				Map = Property->ContainerPtrToValuePtr<void>(SlidingTabs);
			}
		}

		bool IsValid() const { return Map != nullptr; }

		bool Find(int32 Index, double& OutOffset) const
		{
			if (!IsValid())
			{
				return false;
			}
			FScriptMapHelper Helper(Property, Map);
			const uint8* Value = Helper.FindValueFromHash(&Index);
			if (!Value)
			{
				return false;
			}
			OutOffset = *reinterpret_cast<const double*>(Value);
			return true;
		}

		void Set(int32 Index, double Offset)
		{
			if (IsValid())
			{
				FScriptMapHelper Helper(Property, Map);
				Helper.AddPair(&Index, &Offset);
			}
		}

		FString Describe() const
		{
			if (!IsValid())
			{
				return TEXT("(not found)");
			}
			FString Result;
			FScriptMapHelper Helper(Property, Map);
			for (int32 Slot = 0; Slot < Helper.GetMaxIndex(); ++Slot)
			{
				if (Helper.IsValidIndex(Slot))
				{
					Result += FString::Printf(TEXT("%d=%.0f "), *reinterpret_cast<const int32*>(Helper.GetKeyPtr(Slot)),
						*reinterpret_cast<const double*>(Helper.GetValuePtr(Slot)));
				}
			}
			return Result;
		}
	};
}

UUserWidget* UAMAutomationTab::GetHostWindow() const
{
	// We sit in the window's widget tree, whose outer is the window.
	if (const UWidgetTree* Tree = Cast<UWidgetTree>(GetOuter()))
	{
		if (UUserWidget* Host = Cast<UUserWidget>(Tree->GetOuter()))
		{
			return Host;
		}
	}
	return GetTypedOuter<UUserWidget>();
}

UObject* UAMAutomationTab::GetWindowFrame() const
{
	return AMAT_GetObject(GetHostWindow(), AMAT_FrameProperty);
}

UTextBlock* UAMAutomationTab::MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetAutoWrapText(true);
	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = FontSize;
	TextBlock->SetFont(Font);
	mTexts.Add(TextBlock);
	mTextSizes.Add(FontSize);
	return TextBlock;
}

void UAMAutomationTab::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AMRoot"));
	Root->SetWidthOverride(760.f);
	Root->SetMinDesiredHeight(520.f);
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AMBackground"));
	Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.018f, 0.92f));
	Background->SetPadding(FMargin(28.f, 24.f));
	Root->AddChild(Background);

	mContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AMContent"));
	Background->AddChild(mContentBox);

	auto AddRow = [this](UWidget* Widget, float TopPadding)
	{
		if (UVerticalBoxSlot* RowSlot = mContentBox->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
		}
	};

	AddRow(MakeText(TEXT("AUTOMATION"), 26, AMAT_Orange), 0.f);
	mMachineText = MakeText(TEXT(""), 16, AMAT_Grey);
	AddRow(mMachineText, 4.f);

	AddRow(MakeText(TEXT("FIRE EVENTS"), 18, AMAT_White), 28.f);
	AddRow(MakeText(TEXT("This machine fires no events yet."), 14, AMAT_Grey), 6.f);

	AddRow(MakeText(TEXT("REACTIONS"), 18, AMAT_White), 28.f);
	AddRow(MakeText(TEXT("This machine reacts to no events yet."), 14, AMAT_Grey), 6.f);

	AddRow(MakeText(TEXT("Coming soon: add rules here, e.g. \"Pause, when event PLATES BACKED UP\"."), 13, AMAT_Grey), 36.f);
}

void UAMAutomationTab::AddTabEntry()
{
	UObject* Frame = GetWindowFrame();
	FArrayProperty* TabsProperty = Frame ? FindFProperty<FArrayProperty>(Frame->GetClass(), AMAT_TabsProperty) : nullptr;
	FStructProperty* EntryProperty = TabsProperty ? CastField<FStructProperty>(TabsProperty->Inner) : nullptr;
	FTextProperty* TextField = EntryProperty ? AMAT_FindTextField(EntryProperty->Struct) : nullptr;
	if (!TextField)
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Automation tab: tab list not found (frame %s)"), Frame ? *Frame->GetClass()->GetName() : TEXT("null"));
		return;
	}

	FScriptArrayHelper Tabs(TabsProperty, TabsProperty->ContainerPtrToValuePtr<void>(Frame));
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		if (TextField->GetPropertyValue_InContainer(Tabs.GetRawPtr(Index)).ToString() == AMAT_TabTitle)
		{
			TabIndex = Index;
			return;
		}
	}
	if (Tabs.Num() == 0)
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Automation tab: the window has no tabs to copy"));
		return;
	}

	// Copy the last tab (Production: its icon and style), then give it our title.
	const int32 NewIndex = Tabs.AddValue();
	EntryProperty->Struct->CopyScriptStruct(Tabs.GetRawPtr(NewIndex), Tabs.GetRawPtr(NewIndex - 1));
	TextField->SetPropertyValue_InContainer(Tabs.GetRawPtr(NewIndex), FText::FromString(AMAT_TabTitle));
	TabIndex = NewIndex;

	int32 PageIndex = INDEX_NONE;
	if (UPanelWidget* Pages = GetParent())
	{
		PageIndex = Pages->GetChildIndex(this);
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: added tab entry %d (our page is child %d of %s)"),
		TabIndex, PageIndex, GetParent() ? *GetParent()->GetName() : TEXT("nothing"));
}

void UAMAutomationTab::EnsureTabButtonExists()
{
	UObject* Frame = GetWindowFrame();
	UObject* TabsContainer = AMAT_GetObject(Frame, AMAT_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
	const int32 ButtonCount = Buttons ? Buttons->GetChildrenCount() : -1;
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: %d tab buttons exist, our tab index is %d"), ButtonCount, TabIndex);

	if (Buttons && TabIndex != INDEX_NONE && ButtonCount > 0 && ButtonCount <= TabIndex)
	{
		// The frame made its buttons before our entry was in the list: make them again.
		AMAT_CallNoParams(Frame, TEXT("InitTabs"));
		UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: tabs made again, now %d buttons"), Buttons->GetChildrenCount());
	}
}

void UAMAutomationTab::ApplyGameFonts()
{
	const UUserWidget* Host = GetHostWindow();
	const UTextBlock* Source = Host && Host->WidgetTree ? Cast<UTextBlock>(Host->WidgetTree->FindWidget(AMAT_FontSourceWidget)) : nullptr;
	if (!Source)
	{
		return;
	}
	for (int32 Index = 0; Index < mTexts.Num(); ++Index)
	{
		if (mTexts[Index])
		{
			FSlateFontInfo Font = Source->GetFont();
			Font.Size = mTextSizes.IsValidIndex(Index) ? mTextSizes[Index] : Font.Size;
			mTexts[Index]->SetFont(Font);
		}
	}
}

void UAMAutomationTab::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
	AddTabEntry();
}

void UAMAutomationTab::NativeConstruct()
{
	Super::NativeConstruct();

	UObject* Frame = GetWindowFrame();
	if (!bBoundToFrame && Frame)
	{
		if (FMulticastDelegateProperty* ClickedProperty = FindFProperty<FMulticastDelegateProperty>(Frame->GetClass(), AMAT_TabClickedProperty))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAMAutomationTab, OnWindowTabButtonClicked));
			ClickedProperty->AddDelegate(MoveTemp(Delegate), Frame);
			bBoundToFrame = true;
		}
	}

	EnsureTabButtonExists();
	ApplyGameFonts();

	if (mMachineText && Frame)
	{
		if (const FTextProperty* TitleProperty = FindFProperty<FTextProperty>(Frame->GetClass(), AMAT_TitleProperty))
		{
			mMachineText->SetText(TitleProperty->GetPropertyValue_InContainer(Frame));
		}
	}
}

void UAMAutomationTab::NativeDestruct()
{
	UObject* Frame = GetWindowFrame();
	if (bBoundToFrame && Frame)
	{
		if (FMulticastDelegateProperty* ClickedProperty = FindFProperty<FMulticastDelegateProperty>(Frame->GetClass(), AMAT_TabClickedProperty))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAMAutomationTab, OnWindowTabButtonClicked));
			ClickedProperty->RemoveDelegate(Delegate, Frame);
		}
		bBoundToFrame = false;
	}
	Super::NativeDestruct();
}

void UAMAutomationTab::OnWindowTabButtonClicked(int32 ButtonIndex)
{
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: tab button %d clicked"), ButtonIndex);
	if (ButtonIndex == TabIndex)
	{
		ShowThisTab();
	}
}

void UAMAutomationTab::ShowThisTab()
{
	UObject* SlidingTabs = AMAT_GetObject(GetHostWindow(), AMAT_SlidingTabsProperty);
	if (!SlidingTabs || TabIndex == INDEX_NONE)
	{
		return;
	}

	FAMATIndexOffsets Offsets(SlidingTabs);
	double Offset = 0.0;
	if (!Offsets.Find(TabIndex, Offset))
	{
		// The pages' offsets were worked out before our page existed: work them out again.
		AMAT_CallNoParams(SlidingTabs, TEXT("SetupIndexOffset"));
	}
	if (!Offsets.Find(TabIndex, Offset))
	{
		// Still missing: our page starts where the previous one ends.
		double PreviousOffset = 0.0;
		const UPanelWidget* Pages = Cast<UPanelWidget>(AMAT_GetObject(SlidingTabs, AMAT_PagesProperty));
		const UWidget* PreviousPage = Pages ? Pages->GetChildAt(TabIndex - 1) : nullptr;
		if (PreviousPage && Offsets.Find(TabIndex - 1, PreviousOffset))
		{
			Offsets.Set(TabIndex, PreviousOffset - PreviousPage->GetDesiredSize().X);
		}
	}

	int32 ActiveIndex = INDEX_NONE;
	AMAT_GetInt(SlidingTabs, AMAT_ActiveIndexProperty, ActiveIndex);
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: offsets %s, active page %d"), *Offsets.Describe(), ActiveIndex);

	if (ActiveIndex != TabIndex)
	{
		const int32 Index = TabIndex;
		AMAT_Call(SlidingTabs, TEXT("SetActiveIndex"), [Index](UFunction* Function, uint8* Params)
		{
			if (const FIntProperty* IndexParam = CastField<FIntProperty>(Function->FindPropertyByName(TEXT("Index"))))
			{
				IndexParam->SetPropertyValue_InContainer(Params, Index);
			}
			if (const FBoolProperty* AnimateParam = CastField<FBoolProperty>(Function->FindPropertyByName(TEXT("Animate"))))
			{
				AnimateParam->SetPropertyValue_InContainer(Params, true);
			}
		});
		AMAT_GetInt(SlidingTabs, AMAT_ActiveIndexProperty, ActiveIndex);
		UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: slid to page %d"), ActiveIndex);
	}
}
