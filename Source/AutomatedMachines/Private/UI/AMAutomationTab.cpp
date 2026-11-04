#include "UI/AMAutomationTab.h"
#include "AutomatedMachinesModule.h"
#include "UI/AMRuleRow.h"
#include "UI/AMUIHelpers.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Net/AMRemoteCallObject.h"
#include "Buildables/FGBuildable.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
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
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FName AMAT_FrameProperty(TEXT("Widget_Window_DarkMode"));
	const FName AMAT_SlidingTabsProperty(TEXT("Widget_SlidingTabs"));
	const FName AMAT_TabsProperty(TEXT("mTabs"));
	const FName AMAT_TabClickedProperty(TEXT("OnTabButtonClicked"));
	const FName AMAT_MachineProperty(TEXT("mInteractObject"));
	const FName AMAT_MachineNameProperty(TEXT("mDisplayName"));
	const FName AMAT_TabsContainerProperty(TEXT("Widget_TabsContainer"));
	const FName AMAT_ButtonsPanelProperty(TEXT("mContainer"));
	const FName AMAT_IndexOffsetProperty(TEXT("mIndexOffset"));
	const FName AMAT_ActiveIndexProperty(TEXT("mActiveIndex"));
	const FName AMAT_PagesProperty(TEXT("mHorizontalBox"));
	const FName AMAT_FontSourceWidget(TEXT("mRecipeName"));

	const TCHAR* const AMAT_TabTitle = TEXT("Automation");


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

void UAMAutomationTab::TrackText(UTextBlock* Text, int32 FontSize)
{
	if (Text)
	{
		Text->SetFont(AMUI::SizedFont(GetGameFont(), FontSize));
		mTexts.Add(Text);
		mTextSizes.Add(FontSize);
	}
}

FSlateFontInfo UAMAutomationTab::GetGameFont() const
{
	return bHasGameFont ? mGameFont : GetDefault<UTextBlock>()->GetFont();
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
	Root->SetMaxDesiredHeight(680.f);
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AMBackground"));
	Background->SetBrushColor(AMUI::PageBackground);
	Background->SetPadding(FMargin(28.f, 24.f));
	Root->AddChild(Background);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("AMScroll"));
	Background->AddChild(Scroll);

	mContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AMContent"));
	Scroll->AddChild(mContentBox);

	auto AddRow = [this](UWidget* Widget, float TopPadding)
	{
		if (UVerticalBoxSlot* RowSlot = mContentBox->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
			RowSlot->SetHorizontalAlignment(HAlign_Left);
		}
	};
	auto AddButton = [this, &AddRow](const FString& Label, float TopPadding) -> UButton*
	{
		UButton* Button = AMUI::MakeButton(WidgetTree, Label, GetGameFont(), false, nullptr);
		TrackText(Cast<UTextBlock>(Button->GetChildAt(0)), 14);
		AddRow(Button, TopPadding);
		return Button;
	};

	AddRow(MakeText(TEXT("AUTOMATION"), 26, AMUI::Orange), 0.f);
	mMachineText = MakeText(TEXT(""), 16, AMUI::Grey);
	AddRow(mMachineText, 4.f);

	mNoticeText = MakeText(TEXT(""), 14, AMUI::Orange);
	mNoticeText->SetVisibility(ESlateVisibility::Collapsed);
	AddRow(mNoticeText, 12.f);

	AddRow(MakeText(TEXT("ALARMS"), 18, AMUI::White), 26.f);
	AddRow(MakeText(TEXT("This machine rings an alarm when its status changes."), 13, AMUI::Grey), 4.f);
	mFireRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AMFireRows"));
	AddRow(mFireRowsBox, 6.f);
	AddButton(TEXT("+ Add alarm"), 10.f)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnAddFireClicked);

	AddRow(MakeText(TEXT("ACTIONS"), 18, AMUI::White), 30.f);
	AddRow(MakeText(TEXT("This machine does something when an alarm rings, on any machine."), 13, AMUI::Grey), 4.f);
	mReactionRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AMReactionRows"));
	AddRow(mReactionRowsBox, 6.f);
	AddButton(TEXT("+ Add action"), 10.f)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnAddReactionClicked);
}

void UAMAutomationTab::LoadRules()
{
	if (bRulesLoaded)
	{
		return;
	}
	AFGBuildable* Machine = Cast<AFGBuildable>(GetMachine());
	if (!Machine)
	{
		return;
	}
	const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this);
	if (!Subsystem)
	{
		mNoticeText->SetText(FText::FromString(TEXT("Automation is not available right now (the mod's save object is missing).")));
		mNoticeText->SetVisibility(ESlateVisibility::Visible);
		return;
	}
	mNoticeText->SetVisibility(ESlateVisibility::Collapsed);
	if (const FAMMachineRules* Rules = Subsystem->FindRules(Machine))
	{
		mFires = Rules->Fires;
		mReactions = Rules->Reactions;
	}
	bRulesLoaded = true;
	RebuildRows();
}

void UAMAutomationTab::RebuildRows()
{
	if (!mFireRowsBox || !mReactionRowsBox)
	{
		return;
	}
	mFireRowsBox->ClearChildren();
	mReactionRowsBox->ClearChildren();
	mRows.Reset();

	auto AddRowWidget = [](UVerticalBox* Box, UWidget* Widget)
	{
		if (UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			RowSlot->SetHorizontalAlignment(HAlign_Left);
		}
	};
	const FSlateFontInfo HintFont = AMUI::SizedFont(GetGameFont(), 13);

	for (int32 Index = 0; Index < mFires.Num(); ++Index)
	{
		UAMRuleRow* Row = NewObject<UAMRuleRow>(this);
		Row->Build(this, true, Index);
		mRows.Add(Row);
		AddRowWidget(mFireRowsBox, Row->GetRootWidget());
	}
	if (mFires.Num() == 0)
	{
		AddRowWidget(mFireRowsBox, AMUI::MakeText(WidgetTree, TEXT("No alarms yet."), HintFont, AMUI::Grey));
	}

	for (int32 Index = 0; Index < mReactions.Num(); ++Index)
	{
		UAMRuleRow* Row = NewObject<UAMRuleRow>(this);
		Row->Build(this, false, Index);
		mRows.Add(Row);
		AddRowWidget(mReactionRowsBox, Row->GetRootWidget());
	}
	if (mReactions.Num() == 0)
	{
		AddRowWidget(mReactionRowsBox, AMUI::MakeText(WidgetTree, TEXT("No actions yet."), HintFont, AMUI::Grey));
	}
}

TArray<FString> UAMAutomationTab::GetEventNames() const
{
	TSet<FString> Names;
	if (const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this))
	{
		Names.Append(Subsystem->GetAllEventNames());
	}
	for (const FAMFireRule& Rule : mFires)
	{
		if (!Rule.Event.IsEmpty())
		{
			Names.Add(Rule.Event);
		}
	}
	for (const FAMReactionRule& Rule : mReactions)
	{
		if (!Rule.Event.IsEmpty())
		{
			Names.Add(Rule.Event);
		}
	}
	TArray<FString> Result = Names.Array();
	Result.Sort();
	return Result;
}

void UAMAutomationTab::OnRulesEdited()
{
	if (AFGBuildable* Machine = Cast<AFGBuildable>(GetMachine()))
	{
		UAMRemoteCallObject::RequestSetRules(GetOwningPlayer(), Machine, mFires, mReactions);
	}
}

void UAMAutomationTab::RemoveRule(bool bFire, int32 Index)
{
	if (bFire && mFires.IsValidIndex(Index))
	{
		mFires.RemoveAt(Index);
	}
	else if (!bFire && mReactions.IsValidIndex(Index))
	{
		mReactions.RemoveAt(Index);
	}
	else
	{
		return;
	}
	OnRulesEdited();
	// Not now: we are inside the click of a button that the rebuild removes.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::RebuildRows));
	}
}

void UAMAutomationTab::OnAddFireClicked()
{
	mFires.AddDefaulted();
	RebuildRows();
}

void UAMAutomationTab::OnAddReactionClicked()
{
	mReactions.AddDefaulted();
	RebuildRows();
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

UObject* UAMAutomationTab::GetMachine() const
{
	return AMAT_GetObject(GetHostWindow(), AMAT_MachineProperty);
}

void UAMAutomationTab::UpdateMachineName()
{
	const UObject* Machine = GetMachine();
	const FTextProperty* NameProperty = Machine ? FindFProperty<FTextProperty>(Machine->GetClass(), AMAT_MachineNameProperty) : nullptr;
	if (mMachineText && NameProperty)
	{
		mMachineText->SetText(NameProperty->GetPropertyValue_InContainer(Machine));
	}
}

void UAMAutomationTab::EnsureTabButtonExists()
{
	// Also a good moment to read the machine: the window has it by now.
	UpdateMachineName();
	LoadRules();

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
	mGameFont = Source->GetFont();
	bHasGameFont = true;
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
	// The tab entry is added in NativeConstruct: while the window is still being initialized, its
	// variables (the frame) are not set yet.
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

	if (TabIndex == INDEX_NONE)
	{
		AddTabEntry();
	}

	// The frame may make its buttons after us (then it picks up our entry by itself) or may have made
	// them already: check one frame later, when all of the window has been constructed.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::EnsureTabButtonExists));
	}

	ApplyGameFonts();
	UpdateMachineName();
	LoadRules();
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
