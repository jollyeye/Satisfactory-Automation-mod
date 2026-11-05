#include "UI/AMAutomationTab.h"
#include "AutomatedMachinesModule.h"
#include "UI/AMRuleRow.h"
#include "UI/AMUIHelpers.h"
#include "Automation/AMAutomationSubsystem.h"
#include "Net/AMRemoteCallObject.h"
#include "Buildables/FGBuildable.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
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

	if (bWorldMode)
	{
		// The HUB rings no alarms yet (world events come later), so no add button.
		MakePanel(TEXT("Alarms"), TEXT("Alarm Name"), AMRowLayout::AlarmName, TEXT("World Event"), AMRowLayout::MachineEvent,
			FString(), mFireRowsBox, 0.f, AMAT_AlarmsWidth);
	}
	else
	{
		MakePanel(TEXT("Alarms"), TEXT("Alarm Name"), AMRowLayout::AlarmName, TEXT("Machine Event"), AMRowLayout::MachineEvent,
			TEXT("+ Add alarm"), mFireRowsBox, 0.f, AMAT_AlarmsWidth)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnAddFireClicked);
	}
	MakePanel(TEXT("Actions"), TEXT("Do"), AMRowLayout::Do, TEXT("On Alarm"), AMRowLayout::OnAlarm,
		TEXT("+ Add action"), mReactionRowsBox, AMAT_PanelGap, AMAT_ActionsWidth)->OnClicked.AddDynamic(this, &UAMAutomationTab::OnAddReactionClicked);

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

		mPassWrongButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AMOptionPassWrong"));
		AMUI::SetButtonColor(mPassWrongButton, AMUI::Button);
		mPassWrongButton->OnClicked.AddDynamic(this, &UAMAutomationTab::OnPassWrongClicked);
		mPassWrongButton->OnHovered.AddDynamic(this, &UAMAutomationTab::OnPassWrongHovered);
		OptionsBody->AddChild(mPassWrongButton);

		UHorizontalBox* OptionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UButtonSlot* RowSlot = Cast<UButtonSlot>(mPassWrongButton->AddChild(OptionRow)))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* OptionName = MakeText(TEXT("Pass wrong items through"), 14, AMUI::White);
		mAddLabels.Add(OptionName);
		if (UHorizontalBoxSlot* NameSlot = OptionRow->AddChildToHorizontalBox(OptionName))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}
		// 2:1 like the textures; the size box keeps it from being squeezed.
		mPassWrongSwitch = AMUI::MakeImage(WidgetTree, AMUI::SwitchOffPath, FVector2D(44.f, 22.f));
		USizeBox* SwitchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SwitchSize->SetWidthOverride(44.f);
		SwitchSize->SetHeightOverride(22.f);
		SwitchSize->AddChild(mPassWrongSwitch);
		if (UHorizontalBoxSlot* SwitchSlot = OptionRow->AddChildToHorizontalBox(SwitchSize))
		{
			SwitchSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
			SwitchSlot->SetVerticalAlignment(VAlign_Center);
		}
		RefreshOptions();
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
	if (bWorldMode)
	{
		ShowInfo(FText::FromString(TEXT("Actions for the whole world")),
			FText::FromString(TEXT("The HUB hears every alarm in the world. These actions need no machine: they happen whenever the alarm rings, rung by any machine.\n\nClick an action or an alarm to see what it does.")));
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
		mReactions = Subsystem->GetWorldActions();
	}
	else if (const FAMMachineRules* Rules = Subsystem->FindRules(Machine))
	{
		mFires = Rules->Fires;
		mReactions = Rules->Reactions;
		mOptions = Rules->Options;
	}
	bRulesLoaded = true;
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

void UAMAutomationTab::OpenItemPicker(UAMRuleRow* Row, UWidget* Anchor, UClass* Current)
{
	CloseItemPicker();
	if (!Row || !Anchor || !mPopupLayer || !mRootOverlay)
	{
		return;
	}
	constexpr float PopupWidth = 300.f;
	constexpr float PopupHeight = 340.f;
	const FSlateFontInfo Font = GetRowFont();

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

	// The window: a dark box with a scrolling list.
	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WindowSize->SetWidthOverride(PopupWidth);
	WindowSize->SetHeightOverride(PopupHeight);
	UBorder* Window = AMUI::MakeBox(WidgetTree, AMUI::SidePanel, FMargin(4.f));
	WindowSize->AddChild(Window);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
	Window->AddChild(Scroll);
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(List);

	const TArray<UClass*> Items = GetPickableItems();
	if (Items.Num() == 0)
	{
		List->AddChildToVerticalBox(AMUI::MakeText(WidgetTree, TEXT("No items known yet."), Font, AMUI::Grey));
	}
	for (UClass* Item : Items)
	{
		UAMChoice* Handler = NewObject<UAMChoice>(Row);
		Handler->Row = Row;
		Handler->Kind = EAMChoiceKind::Item;
		Handler->ItemClass = Item;
		mPopupHandlers.Add(Handler);

		UButton* Option = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		AMUI::SetButtonColor(Option, AMUI::Option);
		Option->OnClicked.AddDynamic(Handler, &UAMChoice::OnClicked);
		Option->OnHovered.AddDynamic(Handler, &UAMChoice::OnHovered);
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UButtonSlot* LineSlot = Cast<UButtonSlot>(Option->AddChild(Line)))
		{
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
			LineSlot->SetPadding(FMargin(6.f, 3.f));
		}
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UTexture2D* Texture = UFGItemDescriptor::GetSmallIcon(Item))
		{
			Icon->SetBrushFromTexture(Texture, false);
		}
		Icon->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSize->SetWidthOverride(28.f);
		IconSize->SetHeightOverride(28.f);
		IconSize->AddChild(Icon);
		if (UHorizontalBoxSlot* IconSlot = Line->AddChildToHorizontalBox(IconSize))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* Name = AMUI::MakeText(WidgetTree, UFGItemDescriptor::GetItemName(Item).ToString(), Font, Item == Current ? AMUI::Orange : AMUI::White);
		if (UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name))
		{
			NameSlot->SetVerticalAlignment(VAlign_Center);
			NameSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* OptionSlot = List->AddChildToVerticalBox(Option))
		{
			OptionSlot->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
	}

	// Under the dropdown, or above it if there is no room below; inside the page.
	const FGeometry& PageGeometry = mRootOverlay->GetCachedGeometry();
	const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
	const FVector2D PageSize = PageGeometry.GetLocalSize();
	const FVector2D AnchorPosition = AnchorGeometry.GetAbsolutePosition();
	const FVector2D AnchorSize = AnchorGeometry.GetAbsoluteSize();
	const FVector2D AnchorTopLeft = PageGeometry.AbsoluteToLocal(AnchorPosition);
	const FVector2D AnchorBottomRight = PageGeometry.AbsoluteToLocal(AnchorPosition + AnchorSize);
	float X = static_cast<float>(AnchorTopLeft.X);
	float Y = static_cast<float>(AnchorBottomRight.Y) + 2.f;
	if (PageSize.Y > 0.0 && Y + PopupHeight > PageSize.Y)
	{
		Y = FMath::Max(0.f, static_cast<float>(AnchorTopLeft.Y) - PopupHeight - 2.f);
	}
	if (PageSize.X > 0.0)
	{
		X = FMath::Clamp(X, 0.f, FMath::Max(0.f, static_cast<float>(PageSize.X) - PopupWidth));
	}
	if (UCanvasPanelSlot* WindowSlot = mPopupLayer->AddChildToCanvas(WindowSize))
	{
		WindowSlot->SetAnchors(FAnchors(0.f, 0.f));
		WindowSlot->SetAlignment(FVector2D::ZeroVector);
		WindowSlot->SetAutoSize(true);
		WindowSlot->SetPosition(FVector2D(X, Y));
	}
	mPopup = WindowSize;
	mPopupLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UAMAutomationTab::CloseItemPicker()
{
	if (mPopupLayer)
	{
		mPopupLayer->ClearChildren();
		mPopupLayer->SetVisibility(ESlateVisibility::Collapsed);
	}
	mPopup = nullptr;
	mPopupHandlers.Reset();
}

void UAMAutomationTab::OnPopupBackgroundClicked()
{
	CloseItemPicker();
}

void UAMAutomationTab::RebuildRows()
{
	CloseItemPicker();
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
	if (mFires.Num() == 0 && !bWorldMode)
	{
		if (mFireRowsBox)
		{
			AddRowWidget(mFireRowsBox, AMUI::MakeText(WidgetTree, TEXT("No alarms yet."), HintFont, AMUI::Grey));
		}
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
	if (bWorldMode)
	{
		UAMRemoteCallObject::RequestSetWorldActions(GetOwningPlayer(), mReactions);
	}
	else if (AFGBuildable* Machine = Cast<AFGBuildable>(GetMachine()))
	{
		UAMRemoteCallObject::RequestSetRules(GetOwningPlayer(), Machine, mFires, mReactions, mOptions);
	}
}

void UAMAutomationTab::RefreshOptions()
{
	AMUI::SetImage(mPassWrongSwitch, mOptions.bPassWrongItems ? AMUI::SwitchOnPath : AMUI::SwitchOffPath);
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

void UAMAutomationTab::OnAddFireClicked()
{
	FAMFireRule& NewRule = mFires.AddDefaulted_GetRef();
	// The first machine event this machine has (a container has no "Output full").
	for (uint8 Value = 0; Value < static_cast<uint8>(EAMCondition::Count); ++Value)
	{
		if (AMRules::IsConditionAvailable(static_cast<EAMCondition>(Value), GetMachine()))
		{
			NewRule.Condition = static_cast<EAMCondition>(Value);
			NewRule.Value = AMRules::GetConditionValueInfo(NewRule.Condition).Default;
			break;
		}
	}
	RebuildRows();
}

void UAMAutomationTab::OnAddReactionClicked()
{
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
			mMachineText->SetText(FText::FromString(TEXT("The HUB hears every alarm in the world, and does these actions when one rings.")));
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
	bWorldMode = Host && Host->GetClass()->GetName() == AMAT_HubWindowClass;
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

void UAMAutomationTab::OnWindowTabButtonClicked(int32 ButtonIndex)
{
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
	const FText Title = FText::FromString(AMAT_TabTitle);
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
