#include "UI/AMAutomationTab.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePriorityPowerSwitch.h"
#include "AutomatedMachinesModule.h"
#include "UI/AMRuleRow.h"
#include "UI/AMUIHelpers.h"
#include "UI/AMHoldBin.h"
#include "Automation/AMLocator.h"
#include "EngineUtils.h"
#include "Components/ProgressBar.h"
#include "Brushes/SlateColorBrush.h"
#include "GameFramework/Pawn.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Net/AMRemoteCallObject.h"
#include "Buildables/FGBuildable.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/EditableText.h"
#include "Components/NamedSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Styling/SlateBrush.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGBuildDescriptor.h"
#include "Buildables/FGBuildableStorage.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGPlayerState.h"
#include "Automation/AMClipboard.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Engine/Texture2D.h"
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
	const FName AMAT_TabTitleWidget(TEXT("mTitleObject"));
	const TCHAR* const AMAT_HubWindowClass = TEXT("BPW_Hub_C");
	const TCHAR* const AMAT_MamWindowClass = TEXT("BPW_MAM_C");
	const FName AMAT_ActiveTabChangedProperty(TEXT("OnActiveTabChanged"));
	const FName AMAT_ProductionPageName(TEXT("mManufacturingTab"));
	const FName AMAT_WindowBodyProperty(TEXT("WindowBody"));
	const TCHAR* const AMAT_FrameClass = TEXT("Widget_Window_DarkMode_C");

	/** Made by Tools/create_assets.py. */
	const TCHAR* const AMAT_TabIconPath = TEXT("/AutomatedMachines/Icons/T_AM_TabIcon.T_AM_TabIcon");

	/** Our page: about the Production page plus the inventory panel it replaces. */
	constexpr float AMAT_PagePadding = 16.f;
	constexpr float AMAT_PanelGap = 12.f;
	constexpr float AMAT_PanelPadding = 8.f;
	/** Room for the scroll bar at the right of the rows (without it, the rows' last cell got squeezed). */
	constexpr float AMAT_ScrollbarRoom = 14.f;
	/** Fixed panel widths (the rows' cells plus padding), so adding a row never moves anything. */
	constexpr float AMAT_AlarmsWidth = AMRowLayout::AlarmName + AMRowLayout::MachineEvent + AMRowLayout::Remove + 2 * AMRowLayout::Gap + 2 * AMAT_PanelPadding + AMAT_ScrollbarRoom;
	constexpr float AMAT_ActionsWidth = AMRowLayout::Do + AMRowLayout::OnAlarm + AMRowLayout::Remove + 2 * AMRowLayout::Gap + 2 * AMAT_PanelPadding + AMAT_ScrollbarRoom;
	/** The side panel, like the recipe tab's info panel. */
	constexpr float AMAT_InfoWidth = 312.f;

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
	// Usually "Widget_Window_DarkMode"; the storage window calls it "WidgetWindowDarkMode". Else the
	// first variable holding a Widget_Window_DarkMode_C.
	const UUserWidget* Host = GetHostWindow();
	if (UObject* Frame = AMAT_GetObject(Host, AMAT_FrameProperty))
	{
		return Frame;
	}
	if (!Host)
	{
		return nullptr;
	}
	for (TFieldIterator<FObjectPropertyBase> It(Host->GetClass()); It; ++It)
	{
		UObject* Value = It->GetObjectPropertyValue_InContainer(Host);
		if (Value && Value->GetClass()->GetName() == AMAT_FrameClass)
		{
			return Value;
		}
	}
	return nullptr;
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

FSlateFontInfo UAMAutomationTab::GetRowFont() const
{
	// The title of the first tab button: frame -> tabs container -> buttons panel -> button -> mTitleObject.
	UObject* TabsContainer = AMAT_GetObject(GetWindowFrame(), AMAT_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
	const UUserWidget* FirstButton = Buttons && Buttons->GetChildrenCount() > 0 ? Cast<UUserWidget>(Buttons->GetChildAt(0)) : nullptr;
	const UTextBlock* Title = FirstButton && FirstButton->WidgetTree ? Cast<UTextBlock>(FirstButton->WidgetTree->FindWidget(AMAT_TabTitleWidget)) : nullptr;
	if (Title)
	{
		return Title->GetFont();
	}
	return AMUI::SizedFont(GetGameFont(), AMRowLayout::FallbackFontSize);
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

	// Like the recipe tab: a dark, slightly see-through page with a lighter side panel on the right.
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AMRoot"));
	Root->SetMinDesiredHeight(560.f);
	WidgetTree->RootWidget = Root;
	// The page, and over it a layer for the item window (empty and collapsed until one opens).
	mRootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("AMRootOverlay"));
	Root->AddChild(mRootOverlay);
	UHorizontalBox* Page = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AMPage"));
	if (UOverlaySlot* PageInOverlay = mRootOverlay->AddChildToOverlay(Page))
	{
		PageInOverlay->SetHorizontalAlignment(HAlign_Fill);
		PageInOverlay->SetVerticalAlignment(VAlign_Fill);
	}
	mPopupLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AMPopupLayer"));
	mPopupLayer->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* LayerSlot = mRootOverlay->AddChildToOverlay(mPopupLayer))
	{
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	mContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AMContent"));
	UBorder* PageBorder = AMUI::MakeBox(WidgetTree, AMUI::PageBackground, FMargin(AMAT_PagePadding, 14.f));
	PageBorder->AddChild(mContentBox);
	if (UHorizontalBoxSlot* PageSlot = Page->AddChildToHorizontalBox(PageBorder))
	{
		PageSlot->SetVerticalAlignment(VAlign_Fill);
		if (bOverviewMode)
		{
			// The overview is as wide as the HUB's widest page (UpdatePageHeight): the page takes it.
			PageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	auto AddTo = [](UVerticalBox* Box, UWidget* Widget, float TopPadding, bool bFill = false)
	{
		if (UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
			RowSlot->SetHorizontalAlignment(bFill ? HAlign_Fill : HAlign_Left);
			if (bFill)
			{
				RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
	};

	// One line on top, like "Locomotive should stay at the station until..."
	mMachineText = MakeText(TEXT(""), 15, AMUI::White);
	AddTo(mContentBox, mMachineText, 0.f);
	mNoticeText = MakeText(TEXT(""), 14, AMUI::Orange);
	mNoticeText->SetVisibility(ESlateVisibility::Collapsed);
	AddTo(mContentBox, mNoticeText, 6.f);
	mGridWarningText = MakeText(TEXT("Power Grid A and B are joined by a cable somewhere else: they are one grid. This switch separates nothing, and the A / B usage and power alarms cannot ring until the cable is removed."), 14, AMUI::Orange);
	mGridWarningText->SetVisibility(ESlateVisibility::Collapsed);
	AddTo(mContentBox, mGridWarningText, 6.f);
	// A wrapping text still asks for its whole one-line width, which widens the window: these two
	// wrap at the panels' width instead (the alarms and actions panels side by side).
	for (UTextBlock* LongText : { mMachineText.Get(), mNoticeText.Get(), mGridWarningText.Get() })
	{
		LongText->SetAutoWrapText(false);
		LongText->SetWrapTextAt(AMAT_AlarmsWidth + AMAT_PanelGap + AMAT_ActionsWidth);
	}

	UHorizontalBox* Panels = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AMPanels"));
	AddTo(mContentBox, Panels, 14.f, true);

	// A panel: title above; inside, column names, the rows (scrolling), and the add button at the bottom.
	auto MakePanel = [this, &AddTo, Panels](const FString& Title, const FString& FirstColumn, float FirstWidth,
		const FString& SecondColumn, float SecondWidth, const FString& AddLabel, TObjectPtr<UVerticalBox>& OutRows,
		float LeftPadding, float PanelWidth) -> UButton*
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		USizeBox* ColumnSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ColumnSize->SetWidthOverride(PanelWidth);
		ColumnSize->AddChild(Column);
		if (UHorizontalBoxSlot* ColumnSlot = Panels->AddChildToHorizontalBox(ColumnSize))
		{
			ColumnSlot->SetPadding(FMargin(LeftPadding, 0.f, 0.f, 0.f));
			ColumnSlot->SetVerticalAlignment(VAlign_Fill);
		}
		AddTo(Column, MakeText(Title, 15, AMUI::White), 0.f);

		UBorder* Body = AMUI::MakeBox(WidgetTree, AMUI::PanelBody, FMargin(AMAT_PanelPadding));
		AddTo(Column, Body, 8.f, true);
		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Body->AddChild(Inside);

		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddHeader = [this, Header](const FString& Name, float Width, float HeaderLeft)
		{
			USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Cell->SetWidthOverride(Width);
			Cell->AddChild(MakeText(Name, 13, AMUI::Grey));
			if (UHorizontalBoxSlot* CellSlot = Header->AddChildToHorizontalBox(Cell))
			{
				CellSlot->SetPadding(FMargin(HeaderLeft, 0.f, 0.f, 0.f));
			}
		};
		AddHeader(FirstColumn, FirstWidth, 2.f);
		AddHeader(SecondColumn, SecondWidth, AMRowLayout::Gap - 2.f);
		AddTo(Inside, Header, 0.f);

		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
		AddTo(Inside, Scroll, 6.f, true);
		OutRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(OutRows);

		if (AddLabel.IsEmpty())
		{
			// No add button: a note instead (the HUB's alarms, until there are world events).
			UTextBlock* Note = MakeText(TEXT("World events come here later, e.g. night falls or a power fuse blows. Until then, alarms are rung by machines."), 13, AMUI::Grey);
			if (UVerticalBoxSlot* NoteSlot = Inside->AddChildToVerticalBox(Note))
			{
				NoteSlot->SetPadding(FMargin(2.f, 8.f, 2.f, 4.f));
			}
			return nullptr;
		}
		UButton* AddButton = AMUI::MakeButton(WidgetTree, AddLabel, GetGameFont(), AMUI::Button, nullptr);
		mAddLabels.Add(Cast<UTextBlock>(AddButton->GetChildAt(0)));
		if (UVerticalBoxSlot* AddSlot = Inside->AddChildToVerticalBox(AddButton))
		{
			AddSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			AddSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		return AddButton;
	};

	if (bOverviewMode)
	{
		// Alarms Overview: two regions. Left, every alarm of the world (Name | Rung by | Listening);
		// right, the machines of the count clicked (empty until then, with a line saying how).
		constexpr float CountWidth = 150.f;
		constexpr float AlarmsColumnWidth = 600.f;
		auto MakeRegion = [this, &AddTo, Panels](const FString& Title, float Width, float LeftPadding) -> UVerticalBox*
		{
			UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UWidget* ColumnWidget = Column;
			if (Width > 0.f)
			{
				USizeBox* ColumnSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				ColumnSize->SetWidthOverride(Width);
				ColumnSize->AddChild(Column);
				ColumnWidget = ColumnSize;
			}
			if (UHorizontalBoxSlot* ColumnSlot = Panels->AddChildToHorizontalBox(ColumnWidget))
			{
				ColumnSlot->SetVerticalAlignment(VAlign_Fill);
				ColumnSlot->SetPadding(FMargin(LeftPadding, 0.f, 0.f, 0.f));
				if (Width <= 0.f)
				{
					ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
			}
			AddTo(Column, MakeText(Title, 15, AMUI::White), 0.f);
			UBorder* Body = AMUI::MakeBox(WidgetTree, AMUI::PanelBody, FMargin(AMAT_PanelPadding));
			AddTo(Column, Body, 8.f, true);
			UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Body->AddChild(Inside);
			return Inside;
		};
		UVerticalBox* Inside = MakeRegion(TEXT("Alarms"), AlarmsColumnWidth, 0.f);
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		// The name takes all the room the two counts leave (Width 0 = fill).
		auto AddHeader = [this, Header](const FString& Name, float Width, float RightGap)
		{
			USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			if (Width > 0.f)
			{
				Cell->SetWidthOverride(Width);
			}
			Cell->AddChild(MakeText(Name, 13, AMUI::Grey));
			if (UHorizontalBoxSlot* CellSlot = Header->AddChildToHorizontalBox(Cell))
			{
				CellSlot->SetPadding(FMargin(2.f, 0.f, RightGap, 0.f));
				if (Width <= 0.f)
				{
					CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
			}
		};
		AddHeader(TEXT("Name"), 0.f, AMRowLayout::Gap - 2.f);
		AddHeader(TEXT("Rung by"), CountWidth - 2.f, AMRowLayout::Gap);
		AddHeader(TEXT("Listening"), CountWidth - 2.f, 0.f);
		// Full width (not full height), so the name header stretches like the rows' names and the others line up.
		if (UVerticalBoxSlot* HeaderSlot = Inside->AddChildToVerticalBox(Header))
		{
			HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
		AddTo(Inside, Scroll, 6.f, true);
		mOverviewRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(mOverviewRows);

		mMachinesBox = MakeRegion(TEXT("Machines"), 0.f, AMAT_PanelGap);
	}
	else if (bWorldMode)
	{
		// The HUB rings alarms on world events (e.g. a milestone completed).
		MakePanel(TEXT("Alarms"), TEXT("Alarm Name"), AMRowLayout::AlarmName, TEXT("World Event"), AMRowLayout::MachineEvent,
			TEXT("+ Add alarm"), mFireRowsBox, 0.f, AMAT_AlarmsWidth)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnAddFireClicked);
	}
	else
	{
		MakePanel(TEXT("Alarms"), TEXT("Alarm Name"), AMRowLayout::AlarmName, TEXT("Machine Event"), AMRowLayout::MachineEvent,
			TEXT("+ Add alarm"), mFireRowsBox, 0.f, AMAT_AlarmsWidth)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnAddFireClicked);
	}
	if (!bOverviewMode && !bMamMode)
	{
		MakePanel(TEXT("Actions"), TEXT("On Alarm / Do"), AMRowLayout::Do + AMRowLayout::Gap + AMRowLayout::OnAlarm, TEXT(""), AMRowLayout::Remove,
			TEXT("+ Listen to an alarm"), mReactionRowsBox, AMAT_PanelGap, AMAT_ActionsWidth)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnAddReactionClicked);
	}
	else if (bMamMode)
	{
		// No actions here (the HUB has the world's actions, machines their own): the list exists, hidden.
		mReactionRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	}

	// Options of the machine, at the bottom (the HUB has none).
	if (!bWorldMode)
	{
		UTextBlock* OptionsTitle = MakeText(TEXT("Options"), 15, AMUI::White);
		AddTo(mContentBox, OptionsTitle, 14.f);
		mOptionsWidgets.Add(OptionsTitle);
		// As wide as the Alarms panel; each option is one button (the whole row clicks), coloured
		// like "+ Add alarm": its name on the left, a switch on the right.
		USizeBox* OptionsSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		OptionsSize->SetWidthOverride(AMAT_AlarmsWidth);
		AddTo(mContentBox, OptionsSize, 8.f);
		mOptionsWidgets.Add(OptionsSize);
		UBorder* OptionsBody = AMUI::MakeBox(WidgetTree, AMUI::PanelBody, FMargin(AMAT_PanelPadding));
		OptionsSize->AddChild(OptionsBody);

		// One row per option, under each other.
		UVerticalBox* OptionsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AMOptionsList"));
		OptionsBody->AddChild(OptionsList);
		auto MakeOption = [this, OptionsList](const FName WidgetName, const FString& Name, TObjectPtr<UImage>& OutSwitch) -> UButton*
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
			AMUI::SetButtonColor(Button, AMUI::Button);
			if (UVerticalBoxSlot* ListSlot = OptionsList->AddChildToVerticalBox(Button))
			{
				ListSlot->SetPadding(FMargin(0.f, OptionsList->GetChildrenCount() > 1 ? 6.f : 0.f, 0.f, 0.f));
			}
			UHorizontalBox* OptionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UButtonSlot* RowSlot = Cast<UButtonSlot>(Button->AddChild(OptionRow)))
			{
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
				RowSlot->SetVerticalAlignment(VAlign_Center);
			}
			UTextBlock* OptionName = MakeText(Name, 14, AMUI::White);
			mAddLabels.Add(OptionName);
			if (UHorizontalBoxSlot* NameSlot = OptionRow->AddChildToHorizontalBox(OptionName))
			{
				NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				NameSlot->SetVerticalAlignment(VAlign_Center);
			}
			// 2:1 like the textures; the size box keeps it from being squeezed.
			OutSwitch = AMUI::MakeImage(WidgetTree, AMUI::SwitchOffPath, FVector2D(44.f, 22.f));
			USizeBox* SwitchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			SwitchSize->SetWidthOverride(44.f);
			SwitchSize->SetHeightOverride(22.f);
			SwitchSize->AddChild(OutSwitch);
			if (UHorizontalBoxSlot* SwitchSlot = OptionRow->AddChildToHorizontalBox(SwitchSize))
			{
				SwitchSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
				SwitchSlot->SetVerticalAlignment(VAlign_Center);
			}
			return Button;
		};
		mPassWrongButton = MakeOption(TEXT("AMOptionPassWrong"), TEXT("Pass wrong items through"), mPassWrongSwitch);
		mPassWrongButton->OnClicked.AddDynamic(this, &UAMAutomationTab::OnPassWrongClicked);
		mPassWrongButton->OnHovered.AddDynamic(this, &UAMAutomationTab::OnPassWrongHovered);
		mPassExtraButton = MakeOption(TEXT("AMOptionPassExtra"), TEXT("Pass extra items through"), mPassExtraSwitch);
		mPassExtraButton->OnClicked.AddDynamic(this, &UAMAutomationTab::OnPassExtraClicked);
		mPassExtraButton->OnHovered.AddDynamic(this, &UAMAutomationTab::OnPassExtraHovered);
		RefreshOptions();
	}

	// Copy / Paste automation settings: the automation only (Ctrl+C / Ctrl+V copy everything, the
	// game's Copy / Paste Settings buttons only the game's settings).
	if (!bWorldMode)
	{
		UHorizontalBox* ClipboardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AMClipboardRow"));
		mClipboardRow = ClipboardRow;
		auto AddClipboardButton = [this, ClipboardRow](const FString& Label, float LeftPadding) -> UButton*
		{
			UTextBlock* ButtonLabel = nullptr;
			UButton* Button = AMUI::MakeButton(WidgetTree, Label, GetGameFont(), AMUI::Button, &ButtonLabel);
			mAddLabels.Add(ButtonLabel);
			Button->OnHovered.AddDynamic(this, &UAMAutomationTab::OnCopyPasteHovered);
			USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			ButtonSize->SetWidthOverride(AMAT_AlarmsWidth / 2.f - 4.f);
			ButtonSize->AddChild(Button);
			if (UHorizontalBoxSlot* ButtonSlot = ClipboardRow->AddChildToHorizontalBox(ButtonSize))
			{
				ButtonSlot->SetPadding(FMargin(LeftPadding, 0.f, 0.f, 0.f));
			}
			return Button;
		};
		AddClipboardButton(TEXT("Copy automation settings"), 0.f)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnCopyAutomationClicked);
		AddClipboardButton(TEXT("Paste automation settings"), 8.f)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnPasteAutomationClicked);
		ClipboardRow->SetVisibility(ESlateVisibility::Collapsed);
		AddTo(mContentBox, ClipboardRow, 14.f);
	}

	// Side panel, full height on the right like the recipe tab's: explains the machine event,
	// action or alarm last picked or pointed at.
	USizeBox* InfoSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AMInfoSize"));
	InfoSize->SetWidthOverride(AMAT_InfoWidth);
	if (UHorizontalBoxSlot* InfoSlot = Page->AddChildToHorizontalBox(InfoSize))
	{
		InfoSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UBorder* InfoBox = AMUI::MakeBox(WidgetTree, AMUI::SidePanel, FMargin(18.f, 16.f));
	InfoSize->AddChild(InfoBox);
	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AMInfo"));
	InfoBox->AddChild(Info);
	auto AddInfo = [Info](UWidget* Widget, float TopPadding)
	{
		if (UVerticalBoxSlot* InfoRowSlot = Info->AddChildToVerticalBox(Widget))
		{
			InfoRowSlot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
			InfoRowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	UTextBlock* InfoLabel = MakeText(TEXT("Info"), 13, AMUI::White);
	InfoLabel->SetJustification(ETextJustify::Center);
	AddInfo(InfoLabel, 0.f);
	AddInfo(AMUI::MakeBox(WidgetTree, AMUI::Grey, FMargin(0.f, 0.5f)), 6.f);
	mInfoTitle = MakeText(TEXT(""), 20, AMUI::White);
	AddInfo(mInfoTitle, 14.f);
	mInfoBody = MakeText(TEXT(""), 14, AMUI::White);
	AddInfo(mInfoBody, 12.f);
	ShowDefaultInfo();
}

void UAMAutomationTab::ShowInfo(const FText& Title, const FText& Body)
{
	if (mInfoTitle && mInfoBody)
	{
		mInfoTitle->SetText(Title);
		mInfoBody->SetText(Body);
	}
}

void UAMAutomationTab::ShowDefaultInfo()
{
	if (bOverviewMode)
	{
		ShowInfo(FText::FromString(TEXT("Every alarm in the world")),
			FText::FromString(TEXT("Rung by: how many machines ring the alarm. Listening: how many machines, signs and the HUB do something when it rings.\n\nAn alarm nobody rings never makes its listeners act; one nobody listens to does nothing.\n\nThe bins remove the alarm from all of them at once: hold one down for 2.5 seconds.")));
		return;
	}
	if (bMamMode)
	{
		ShowInfo(FText::FromString(TEXT("Alarms of the MAM")),
			FText::FromString(TEXT("The MAM rings these alarms on its events, such as a research completed.\n\nWhat happens when they ring is set where it happens: on a machine, a sign, or in the HUB's Automation tab (Show message, Play sound).")));
		return;
	}
	if (bWorldMode)
	{
		ShowInfo(FText::FromString(TEXT("Alarms and actions of the whole world")),
			FText::FromString(TEXT("On the left: alarms the world rings, on world events such as a milestone completed.\n\nOn the right: what happens when an alarm rings, rung by any machine or by the world. These actions need no machine.\n\nClick a world event, an action or an alarm to see what it does.")));
		return;
	}
	ShowInfo(FText::FromString(TEXT("Alarms connect your machines")),
		FText::FromString(TEXT("On the left: when this machine rings an alarm.\n\nOn the right: what this machine does when an alarm rings, rung by this machine or any other.\n\nClick a machine event, an action or an alarm to see what it does.")));
}

void UAMAutomationTab::ShowAlarmInfo(const FString& Alarm)
{
	if (Alarm.IsEmpty())
	{
		ShowInfo(FText::FromString(TEXT("Alarm name")),
			FText::FromString(TEXT("Any name you like, e.g. PLATES BACKED UP. Names are always in capitals.\n\nEvery machine with an action for the same name reacts when this machine rings it.")));
		return;
	}
	int32 Ringers = 0;
	int32 Actors = 0;
	if (const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this))
	{
		Subsystem->CountEventUses(Alarm, Ringers, Actors);
	}
	ShowInfo(FText::FromString(Alarm), FText::FromString(FString::Printf(
		TEXT("Rung by %d machine%s, acted on by %d machine%s.\n\nEvery machine with an action for this alarm reacts when any of them rings it."),
		Ringers, Ringers == 1 ? TEXT("") : TEXT("s"), Actors, Actors == 1 ? TEXT("") : TEXT("s"))));
}

void UAMAutomationTab::LoadRules()
{
	if (bOverviewMode)
	{
		RebuildOverview();
		return;
	}
	if (bRulesLoaded)
	{
		return;
	}
	AFGBuildable* Machine = Cast<AFGBuildable>(GetMachine());
	if (!Machine && !bWorldMode)
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
	if (bWorldMode)
	{
		// The HUB's world alarms only (it saves those; the MAM's stay).
		const bool bMam = bMamMode;
		mFires = Subsystem->GetWorldFires().FilterByPredicate([bMam](const FAMFireRule& Rule) { return AMRules::IsMamCondition(Rule.Condition) == bMam; });
		if (!bMamMode)
		{
			mReactions = Subsystem->GetWorldActions();
		}
	}
	else if (const FAMMachineRules* Rules = Subsystem->FindRules(Machine))
	{
		mFires = Rules->Fires;
		mReactions = Rules->Reactions;
		mOptions = Rules->Options;
	}
	bRulesLoaded = true;
	if (mClipboardRow)
	{
		mClipboardRow->SetVisibility(Machine ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	for (UWidget* OptionsWidget : mOptionsWidgets)
	{
		if (OptionsWidget)
		{
			OptionsWidget->SetVisibility(AMRules::HasPassWrongItems(Machine) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	RefreshOptions();
	RebuildRows();
}

TArray<UClass*> UAMAutomationTab::GetPickableItems() const
{
	TSet<UClass*> Seen;
	TArray<UClass*> Contents;
	TArray<UClass*> Known;
	auto IsPickable = [](UClass* Item)
	{
		return Item && Item->IsChildOf(UFGItemDescriptor::StaticClass()) && !Item->IsChildOf(UFGBuildDescriptor::StaticClass())
			&& UFGItemDescriptor::GetForm(Item) == EResourceForm::RF_SOLID;
	};

	// What this container holds now, first.
	if (AFGBuildableStorage* Storage = Cast<AFGBuildableStorage>(GetMachine()))
	{
		if (const UFGInventoryComponent* Inventory = Storage->GetStorageInventory())
		{
			for (int32 SlotIndex = 0; SlotIndex < Inventory->GetSizeLinear(); ++SlotIndex)
			{
				FInventoryStack Stack;
				UClass* Item = Inventory->GetStackFromIndex(SlotIndex, Stack) && Stack.HasItems() ? Stack.Item.GetItemClass().Get() : nullptr;
				if (IsPickable(Item) && !Seen.Contains(Item))
				{
					Seen.Add(Item);
					Contents.Add(Item);
				}
			}
		}
	}

	// Every item of the unlocked recipes (ingredients and products).
	if (AFGRecipeManager* Recipes = AFGRecipeManager::Get(const_cast<UAMAutomationTab*>(this)))
	{
		for (const TSubclassOf<UFGRecipe>& Recipe : Recipes->GetAllAvailableRecipes())
		{
			TArray<FItemAmount> Amounts = UFGRecipe::GetProducts(Recipe);
			Amounts.Append(UFGRecipe::GetIngredients(this, Recipe));
			for (const FItemAmount& Amount : Amounts)
			{
				UClass* Item = Amount.ItemClass.Get();
				if (IsPickable(Item) && !Seen.Contains(Item))
				{
					Seen.Add(Item);
					Known.Add(Item);
				}
			}
		}
	}
	auto ByName = [](const UClass& A, const UClass& B)
	{
		return UFGItemDescriptor::GetItemName(const_cast<UClass*>(&A)).ToString() < UFGItemDescriptor::GetItemName(const_cast<UClass*>(&B)).ToString();
	};
	Contents.Sort(ByName);
	Known.Sort(ByName);
	Contents.Append(Known);
	return Contents;
}

void UAMAutomationTab::OpenPicker(UAMRuleRow* Row, UWidget* Anchor, TArray<FAMPickerOption>&& Options, const FString& EmptyMessage, float MinWidth)
{
	ClosePicker();
	if (!Row || !Anchor || !mPopupLayer || !mRootOverlay)
	{
		return;
	}
	mPickerOptions = MoveTemp(Options);
	mPickerEmptyMessage = EmptyMessage;
	for (const FAMPickerOption& Option : mPickerOptions)
	{
		mPopupHandlers.Add(Option.Handler);
	}
	const FSlateFontInfo Font = GetRowFont();

	// Where the dropdown is, in the page's units.
	const FGeometry& PageGeometry = mRootOverlay->GetCachedGeometry();
	const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
	const FVector2D PageSize = PageGeometry.GetLocalSize();
	const FVector2D AnchorPosition = AnchorGeometry.GetAbsolutePosition();
	const FVector2D AnchorSize = AnchorGeometry.GetAbsoluteSize();
	const FVector2D AnchorTopLeft = PageGeometry.AbsoluteToLocal(AnchorPosition);
	const FVector2D AnchorBottomRight = PageGeometry.AbsoluteToLocal(AnchorPosition + AnchorSize);
	const float AnchorHeight = FMath::Max(24.f, static_cast<float>(AnchorBottomRight.Y - AnchorTopLeft.Y));
	const float Width = FMath::Max(MinWidth, static_cast<float>(AnchorBottomRight.X - AnchorTopLeft.X));

	// The list: up to 320 high, less if the page ends sooner (it scrolls); moved up only if very short.
	constexpr float MaxListHeight = 320.f;
	constexpr float MinListHeight = 140.f;
	float X = static_cast<float>(AnchorTopLeft.X);
	float Y = static_cast<float>(AnchorTopLeft.Y);
	float ListHeight = MaxListHeight;
	if (PageSize.Y > 0.0)
	{
		ListHeight = FMath::Clamp(static_cast<float>(PageSize.Y) - Y - AnchorHeight - 12.f, MinListHeight, MaxListHeight);
		Y = FMath::Max(0.f, FMath::Min(Y, static_cast<float>(PageSize.Y) - AnchorHeight - ListHeight - 12.f));
	}
	if (PageSize.X > 0.0)
	{
		X = FMath::Clamp(X, 0.f, FMath::Max(0.f, static_cast<float>(PageSize.X) - Width));
	}

	// See-through background over the whole page: a click outside the window closes it.
	UButton* Background = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Clear;
	Clear.SetNormal(FSlateNoResource());
	Clear.SetHovered(FSlateNoResource());
	Clear.SetPressed(FSlateNoResource());
	Background->SetStyle(Clear);
	Background->OnClicked.AddDynamic(this, &UAMAutomationTab::OnPopupBackgroundClicked);
	if (UCanvasPanelSlot* BackgroundSlot = mPopupLayer->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));
	}

	// The window: the search field over the dropdown, the list under it.
	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WindowSize->SetWidthOverride(Width);
	UBorder* Window = AMUI::MakeBox(WidgetTree, AMUI::SidePanel, FMargin(0.f, 0.f, 0.f, 4.f));
	WindowSize->AddChild(Window);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Window->AddChild(Column);

	USizeBox* SearchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SearchSize->SetHeightOverride(AnchorHeight);
	UBorder* SearchField = AMUI::MakeBox(WidgetTree, AMUI::Field, FMargin(10.f, 0.f));
	SearchField->SetVerticalAlignment(VAlign_Center);
	AMUI::MarkAsInputBox(SearchField);
	SearchSize->AddChild(SearchField);
	mPickerSearch = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
	mPickerSearch->SetFont(Font);
	mPickerSearch->SetHintText(FText::FromString(TEXT("type to search")));
	mPickerSearch->OnTextChanged.AddDynamic(this, &UAMAutomationTab::OnPickerSearchChanged);
	mPickerSearch->OnTextCommitted.AddDynamic(this, &UAMAutomationTab::OnPickerSearchCommitted);
	SearchField->AddChild(mPickerSearch);
	Column->AddChildToVerticalBox(SearchSize);

	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ListSize->SetMaxDesiredHeight(ListHeight);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
	ListSize->AddChild(Scroll);
	mPickerList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(mPickerList);
	if (UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(ListSize))
	{
		ListSlot->SetPadding(FMargin(4.f, 4.f, 4.f, 0.f));
	}
	FillPickerList(FString());

	if (UCanvasPanelSlot* WindowSlot = mPopupLayer->AddChildToCanvas(WindowSize))
	{
		WindowSlot->SetAnchors(FAnchors(0.f, 0.f));
		WindowSlot->SetAlignment(FVector2D::ZeroVector);
		WindowSlot->SetAutoSize(true);
		WindowSlot->SetPosition(FVector2D(X, Y));
	}
	mPopup = WindowSize;
	mPopupLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Type right away (the field exists on screen from the next frame).
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

void UAMAutomationTab::FillPickerList(const FString& Filter)
{
	if (!mPickerList)
	{
		return;
	}
	mPickerList->ClearChildren();
	const FSlateFontInfo Font = GetRowFont();
	int32 Shown = 0;
	for (const FAMPickerOption& Option : mPickerOptions)
	{
		if (!Option.Handler || (!Filter.IsEmpty() && !Option.Label.Contains(Filter, ESearchCase::IgnoreCase)))
		{
			continue;
		}
		++Shown;
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		AMUI::SetButtonColor(Button, AMUI::Option);
		Button->OnClicked.AddDynamic(Option.Handler, &UAMChoice::OnClicked);
		Button->OnHovered.AddDynamic(Option.Handler, &UAMChoice::OnHovered);
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UButtonSlot* LineSlot = Cast<UButtonSlot>(Button->AddChild(Line)))
		{
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
			LineSlot->SetPadding(FMargin(Option.Icon ? 4.f : 0.f, Option.Icon ? 2.f : 0.f));
		}
		if (Option.Icon)
		{
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Icon->SetBrushFromTexture(Option.Icon, false);
			Icon->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
			USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			IconSize->SetWidthOverride(28.f);
			IconSize->SetHeightOverride(28.f);
			IconSize->AddChild(Icon);
			if (UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(IconSize))
			{
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
		}
		UTextBlock* Label = AMUI::MakeText(WidgetTree, Option.Label, Font, Option.bCurrent ? AMUI::Orange : AMUI::White);
		if (UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* OptionSlot = mPickerList->AddChildToVerticalBox(Button))
		{
			OptionSlot->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
	}
	if (Shown == 0)
	{
		UBorder* Message = AMUI::MakeBox(WidgetTree, AMUI::Option, FMargin(12.f, 7.f));
		Message->AddChild(AMUI::MakeText(WidgetTree, Filter.IsEmpty() ? mPickerEmptyMessage : FString(TEXT("Nothing found.")), Font, AMUI::Grey));
		mPickerList->AddChildToVerticalBox(Message);
	}
}

void UAMAutomationTab::OnPickerSearchChanged(const FText& Text)
{
	FillPickerList(Text.ToString().TrimStartAndEnd());
}

void UAMAutomationTab::OnPickerSearchCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnCleared)
	{
		ClosePicker(true);
		return;
	}
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}
	// Enter: the first choice shown.
	const FString Filter = Text.ToString().TrimStartAndEnd();
	for (const FAMPickerOption& Option : mPickerOptions)
	{
		if (Option.Handler && (Filter.IsEmpty() || Option.Label.Contains(Filter, ESearchCase::IgnoreCase)))
		{
			Option.Handler->OnClicked();
			return;
		}
	}
}

void UAMAutomationTab::ClosePicker(bool bDeferred)
{
	if (bDeferred)
	{
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UAMAutomationTab> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis]()
			{
				if (UAMAutomationTab* Tab = WeakThis.Get())
				{
					Tab->ClosePicker(false);
				}
			});
			return;
		}
	}
	if (mPopupLayer)
	{
		mPopupLayer->ClearChildren();
		mPopupLayer->SetVisibility(ESlateVisibility::Collapsed);
	}
	mPopup = nullptr;
	mPickerList = nullptr;
	mPickerSearch = nullptr;
	mPickerOptions.Reset();
	mPopupHandlers.Reset();
}

void UAMAutomationTab::ShowFloatingList(UWidget* List, UWidget* Anchor)
{
	if (!List || !mPopupLayer)
	{
		return;
	}
	if (List->GetParent() != mPopupLayer)
	{
		List->RemoveFromParent();
		mPopupLayer->AddChildToCanvas(List);
	}
	FVector2D Position(16.0, 60.0);
	if (Anchor)
	{
		const FGeometry& TabGeometry = GetCachedGeometry();
		const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
		Position = TabGeometry.AbsoluteToLocal(AnchorGeometry.GetAbsolutePosition() + FVector2D(0.0, AnchorGeometry.GetAbsoluteSize().Y));
	}
	if (UCanvasPanelSlot* ListSlot = Cast<UCanvasPanelSlot>(List->Slot))
	{
		ListSlot->SetAnchors(FAnchors(0.f, 0.f));
		ListSlot->SetAlignment(FVector2D::ZeroVector);
		ListSlot->SetAutoSize(true);
		ListSlot->SetPosition(Position + FVector2D(0.0, 2.0));
		ListSlot->SetZOrder(10);
	}
	// The layer itself lets clicks through; only the list takes them.
	mPopupLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UAMAutomationTab::HideFloatingList(UWidget* List)
{
	if (List && mPopupLayer && List->GetParent() == mPopupLayer)
	{
		List->RemoveFromParent();
		if (mPopupLayer->GetChildrenCount() == 0)
		{
			mPopupLayer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UAMAutomationTab::UpdatePageHeight()
{
	if (bSinglePageMode)
	{
		return;
	}
	const UPanelWidget* Pages = GetParent();
	USizeBox* Root = Cast<USizeBox>(WidgetTree ? WidgetTree->RootWidget : nullptr);
	if (!Pages || !Root)
	{
		return;
	}
	float Tallest = 0.f;
	float Widest = 0.f;
	for (int32 Index = 0; Index < Pages->GetChildrenCount(); ++Index)
	{
		const UWidget* Page = Pages->GetChildAt(Index);
		if (Page && !Page->IsA<UAMAutomationTab>() && Page->IsVisible())
		{
			Tallest = FMath::Max(Tallest, static_cast<float>(Page->GetDesiredSize().Y));
			Widest = FMath::Max(Widest, static_cast<float>(Page->GetDesiredSize().X));
		}
	}
	// The Alarms Overview is as wide as the widest game page (the HUB's Select Milestone).
	if (bOverviewMode && Widest > 600.f && !FMath::IsNearlyEqual(Root->GetWidthOverride(), Widest, 1.f))
	{
		Root->SetWidthOverride(Widest);
		UE_LOG(LogAutomatedMachines, Log, TEXT("Alarms Overview: page width %.0f (the game's widest page)"), Widest);
	}
	if (Tallest > 100.f && !FMath::IsNearlyEqual(Root->GetHeightOverride(), Tallest, 1.f))
	{
		Root->SetHeightOverride(Tallest);
		UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: page height %.0f (the game's tallest page)"), Tallest);
	}
}

void UAMAutomationTab::OnPopupBackgroundClicked()
{
	ClosePicker(true);
}

void UAMAutomationTab::RebuildRows()
{
	ClosePicker();
	if (!mReactionRowsBox || (!mFireRowsBox && !bWorldMode))
	{
		return;
	}
	if (mFireRowsBox)
	{
		mFireRowsBox->ClearChildren();
	}
	mReactionRowsBox->ClearChildren();
	mRows.Reset();

	auto AddRowWidget = [](UVerticalBox* Box, UWidget* Widget)
	{
		if (UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Widget))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			RowSlot->SetHorizontalAlignment(HAlign_Left);
		}
	};
	const FSlateFontInfo HintFont = GetRowFont();
	for (UTextBlock* AddLabel : mAddLabels)
	{
		if (AddLabel)
		{
			AddLabel->SetFont(HintFont);
		}
	}

	for (int32 Index = 0; Index < mFires.Num(); ++Index)
	{
		UAMRuleRow* Row = NewObject<UAMRuleRow>(this);
		Row->Build(this, true, Index);
		mRows.Add(Row);
		if (mFireRowsBox)
		{
			AddRowWidget(mFireRowsBox, Row->GetRootWidget());
		}
	}
	if (mFires.Num() == 0)
	{
		if (mFireRowsBox)
		{
			AddRowWidget(mFireRowsBox, AMUI::MakeText(WidgetTree, TEXT("No alarms yet."), HintFont, AMUI::Grey));
		}
	}

	// Actions grouped by alarm, in the order the alarms first appear: a header, the alarm's
	// actions, "+ action".
	TArray<FString> GroupEvents;
	for (const FAMReactionRule& Rule : mReactions)
	{
		GroupEvents.AddUnique(Rule.Event);
	}
	for (int32 Group = 0; Group < GroupEvents.Num(); ++Group)
	{
		const FString& Event = GroupEvents[Group];
		int32 Count = 0;
		for (const FAMReactionRule& Rule : mReactions)
		{
			Count += Rule.Event == Event ? 1 : 0;
		}
		UAMRuleRow* Header = NewObject<UAMRuleRow>(this);
		Header->BuildGroupHeader(this, Event, Count);
		mRows.Add(Header);
		AddRowWidget(mReactionRowsBox, Header->GetRootWidget());
		if (Group > 0)
		{
			if (UVerticalBoxSlot* HeaderSlot = Cast<UVerticalBoxSlot>(Header->GetRootWidget()->Slot))
			{
				HeaderSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
			}
		}
		for (int32 Index = 0; Index < mReactions.Num(); ++Index)
		{
			if (mReactions[Index].Event != Event)
			{
				continue;
			}
			UAMRuleRow* Row = NewObject<UAMRuleRow>(this);
			Row->Build(this, false, Index, true);
			mRows.Add(Row);
			AddRowWidget(mReactionRowsBox, Row->GetRootWidget());
		}
		AddRowWidget(mReactionRowsBox, Header->GetFooterWidget());
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
	if (bWorldMode)
	{
		UAMRemoteCallObject::RequestSetWorldRules(GetOwningPlayer(), mFires, mReactions, bMamMode);
	}
	else if (AFGBuildable* Machine = Cast<AFGBuildable>(GetMachine()))
	{
		UAMRemoteCallObject::RequestSetRules(GetOwningPlayer(), Machine, mFires, mReactions, mOptions);
	}
}

void UAMAutomationTab::OnCopyAutomationClicked()
{
	AFGPlayerState* Player = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AFGPlayerState>() : nullptr;
	UObject* Machine = GetMachine();
	AMClipboard::Copy(Player, Machine);
	const AMClipboard::FClip* Clip = AMClipboard::Find(Player, Machine);
	ShowInfo(FText::FromString(TEXT("Copy automation settings")), FText::FromString(Clip
		? FString::Printf(TEXT("Copied %d alarms and %d actions. Paste them on another machine of this kind with its Paste automation settings button, or with Ctrl+V while looking at it."), Clip->Fires.Num(), Clip->Reactions.Num())
		: FString(TEXT("This machine has no automation to copy."))));
}

void UAMAutomationTab::OnPasteAutomationClicked()
{
	AFGPlayerState* Player = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AFGPlayerState>() : nullptr;
	UObject* Machine = GetMachine();
	const AMClipboard::FClip* Clip = AMClipboard::Find(Player, Machine);
	if (!Clip)
	{
		ShowInfo(FText::FromString(TEXT("Paste automation settings")), FText::FromString(TEXT("Nothing copied for this kind of machine yet: copy one with Copy automation settings, or Ctrl+C while looking at it.")));
		return;
	}
	// Show it here at once (the server's copy comes back the same).
	mFires = Clip->Fires;
	mReactions = Clip->Reactions;
	mOptions = Clip->Options;
	AMClipboard::Paste(Player, Machine);
	RebuildRows();
	RefreshOptions();
	ShowInfo(FText::FromString(TEXT("Paste automation settings")), FText::FromString(FString::Printf(
		TEXT("Pasted %d alarms and %d actions: they replace this machine's."), Clip->Fires.Num(), Clip->Reactions.Num())));
}

void UAMAutomationTab::OnCopyPasteHovered()
{
	ShowInfo(FText::FromString(TEXT("Copy and paste")), FText::FromString(
		TEXT("Copy automation settings: remembers only this machine's alarms, actions and options.\n\nPaste automation settings: gives them to this machine, replacing its own. Works between the same machines.\n\nCtrl+C / Ctrl+V while looking at a machine copy everything (its settings and its automation), and so does building after a middle click. The window's Copy Settings / Paste Settings buttons copy only the machine's own settings.")));
}

void UAMAutomationTab::OnWorldRulesChanged()
{
	if (bOverviewMode)
	{
		RebuildOverview();
		return;
	}
	if (bWorldMode || !bRulesLoaded)
	{
		return;
	}
	const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this);
	const FAMMachineRules* Rules = Subsystem ? Subsystem->FindRules(Cast<AFGBuildable>(GetMachine())) : nullptr;
	const TArray<FAMFireRule> NoFires;
	const TArray<FAMReactionRule> NoReactions;
	const TArray<FAMFireRule>& Fires = Rules ? Rules->Fires : NoFires;
	const TArray<FAMReactionRule>& Reactions = Rules ? Rules->Reactions : NoReactions;
	bool bSame = Fires.Num() == mFires.Num() && Reactions.Num() == mReactions.Num()
		&& (Rules ? Rules->Options : FAMMachineOptions()) == mOptions;
	for (int32 Index = 0; bSame && Index < Fires.Num(); ++Index)
	{
		bSame = Fires[Index].Event == mFires[Index].Event && Fires[Index].Condition == mFires[Index].Condition
			&& Fires[Index].Value == mFires[Index].Value && Fires[Index].Item.Get() == mFires[Index].Item.Get();
	}
	for (int32 Index = 0; bSame && Index < Reactions.Num(); ++Index)
	{
		bSame = Reactions[Index].Event == mReactions[Index].Event && Reactions[Index].Action == mReactions[Index].Action
			&& Reactions[Index].Value == mReactions[Index].Value && Reactions[Index].Recipe.Get() == mReactions[Index].Recipe.Get()
			&& Reactions[Index].Message == mReactions[Index].Message;
	}
	if (bSame)
	{
		return;
	}
	// Changed elsewhere (e.g. the game's Paste Settings on the other tab): show what the world has.
	// Rows with an empty alarm name are not in the world yet: those are kept by not reloading while
	// the counts only differ by such rows.
	for (const FAMFireRule& Rule : mFires)
	{
		if (Rule.Event.IsEmpty())
		{
			return;
		}
	}
	for (const FAMReactionRule& Rule : mReactions)
	{
		if (Rule.Event.IsEmpty())
		{
			return;
		}
	}
	mFires = Fires;
	mReactions = Reactions;
	mOptions = Rules ? Rules->Options : FAMMachineOptions();
	RebuildRows();
	RefreshOptions();
}

void UAMAutomationTab::RefreshOptions()
{
	AMUI::SetImage(mPassWrongSwitch, mOptions.bPassWrongItems ? AMUI::SwitchOnPath : AMUI::SwitchOffPath);
	AMUI::SetImage(mPassExtraSwitch, mOptions.bPassExtraItems ? AMUI::SwitchOnPath : AMUI::SwitchOffPath);
}

void UAMAutomationTab::ShowPassExtraInfo()
{
	ShowInfo(FText::FromString(TEXT("Pass extra items through")), FText::FromString(
		TEXT("When an input slot is full and its belt brings more of that ingredient, the machine takes the extra item off the belt and puts it straight on its output belt, so the input belt keeps moving.\n\n")
		TEXT("Use it to chain machines on one belt: what one machine cannot take goes on to the next machine.\n\n")
		TEXT("The machine's own products go first: the extra item goes out when the output has nothing to give. The machine needs an output belt.")));
}

void UAMAutomationTab::OnPassExtraClicked()
{
	if (!bRulesLoaded)
	{
		return;
	}
	mOptions.bPassExtraItems = !mOptions.bPassExtraItems;
	RefreshOptions();
	ShowPassExtraInfo();
	OnRulesEdited();
}

void UAMAutomationTab::OnPassExtraHovered()
{
	ShowPassExtraInfo();
}

void UAMAutomationTab::ShowPassWrongInfo()
{
	ShowInfo(FText::FromString(TEXT("Pass wrong items through")), FText::FromString(
		TEXT("When a belt brings an item that is not an ingredient of the recipe, the machine takes it off the belt and puts it straight on its output belt, so the input belt moves again. Production does not stop.\n\n")
		TEXT("The machine's own products go first: the wrong item goes out when the output has nothing to give. The machine needs an output belt.\n\n")
		TEXT("Only for belts: a pipe with the wrong fluid stays stuck (use the alarm \"Wrong input\" to hear about it).")));
}

void UAMAutomationTab::OnPassWrongClicked()
{
	if (!bRulesLoaded)
	{
		return;
	}
	mOptions.bPassWrongItems = !mOptions.bPassWrongItems;
	RefreshOptions();
	ShowPassWrongInfo();
	OnRulesEdited();
}

void UAMAutomationTab::OnPassWrongHovered()
{
	ShowPassWrongInfo();
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

bool UAMAutomationTab::HasActionGroup(const FString& Event) const
{
	return mReactions.ContainsByPredicate([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; });
}

bool UAMAutomationTab::ActionClashesInGroup(int32 Index, EAMAction Action) const
{
	if (!mReactions.IsValidIndex(Index))
	{
		return false;
	}
	const FString& Event = mReactions[Index].Event;
	for (int32 Other = 0; Other < mReactions.Num(); ++Other)
	{
		if (Other != Index && mReactions[Other].Event == Event && AMRules::ActionsConflict(mReactions[Other].Action, Action))
		{
			return true;
		}
	}
	return false;
}

bool UAMAutomationTab::ActionClashesWithAlarm(const FString& Event, EAMAction Action) const
{
	return mReactions.ContainsByPredicate([&Event, Action](const FAMReactionRule& Rule)
	{
		return Rule.Event == Event && AMRules::ActionsConflict(Rule.Action, Action);
	});
}

bool UAMAutomationTab::IsActionAvailableHere(EAMAction Action) const
{
	return bWorldMode ? AMRules::IsWorldAction(Action) : (!AMRules::IsWorldAction(Action) && AMRules::IsActionAvailable(Action, GetMachine()));
}

void UAMAutomationTab::AddActionToGroup(const FString& Event, EAMAction Chosen)
{
	// The chosen action, or the first of this machine (or the HUB) that clashes with none of the alarm's.
	for (uint8 Value = 0; Value < static_cast<uint8>(EAMAction::Count); ++Value)
	{
		const EAMAction Action = Chosen != EAMAction::Count ? Chosen : static_cast<EAMAction>(Value);
		if (!IsActionAvailableHere(Action) || ActionClashesWithAlarm(Event, Action))
		{
			if (Chosen != EAMAction::Count)
			{
				break;
			}
			continue;
		}
		FAMReactionRule& NewRule = mReactions.AddDefaulted_GetRef();
		NewRule.Event = Event;
		NewRule.Action = Action;
		NewRule.Value = AMRules::GetActionValueInfo(Action).Default;
		OnRulesEdited();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::RebuildRows));
		}
		return;
	}
	ShowInfo(FText::FromString(Event.IsEmpty() ? TEXT("Actions") : Event),
		FText::FromString(TEXT("Every action of this machine is already used for this alarm (actions of one kind, like Pause and Resume, cannot go together).")));
}

void UAMAutomationTab::RenameActionGroup(const FString& OldEvent, const FString& NewEvent)
{
	if (OldEvent == NewEvent || NewEvent.IsEmpty())
	{
		return;
	}
	for (FAMReactionRule& Rule : mReactions)
	{
		if (Rule.Event == OldEvent)
		{
			Rule.Event = NewEvent;
		}
	}
	OnRulesEdited();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::RebuildRows));
	}
	ShowAlarmInfo(NewEvent);
}

void UAMAutomationTab::RemoveActionGroup(const FString& Event)
{
	const int32 Removed = mReactions.RemoveAll([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; });
	if (Removed == 0)
	{
		return;
	}
	OnRulesEdited();
	// Not now: we may be inside the click of a button that the rebuild removes.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::RebuildRows));
	}
}

void UAMAutomationTab::StartGroupHold(const FString& Event, int32 Count)
{
	StartOverviewHold(Event, false);
	bHoldGroup = true;
	if (mHoldBin)
	{
		mHoldBin->SetLabel(FString::Printf(TEXT("Hold to remove the %d actions of %s"), Count, Event.IsEmpty() ? TEXT("this alarm") : *Event));
	}
}

void UAMAutomationTab::ScrollToEndLater(UWidget* Rows)
{
	// The rows' list scrolls to its end once the new row has been laid out (next tick).
	UScrollBox* Scroll = Rows ? Cast<UScrollBox>(Rows->GetParent()) : nullptr;
	UWorld* World = GetWorld();
	if (!Scroll || !World)
	{
		return;
	}
	TWeakObjectPtr<UScrollBox> WeakScroll(Scroll);
	World->GetTimerManager().SetTimerForNextTick([WeakScroll]()
	{
		if (UScrollBox* List = WeakScroll.Get())
		{
			List->ScrollToEnd();
		}
	});
}

bool UAMAutomationTab::IsConditionAvailableHere(EAMCondition Condition) const
{
	// The HUB: its own world events (the MAM's are in the MAM's tab).
	return bWorldMode ? (AMRules::IsWorldCondition(Condition) && AMRules::IsMamCondition(Condition) == bMamMode) : AMRules::IsConditionAvailable(Condition, GetMachine());
}

void UAMAutomationTab::OnAddFireClicked()
{
	FAMFireRule& NewRule = mFires.AddDefaulted_GetRef();
	// The first machine event this machine has (a container has no "Output full").
	for (uint8 Value = 0; Value < static_cast<uint8>(EAMCondition::Count); ++Value)
	{
		if (IsConditionAvailableHere(static_cast<EAMCondition>(Value)))
		{
			NewRule.Condition = static_cast<EAMCondition>(Value);
			NewRule.Value = AMRules::GetConditionValueInfo(NewRule.Condition).Default;
			break;
		}
	}
	RebuildRows();
	ScrollToEndLater(mFireRowsBox);
}

void UAMAutomationTab::OnAddReactionClicked()
{
	// A new alarm group (its alarm chosen in its header). One without an alarm already: that one.
	if (HasActionGroup(FString()))
	{
		ScrollToEndLater(mReactionRowsBox);
		ShowInfo(FText::FromString(TEXT("Choose an alarm")), FText::FromString(TEXT("Choose the alarm of the actions without one first (click \"choose an alarm\").")));
		return;
	}
	FAMReactionRule& NewRule = mReactions.AddDefaulted_GetRef();
	if (bWorldMode)
	{
		NewRule.Action = EAMAction::ShowMessage;
	}
	else
	{
		// The first action this machine has (a container can only delete its contents).
		for (uint8 Value = 0; Value < static_cast<uint8>(EAMAction::Count); ++Value)
		{
			const EAMAction Action = static_cast<EAMAction>(Value);
			if (!AMRules::IsWorldAction(Action) && AMRules::IsActionAvailable(Action, GetMachine()))
			{
				NewRule.Action = Action;
				NewRule.Value = AMRules::GetActionValueInfo(Action).Default;
				break;
			}
		}
	}
	RebuildRows();
	ScrollToEndLater(mReactionRowsBox);
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
		if (TextField->GetPropertyValue_InContainer(Tabs.GetRawPtr(Index)).ToString() == GetTabTitle())
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

	// Copy the last tab (Production: its icon and style), then give it our title. At our page's
	// place among the pages when that is inside the list (the HUB has two pages of ours).
	int32 NewIndex = Tabs.Num();
	if (const UPanelWidget* OurPages = GetParent())
	{
		const int32 PagePlace = OurPages->GetChildIndex(this);
		if (PagePlace >= 0 && PagePlace < Tabs.Num())
		{
			NewIndex = PagePlace;
		}
	}
	const int32 TemplateIndex = Tabs.Num() - 1;
	if (NewIndex == Tabs.Num())
	{
		Tabs.AddValue();
	}
	else
	{
		Tabs.InsertValues(NewIndex, 1);
	}
	EntryProperty->Struct->CopyScriptStruct(Tabs.GetRawPtr(NewIndex), Tabs.GetRawPtr(NewIndex <= TemplateIndex ? TemplateIndex + 1 : TemplateIndex));
	TextField->SetPropertyValue_InContainer(Tabs.GetRawPtr(NewIndex), FText::FromString(GetTabTitle()));
	TabIndex = NewIndex;

	// Our own icon (the entry's texture variable is its only object variable).
	if (UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, AMAT_TabIconPath))
	{
		for (TFieldIterator<FProperty> It(EntryProperty->Struct); It; ++It)
		{
			if (FObjectPropertyBase* TextureField = CastField<FObjectPropertyBase>(*It))
			{
				TextureField->SetObjectPropertyValue_InContainer(Tabs.GetRawPtr(NewIndex), Icon);
				break;
			}
		}
	}

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
	if (bWorldMode)
	{
		if (mMachineText)
		{
			mMachineText->SetText(FText::FromString(bOverviewMode
				? TEXT("Every alarm in the world: who rings it and who listens to it.")
				: bMamMode ? TEXT("The MAM rings these alarms on its events.")
				: TEXT("The HUB rings alarms on world events, hears every alarm in the world, and does these actions when one rings.")));
		}
		return;
	}
	const UObject* Machine = GetMachine();
	const FTextProperty* NameProperty = Machine ? FindFProperty<FTextProperty>(Machine->GetClass(), AMAT_MachineNameProperty) : nullptr;
	if (mMachineText && NameProperty)
	{
		const FString Name = NameProperty->GetPropertyValue_InContainer(Machine).ToString();
		mMachineText->SetText(FText::FromString(FString::Printf(
			TEXT("This %s rings alarms when its status changes, and does actions when alarms ring."), *Name)));
	}
}

void UAMAutomationTab::EnsureTabButtonExists()
{
	// Also a good moment to read the machine: the window has it by now.
	UpdateMachineName();
	if (bRulesLoaded)
	{
		// The rows may have been made before the tab buttons existed: again, with the buttons' font.
		RebuildRows();
	}
	LoadRules();

	RefreshTabIndex();
	UObject* Frame = GetWindowFrame();
	UObject* TabsContainer = AMAT_GetObject(Frame, AMAT_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
	const int32 ButtonCount = Buttons ? Buttons->GetChildrenCount() : -1;
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: %d tab buttons exist, our tab index is %d"), ButtonCount, TabIndex);

	if (Buttons && TabIndex != INDEX_NONE && ButtonCount > 0 && ButtonCount <= TabIndex)
	{
		// The frame made its buttons before our entry was in the list: make them again.
		RegenerateTabButtons();
	}
	// A plain power switch shares its window with the priority switch: the priority list's tab
	// (page 1) stays hidden there, also after the buttons were made again.
	const UObject* Machine = GetMachine();
	if (Buttons && Machine && Machine->IsA<AFGBuildableCircuitSwitch>() && !Machine->IsA<AFGBuildablePriorityPowerSwitch>()
		&& TabIndex != 1 && Buttons->GetChildrenCount() > 1)
	{
		if (UWidget* PriorityButton = Buttons->GetChildAt(1))
		{
			PriorityButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		// Where the buttons sit (to check there is no gap where the hidden one was).
		for (int32 Index = 0; Index < Buttons->GetChildrenCount(); ++Index)
		{
			const UWidget* Button = Buttons->GetChildAt(Index);
			const UCanvasPanelSlot* ButtonSlot = Button ? Cast<UCanvasPanelSlot>(Button->Slot) : nullptr;
			UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: switch window button %d vis=%d pos=%s size=%s"), Index,
				Button ? static_cast<int32>(Button->GetVisibility()) : -1,
				ButtonSlot ? *ButtonSlot->GetPosition().ToString() : TEXT("-"), ButtonSlot ? *ButtonSlot->GetSize().ToString() : TEXT("-"));
		}
	}
	UpdatePageHeight();
}

void UAMAutomationTab::ApplyGameFonts()
{
	const UUserWidget* Host = GetHostWindow();
	const UTextBlock* Source = Host && Host->WidgetTree ? Cast<UTextBlock>(Host->WidgetTree->FindWidget(AMAT_FontSourceWidget)) : nullptr;
	if (!Source)
	{
		// No recipe name in this window (miners): the tab buttons' font.
		UObject* TabsContainer = AMAT_GetObject(GetWindowFrame(), AMAT_TabsContainerProperty);
		const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
		const UUserWidget* FirstButton = Buttons && Buttons->GetChildrenCount() > 0 ? Cast<UUserWidget>(Buttons->GetChildAt(0)) : nullptr;
		Source = FirstButton && FirstButton->WidgetTree ? Cast<UTextBlock>(FirstButton->WidgetTree->FindWidget(AMAT_TabTitleWidget)) : nullptr;
	}
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
	// The info panel reads lighter, like the recipe tab's description.
	if (mInfoTitle && mInfoBody)
	{
		mInfoTitle->SetFont(AMUI::RegularFont(mGameFont, 20));
		mInfoBody->SetFont(AMUI::RegularFont(mGameFont, 14));
	}
}

void UAMAutomationTab::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// In the HUB terminal: world actions only.
	const UUserWidget* Host = GetHostWindow();
	bMamMode = Host && Host->GetClass()->GetName() == AMAT_MamWindowClass;
	bWorldMode = Host && (Host->GetClass()->GetName() == AMAT_HubWindowClass || bMamMode);
	bOverviewMode = bWorldMode && GetFName() == FName(TEXT("AM_OverviewTab"));
	// No sliding pages (the miner's window): we make our own switch between its page and ours.
	bSinglePageMode = Host && !bWorldMode && !FindFProperty<FObjectPropertyBase>(Host->GetClass(), AMAT_SlidingTabsProperty);
	BuildLayout();
	if (bSinglePageMode)
	{
		// Until we have moved next to the machine's page (we start inside it).
		SetVisibility(ESlateVisibility::Collapsed);
	}
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

	if (TabIndex == INDEX_NONE && !bSinglePageMode)
	{
		AddTabEntry();
	}

	UObject* Pages = AMAT_GetObject(GetHostWindow(), AMAT_SlidingTabsProperty);
	if (!bBoundToPages && Pages)
	{
		if (FMulticastDelegateProperty* ChangedProperty = FindFProperty<FMulticastDelegateProperty>(Pages->GetClass(), AMAT_ActiveTabChangedProperty))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAMAutomationTab, OnActiveTabChanged));
			ChangedProperty->AddDelegate(MoveTemp(Delegate), Pages);
			bBoundToPages = true;
		}
	}

	// The frame may make its buttons after us (then it picks up our entry by itself) or may have made
	// them already: check one frame later, when all of the window has been constructed.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::EnsureTabButtonExists));
		if (bSinglePageMode)
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::SetupSinglePage));
		}
	}

	ApplyGameFonts();
	UpdateMachineName();
	LoadRules();

	// Power switches: watch for the two grids being joined by a cable.
	if (UWorld* World = GetWorld(); World && !bWorldMode)
	{
		CheckJoinedGrids();
		World->GetTimerManager().SetTimer(mGridCheckTimer, FTimerDelegate::CreateUObject(this, &UAMAutomationTab::CheckJoinedGrids), 1.f, true);
	}

	if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this))
	{
		if (!mRulesChangedHandle.IsValid())
		{
			mRulesChangedHandle = Subsystem->OnRulesChanged.AddUObject(this, &UAMAutomationTab::OnWorldRulesChanged);
		}
	}
}

void UAMAutomationTab::NativeDestruct()
{
	StopOverviewHold(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(mGridCheckTimer);
	}
	if (AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this))
	{
		Subsystem->OnRulesChanged.Remove(mRulesChangedHandle);
	}
	mRulesChangedHandle.Reset();
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
	UObject* Pages = AMAT_GetObject(GetHostWindow(), AMAT_SlidingTabsProperty);
	if (bBoundToPages && Pages)
	{
		if (FMulticastDelegateProperty* ChangedProperty = FindFProperty<FMulticastDelegateProperty>(Pages->GetClass(), AMAT_ActiveTabChangedProperty))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAMAutomationTab, OnActiveTabChanged));
			ChangedProperty->RemoveDelegate(Delegate, Pages);
		}
		bBoundToPages = false;
	}
	Super::NativeDestruct();
}

void UAMAutomationTab::CheckJoinedGrids()
{
	if (!mGridWarningText)
	{
		return;
	}
	// Both connections on the same circuit: only a cable does that (a switch in between keeps two
	// circuits, it only lets them share power). The circuit IDs are replicated: works on clients too.
	const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(GetMachine());
	const bool bJoined = Switch && Switch->GetCircuitID0() != INDEX_NONE && Switch->GetCircuitID0() == Switch->GetCircuitID1();
	const ESlateVisibility Wanted = bJoined ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (mGridWarningText->GetVisibility() != Wanted)
	{
		mGridWarningText->SetVisibility(Wanted);
	}
	if (!Switch && GetMachine())
	{
		// Not a switch: nothing to watch in this window.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(mGridCheckTimer);
		}
	}
}

void UAMAutomationTab::OnWindowTabButtonClicked(int32 ButtonIndex)
{
	RefreshTabIndex();
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: tab button %d clicked"), ButtonIndex);
	if (bSinglePageMode)
	{
		ShowSinglePage(ButtonIndex == TabIndex && TabIndex != INDEX_NONE);
		SetOurTabButtonLook();
		return;
	}
	if (ButtonIndex == TabIndex)
	{
		ShowThisTab();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::HideInventory));
		}
	}
}

void UAMAutomationTab::OnActiveTabChanged(UWidget* PreviouslyActiveWidget, UWidget* ActiveWidget)
{
	if (ActiveWidget == this)
	{
		// After the game's own handler (which may set the inventory for its own pages).
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::HideInventory));
		}
	}
	else if (PreviouslyActiveWidget == this && ActiveWidget && ActiveWidget->GetFName() == AMAT_ProductionPageName)
	{
		// Back to Production: its inventory panel again (the game may not do it when coming from us).
		UObject* Frame = GetWindowFrame();
		AMAT_Call(Frame, TEXT("SetInventoryVisibility"), [](UFunction* Function, uint8* Params)
		{
			int32 BoolIndex = 0;
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (const FBoolProperty* BoolParam = CastField<FBoolProperty>(*It))
				{
					BoolParam->SetPropertyValue_InContainer(Params, true); // show, animated
					++BoolIndex;
				}
			}
		});
	}
}

void UAMAutomationTab::HideInventory()
{
	SetInventoryShown(false);
}

void UAMAutomationTab::SetInventoryShown(bool bShow)
{
	UObject* Frame = GetWindowFrame();
	AMAT_Call(Frame, TEXT("SetInventoryVisibility"), [bShow](UFunction* Function, uint8* Params)
	{
		// (bool mShowInventory, bool Animate): animated.
		int32 BoolIndex = 0;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (const FBoolProperty* BoolParam = CastField<FBoolProperty>(*It))
			{
				BoolParam->SetPropertyValue_InContainer(Params, BoolIndex == 0 ? bShow : true);
				++BoolIndex;
			}
		}
	});
}

void UAMAutomationTab::SetupSinglePage()
{
	UNamedSlot* Body = Cast<UNamedSlot>(AMAT_GetObject(GetWindowFrame(), AMAT_WindowBodyProperty));
	UPanelWidget* WindowRow = Body ? Body->GetParent() : nullptr;
	if (!WindowRow)
	{
		UE_LOG(LogAutomatedMachines, Warning, TEXT("Automation tab: the window has no body to sit next to"));
		return;
	}
	if (!mMachinePage)
	{
		mMachinePage = Body;
		mMachinePageVisibility = Body->GetVisibility();
	}
	if (GetParent() != WindowRow)
	{
		// From inside the machine's page to the frame's row (order does not matter: when we show,
		// the body and the inventory are hidden).
		RemoveFromParent();
		if (UHorizontalBoxSlot* RowSlot = Cast<UHorizontalBoxSlot>(WindowRow->AddChild(this)))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: placed next to %s in %s"), *Body->GetName(), *WindowRow->GetName());
	}

	// The window fills its tab list in its own construct, after ours: now our entry can go in.
	if (TabIndex == INDEX_NONE)
	{
		AddTabEntry();
	}
	if (TabIndex == INDEX_NONE)
	{
		if (++mSetupTries < 20)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAMAutomationTab::SetupSinglePage));
			}
		}
		return;
	}
	mSetupTries = 0;
	RegenerateTabButtons();
	SetOurTabButtonLook();
	BindSingleTabButtons();
	ShowSinglePage(false);
}

void UAMAutomationTab::RegenerateTabButtons()
{
	UObject* Frame = GetWindowFrame();
	UObject* TabsContainer = AMAT_GetObject(Frame, AMAT_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
	if (!Buttons || TabIndex == INDEX_NONE || Buttons->GetChildrenCount() > TabIndex)
	{
		return;
	}
	AMAT_CallNoParams(Frame, TEXT("InitTabs"));
	if (Buttons->GetChildrenCount() > TabIndex)
	{
		UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: tabs made again, now %d buttons"), Buttons->GetChildrenCount());
		return;
	}
	// The frame kept its old buttons: give the tabs container the frame's list and let it make them.
	FArrayProperty* FrameTabs = FindFProperty<FArrayProperty>(Frame->GetClass(), AMAT_TabsProperty);
	FArrayProperty* ContainerButtons = FindFProperty<FArrayProperty>(TabsContainer->GetClass(), TEXT("mButtons"));
	if (FrameTabs && ContainerButtons && FrameTabs->Inner->SameType(ContainerButtons->Inner))
	{
		ContainerButtons->CopyCompleteValue(ContainerButtons->ContainerPtrToValuePtr<void>(TabsContainer), FrameTabs->ContainerPtrToValuePtr<void>(Frame));
		AMAT_CallNoParams(TabsContainer, TEXT("GenerateButtons"));
	}
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: tab buttons made by the tabs container, now %d buttons"), Buttons->GetChildrenCount());

	// A window made for one tab stretches its button over the whole bar: put the buttons side by
	// side at the container's button size, like the production window's.
	FVector2D ButtonSize(220.0, 36.0);
	if (const FStructProperty* SizeProperty = FindFProperty<FStructProperty>(TabsContainer->GetClass(), TEXT("mButtonSize")))
	{
		if (SizeProperty->Struct == TBaseStructure<FVector2D>::Get())
		{
			ButtonSize = *SizeProperty->ContainerPtrToValuePtr<FVector2D>(TabsContainer);
		}
	}
	for (int32 Index = 0; Index < Buttons->GetChildrenCount(); ++Index)
	{
		UWidget* Button = Buttons->GetChildAt(Index);
		UCanvasPanelSlot* ButtonSlot = Button ? Cast<UCanvasPanelSlot>(Button->Slot) : nullptr;
		if (!ButtonSlot)
		{
			continue;
		}
		const FAnchors Anchors = ButtonSlot->GetAnchors();
		UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: button %d %s vis=%d pos=%s size=%s anchors=(%.2f,%.2f)-(%.2f,%.2f) auto=%d"),
			Index, *Button->GetName(), static_cast<int32>(Button->GetVisibility()), *ButtonSlot->GetPosition().ToString(), *ButtonSlot->GetSize().ToString(),
			Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y, ButtonSlot->GetAutoSize() ? 1 : 0);
		ButtonSlot->SetAnchors(FAnchors(0.f, 0.f));
		ButtonSlot->SetAlignment(FVector2D::ZeroVector);
		ButtonSlot->SetAutoSize(false);
		ButtonSlot->SetPosition(FVector2D(ButtonSize.X * Index, 0.0));
		ButtonSlot->SetSize(ButtonSize);
		// The single button of a one-tab window is a title (not clickable): make them all clickable.
		Button->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	SetOurTabButtonLook();
	BindSingleTabButtons();
}

void UAMAutomationTab::BindSingleTabButtons()
{
	UObject* TabsContainer = AMAT_GetObject(GetWindowFrame(), AMAT_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
	if (!Buttons)
	{
		return;
	}
	for (int32 Index = 0; Index < Buttons->GetChildrenCount(); ++Index)
	{
		UButton* Inner = Cast<UButton>(AMAT_GetObject(Buttons->GetChildAt(Index), TEXT("mButton")));
		if (!Inner)
		{
			UE_LOG(LogAutomatedMachines, Warning, TEXT("Automation tab: tab button %d has no mButton"), Index);
			continue;
		}
		if (Index == TabIndex)
		{
			Inner->OnClicked.AddUniqueDynamic(this, &UAMAutomationTab::OnAutomationTabPressed);
		}
		else
		{
			Inner->OnClicked.AddUniqueDynamic(this, &UAMAutomationTab::OnMachineTabPressed);
		}
	}
}

void UAMAutomationTab::OnMachineTabPressed()
{
	SelectSingleTab(0);
}

void UAMAutomationTab::OnAutomationTabPressed()
{
	SelectSingleTab(TabIndex);
}

void UAMAutomationTab::SelectSingleTab(int32 Index)
{
	UE_LOG(LogAutomatedMachines, Log, TEXT("Automation tab: tab button %d clicked (one-page window)"), Index);
	UObject* TabsContainer = AMAT_GetObject(GetWindowFrame(), AMAT_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
	UWidget* Clicked = Buttons ? Buttons->GetChildAt(Index) : nullptr;
	if (Clicked)
	{
		// The highlight follows the clicked button.
		AMAT_Call(TabsContainer, TEXT("SetActiveButton"), [Clicked](UFunction* Function, uint8* Params)
		{
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (const FObjectPropertyBase* ButtonParam = CastField<FObjectPropertyBase>(*It))
				{
					ButtonParam->SetObjectPropertyValue_InContainer(Params, Clicked);
					break;
				}
			}
		});
	}
	ShowSinglePage(Index == TabIndex && TabIndex != INDEX_NONE);
	SetOurTabButtonLook();
}

void UAMAutomationTab::SetOurTabButtonLook()
{
	// One-tab windows give every button the window's title and the default icon: ours gets its own.
	UObject* TabsContainer = AMAT_GetObject(GetWindowFrame(), AMAT_TabsContainerProperty);
	const UPanelWidget* Buttons = Cast<UPanelWidget>(AMAT_GetObject(TabsContainer, AMAT_ButtonsPanelProperty));
	UWidget* OurButton = Buttons && TabIndex != INDEX_NONE ? Buttons->GetChildAt(TabIndex) : nullptr;
	if (!OurButton)
	{
		return;
	}
	const FText Title = FText::FromString(GetTabTitle());
	AMAT_Call(OurButton, TEXT("SetTitle"), [&Title](UFunction* Function, uint8* Params)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (const FTextProperty* TextParam = CastField<FTextProperty>(*It))
			{
				TextParam->SetPropertyValue_InContainer(Params, Title);
				break;
			}
		}
	});
	if (UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, AMAT_TabIconPath))
	{
		AMAT_Call(OurButton, TEXT("SetIcon"), [Icon](UFunction* Function, uint8* Params)
		{
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (const FObjectPropertyBase* IconParam = CastField<FObjectPropertyBase>(*It))
				{
					IconParam->SetObjectPropertyValue_InContainer(Params, Icon);
					break;
				}
			}
		});
	}
}

void UAMAutomationTab::ShowSinglePage(bool bAutomation)
{
	if (!mMachinePage)
	{
		return;
	}
	// Our page keeps its own width; its height is the machine page's, so the window only changes width.
	if (bAutomation)
	{
		const float PageHeight = static_cast<float>(mMachinePage->GetDesiredSize().Y);
		if (mMachinePage->GetVisibility() != ESlateVisibility::Collapsed && PageHeight > 100.f)
		{
			mMachinePageHeight = PageHeight;
		}
		if (USizeBox* Root = Cast<USizeBox>(WidgetTree ? WidgetTree->RootWidget : nullptr))
		{
			if (mMachinePageHeight > 0.f)
			{
				Root->SetHeightOverride(mMachinePageHeight);
			}
		}
	}
	mMachinePage->SetVisibility(bAutomation ? ESlateVisibility::Collapsed : mMachinePageVisibility);
	SetVisibility(bAutomation ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetInventoryShown(!bAutomation);
}

void UAMAutomationTab::ShowThisTab()
{
	RefreshTabIndex();
	UpdatePageHeight();
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

const TCHAR* UAMAutomationTab::GetTabTitle() const
{
	return bOverviewMode ? TEXT("Alarms Overview") : AMAT_TabTitle;
}

void UAMAutomationTab::RefreshTabIndex()
{
	UObject* Frame = GetWindowFrame();
	FArrayProperty* TabsProperty = Frame ? FindFProperty<FArrayProperty>(Frame->GetClass(), AMAT_TabsProperty) : nullptr;
	FStructProperty* EntryProperty = TabsProperty ? CastField<FStructProperty>(TabsProperty->Inner) : nullptr;
	FTextProperty* TextField = EntryProperty ? AMAT_FindTextField(EntryProperty->Struct) : nullptr;
	if (!TextField)
	{
		return;
	}
	FScriptArrayHelper Tabs(TabsProperty, TabsProperty->ContainerPtrToValuePtr<void>(Frame));
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		if (TextField->GetPropertyValue_InContainer(Tabs.GetRawPtr(Index)).ToString() == GetTabTitle())
		{
			TabIndex = Index;
			return;
		}
	}
}

void UAMAutomationTab::RebuildOverview()
{
	if (!mOverviewRows)
	{
		return;
	}
	if (bHolding)
	{
		// Not while a bin is held: its button would be gone. Made again when it is let go.
		bOverviewDirty = true;
		return;
	}
	bOverviewDirty = false;
	mOverviewRows->ClearChildren();
	mOverviewHandlers.Reset();
	// A list still being made belongs to the rows just removed: its batches stop.
	++mExpandGeneration;
	mExpandedRows = nullptr;
	mExpandedLoading = nullptr;
	const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this);
	const FSlateFontInfo Font = GetRowFont();
	TArray<FString> Alarms = Subsystem ? Subsystem->GetAllEventNames() : TArray<FString>();
	Alarms.RemoveAll([](const FString& Alarm) { return Alarm.IsEmpty(); });
	if (Alarms.Num() == 0)
	{
		mOverviewRows->AddChildToVerticalBox(AMUI::MakeText(WidgetTree, TEXT("No alarms in the world yet."), Font, AMUI::Grey));
		mExpandedEvent.Reset();
		RebuildMachinesRegion();
		return;
	}
	constexpr float CountWidth = 150.f;
	for (const FString& Alarm : Alarms)
	{
		int32 Ringers = 0;
		int32 Listeners = 0;
		Subsystem->CountEventUses(Alarm, Ringers, Listeners);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		auto MakeHandler = [this, &Alarm](bool bRingers)
		{
			UAMOverviewHandler* Handler = NewObject<UAMOverviewHandler>(this);
			Handler->Tab = this;
			Handler->Event = Alarm;
			Handler->bRingers = bRingers;
			mOverviewHandlers.Add(Handler);
			return Handler;
		};
		// The name, taking the rest of the row: pointing at it explains the alarm. A button only to
		// hear the pointer; it looks the same hovered and pressed, so it does not seem clickable.
		UTextBlock* NameLabel = nullptr;
		UButton* NameButton = AMUI::MakeButton(WidgetTree, Alarm, Font, AMUI::Field, &NameLabel);
		if (UButtonSlot* NameSlot = NameLabel ? Cast<UButtonSlot>(NameLabel->Slot) : nullptr)
		{
			NameSlot->SetHorizontalAlignment(HAlign_Left);
		}
		FButtonStyle NameStyle = NameButton->GetStyle();
		NameStyle.Hovered = NameStyle.Normal;
		NameStyle.Pressed = NameStyle.Normal;
		NameButton->SetStyle(NameStyle);
		NameButton->SetCursor(EMouseCursor::Default);
		NameButton->OnHovered.AddDynamic(MakeHandler(true), &UAMOverviewHandler::OnNameHovered);
		if (UHorizontalBoxSlot* NameCellSlot = Row->AddChildToHorizontalBox(NameButton))
		{
			NameCellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameCellSlot->SetPadding(FMargin(0.f, 0.f, AMRowLayout::Gap, 0.f));
		}

		// "3 machines [bin]": a count, orange when it is 0 (the alarm is half-made), and its bin.
		auto AddCount = [&](int32 Count, bool bRingers)
		{
			UHorizontalBox* Cell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UWidget* CountWidget = nullptr;
			if (Count > 0)
			{
				// Clickable: lists the machines under the row (lighter while its list is open).
				const bool bOpen = mExpandedEvent == Alarm && bExpandedRingers == bRingers;
				UTextBlock* CountLabel = nullptr;
				UButton* CountButton = AMUI::MakeButton(WidgetTree, FString::Printf(TEXT("%d"), Count), Font,
					bOpen ? AMUI::Orange : AMUI::Field, &CountLabel);
				if (UButtonSlot* CountSlot = CountLabel ? Cast<UButtonSlot>(CountLabel->Slot) : nullptr)
				{
					CountSlot->SetHorizontalAlignment(HAlign_Left);
				}
				UAMOverviewHandler* CountHandler = MakeHandler(bRingers);
				CountButton->OnClicked.AddDynamic(CountHandler, &UAMOverviewHandler::OnCountClicked);
				CountButton->OnHovered.AddDynamic(CountHandler, &UAMOverviewHandler::OnCountHovered);
				CountWidget = CountButton;
			}
			else
			{
				UBorder* Box = AMUI::MakeBox(WidgetTree, AMUI::Field, FMargin(10.f, 7.f));
				Box->SetVerticalAlignment(VAlign_Center);
				Box->AddChild(AMUI::MakeText(WidgetTree, TEXT("nobody"), Font, AMUI::Orange));
				CountWidget = Box;
			}
			if (UHorizontalBoxSlot* BoxSlot = Cell->AddChildToHorizontalBox(CountWidget))
			{
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			if (Count > 0)
			{
				UAMOverviewHandler* Handler = MakeHandler(bRingers);
				UButton* Bin = AMUI::MakeRemoveButton(WidgetTree, Font);
				Bin->OnPressed.AddDynamic(Handler, &UAMOverviewHandler::OnBinPressed);
				Bin->OnReleased.AddDynamic(Handler, &UAMOverviewHandler::OnBinReleased);
				Bin->OnHovered.AddDynamic(Handler, &UAMOverviewHandler::OnBinHovered);
				USizeBox* BinSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				BinSize->SetWidthOverride(AMRowLayout::Remove + 10.f);
				BinSize->AddChild(Bin);
				if (UHorizontalBoxSlot* BinSlot = Cell->AddChildToHorizontalBox(BinSize))
				{
					BinSlot->SetPadding(FMargin(AMRowLayout::Gap, 0.f, 0.f, 0.f));
				}
			}
			USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CellSize->SetWidthOverride(CountWidth);
			CellSize->AddChild(Cell);
			if (UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(CellSize))
			{
				CellSlot->SetPadding(FMargin(0.f, 0.f, bRingers ? AMRowLayout::Gap : 0.f, 0.f));
			}
		};
		AddCount(Ringers, true);
		AddCount(Listeners, false);
		if (UVerticalBoxSlot* RowSlot = mOverviewRows->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}
	if (!Alarms.Contains(mExpandedEvent))
	{
		mExpandedEvent.Reset();
	}
	RebuildMachinesRegion();
}

namespace
{
	/** Rows of an alarm's machine list made per tick (thousands of machines: a few frames). */
	constexpr int32 AMAT_ExpandBatch = 50;
	constexpr float AMAT_ExpandRowHeight = 34.f;
	const TCHAR* const AMAT_LocateIconPath = TEXT("/AutomatedMachines/Icons/T_AM_Locate.T_AM_Locate");
}

void UAMAutomationTab::OnOverviewCountClicked(const FString& Event, bool bRingers)
{
	if (mExpandedEvent == Event && bExpandedRingers == bRingers)
	{
		mExpandedEvent.Reset();
	}
	else
	{
		mExpandedEvent = Event;
		bExpandedRingers = bRingers;
	}
	// The rows are made again on the next tick (not inside the click of one of them).
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UAMAutomationTab> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick([WeakThis]()
		{
			if (UAMAutomationTab* Tab = WeakThis.Get())
			{
				Tab->RebuildOverview();
			}
		});
	}
}

void UAMAutomationTab::ExplainOverviewCount(const FString& Event, bool bRingers)
{
	ShowInfo(FText::FromString(Event), FText::FromString(bRingers
		? TEXT("Click to list the machines that ring this alarm, nearest first; click again to close the list.\n\nEach machine has a locate button and a bin that removes the alarm from that machine only.")
		: TEXT("Click to list the machines, signs and the HUB that act when this alarm rings, nearest first; click again to close the list.\n\nEach has a locate button and a bin that removes its actions on this alarm only.")));
}

void UAMAutomationTab::OnOverviewLocate(AFGBuildable* Machine, bool bHub, const FString& TargetName)
{
	AFGBuildable* Target = Machine;
	if (!Target && bHub)
	{
		// The HUB: the trading post building (looked for when asked: rare).
		for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("TradingPost")))
			{
				Target = *It;
				break;
			}
		}
	}
	AAMLocator* Locator = AAMLocator::Get(GetWorld());
	if (!Target || !Locator)
	{
		ShowInfo(FText::FromString(TargetName), FText::FromString(TEXT("Not found in the world.")));
		return;
	}
	if (Locator->Toggle(Target, UFGItemDescriptor::GetSmallIcon(Target->GetBuiltWithDescriptor()), TargetName))
	{
		ShowInfo(FText::FromString(FString::Printf(TEXT("Locating %s"), *TargetName)), FText::FromString(
			TEXT("Its icon is on your compass and your map, and it pulses where it stands. Both go away when you get there.\n\nClick locate again to stop.")));
	}
	else
	{
		ShowInfo(FText::FromString(TargetName), FText::FromString(TEXT("No longer located.")));
	}
}

void UAMAutomationTab::RemoveFromOne(const FString& Event, bool bRingers, AFGBuildable* Machine, bool bHub, bool bMam, const FString& TargetName)
{
	// Never all of them: a machine that is gone meanwhile removes nothing.
	if (!Machine && !bHub)
	{
		return;
	}
	UAMRemoteCallObject::RequestClearAlarm(GetOwningPlayer(), Event, bRingers, Machine, bHub, bMam);
	ShowInfo(FText::FromString(Event), FText::FromString(FString::Printf(TEXT("Removed from %s."), *TargetName)));
}

void UAMAutomationTab::ExplainOverviewLocate(const FString& TargetName)
{
	ShowInfo(FText::FromString(FString::Printf(TEXT("Locate %s"), *TargetName)),
		FText::FromString(TEXT("Shows where it is: its icon on your compass and your map, and a pulse where it stands, until you get there.")));
}

void UAMAutomationTab::RebuildMachinesRegion()
{
	if (!mMachinesBox)
	{
		return;
	}
	mMachinesBox->ClearChildren();
	mExpandedRows = nullptr;
	mExpandedLoading = nullptr;
	mExpandedProgress = nullptr;
	mExpandedProgressText = nullptr;
	if (!mExpandedEvent.IsEmpty())
	{
		AddExpandedList();
		return;
	}
	UTextBlock* Hint = AMUI::MakeText(WidgetTree,
		TEXT("Click a number under Rung by or Listening to list its machines here, nearest first.\n\nEach one can be located, or have the alarm removed from it only."),
		GetRowFont(), AMUI::Grey);
	Hint->SetAutoWrapText(true);
	if (UVerticalBoxSlot* HintSlot = mMachinesBox->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetPadding(FMargin(4.f, 4.f, 4.f, 0.f));
	}
}

void UAMAutomationTab::AddExpandedList()
{
	const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this);
	if (!Subsystem || !mMachinesBox)
	{
		return;
	}
	// Who rings (or listens to) the alarm, nearest first.
	mExpandedEntries.Reset();
	mExpandedBuilt = 0;
	const FString& Event = mExpandedEvent;
	const APawn* Pawn = GetOwningPlayerPawn();
	const FVector From = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	for (const FAMMachineRules& Entry : Subsystem->GetAllMachines())
	{
		AFGBuildable* Machine = Entry.Machine.Get();
		if (!IsValid(Machine))
		{
			continue;
		}
		const bool bUses = bExpandedRingers
			? Entry.Fires.ContainsByPredicate([&Event](const FAMFireRule& Rule) { return Rule.Event == Event; })
			: (Entry.Reactions.ContainsByPredicate([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; })
				|| Entry.SignDesigns.ContainsByPredicate([&Event](const FAMSignDesign& Design) { return Design.Event == Event; })
				|| Entry.LightSettings.ContainsByPredicate([&Event](const FAMLightSetting& Setting) { return Setting.Event == Event; }));
		if (!bUses)
		{
			continue;
		}
		FAMOverviewEntry& Added = mExpandedEntries.AddDefaulted_GetRef();
		Added.Machine = Machine;
		const FTextProperty* NameProperty = FindFProperty<FTextProperty>(Machine->GetClass(), AMAT_MachineNameProperty);
		Added.Name = NameProperty ? NameProperty->GetPropertyValue_InContainer(Machine).ToString() : Machine->GetClass()->GetName();
		Added.Icon = UFGItemDescriptor::GetSmallIcon(Machine->GetBuiltWithDescriptor());
		Added.Distance = static_cast<float>(FVector::Dist(From, Machine->GetActorLocation()));
	}
	mExpandedEntries.Sort([](const FAMOverviewEntry& A, const FAMOverviewEntry& B) { return A.Distance < B.Distance; });
	// The HUB and the MAM (world events, world actions) first.
	auto AddWorldEntry = [this](const TCHAR* Name, bool bMam)
	{
		FAMOverviewEntry World;
		World.bHub = true;
		World.bMam = bMam;
		World.Name = Name;
		World.Distance = -1.f;
		mExpandedEntries.Insert(World, 0);
	};
	if (bExpandedRingers)
	{
		const TArray<FAMFireRule>& WorldFires = Subsystem->GetWorldFires();
		if (WorldFires.ContainsByPredicate([&Event](const FAMFireRule& Rule) { return Rule.Event == Event && AMRules::IsMamCondition(Rule.Condition); }))
		{
			AddWorldEntry(TEXT("The MAM (world events)"), true);
		}
		if (WorldFires.ContainsByPredicate([&Event](const FAMFireRule& Rule) { return Rule.Event == Event && !AMRules::IsMamCondition(Rule.Condition); }))
		{
			AddWorldEntry(TEXT("The HUB (world events)"), false);
		}
	}
	else if (Subsystem->GetWorldActions().ContainsByPredicate([&Event](const FAMReactionRule& Rule) { return Rule.Event == Event; }))
	{
		AddWorldEntry(TEXT("The HUB (world actions)"), false);
	}

	// In the Machines region: what it is, the rows (scrolling, the region's height), and the progress while they are made.
	const FSlateFontInfo Font = GetRowFont();
	UVerticalBox* Box = mMachinesBox;
	const int32 Total = mExpandedEntries.Num();
	const FString Title = FString::Printf(TEXT("%s %s: %d, nearest first"), bExpandedRingers ? TEXT("Rung by") : TEXT("Listening to"),
		*Event, Total);
	if (UVerticalBoxSlot* TitleSlot = Box->AddChildToVerticalBox(AMUI::MakeText(WidgetTree, Title, AMUI::SizedFont(Font, 13), AMUI::Grey)))
	{
		TitleSlot->SetPadding(FMargin(2.f, 0.f, 0.f, 4.f));
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
	mExpandedRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(mExpandedRows);
	if (UVerticalBoxSlot* ScrollSlot = Box->AddChildToVerticalBox(Scroll))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UHorizontalBox* Loading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	mExpandedProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(FSlateColorBrush(AMUI::Field));
	BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
	mExpandedProgress->SetWidgetStyle(BarStyle);
	mExpandedProgress->SetFillColorAndOpacity(AMUI::Orange);
	mExpandedProgress->SetPercent(0.f);
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarSize->SetHeightOverride(8.f);
	BarSize->AddChild(mExpandedProgress);
	if (UHorizontalBoxSlot* BarSlot = Loading->AddChildToHorizontalBox(BarSize))
	{
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BarSlot->SetVerticalAlignment(VAlign_Center);
	}
	mExpandedProgressText = AMUI::MakeText(WidgetTree, TEXT(""), AMUI::SizedFont(Font, 13), AMUI::Grey);
	if (UHorizontalBoxSlot* TextSlot = Loading->AddChildToHorizontalBox(mExpandedProgressText))
	{
		TextSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
	mExpandedLoading = Loading;
	if (UVerticalBoxSlot* LoadingSlot = Box->AddChildToVerticalBox(Loading))
	{
		LoadingSlot->SetPadding(FMargin(2.f, 4.f, 0.f, 0.f));
	}
	BuildExpandedRows();
}

void UAMAutomationTab::BuildExpandedRows()
{
	if (!mExpandedRows)
	{
		return;
	}
	const FSlateFontInfo Font = GetRowFont();
	const int32 Total = mExpandedEntries.Num();
	const int32 End = FMath::Min(Total, mExpandedBuilt + AMAT_ExpandBatch);
	for (; mExpandedBuilt < End; ++mExpandedBuilt)
	{
		const FAMOverviewEntry& Entry = mExpandedEntries[mExpandedBuilt];
		UAMOverviewHandler* Handler = NewObject<UAMOverviewHandler>(this);
		Handler->Tab = this;
		Handler->Event = mExpandedEvent;
		Handler->bRingers = bExpandedRingers;
		Handler->Machine = Entry.Machine;
		Handler->bHub = Entry.bHub;
		Handler->bMam = Entry.bMam;
		Handler->bOne = true;
		Handler->TargetName = Entry.Name;
		mOverviewHandlers.Add(Handler);

		// [icon] [name] [distance] [locate] [bin]
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* Back = AMUI::MakeBox(WidgetTree, AMUI::Field, FMargin(8.f, 0.f));
		Back->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Back->AddChild(Line);
		UImage* Icon = nullptr;
		if (UTexture2D* Texture = Entry.Icon.Get())
		{
			Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Icon->SetBrushFromTexture(Texture, false);
			Icon->SetDesiredSizeOverride(FVector2D(26.f, 26.f));
		}
		else
		{
			Icon = AMUI::MakeImage(WidgetTree, AMAT_TabIconPath, FVector2D(22.f, 22.f));
		}
		if (UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(Icon))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		USizeBox* NameClip = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NameClip->SetClipping(EWidgetClipping::ClipToBounds);
		NameClip->AddChild(AMUI::MakeText(WidgetTree, Entry.Name, Font, AMUI::White));
		if (UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(NameClip))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}
		const FString Distance = Entry.Distance < 0.f ? FString() : FString::Printf(TEXT("%.0f m"), Entry.Distance / 100.f);
		if (UHorizontalBoxSlot* DistanceSlot = Line->AddChildToHorizontalBox(AMUI::MakeText(WidgetTree, Distance, Font, AMUI::Grey)))
		{
			DistanceSlot->SetVerticalAlignment(VAlign_Center);
			DistanceSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		USizeBox* BackSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BackSize->SetHeightOverride(AMAT_ExpandRowHeight);
		BackSize->AddChild(Back);
		if (UHorizontalBoxSlot* BackSlot = Row->AddChildToHorizontalBox(BackSize))
		{
			BackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		auto AddIconButton = [&](UButton* Button)
		{
			USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			ButtonSize->SetWidthOverride(AMRowLayout::Remove + 10.f);
			ButtonSize->SetHeightOverride(AMAT_ExpandRowHeight);
			ButtonSize->AddChild(Button);
			if (UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(ButtonSize))
			{
				ButtonSlot->SetPadding(FMargin(AMRowLayout::Gap, 0.f, 0.f, 0.f));
			}
		};
		// Locate: the bin button's look with a map pin instead of the bin. Not for the HUB or the MAM
		// (world entries), nor for a machine already this close (it would end at once): an empty space.
		constexpr float AMAT_NoLocateCloserThan = 800.f;
		const bool bCanLocate = !Entry.bHub && Entry.Distance > AMAT_NoLocateCloserThan;
		if (!bCanLocate)
		{
			USizeBox* Space = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Space->SetWidthOverride(AMRowLayout::Remove + 10.f);
			if (UHorizontalBoxSlot* SpaceSlot = Row->AddChildToHorizontalBox(Space))
			{
				SpaceSlot->SetPadding(FMargin(AMRowLayout::Gap, 0.f, 0.f, 0.f));
			}
		}
		UButton* Locate = bCanLocate ? AMUI::MakeRemoveButton(WidgetTree, Font) : nullptr;
		if (Locate)
		{
			USizeBox* PinSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			PinSize->SetWidthOverride(20.f);
			PinSize->SetHeightOverride(20.f);
			PinSize->AddChild(AMUI::MakeImage(WidgetTree, AMAT_LocateIconPath, FVector2D(20.f, 20.f)));
			Locate->SetContent(PinSize);
			Locate->SetToolTipText(FText::FromString(TEXT("Locate")));
			Locate->OnHovered.AddDynamic(Handler, &UAMOverviewHandler::OnLocateHovered);
			if (UButtonSlot* PinSlot = Cast<UButtonSlot>(PinSize->Slot))
			{
				PinSlot->SetHorizontalAlignment(HAlign_Center);
				PinSlot->SetVerticalAlignment(VAlign_Center);
			}
			Locate->OnClicked.AddDynamic(Handler, &UAMOverviewHandler::OnLocateClicked);
			AddIconButton(Locate);
		}
		// One machine at a time: a click removes it (no hold; an undo may come later).
		UButton* Bin = AMUI::MakeRemoveButton(WidgetTree, Font);
		Bin->OnClicked.AddDynamic(Handler, &UAMOverviewHandler::OnSingleBinClicked);
		Bin->OnHovered.AddDynamic(Handler, &UAMOverviewHandler::OnBinHovered);
		AddIconButton(Bin);

		if (UVerticalBoxSlot* RowSlot = mExpandedRows->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, mExpandedBuilt == 0 ? 0.f : 2.f, 0.f, 0.f));
		}
	}

	// Progress while there are more; the next batch on the next tick.
	const bool bMore = mExpandedBuilt < Total;
	if (mExpandedLoading)
	{
		mExpandedLoading->SetVisibility(bMore ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bMore)
	{
		if (mExpandedProgress)
		{
			mExpandedProgress->SetPercent(Total > 0 ? static_cast<float>(mExpandedBuilt) / Total : 1.f);
		}
		if (mExpandedProgressText)
		{
			mExpandedProgressText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), mExpandedBuilt, Total)));
		}
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UAMAutomationTab> WeakThis(this);
			const int32 Generation = mExpandGeneration;
			World->GetTimerManager().SetTimerForNextTick([WeakThis, Generation]()
			{
				UAMAutomationTab* Tab = WeakThis.Get();
				if (Tab && Tab->mExpandGeneration == Generation)
				{
					Tab->BuildExpandedRows();
				}
			});
		}
	}
}

void UAMAutomationTab::ExplainOverviewBin(const FString& Event, bool bRingers, bool bOne, const FString& TargetName)
{
	if (bOne)
	{
		ShowInfo(FText::FromString(FString::Printf(TEXT("Remove from %s"), *TargetName)), FText::FromString(bRingers
			? FString::Printf(TEXT("Removes the alarm %s from %s only: its alarm rows with this name are deleted, at once."), *Event, *TargetName)
			: FString::Printf(TEXT("Removes the actions of %s on the alarm %s (a sign: its design for it), at once."), *TargetName, *Event)));
		return;
	}
	int32 Ringers = 0;
	int32 Listeners = 0;
	if (const AAMAutomationSubsystem* Subsystem = AAMAutomationSubsystem::Get(this))
	{
		Subsystem->CountEventUses(Event, Ringers, Listeners);
	}
	ShowInfo(FText::FromString(bRingers ? TEXT("Remove from the machines ringing it") : TEXT("Remove from the listeners")),
		FText::FromString(bRingers
			? FString::Printf(TEXT("Removes the alarm %s from the %d machine%s that ring it: their alarm rows with this name are deleted.\n\nHold the bin down for 2.5 seconds to do it."), *Event, Ringers, Ringers == 1 ? TEXT("") : TEXT("s"))
			: FString::Printf(TEXT("Removes every action on the alarm %s from the %d listener%s (machines, the HUB), and the signs' designs for it.\n\nHold the bin down for 2.5 seconds to do it."), *Event, Listeners, Listeners == 1 ? TEXT("") : TEXT("s"))));
}

namespace
{
	/** How long a bin is held down to remove (seconds). */
	constexpr double AMAT_HoldSeconds = 2.5;
}

void UAMAutomationTab::StartOverviewHold(const FString& Event, bool bRingers, AFGBuildable* OnlyMachine, bool bOnlyHub, bool bOne, const FString& TargetName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	mHoldEvent = Event;
	bHoldRingers = bRingers;
	mHoldMachine = OnlyMachine;
	bHoldHub = bOnlyHub;
	bHoldOne = bOne;
	mHoldTargetName = TargetName;
	bHolding = true;
	mHoldStart = FPlatformTime::Seconds();
	if (!mHoldBin)
	{
		mHoldBin = CreateWidget<UAMHoldBin>(this, UAMHoldBin::StaticClass());
	}
	if (mHoldBin)
	{
		mHoldBin->SetLabel(bOne ? FString::Printf(TEXT("Hold to remove %s from %s"), *Event, *TargetName)
			: FString::Printf(TEXT("Hold to remove %s from %s"), *Event, bRingers ? TEXT("the machines ringing it") : TEXT("its listeners")));
		mHoldBin->SetProgress(0.f);
		if (!mHoldBin->IsInViewport())
		{
			mHoldBin->AddToViewport(10000);
		}
	}
	World->GetTimerManager().SetTimer(mHoldTimer, this, &UAMAutomationTab::TickOverviewHold, 1.f / 60.f, true);
}

void UAMAutomationTab::TickOverviewHold()
{
	if (!bHolding)
	{
		StopOverviewHold(false);
		return;
	}
	// Let go somewhere the button did not hear it: stop.
	if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		StopOverviewHold(false);
		return;
	}
	const float Progress = static_cast<float>((FPlatformTime::Seconds() - mHoldStart) / AMAT_HoldSeconds);
	if (mHoldBin)
	{
		mHoldBin->SetProgress(Progress);
	}
	if (Progress >= 1.f)
	{
		StopOverviewHold(true);
	}
}

void UAMAutomationTab::StopOverviewHold(bool bDone)
{
	const bool bWasHolding = bHolding;
	bHolding = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(mHoldTimer);
	}
	if (mHoldBin && mHoldBin->IsInViewport())
	{
		mHoldBin->RemoveFromParent();
	}
	if (!bWasHolding)
	{
		return;
	}
	if (bHoldGroup)
	{
		// An alarm's actions on this page only.
		bHoldGroup = false;
		if (bDone)
		{
			RemoveActionGroup(mHoldEvent);
		}
		else
		{
			ShowInfo(FText::FromString(TEXT("Not removed")), FText::FromString(TEXT("Hold the bin down until the big bin is full (2.5 seconds) to remove all the actions of this alarm.")));
		}
		return;
	}
	if (bDone && bHoldOne)
	{
		// One machine (or the HUB): never all of them, also if that machine is gone meanwhile.
		AFGBuildable* Target = mHoldMachine.Get();
		if (Target || bHoldHub)
		{
			UAMRemoteCallObject::RequestClearAlarm(GetOwningPlayer(), mHoldEvent, bHoldRingers, Target, bHoldHub);
			ShowInfo(FText::FromString(mHoldEvent), FText::FromString(FString::Printf(TEXT("Removed from %s."), *mHoldTargetName)));
		}
	}
	else if (bDone)
	{
		UAMRemoteCallObject::RequestClearAlarm(GetOwningPlayer(), mHoldEvent, bHoldRingers);
		ShowInfo(FText::FromString(mHoldEvent), FText::FromString(bHoldRingers ? TEXT("Removed from the machines that rang it.") : TEXT("Removed from every listener.")));
	}
	else
	{
		ShowInfo(FText::FromString(TEXT("Not removed")), FText::FromString(TEXT("Hold the bin down until the big bin is full (2.5 seconds) to remove.")));
	}
	if (bOverviewDirty)
	{
		RebuildOverview();
	}
}

void UAMOverviewHandler::OnBinPressed()
{
	if (Tab)
	{
		Tab->StartOverviewHold(Event, bRingers, bOne ? Machine.Get() : nullptr, bHub, bOne, TargetName);
	}
}

void UAMOverviewHandler::OnCountClicked()
{
	if (Tab)
	{
		Tab->OnOverviewCountClicked(Event, bRingers);
	}
}

void UAMOverviewHandler::OnCountHovered()
{
	if (Tab)
	{
		Tab->ExplainOverviewCount(Event, bRingers);
	}
}

void UAMOverviewHandler::OnSingleBinClicked()
{
	if (Tab)
	{
		Tab->RemoveFromOne(Event, bRingers, Machine.Get(), bHub, bMam, TargetName);
	}
}

void UAMOverviewHandler::OnLocateHovered()
{
	if (Tab)
	{
		Tab->ExplainOverviewLocate(TargetName);
	}
}

void UAMOverviewHandler::OnLocateClicked()
{
	if (Tab)
	{
		Tab->OnOverviewLocate(Machine.Get(), bHub, TargetName);
	}
}

void UAMOverviewHandler::OnBinReleased()
{
	if (Tab)
	{
		Tab->StopOverviewHold(false);
	}
}

void UAMOverviewHandler::OnBinHovered()
{
	if (Tab)
	{
		Tab->ExplainOverviewBin(Event, bRingers, bOne, TargetName);
	}
}

void UAMOverviewHandler::OnNameHovered()
{
	if (Tab)
	{
		Tab->ShowAlarmInfo(Event);
	}
}
