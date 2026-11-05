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
#include "Components/PanelWidget.h"
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
	const TCHAR* const AMST_AddTitle = TEXT("+ Alarm");
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
		WidgetTree->RootWidget = mLayer;
	}
	SetVisibility(ESlateVisibility::Collapsed);
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
	if (!bSetUp)
	{
		return;
	}
	if (GetSign() != mSetupSign.Get())
	{
		// The window now edits another sign: its designs, not the last sign's.
		UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: the window shows another sign, setting up again"));
		bSetUp = false;
		mSetupTries = 0;
		Setup();
		return;
	}
	UObject* TabsContainer = AMST_GetObject(GetWindowFrame(), AMST_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMST_GetObject(TabsContainer, AMST_ButtonsPanelProperty));
	if (!Buttons)
	{
		return;
	}
	bool bSame = Buttons->GetChildrenCount() == mBoundButtons.Num();
	for (int32 Index = 0; bSame && Index < mBoundButtons.Num(); ++Index)
	{
		bSame = Buttons->GetChildAt(Index) == mBoundButtons[Index].Get();
	}
	if (!bSame)
	{
		UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: the window made its tab buttons again, ours again"));
		RebuildTabs();
		return;
	}

	// A window with one tab shows its tab bar as a title, not clickable (HitTestInvisible): after
	// reopening it can set that again after our setup. Keep the bar and our buttons clickable.
	auto MakeClickable = [](UWidget* Widget, const TCHAR* What)
	{
		if (Widget && (Widget->GetVisibility() == ESlateVisibility::HitTestInvisible || !Widget->GetIsEnabled()))
		{
			UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: %s %s was not clickable (visibility %d, enabled %d)"),
				What, *Widget->GetName(), static_cast<int32>(Widget->GetVisibility()), Widget->GetIsEnabled() ? 1 : 0);
			Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Widget->SetIsEnabled(true);
		}
	};
	MakeClickable(Cast<UWidget>(TabsContainer), TEXT("tab bar"));
	MakeClickable(const_cast<UPanelWidget*>(Buttons), TEXT("tab button panel"));
	for (const TWeakObjectPtr<UWidget>& Button : mBoundButtons)
	{
		MakeClickable(Button.Get(), TEXT("tab button"));
		if (UUserWidget* ButtonWidget = Cast<UUserWidget>(Button.Get()))
		{
			if (UWidget* Inner = Cast<UWidget>(AMST_GetObject(ButtonWidget, TEXT("mButton"))))
			{
				if (!Inner->GetIsEnabled() || Inner->GetVisibility() != ESlateVisibility::Visible)
				{
					UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: inner button of %s was off (visibility %d, enabled %d)"),
						*ButtonWidget->GetName(), static_cast<int32>(Inner->GetVisibility()), Inner->GetIsEnabled() ? 1 : 0);
					Inner->SetVisibility(ESlateVisibility::Visible);
					Inner->SetIsEnabled(true);
				}
			}
		}
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
	bSetUp = false;
	mSetupSign = nullptr;
	Super::NativeDestruct();
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
	RebuildTabs();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs for %s: %d alarm designs, tab %d selected"), *Sign->GetName(), mDesigns.Num(), mSelected);
}

void UAMSignTabs::RebuildTabs()
{
	UObject* Frame = GetWindowFrame();
	UObject* TabsContainer = AMST_GetObject(Frame, AMST_TabsContainerProperty);
	FArrayProperty* TabsProperty = Frame ? FindFProperty<FArrayProperty>(Frame->GetClass(), AMST_TabsProperty) : nullptr;
	FStructProperty* EntryProperty = TabsProperty ? CastField<FStructProperty>(TabsProperty->Inner) : nullptr;
	FTextProperty* TextField = EntryProperty ? AMST_FindTextField(EntryProperty->Struct) : nullptr;
	if (!TextField || !TabsContainer)
	{
		return;
	}

	// mTabs = [the window's own tab] + one per design + "+ Alarm".
	TArray<FText> Titles;
	{
		FScriptArrayHelper Tabs(TabsProperty, TabsProperty->ContainerPtrToValuePtr<void>(Frame));
		if (Tabs.Num() == 0)
		{
			return;
		}
		if (mWindowTitle.IsEmpty())
		{
			mWindowTitle = TextField->GetPropertyValue_InContainer(Tabs.GetRawPtr(0));
		}
		Tabs.Resize(1);
		Titles.Add(mWindowTitle);
		for (const FAMSignDesign& Design : mDesigns)
		{
			Titles.Add(FText::FromString(Design.Event));
		}
		Titles.Add(FText::FromString(AMST_AddTitle));
		for (int32 Index = 1; Index < Titles.Num(); ++Index)
		{
			const int32 NewIndex = Tabs.AddValue();
			EntryProperty->Struct->CopyScriptStruct(Tabs.GetRawPtr(NewIndex), Tabs.GetRawPtr(0));
			TextField->SetPropertyValue_InContainer(Tabs.GetRawPtr(NewIndex), Titles[Index]);
		}
	}

	// The buttons: made by the tabs container from the list.
	FArrayProperty* ContainerButtons = FindFProperty<FArrayProperty>(TabsContainer->GetClass(), TEXT("mButtons"));
	if (ContainerButtons && TabsProperty->Inner->SameType(ContainerButtons->Inner))
	{
		ContainerButtons->CopyCompleteValue(ContainerButtons->ContainerPtrToValuePtr<void>(TabsContainer), TabsProperty->ContainerPtrToValuePtr<void>(Frame));
		AMST_CallNoParams(TabsContainer, TEXT("GenerateButtons"));
	}
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMST_GetObject(TabsContainer, AMST_ButtonsPanelProperty));
	if (!Buttons)
	{
		return;
	}

	// Side by side; narrower when there are many.
	FVector2D ButtonSize(220.0, 36.0);
	if (const FStructProperty* SizeProperty = FindFProperty<FStructProperty>(TabsContainer->GetClass(), TEXT("mButtonSize")))
	{
		if (SizeProperty->Struct == TBaseStructure<FVector2D>::Get())
		{
			ButtonSize = *SizeProperty->ContainerPtrToValuePtr<FVector2D>(TabsContainer);
		}
	}
	const int32 Count = Buttons->GetChildrenCount();
	if (Count > 4)
	{
		ButtonSize.X = FMath::Max(120.0, ButtonSize.X * 4.0 / Count);
	}

	mHandlers.Reset();
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, AMST_TabIconPath);
	const FSlateFontInfo Font = GetFont();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UWidget* Button = Buttons->GetChildAt(Index);
		if (!Button)
		{
			continue;
		}
		if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Button->Slot))
		{
			ButtonSlot->SetAnchors(FAnchors(0.f, 0.f));
			ButtonSlot->SetAlignment(FVector2D::ZeroVector);
			ButtonSlot->SetAutoSize(false);
			ButtonSlot->SetPosition(FVector2D(ButtonSize.X * Index, 0.0));
			ButtonSlot->SetSize(ButtonSize);
		}
		Button->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (Titles.IsValidIndex(Index))
		{
			AMST_CallWith<FTextProperty>(Button, TEXT("SetTitle"), Titles[Index]);
		}
		const bool bDesignTab = Index > 0 && Index < Count - 1;
		if (bDesignTab && Icon)
		{
			AMST_CallWithObject(Button, TEXT("SetIcon"), Icon);
		}

		UAMSignTabHandler* Handler = NewObject<UAMSignTabHandler>(this);
		Handler->Owner = this;
		Handler->Index = Index;
		mHandlers.Add(Handler);
		if (UButton* Inner = Cast<UButton>(AMST_GetObject(Button, TEXT("mButton"))))
		{
			Inner->OnClicked.AddDynamic(Handler, &UAMSignTabHandler::OnTabClicked);
		}
		// A bin on each design's tab.
		if (bDesignTab)
		{
			if (UHorizontalBox* Content = Cast<UHorizontalBox>(AMST_GetObject(Button, TEXT("mContent"))))
			{
				UButton* Remove = AMUI::MakeRemoveButton(WidgetTree, Font);
				Remove->OnClicked.AddDynamic(Handler, &UAMSignTabHandler::OnRemoveClicked);
				if (UHorizontalBoxSlot* RemoveSlot = Content->AddChildToHorizontalBox(Remove))
				{
					RemoveSlot->SetVerticalAlignment(VAlign_Center);
					RemoveSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 0.f));
				}
			}
		}
	}
	if (UWidget* Selected = Buttons->GetChildAt(mSelected))
	{
		AMST_CallWithObject(TabsContainer, TEXT("SetActiveButton"), Selected);
	}
	mBoundButtons.Reset();
	for (int32 Index = 0; Index < Buttons->GetChildrenCount(); ++Index)
	{
		mBoundButtons.Add(Buttons->GetChildAt(Index));
	}
}

void UAMSignTabs::SelectTab(int32 Index)
{
	UE_LOG(LogAutomatedMachines, Log, TEXT("Sign designs: tab %d clicked"), Index);
	const int32 AddIndex = mDesigns.Num() + 1;
	if (Index == AddIndex)
	{
		UObject* TabsContainer = AMST_GetObject(GetWindowFrame(), AMST_TabsContainerProperty);
		const UPanelWidget* Buttons = Cast<UPanelWidget>(AMST_GetObject(TabsContainer, AMST_ButtonsPanelProperty));
		OpenPicker(Buttons ? Buttons->GetChildAt(Index) : nullptr);
		// Keep the highlight on the tab being edited.
		if (UWidget* Selected = Buttons ? Buttons->GetChildAt(mSelected) : nullptr)
		{
			AMST_CallWithObject(TabsContainer, TEXT("SetActiveButton"), Selected);
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
	}
	UObject* TabsContainer = AMST_GetObject(GetWindowFrame(), AMST_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMST_GetObject(TabsContainer, AMST_ButtonsPanelProperty));
	if (UWidget* Selected = Buttons ? Buttons->GetChildAt(mSelected) : nullptr)
	{
		AMST_CallWithObject(TabsContainer, TEXT("SetActiveButton"), Selected);
	}
}

void UAMSignTabs::AddDesign(const FString& Alarm)
{
	ClosePicker();
	const FString Event = AMRules::NormalizeEventName(Alarm);
	if (Event.IsEmpty())
	{
		return;
	}
	CaptureEditor();
	const int32 Existing = mDesigns.IndexOfByPredicate([&Event](const FAMSignDesign& D) { return D.Event == Event; });
	if (Existing != INDEX_NONE)
	{
		SelectTab(Existing + 1);
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

void UAMSignTabs::OpenPicker(UWidget* Anchor)
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
	SearchSize->AddChild(SearchField);
	mPickerSearch = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
	mPickerSearch->SetFont(Font);
	mPickerSearch->SetHintText(FText::FromString(TEXT("alarm name: pick or type a new one")));
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

	// Under the "+ Alarm" tab, inside the page.
	FVector2D Position(16.0, 8.0);
	if (Anchor)
	{
		const FGeometry& PageGeometry = GetCachedGeometry();
		const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
		const FVector2D AnchorPosition = AnchorGeometry.GetAbsolutePosition();
		const FVector2D AnchorLocal = PageGeometry.AbsoluteToLocal(AnchorPosition);
		const FVector2D PageSize = PageGeometry.GetLocalSize();
		Position.X = FMath::Clamp(AnchorLocal.X, 0.0, FMath::Max(0.0, PageSize.X - Width));
	}
	if (UCanvasPanelSlot* WindowSlot = mLayer->AddChildToCanvas(WindowSize))
	{
		WindowSlot->SetAnchors(FAnchors(0.f, 0.f));
		WindowSlot->SetAlignment(FVector2D::ZeroVector);
		WindowSlot->SetAutoSize(true);
		WindowSlot->SetPosition(Position);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

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
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMSignTabs::ClosePicker));
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
	mPickerList = nullptr;
	mPickerSearch = nullptr;
	mPickerHandlers.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}
