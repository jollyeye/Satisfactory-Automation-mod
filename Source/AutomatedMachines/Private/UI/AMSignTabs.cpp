#include "UI/AMSignTabs.h"
#include "AutomatedMachinesModule.h"
#include "UI/AMUIHelpers.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Automation/AMSign.h"
#include "Net/AMRemoteCallObject.h"
#include "Buildables/FGBuildableWidgetSign.h"
#include "FGSignTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* const AMST_FrameClass = TEXT("Widget_Window_DarkMode_C");
	const FName AMST_TabsProperty(TEXT("mTabs"));
	const FName AMST_TabsContainerProperty(TEXT("Widget_TabsContainer"));
	const FName AMST_ButtonsPanelProperty(TEXT("mContainer"));
	const FName AMST_SignProperty(TEXT("mSign"));
	const FName AMST_SignDataProperty(TEXT("mSignData"));
	const FName AMST_SetSignDataFunction(TEXT("Set Sign Data"));
	const TCHAR* const AMST_AddTitle = TEXT("+ AutoDesign");
	/** The sign designer open on this game instance (there is one window at a time). */
	TWeakObjectPtr<UAMSignTabs> AMST_Open;

	const TCHAR* const AMST_TabIconPath = TEXT("/AutomatedMachines/Icons/T_AM_TabIcon.T_AM_TabIcon");

	UObject* AMST_GetObject(const UObject* Owner, FName Name)
	{
		const FObjectPropertyBase* Property = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name) : nullptr;
		return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
	}

	bool AMST_Call(UObject* Target, FName FunctionName, TFunctionRef<void(UFunction*, uint8*)> Fill)
	{
		UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
		if (!Function)
		{
			UE_LOG(LogAutomatedMachines, Warning, TEXT("Sign designs: function %s not found on %s"),
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

	void AMST_CallNoParams(UObject* Target, FName FunctionName)
	{
		AMST_Call(Target, FunctionName, [](UFunction*, uint8*) {});
	}

	/** Sets the first parameter of the given field type (FText for SetTitle, an object for SetIcon / SetActiveButton). */
	template <typename TProperty, typename TValue>
	void AMST_CallWith(UObject* Target, FName FunctionName, const TValue& Value)
	{
		AMST_Call(Target, FunctionName, [&Value](UFunction* Function, uint8* Params)
		{
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (const TProperty* Param = CastField<TProperty>(*It))
				{
					Param->SetPropertyValue_InContainer(Params, Value);
					break;
				}
			}
		});
	}

	void AMST_CallWithObject(UObject* Target, FName FunctionName, UObject* Value)
	{
		AMST_Call(Target, FunctionName, [Value](UFunction* Function, uint8* Params)
		{
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (const FObjectPropertyBase* Param = CastField<FObjectPropertyBase>(*It))
				{
					Param->SetObjectPropertyValue_InContainer(Params, Value);
					break;
				}
			}
		});
	}

	FTextProperty* AMST_FindTextField(const UScriptStruct* Struct)
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
}

void UAMSignTabHandler::OnTabClicked()
{
	if (Owner)
	{
		Owner->SelectTab(Index);
	}
}

// These change the tab buttons or close the picker: on the next tick, not inside their own click.
void UAMSignTabHandler::OnRemoveClicked()
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (World)
	{
		TWeakObjectPtr<UAMSignTabs> WeakOwner(Owner);
		const int32 RemoveIndex = Index;
		World->GetTimerManager().SetTimerForNextTick([WeakOwner, RemoveIndex]()
		{
			if (UAMSignTabs* Tabs = WeakOwner.Get())
			{
				Tabs->RemoveDesign(RemoveIndex);
			}
		});
	}
}

void UAMSignTabHandler::OnAlarmChosen()
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (World)
	{
		TWeakObjectPtr<UAMSignTabs> WeakOwner(Owner);
		const FString Chosen = Alarm;
		World->GetTimerManager().SetTimerForNextTick([WeakOwner, Chosen]()
		{
			if (UAMSignTabs* Tabs = WeakOwner.Get())
			{
				Tabs->AddDesign(Chosen);
			}
		});
	}
}

void UAMSignTabs::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		mLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AMSignLayer"));
		mLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = mLayer;
	}
	// Visible but click-through (empty unless the picker is open): a collapsed widget has no size,
	// and the picker is placed from this widget's geometry.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UAMSignTabs::NativeConstruct()
{
	Super::NativeConstruct();
	bSetUp = false;
	mSetupTries = 0;
	// Over the whole page (for the picker), but hidden.
	if (UCanvasPanelSlot* PageSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		PageSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		PageSlot->SetOffsets(FMargin(0.f));
		PageSlot->SetZOrder(1000);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMSignTabs::Setup));
		World->GetTimerManager().SetTimer(mWatchTimer, this, &UAMSignTabs::Watch, 0.25f, true);
	}
}

void UAMSignTabs::Watch()
{
	UpdateListHeight();
	if (bSetUp && GetSign() != mSetupSign.Get())
	{
		// The window now edits another sign: its designs, not the last sign's.
		UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: the window shows another sign, setting up again"));
		bSetUp = false;
		mSetupTries = 0;
		Setup();
	}
}

void UAMSignTabs::RestoreWindowTabs()
{
	UObject* Frame = GetWindowFrame();
	FArrayProperty* TabsProperty = Frame ? FindFProperty<FArrayProperty>(Frame->GetClass(), AMST_TabsProperty) : nullptr;
	if (TabsProperty)
	{
		FScriptArrayHelper Tabs(TabsProperty, TabsProperty->ContainerPtrToValuePtr<void>(Frame));
		if (Tabs.Num() > 1)
		{
			Tabs.Resize(1);
		}
	}
	mBoundButtons.Reset();
}

void UAMSignTabs::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(mWatchTimer);
	}
	ClosePicker();
	if (bSetUp && GetSign() == mSetupSign.Get())
	{
		CaptureEditor();
		if (mDesigns.Num() > 0 || bHadDesigns)
		{
			SendDesigns();
		}
	}
	RestoreWindowTabs();
	// Our list out of the window (it may be reused for another building).
	if (mSidePanel)
	{
		mSidePanel->RemoveFromParent();
	}
	bSetUp = false;
	mSetupSign = nullptr;
	if (AMST_Open.Get() == this)
	{
		AMST_Open = nullptr;
	}
	Super::NativeDestruct();
}

bool UAMSignTabs::CopyOpenEditor(const AFGBuildableWidgetSign* Sign, UObject* ClipboardSettings)
{
	const UAMSignTabs* Open = AMST_Open.Get();
	if (!Open || !Open->bSetUp || !Sign || Open->mSetupSign.Get() != Sign || !ClipboardSettings)
	{
		return false;
	}
	const UUserWidget* Host = Open->GetHostWindow();
	const FStructProperty* EditorData = Host ? FindFProperty<FStructProperty>(Host->GetClass(), AMST_SignDataProperty) : nullptr;
	const FStructProperty* CopiedData = FindFProperty<FStructProperty>(ClipboardSettings->GetClass(), TEXT("mPrefabSignData"));
	if (!EditorData || !CopiedData || EditorData->Struct != CopiedData->Struct)
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Sign Copy Settings: the editor's design could not be copied (no mSignData / mPrefabSignData)"));
		return false;
	}
	EditorData->Struct->CopyScriptStruct(CopiedData->ContainerPtrToValuePtr<void>(ClipboardSettings), EditorData->ContainerPtrToValuePtr<void>(Host));
	UE_LOG(LogAutomatedMachines, Log, TEXT("Sign Copy Settings: copied the design open in the editor (tab %d)"), Open->mSelected);
	return true;
}

namespace
{
	/** "3 texts: Title='IRON' Line1='' ..." of a sign design, for the log. */
	FString AMST_DescribeTexts(const FPrefabSignData& Data)
	{
		FString Out = FString::Printf(TEXT("%d texts:"), Data.TextElementData.Num());
		for (const TPair<FString, FString>& Text : Data.TextElementData)
		{
			Out += FString::Printf(TEXT(" %s='%s'"), *Text.Key, *Text.Value.Left(24));
		}
		return Out;
	}

	/** The sign clipboard objects (with mPrefabSignData) in a player state's factory clipboard. */
	TArray<UObject*> AMST_SignClipboards(UObject* PlayerState)
	{
		TArray<UObject*> Found;
		const FMapProperty* MapProperty = PlayerState ? FindFProperty<FMapProperty>(PlayerState->GetClass(), TEXT("mFactoryClipboard")) : nullptr;
		const FObjectPropertyBase* ValueProperty = MapProperty ? CastField<FObjectPropertyBase>(MapProperty->ValueProp) : nullptr;
		if (!ValueProperty)
		{
			return Found;
		}
		FScriptMapHelper Map(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(PlayerState));
		for (int32 Index = 0; Index < Map.GetMaxIndex(); ++Index)
		{
			if (!Map.IsValidIndex(Index))
			{
				continue;
			}
			UObject* Value = ValueProperty->GetObjectPropertyValue(Map.GetValuePtr(Index));
			const FStructProperty* DataProperty = Value ? FindFProperty<FStructProperty>(Value->GetClass(), TEXT("mPrefabSignData")) : nullptr;
			if (DataProperty && DataProperty->Struct == FPrefabSignData::StaticStruct())
			{
				Found.Add(Value);
			}
		}
		return Found;
	}

	const FPrefabSignData* AMST_ClipboardData(const UObject* Clipboard)
	{
		const FStructProperty* DataProperty = Clipboard ? FindFProperty<FStructProperty>(Clipboard->GetClass(), TEXT("mPrefabSignData")) : nullptr;
		return DataProperty && DataProperty->Struct == FPrefabSignData::StaticStruct() ? DataProperty->ContainerPtrToValuePtr<FPrefabSignData>(Clipboard) : nullptr;
	}
}

void UAMSignTabs::OnGameClipboardCopied(UObject* Object, UObject* PlayerState)
{
	const AFGBuildableWidgetSign* Sign = Cast<AFGBuildableWidgetSign>(Object);
	if (!Sign)
	{
		return;
	}
	for (UObject* Clipboard : AMST_SignClipboards(PlayerState))
	{
		if (const FPrefabSignData* Copied = AMST_ClipboardData(Clipboard))
		{
			UE_LOG(LogAutomatedMachines, Log, TEXT("Sign copied by the game (%s): %s"), *Sign->GetName(), *AMST_DescribeTexts(*Copied));
		}
		if (CopyOpenEditor(Sign, Clipboard))
		{
			if (const FPrefabSignData* Fixed = AMST_ClipboardData(Clipboard))
			{
				UE_LOG(LogAutomatedMachines, Log, TEXT("  now the editor's design: %s"), *AMST_DescribeTexts(*Fixed));
			}
		}
	}
}

void UAMSignTabs::OnGameClipboardPasted(UObject* Object, UObject* PlayerState)
{
	const AFGBuildableWidgetSign* Sign = Cast<AFGBuildableWidgetSign>(Object);
	if (!Sign)
	{
		return;
	}
	for (UObject* Clipboard : AMST_SignClipboards(PlayerState))
	{
		if (const FPrefabSignData* Pasted = AMST_ClipboardData(Clipboard))
		{
			UE_LOG(LogAutomatedMachines, Log, TEXT("Sign pasted by the game onto %s: %s"), *Sign->GetName(), *AMST_DescribeTexts(*Pasted));
		}
	}
	// What the editor holds a moment later (the window loads the pasted design itself).
	UAMSignTabs* Open = AMST_Open.Get();
	UWorld* World = Open ? Open->GetWorld() : nullptr;
	if (World && Open->mSetupSign.Get() == Sign)
	{
		TWeakObjectPtr<UAMSignTabs> WeakOpen = Open;
		FTimerHandle Later;
		World->GetTimerManager().SetTimer(Later, FTimerDelegate::CreateLambda([WeakOpen]()
		{
			const UAMSignTabs* Tabs = WeakOpen.Get();
			const UUserWidget* Host = Tabs ? Tabs->GetHostWindow() : nullptr;
			const FStructProperty* DataProperty = Host ? FindFProperty<FStructProperty>(Host->GetClass(), AMST_SignDataProperty) : nullptr;
			if (DataProperty && DataProperty->Struct == FPrefabSignData::StaticStruct())
			{
				UE_LOG(LogAutomatedMachines, Log, TEXT("  the editor after the paste (tab %d): %s"), Tabs->mSelected,
					*AMST_DescribeTexts(*DataProperty->ContainerPtrToValuePtr<FPrefabSignData>(Host)));
			}
		}), 0.2f, false);
	}
}

UUserWidget* UAMSignTabs::GetHostWindow() const
{
	if (const UWidgetTree* Tree = Cast<UWidgetTree>(GetOuter()))
	{
		if (UUserWidget* Host = Cast<UUserWidget>(Tree->GetOuter()))
		{
			return Host;
		}
	}
	return GetTypedOuter<UUserWidget>();
}

UObject* UAMSignTabs::GetWindowFrame() const
{
	const UUserWidget* Host = GetHostWindow();
	if (!Host)
	{
		return nullptr;
	}
	for (TFieldIterator<FObjectPropertyBase> It(Host->GetClass()); It; ++It)
	{
		UObject* Value = It->GetObjectPropertyValue_InContainer(Host);
		if (Value && Value->GetClass()->GetName() == AMST_FrameClass)
		{
			return Value;
		}
	}
	return nullptr;
}

AFGBuildableWidgetSign* UAMSignTabs::GetSign() const
{
	return Cast<AFGBuildableWidgetSign>(AMST_GetObject(GetHostWindow(), AMST_SignProperty));
}

bool UAMSignTabs::ReadEditor(FAMSignDesign& OutDesign, const FString& Event) const
{
	const UUserWidget* Host = GetHostWindow();
	const FStructProperty* DataProperty = Host ? FindFProperty<FStructProperty>(Host->GetClass(), AMST_SignDataProperty) : nullptr;
	if (!DataProperty || DataProperty->Struct != FPrefabSignData::StaticStruct())
	{
		return false;
	}
	OutDesign = AMSign::FromSignData(*DataProperty->ContainerPtrToValuePtr<FPrefabSignData>(Host), Event);
	return true;
}

void UAMSignTabs::LoadEditor(const FAMSignDesign& Design)
{
	UUserWidget* Host = GetHostWindow();
	const FStructProperty* DataProperty = Host ? FindFProperty<FStructProperty>(Host->GetClass(), AMST_SignDataProperty) : nullptr;
	if (!DataProperty || DataProperty->Struct != FPrefabSignData::StaticStruct())
	{
		return;
	}
	// Start from what the window has (keeps fields our design does not hold).
	FPrefabSignData Data = *DataProperty->ContainerPtrToValuePtr<FPrefabSignData>(Host);
	AMSign::ToSignData(Design, Data);
	AMST_Call(Host, AMST_SetSignDataFunction, [&Data](UFunction* Function, uint8* Params)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (const FStructProperty* Param = CastField<FStructProperty>(*It))
			{
				if (Param->Struct == FPrefabSignData::StaticStruct())
				{
					Param->CopyCompleteValue(Param->ContainerPtrToValuePtr<void>(Params), &Data);
					break;
				}
			}
		}
	});
}

FAMSignDesign& UAMSignTabs::GetSlot(int32 Index)
{
	return mDesigns.IsValidIndex(Index - 1) ? mDesigns[Index - 1] : mNormal;
}

void UAMSignTabs::CaptureEditor()
{
	FAMSignDesign& Current = GetSlot(mSelected);
	FAMSignDesign Edited;
	if (ReadEditor(Edited, Current.Event))
	{
		Current = Edited;
	}
}

void UAMSignTabs::SendDesigns()
{
	if (AFGBuildableWidgetSign* Sign = mSetupSign.Get())
	{
		UAMRemoteCallObject::RequestSetSignDesigns(GetOwningPlayer(), Sign, mNormal, mDesigns);
		bHadDesigns = mDesigns.Num() > 0;
	}
}

FSlateFontInfo UAMSignTabs::GetFont() const
{
	UObject* TabsContainer = AMST_GetObject(GetWindowFrame(), AMST_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMST_GetObject(TabsContainer, AMST_ButtonsPanelProperty));
	const UUserWidget* FirstButton = Buttons && Buttons->GetChildrenCount() > 0 ? Cast<UUserWidget>(Buttons->GetChildAt(0)) : nullptr;
	const UTextBlock* Title = FirstButton && FirstButton->WidgetTree ? Cast<UTextBlock>(FirstButton->WidgetTree->FindWidget(TEXT("mTitleObject"))) : nullptr;
	return Title ? Title->GetFont() : GetDefault<UTextBlock>()->GetFont();
}

void UAMSignTabs::Setup()
{
	UObject* Frame = GetWindowFrame();
	AFGBuildableWidgetSign* Sign = GetSign();
	FArrayProperty* TabsProperty = Frame ? FindFProperty<FArrayProperty>(Frame->GetClass(), AMST_TabsProperty) : nullptr;
	const bool bHasTabs = TabsProperty && FScriptArrayHelper(TabsProperty, TabsProperty->ContainerPtrToValuePtr<void>(Frame)).Num() > 0;
	if (!Sign || !bHasTabs)
	{
		// The window fills its tab list in its own construct: again next tick.
		if (++mSetupTries < 20)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMSignTabs::Setup));
			}
		}
		else
		{
			UE_LOG(LogAutomatedMachines, Warning, TEXT("Sign designs: no sign or no tab list in the window"));
		}
		return;
	}

	// The designs: ours if the sign has any, else its current design is the normal one.
	mDesigns.Reset();
	mSelected = 0;
	const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this);
	const FAMMachineRules* Rules = Subsystem ? Subsystem->FindRules(Sign) : nullptr;
	if (Rules && Rules->SignDesigns.Num() > 0)
	{
		mNormal = Rules->SignNormal;
		mDesigns = Rules->SignDesigns;
		// The editor shows what the sign shows: select that tab.
		const int32 Showing = mDesigns.IndexOfByPredicate([Rules](const FAMSignDesign& D) { return D.Event == Rules->SignShowing; });
		mSelected = Showing == INDEX_NONE ? 0 : Showing + 1;
		bHadDesigns = true;
	}
	else
	{
		ReadEditor(mNormal, FString());
		bHadDesigns = false;
	}
	bSetUp = true;
	mSetupSign = Sign;
	AMST_Open = this;
	RebuildTabs();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs for %s: %d alarm designs, tab %d selected"), *Sign->GetName(), mDesigns.Num(), mSelected);
}

void UAMSignTabs::RebuildTabs()
{
	// Our list on the left of the designer, in the window's body row (the game's tab buttons are
	// made again and reused by the window, and a row in the title bar crowded it).
	UObject* Frame = GetWindowFrame();
	const UWidget* Body = Cast<UWidget>(AMST_GetObject(Frame, TEXT("WindowBody")));
	UHorizontalBox* BodyRow = Body ? Cast<UHorizontalBox>(Body->GetParent()) : nullptr;
	if (!BodyRow || !WidgetTree)
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Sign designs: no window body row to put the list in"));
		return;
	}
	constexpr float ListWidth = 250.f;
	constexpr float RowHeight = 36.f;
	if (!mSidePanel)
	{
		mSidePanel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AMSignDesignList"));
		mSidePanel->SetWidthOverride(ListWidth);
	}
	if (mSidePanel->GetParent() != BodyRow)
	{
		mSidePanel->RemoveFromParent();
		if (UHorizontalBoxSlot* ListSlot = Cast<UHorizontalBoxSlot>(BodyRow->InsertChildAt(0, mSidePanel)))
		{
			ListSlot->SetVerticalAlignment(VAlign_Fill);
			ListSlot->SetHorizontalAlignment(HAlign_Fill);
			ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
	if (mTabScroll)
	{
		mTabScrollOffset = mTabScroll->GetScrollOffset();
	}
	mSidePanel->ClearChildren();
	mHandlers.Reset();

	const FSlateFontInfo Font = GetFont();
	UBorder* Panel = AMUI::MakeBox(WidgetTree, AMUI::SidePanel, FMargin(8.f));
	mSidePanel->AddChild(Panel);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->AddChild(Column);
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(AMUI::MakeText(WidgetTree, TEXT("Designs"), AMUI::SizedFont(Font, 15), AMUI::White)))
	{
		TitleSlot->SetPadding(FMargin(4.f, 2.f, 0.f, 8.f));
	}

	mTabScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	mTabScroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	mTabScroll->AddChild(Rows);
	if (UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(mTabScroll))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UWidget* SelectedRow = nullptr;

	// One row: [icon] [name] [bin], the bin a button of its own next to the row's button.
	auto AddRow = [&](int32 Index, const FString& Title, bool bDesign)
	{
		UAMSignTabHandler* Handler = NewObject<UAMSignTabHandler>(this);
		Handler->Owner = this;
		Handler->Index = Index;
		mHandlers.Add(Handler);
		const bool bSelected = Index == mSelected;
		const FLinearColor Colour = bSelected ? AMUI::Orange : FLinearColor(FColor(87, 87, 87));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Label = nullptr;
		UButton* RowButton = AMUI::MakeButton(WidgetTree, Title, Font, Colour, &Label);
		RowButton->OnClicked.AddDynamic(Handler, &UAMSignTabHandler::OnTabClicked);
		if (Label)
		{
			Label->SetColorAndOpacity(FSlateColor(AMUI::White));
		}
		if (bDesign && Label)
		{
			// The design's alarm with the Wi-Fi icon in front.
			UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UImage* IconImage = AMUI::MakeImage(WidgetTree, AMST_TabIconPath, FVector2D(18.f, 18.f));
			if (UHorizontalBoxSlot* IconSlot = Content->AddChildToHorizontalBox(IconImage))
			{
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			Label->RemoveFromParent();
			if (UHorizontalBoxSlot* LabelSlot = Content->AddChildToHorizontalBox(Label))
			{
				LabelSlot->SetVerticalAlignment(VAlign_Center);
				LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(RowButton->AddChild(Content)))
			{
				ContentSlot->SetHorizontalAlignment(HAlign_Fill);
				ContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
		else if (UButtonSlot* LabelSlot = Label ? Cast<UButtonSlot>(Label->Slot) : nullptr)
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Left);
		}
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ButtonSize->SetHeightOverride(RowHeight);
		// A long name stops at the row's edge, before the bin (clipping the row, not the text:
		// the text's own box is shorter than its letters and cut them at the bottom).
		ButtonSize->SetClipping(EWidgetClipping::ClipToBounds);
		ButtonSize->AddChild(RowButton);
		if (UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(ButtonSize))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (bDesign)
		{
			UButton* Remove = AMUI::MakeRemoveButton(WidgetTree, Font);
			AMUI::SetButtonColor(Remove, Colour);
			Remove->OnClicked.AddDynamic(Handler, &UAMSignTabHandler::OnRemoveClicked);
			USizeBox* RemoveSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			RemoveSize->SetWidthOverride(RowHeight + 10.f);
			RemoveSize->SetHeightOverride(RowHeight);
			RemoveSize->AddChild(Remove);
			if (UHorizontalBoxSlot* RemoveSlot = Row->AddChildToHorizontalBox(RemoveSize))
			{
				RemoveSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
			}
		}
		if (UVerticalBoxSlot* RowSlot = Rows->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, Index == 0 ? 0.f : 4.f, 0.f, 0.f));
		}
		if (bSelected)
		{
			SelectedRow = Row;
		}
	};

	AddRow(0, TEXT("Default"), false);
	for (int32 Index = 0; Index < mDesigns.Num(); ++Index)
	{
		AddRow(Index + 1, mDesigns[Index].Event, true);
	}

	// "+ AutoDesign" under the list (its handler's index is one past the designs: SelectTab opens the picker).
	UAMSignTabHandler* AddHandler = NewObject<UAMSignTabHandler>(this);
	AddHandler->Owner = this;
	AddHandler->Index = mDesigns.Num() + 1;
	mHandlers.Add(AddHandler);
	mAddButton = AMUI::MakeButton(WidgetTree, AMST_AddTitle, Font, AMUI::Button, nullptr);
	mAddButton->OnClicked.AddDynamic(AddHandler, &UAMSignTabHandler::OnTabClicked);
	USizeBox* AddSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	AddSize->SetHeightOverride(RowHeight);
	AddSize->AddChild(mAddButton);
	if (UVerticalBoxSlot* AddSlot = Column->AddChildToVerticalBox(AddSize))
	{
		AddSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	UpdateListHeight();

	// Where it was before, and the selected row in view.
	mTabScroll->SetScrollOffset(mTabScrollOffset);
	if (SelectedRow && mSelected > 0)
	{
		mTabScroll->ScrollWidgetIntoView(SelectedRow, true, EDescendantScrollDestination::IntoView);
	}
}

void UAMSignTabs::SelectTab(int32 Index)
{
	UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: tab %d clicked"), Index);
	if (Index == mDesigns.Num() + 1)
	{
		// "+ AutoDesign" opens the picker under it; pressed again ("Cancel"), closes it.
		if (bPickerOpen)
		{
			ClosePicker();
		}
		else
		{
			OpenPicker();
		}
		return;
	}
	if (Index < 0 || Index > mDesigns.Num())
	{
		return;
	}
	if (Index != mSelected)
	{
		CaptureEditor();
		mSelected = Index;
		LoadEditor(GetSlot(Index));
		// The highlight (orange) moves: on the next tick, not inside the click.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMSignTabs::RebuildTabs));
		}
	}
}

void UAMSignTabs::AddDesign(const FString& Alarm)
{
	ClosePicker();
	const FString Event = AMRules::NormalizeEventName(Alarm);
	if (Event.IsEmpty())
	{
		RebuildTabs();
		return;
	}
	CaptureEditor();
	const int32 Existing = mDesigns.IndexOfByPredicate([&Event](const FAMSignDesign& D) { return D.Event == Event; });
	if (Existing != INDEX_NONE)
	{
		SelectTab(Existing + 1);
		RebuildTabs();
		return;
	}
	// Starts as a copy of the design being edited.
	FAMSignDesign Design = GetSlot(mSelected);
	Design.Event = Event;
	mDesigns.Add(Design);
	mSelected = mDesigns.Num();
	RebuildTabs();
	SendDesigns();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: added %s"), *Event);
}

void UAMSignTabs::RemoveDesign(int32 Index)
{
	if (!mDesigns.IsValidIndex(Index - 1))
	{
		return;
	}
	CaptureEditor();
	const bool bWasSelected = Index == mSelected;
	mDesigns.RemoveAt(Index - 1);
	if (bWasSelected)
	{
		mSelected = 0;
		LoadEditor(mNormal);
	}
	else if (mSelected > Index)
	{
		--mSelected;
	}
	RebuildTabs();
	SendDesigns();
}

void UAMSignTabs::UpdateListHeight()
{
	const UWidget* Body = Cast<UWidget>(AMST_GetObject(GetWindowFrame(), TEXT("WindowBody")));
	if (!mSidePanel || !Body)
	{
		return;
	}
	// The designer page's own height (without us: we are its neighbour, not inside it).
	const float PageHeight = static_cast<float>(Body->GetDesiredSize().Y);
	if (PageHeight > 100.f)
	{
		if (!FMath::IsNearlyEqual(mSidePanel->GetHeightOverride(), PageHeight, 1.f))
		{
			mSidePanel->SetHeightOverride(PageHeight);
		}
	}
	else if (mSidePanel->GetMaxDesiredHeight() <= 0.f)
	{
		// Not measured yet: at most this high meanwhile.
		mSidePanel->SetMaxDesiredHeight(560.f);
	}
}

void UAMSignTabs::OpenPicker()
{
	ClosePicker();
	if (!mLayer)
	{
		return;
	}
	const FSlateFontInfo Font = GetFont();

	// Alarms without a design on this sign yet.
	mPickerAlarms.Reset();
	if (const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this))
	{
		for (const FString& Alarm : Subsystem->GetAllEventNames())
		{
			if (!Alarm.IsEmpty() && !mDesigns.ContainsByPredicate([&Alarm](const FAMSignDesign& D) { return D.Event == Alarm; }))
			{
				mPickerAlarms.Add(Alarm);
			}
		}
	}

	// A see-through button over the page: a click outside the picker closes it.
	UButton* Background = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Clear;
	Clear.SetNormal(FSlateNoResource());
	Clear.SetHovered(FSlateNoResource());
	Clear.SetPressed(FSlateNoResource());
	Background->SetStyle(Clear);
	Background->OnClicked.AddDynamic(this, &UAMSignTabs::OnPickerBackgroundClicked);
	if (UCanvasPanelSlot* BackgroundSlot = mLayer->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));
	}

	constexpr float Width = 300.f;
	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WindowSize->SetWidthOverride(Width);
	UBorder* Window = AMUI::MakeBox(WidgetTree, AMUI::SidePanel, FMargin(0.f, 0.f, 0.f, 4.f));
	WindowSize->AddChild(Window);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Window->AddChild(Column);

	USizeBox* SearchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SearchSize->SetHeightOverride(36.f);
	UBorder* SearchField = AMUI::MakeBox(WidgetTree, AMUI::Field, FMargin(10.f, 0.f));
	SearchField->SetVerticalAlignment(VAlign_Center);
	AMUI::MarkAsInputBox(SearchField);
	SearchSize->AddChild(SearchField);
	mPickerSearch = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
	mPickerSearch->SetFont(Font);
	mPickerSearch->SetHintText(FText::FromString(TEXT("design for which alarm?")));
	mPickerSearch->OnTextChanged.AddDynamic(this, &UAMSignTabs::OnPickerSearchChanged);
	mPickerSearch->OnTextCommitted.AddDynamic(this, &UAMSignTabs::OnPickerSearchCommitted);
	SearchField->AddChild(mPickerSearch);
	Column->AddChildToVerticalBox(SearchSize);

	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ListSize->SetMaxDesiredHeight(300.f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
	ListSize->AddChild(Scroll);
	mPickerList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(mPickerList);
	if (UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(ListSize))
	{
		ListSlot->SetPadding(FMargin(4.f, 4.f, 4.f, 0.f));
	}
	FillPicker(FString());

	// Floating over the page, its bottom right just left of and level with "+ AutoDesign" (the list
	// is on the page's right), growing upwards. This widget is never collapsed, so its geometry is
	// known (from the frames already drawn).
	FVector2D Position(8.0, 300.0);
	const FGeometry& PageGeometry = GetCachedGeometry();
	const FVector2D PageSize = PageGeometry.GetLocalSize();
	if (mAddButton)
	{
		const FGeometry& AnchorGeometry = mAddButton->GetCachedGeometry();
		const FVector2D AnchorBottom = PageGeometry.AbsoluteToLocal(AnchorGeometry.GetAbsolutePosition() + FVector2D(0.0, AnchorGeometry.GetAbsoluteSize().Y));
		Position.Y = AnchorBottom.Y;
	}
	Position.X = FMath::Max(0.0, PageSize.X - Width - 8.0);
	Position.Y = FMath::Clamp(Position.Y, 0.0, FMath::Max(0.0, PageSize.Y));
	if (UCanvasPanelSlot* WindowSlot = mLayer->AddChildToCanvas(WindowSize))
	{
		WindowSlot->SetAnchors(FAnchors(0.f, 0.f));
		WindowSlot->SetAlignment(FVector2D(0.0, 1.0));
		WindowSlot->SetAutoSize(true);
		WindowSlot->SetPosition(Position);
	}
	bPickerOpen = true;

	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UEditableText> WeakSearch(mPickerSearch);
		World->GetTimerManager().SetTimerForNextTick([WeakSearch]()
		{
			if (UEditableText* Search = WeakSearch.Get())
			{
				Search->SetKeyboardFocus();
			}
		});
	}
}

void UAMSignTabs::FillPicker(const FString& Filter)
{
	if (!mPickerList)
	{
		return;
	}
	mPickerList->ClearChildren();
	mPickerHandlers.Reset();
	const FSlateFontInfo Font = GetFont();
	const FString Typed = AMRules::NormalizeEventName(Filter);
	int32 Shown = 0;
	for (const FString& Alarm : mPickerAlarms)
	{
		if (!Typed.IsEmpty() && !Alarm.Contains(Typed))
		{
			continue;
		}
		++Shown;
		UAMSignTabHandler* Handler = NewObject<UAMSignTabHandler>(this);
		Handler->Owner = this;
		Handler->Alarm = Alarm;
		mPickerHandlers.Add(Handler);
		UTextBlock* Label = nullptr;
		UButton* Option = AMUI::MakeButton(WidgetTree, Alarm, Font, AMUI::Option, &Label);
		if (UButtonSlot* LabelSlot = Label ? Cast<UButtonSlot>(Label->Slot) : nullptr)
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Left);
		}
		Option->OnClicked.AddDynamic(Handler, &UAMSignTabHandler::OnAlarmChosen);
		if (UVerticalBoxSlot* OptionSlot = mPickerList->AddChildToVerticalBox(Option))
		{
			OptionSlot->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
	}
	if (Shown == 0)
	{
		UBorder* Message = AMUI::MakeBox(WidgetTree, AMUI::Option, FMargin(12.f, 7.f));
		Message->AddChild(AMUI::MakeText(WidgetTree, Typed.IsEmpty()
			? FString(TEXT("No alarms yet: type a name and press Enter."))
			: FString::Printf(TEXT("Enter: new design for %s"), *Typed), Font, AMUI::Grey));
		mPickerList->AddChildToVerticalBox(Message);
	}
}

void UAMSignTabs::OnPickerSearchChanged(const FText& Text)
{
	FillPicker(Text.ToString());
}

void UAMSignTabs::OnPickerSearchCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnCleared)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<UAMSignTabs>(this)]()
			{
				if (UAMSignTabs* Tabs = WeakThis.Get())
				{
					Tabs->ClosePicker();
				}
			});
		}
		return;
	}
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}
	// The first alarm shown, or a new one with the typed name (next tick: not inside the commit).
	const FString Typed = AMRules::NormalizeEventName(Text.ToString());
	FString Chosen = Typed;
	for (const FString& Alarm : mPickerAlarms)
	{
		if (Typed.IsEmpty() || Alarm.Contains(Typed))
		{
			Chosen = Alarm;
			break;
		}
	}
	if (Chosen.IsEmpty())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UAMSignTabs> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick([WeakThis, Chosen]()
		{
			if (UAMSignTabs* Tabs = WeakThis.Get())
			{
				Tabs->AddDesign(Chosen);
			}
		});
	}
}

void UAMSignTabs::OnPickerBackgroundClicked()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMSignTabs::ClosePicker));
	}
}

void UAMSignTabs::ClosePicker()
{
	if (mLayer)
	{
		mLayer->ClearChildren();
	}
	bPickerOpen = false;
	mPickerList = nullptr;
	mPickerSearch = nullptr;
	mPickerHandlers.Reset();
}
