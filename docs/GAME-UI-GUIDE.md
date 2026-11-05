# Adding to the game's own windows: a recipe (Satisfactory 1.x, SML 3.12)

How Automated Machines adds an "Automation" tab to the game's machine windows, written so the next
person (or AI) can do the same for another window in one or two builds instead of ten. Every step
below was tested in game on 4-5 Nov 2026 unless marked otherwise.

The official SML documentation covers **Widget Blueprint Hooks** (adding a widget of yours into a
widget of the game: <https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/WidgetBlueprintHooks.html>).
It says nothing about the game's tab system, window frames or inventory panel: everything about
those below was found by dumping the windows at run time.

## 1. Find the window of a building

1. The building's blueprint names its window. On the dev PC the starter project has placeholder
   copies of every asset under `D:\SFMods\SML\Content\FactoryGame\...`. Run `strings` on the
   building's `.uasset` and look next to `mInteractWidgetSoftClass`:
   - `Build_SmelterMk1` -> `/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Manufacturing.Widget_Manufacturing_C`
     (Smelter, Constructor, Assembler, Foundry, Manufacturer, Refinery, Blender, Packager,
     Particle Accelerator, Converter, Quantum Encoder)
   - `Build_MinerMk1` -> `/Game/FactoryGame/Interface/UI/InGame/Widget_ResourceExtractorUI.Widget_ResourceExtractorUI_C`
   - HUB terminal -> `/Game/FactoryGame/Buildable/Factory/TradingPost/UI/BPW_Hub.BPW_Hub_C`
2. `strings` on the window's own `.uasset` gives its widget and variable names (e.g. whether it has
   `Widget_SlidingTabs`), but not the tree. For the tree, use the dump.

## 2. Dump the window at run time

`Source/AutomatedMachines/Private/Debug/AMWidgetDump.cpp` (development only, remove before release):

- Put the window class in `AMWD_TreeClasses` (e.g. `Widget_ResourceExtractorUI_C`) and the classes
  whose variables and functions you want in `AMWD_ReflectionClasses` (the frame
  `Widget_Window_DarkMode_C`, `Widget_TabsContainer_C`, `Widget_SlidingTabs_C`,
  `Widget_SlidingTabs_Button_C`).
- Build, open the window in game for a few seconds, close the game.
- Read `C:\Users\magikgpdwin4\AppData\Local\FactoryGame\Saved\Logs\FactoryGame.log`, lines
  `[WidgetDump]`: every widget with its class, slot type and visibility; then variables (with values)
  and functions of the reflection classes.

## 3. What every game window has: the dark frame

All the windows above use the same frame, `Widget_Window_DarkMode_C`. The window's variable for it is
usually `Widget_Window_DarkMode`, but the storage window calls it `WidgetWindowDarkMode`: find it by
class (first object variable holding a `Widget_Window_DarkMode_C`), not by name. Useful parts, all reached by reflection (they are Blueprint):

| Name | What |
|---|---|
| `mTabs` (array of struct: a Text and a Texture field) | the tab list; the tab buttons are made from it |
| `InitTabs()` | makes the tab buttons from `mTabs` (not always enough, see 5) |
| `OnTabButtonClicked(int32)` | broadcast when a tab button is clicked (production window: yes; miner window: no) |
| `SetInventoryVisibility(bool Show, bool Animate)` | shows / hides the inventory panel on the right |
| `WindowBody` (NamedSlot) | holds the window's page; its parent is the `WindowHBox` row, next to `mInventorySizeBox` |
| `Widget_TabsContainer` | the tab bar: `mContainer` (CanvasPanel with the buttons), `mButtons` (its own copy of the tab list), `GenerateButtons()`, `SetActiveButton(Button)`, `mButtonSize` (220 x 36) |
| tab button `Widget_SlidingTabs_Button_C` | `mTitleObject` (TextBlock: copy its font for your texts), `mButton` (the plain UButton), `SetTitle(FText)`, `SetIcon(UTexture*)`, `OnClicked(Instigator)` |

The machine is the window's `mInteractObject` (from `UFGInteractWidget`); its display name is the
protected `mDisplayName`, read by reflection.

## 4. Kind A: windows with sliding pages (production machines, HUB)

The window has `Widget_SlidingTabs` with its pages side by side in a HorizontalBox
(`HorizontalBox_1` in `Widget_Manufacturing_C`, `HorizontalBox_25` in `BPW_Hub_C`).

1. **Hook** (C++, `UI/AMWidgetHooks.cpp`): a `UWidgetBlueprintHookData` on the window class,
   `ParentWidgetName` = the pages box, `Direct_Any`, a `UWidgetBlueprintHookSlot_Generic` (fill,
   automatic size). Register it with `UWidgetBlueprintHookManager` from
   `FWorldDelegates::OnPostWorldInitialization` (once). Our page is then the last page.
2. **Tab entry**: in `NativeConstruct` (not `NativeOnInitialized`: the frame is still null then),
   copy the last `mTabs` entry, set its Text to "Automation" and its texture field to our icon.
3. **Buttons**: on the next tick, if the frame has fewer buttons than tabs, call `InitTabs()`.
4. **Clicks**: bind the frame's `OnTabButtonClicked`; for our index, make sure the sliding pages know
   our page (`SetupIndexOffset`, or compute the offset from the previous page) and call
   `SetActiveIndex(Index, Animate)`. Bind `OnActiveTabChanged(Previous, Active)` too.
5. **Inventory**: our page has its own info panel, so hide the inventory one frame after our page
   becomes active (`SetInventoryVisibility(false, true)`), and show it again when going back to the
   page that had it (`mManufacturingTab`); the game does not do that itself when coming from us.

## 5. Kind B: one-page windows (miners)

`Widget_ResourceExtractorUI_C` has no sliding pages: `WindowBody` holds one CanvasPanel
(`CanvasPanel_3`) and the frame shows a single tab button as a title.

1. **Hook** into the page (`CanvasPanel_3`), and start **collapsed** (we would otherwise show at 0,0
   inside the miner's page for a frame).
2. On the tick after construct, **move only our widget** into the frame's row (`WindowBody`'s
   parent, `WindowHBox`). **Never reparent the game's page**: moving `CanvasPanel_3` into an overlay
   rebuilt its child widgets and they showed their designer defaults ("999 Nuclear Uranium Waste",
   placeholder icon).
3. **Tab entry**: the window fills `mTabs` in its own construct, after ours, so `mTabs` is empty in
   our `NativeConstruct`. Add the entry on the next tick (retry for a few ticks while empty).
4. **Buttons**: `InitTabs()` does not make a second button here. Copy the frame's `mTabs` into the
   tabs container's `mButtons` (`FArrayProperty::CopyCompleteValue`, same struct) and call its
   `GenerateButtons()`.
5. The buttons come out stretched over the whole bar (anchors 0..1, width 0) and `HitTestInvisible`
   (a title, not a button). Lay them out yourself in the container's canvas: anchors (0,0), no auto
   size, position `i * mButtonSize.X`, size `mButtonSize`, visibility `SelfHitTestInvisible`.
6. Every button gets the window's title and the default icon: call `SetTitle` / `SetIcon` on ours
   (again after each click, in case the window sets them back).
7. **Clicks**: neither the frame's `OnTabButtonClicked` nor the tab button's own `OnClicked` fire
   (the click sound plays). Bind the plain `UButton` inside each tab button (`mButton`,
   `OnClicked.AddUniqueDynamic`), and call the container's `SetActiveButton(Button)` for the
   highlight.
8. **Switching**: show ours = `WindowBody` collapsed, us visible, inventory hidden; back = the
   reverse. Visibility only, nothing is moved.
9. **Size**: our page keeps its own width (the window fits around whichever page shows); its height
   is set to the machine page's height, measured while that page shows (`USizeBox::SetHeightOverride`).
10. Reopening the window (it can be pooled): construct runs again; the setup finds us already in the
    row and only resets to the machine's page.

Windows done this way (tested): the miner (`Widget_ResourceExtractorUI_C`, hook parent `CanvasPanel_3`)
and the storage containers (`Widget_Storage_C`, hook parent `mInventoryVertBox`, a VerticalBox: no
slot setup, see 8; its frame variable is `WidgetWindowDarkMode`).

Code: `UAMAutomationTab::SetupSinglePage`, `RegenerateTabButtons`, `SetOurTabButtonLook`,
`BindSingleTabButtons`, `SelectSingleTab`, `ShowSinglePage` in `UI/AMAutomationTab.cpp`.

## 6. Our own widgets in the game's look

- Build everything in C++ (`UI/AMUIHelpers.cpp`): flat `FSlateColorBrush` colours sampled from game
  screenshots, the game's font copied from a TextBlock of the window (`mRecipeName`, else the tab
  button's `mTitleObject`), small textures made by `Tools/IconSource/make_icons.py` and imported by
  `Tools/create_assets.py`.
- Wide images: set the brush `ImageSize` and put the image in a SizeBox, or they are drawn square.
- Typing capitals live: a `UEditableText` subclass that changes its Slate text while typing; UMG
  `SetText` in the commit event is ignored until the window is reopened.
- Typing in our fields does not close the window with E (tested).

## 7. Other game systems we hooked into (tested)

- **Notifications** stacked at the side: a `UFGPushNotificationWidget` subclass given to
  `UFGGameUI::PushNotificationWidget` (from `AFGHUD::GetGameUI`); fallback `ShowTextNotification`.
- **Sounds**: Wwise `UAkAudioEvent::PostOnActor` called by name through reflection (no Wwise build
  dependency); event paths under `/Game/WwiseAudio/Events/...`.
- **Belts**: `SUBSCRIBE_METHOD` on `UFGFactoryConnectionComponent::Factory_GrabOutput` and
  `Factory_PeekOutput` (call the original first, add our item only if it had nothing). Belts may call
  these from worker threads: keep the hook cheap and lock shared data.
- **Saving without touching the game's objects**: one `AModSubsystem` with `IFGSaveInterface` holds
  all our data, so the mod can be removed and the save still loads.

## 8. Build pitfalls (each one cost a build)

- **SML 3.12 bug (crash at game start):** `UWidgetBlueprintHookSlot_Generic` crashes when the hook's
  parent is a **VerticalBox** (`SetupPanelSlot` calls `HorizontalBoxSlot->SetSize` on null in its
  VerticalBox branch; stack: `UHorizontalBoxSlot::SetSize` < `SetupPanelSlot` < `RegisterWidgetBlueprintHook`).
  Hooking into a VerticalBox: give no `SlotConfiguration` (set the slot in code later), or pick an
  HorizontalBox / Overlay / Canvas parent.
- Game headers are in subfolders of `SML/Source/FactoryGame/Public` (`Buildables/...`): check the
  path before including.

- C4458 / C4459 are errors: no local or parameter named like a base-class member (`Padding`, `Text`)
  or like a global / namespace name (`Button` inside `AMUI`).
- Unity builds merge `.cpp` files: anonymous-namespace helpers need file-specific prefixes.
- No non-ASCII characters in source files.
- The window frame is null in `NativeOnInitialized`; do window work in `NativeConstruct` or later.
