# Developer notes (Automated Machines)

Notes for the next session working on this mod. The design (what the mod does and why) is in `DESIGN.md`. **Adding to the game's windows** (tabs, pages, inventory panel): read `GAME-UI-GUIDE.md` first, it is the tested recipe.

## What works (tested in game)

| Feature | Tested |
|---|---|
| Automation tab in the production window (Smelter, Constructor; same window for Assembler, Foundry, Manufacturer, Refinery, Blender, Packager, Particle Accelerator, Converter, Quantum Encoder) | 4 Nov 2026 |
| Alarms and actions, saved and loaded; typing E in fields does not close the window | 4 Nov 2026 |
| All production machine events and actions (Delete input/output, Change recipe, Set clock speed...) | 4 Nov 2026 |
| Marker above automated machines, tab icon | 4 Nov 2026 |
| HUB terminal Automation tab: Show message (stacked notifications), Play sound (5 sounds) | 4 Nov 2026 |
| Wrong input / Input correct; Pass wrong items through (straight onto the output belt, products first), also on the Assembler (2 inputs) | 5 Nov 2026 |
| Options strip with the game-like switch | 5 Nov 2026 |
| Miner: Automation tab in its one-page window (tabs side by side, switching, same height), its events and actions, Delete output | 5 Nov 2026 |
| Storage Container / Industrial Storage Container: Automation tab (one-page window, frame named `WidgetWindowDarkMode`), same height | 5 Nov 2026 |
 Many lessons from Road Maker apply here too: see `docs/DEV-NOTES.md` in `jollyeye/Satisfactory-Road-Fence-Tool`.

## Setup on the developer's PC (Windows)

| What                      | Where                                                                    |
|---------------------------|--------------------------------------------------------------------------|
| SML starter project       | `D:\SFMods\SML\FactoryGame.uproject` (shared with Road Maker)            |
| This mod (git clone)      | `D:\SFMods\SML\Mods\AutomatedMachines`                                   |
| Unreal Engine (CSS build) | `D:\UE-CSS` (or `D:\UE-CSS\Unreal Engine - CSS`)                         |
| Game install              | `D:\games\Satisfactory`                                                  |
| One-click update script   | `D:\SFMods\am-update-and-package.bat` (copy of `Tools/am-update-and-package.bat`: git pull, then runs `Tools/build-and-package.bat` from the repo) |
| Script logs               | `D:\SFMods\am-update-log.txt`, `am-assets-log.txt` (our asset script), `am-assets-editor-log.txt` (editor output) |
| Game log                  | `C:\Users\magikgpdwin4\AppData\Local\FactoryGame\Saved\Logs\FactoryGame.log` |

GitHub: `jollyeye/Satisfactory-Automation-mod` (branch `main`). Use the capitalised URL `https://github.com/jollyeye/Satisfactory-Automation-mod.git`.

The update script and its log have an `am-` prefix so they do not overwrite Road Maker's. The mod's own log lines are `LogAutomatedMachines`.

## How we work

Same loop as Road Maker:

1. Claude edits the code in its own clone, commits and pushes to `main`.
2. The developer closes the game and runs `am-update-and-package.bat`: git pull, build the editor, package into the game.
3. If it prints "FAILED", Claude reads `D:\SFMods\am-update-log.txt` through the desktop app's file bridge and fixes the compile error.
4. The developer tests in game and reports. Claude reads `FactoryGame.log` (`LogAutomatedMachines` lines) when something is unclear.

Claude cannot compile; keep edits careful (count braces, check renames). Shadowing is an error (C4458): a local variable named like a member of a base class, e.g. `Padding` in a `UUserWidget`, fails the build. The same for a name hiding a global or namespace one (C4459), e.g. a parameter `Button` inside `AMUI`, which has the colour `AMUI::Button`. Game headers live in subfolders: check the path in `SML/Source/FactoryGame/Public` before including (e.g. `Buildables/FGCentralStorageContainer.h`, not `FGCentralStorageContainer.h`). Unity builds merge `.cpp` files, so helpers in anonymous namespaces get file-specific prefixes (e.g. `AMWD_` in `AMWidgetDump.cpp`).

## Project layout

- `Source/AutomatedMachines/...` C++. Class prefix `AM`.
- `Source/AutomatedMachines/.../Debug/AMWidgetDump.*`: development-only widget tree dump (see below). Remove before release.
- `Tools/am-update-and-package.bat`: the launcher the developer copies to `D:\SFMods` once (git pull, then calls the next one). Build steps can change without copying it again.
- `Tools/build-and-package.bat`: build the editor, run `create_assets.py`, package into the game.
- `Tools/create_assets.py`: editor script making the mod's assets: icon textures (from `Tools/Icons`), the marker material `M_AM_Marker` and mesh `SM_AM_Marker` (from `Tools/Marker/SM_AM_Marker.obj`).
- `Tools/IconSource/make_icons.py`: draws the Wi-Fi icons (Python + Pillow) into `Tools/Icons` (committed).

## What we know about the machine window

Found with `AMWidgetDump` (first run, 4 Nov 2026):

- The Smelter and the Constructor use **the same window class, `Widget_Manufacturing_C`** (Assembler, Foundry, Manufacturer... most likely too). One hook covers them all.
- Layout (abridged):

```
Widget_Manufacturing_C
  Overlay_0
    Widget_Window_DarkMode                        the window frame
      mMainContainer (VerticalBox)
        CanvasPanel_1
          Widget_TabsContainer                    the tab buttons
            HorizontalBox_0
              mContainer (CanvasPanel)
                Widget_SlidingTabs_Button_C x2    "Select Recipe", "Production"
          mCloseButtonContainer
        CanvasPanel_0
          WindowHBox
            WindowBody (NamedSlot)
              Widget_SlidingTabs                  the tab pages, side by side, slid into view
                ... mContent (NamedSlot)
                  HorizontalBox_1
                    mRecipesTab (VerticalBox)       page 1: recipe list
                    mManufacturingTab (HorizontalBox) page 2: production (standby button, overclock, IO slots...)
            mInventorySizeBox                     the player inventory on the right
```

- How the tabs work (second dump):
  - The window frame `Widget_Window_DarkMode_C` has `mTabs`: an array of structs `{Texture_4_..., Text_5_...}` (icon + title). `InitTabs()` hands it to `Widget_TabsContainer_C` (`mButtons`), whose `GenerateButtons()` makes one `Widget_SlidingTabs_Button_C` per entry in `mContainer`.
  - A click goes button `OnClicked` -> `TabsContainer.IfButtonClicked` -> `TabsContainer.OnButtonClicked(index)` -> frame `OnTabButtonClicked(index)` -> the window's handler.
  - `Widget_SlidingTabs_C` holds the pages (`mHorizontalBox` = `HorizontalBox_1`), `mIndexOffset` (TMap page index -> x offset, e.g. 0 -> 0, 1 -> -956, made by `SetupIndexOffset()`), `mActiveIndex`, and `SetActiveIndex(Index, Animate)` slides.
  - `Widget_Manufacturing_C` derives from `Widget_UseableBase_C` -> `UFGInteractWidget`; `mInteractObject` / `mBuildableManufacturer` is the machine. The frame's `mTitleText` is the machine name.
  - Asset path: `/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Manufacturing.Widget_Manufacturing_C`.

## The Automation tab (first version)

- `AMWidgetHooks` registers an SML widget blueprint hook (from C++, no asset): `UAMAutomationTab` is added to `HorizontalBox_1` of `Widget_Manufacturing_C`, so it is the third page. Registered on the first world initialization.
- `UAMAutomationTab` (C++ `UUserWidget`, widgets built in code):
  - on initialize: builds its page (the window's variables are not set yet at this point, so nothing else);
  - on construct: appends a copy of the last `mTabs` entry titled "Automation"; binds to the frame's `OnTabButtonClicked`; one frame later, if the buttons were made without ours, calls `InitTabs()` again; copies the game font from `mRecipeName`;
  - tested 4 Nov 2026: works in the Smelter and the Constructor. The game picks up our `mTabs` entry by itself (3 buttons) and its own click handler slides to our page; our fallbacks are not needed but stay. The page is the third child of `HorizontalBox_1` and `SetupIndexOffset` gives it an offset by itself (2 -> -1805).
  - on its button: if `mIndexOffset` has no entry for our page, calls `SetupIndexOffset()`, else computes it from the previous page; then `SetActiveIndex(ours, true)` unless the game's handler already did.
  - Logs every step (`Automation tab: ...`).


## Rules, saving and the engine (version 0.1, works: tested 4 Nov 2026)

Tested: Constructor rings PLATES BACKED UP / PLATES FLOWING, Smelter pauses / resumes; rules survive save and load; typing E in a name field does not close the window.

Player-facing words differ from the code (see `DESIGN.md`, "Words the player sees"): event = **alarm**, fire = **ring**, reaction = **action**. Code names stay as they are.

- `AMTypes.h`: `FAMFireRule` (event name + `EAMCondition`), `FAMReactionRule` (`EAMAction` + event name), `FAMMachineRules` (machine + both lists). Event names are uppercase (`AMRules::NormalizeEventName`).
- `AAMAutomationSubsystem` (SML `AModSubsystem` + `IFGSaveInterface`, `SpawnOnServer_Replicate`): `mMachines` is `SaveGame` and replicated. Registered from C++ on `FWorldDelegates::OnWorldInitializedActors` with `USubsystemActorManager::RegisterSubsystemActor`, the same moment SML registers world-module subsystems, so the saved actor is found and reused.
  - Tick (server, every 0.25 s): removes dismantled machines; for each fire rule checks its condition and fires the event when it becomes true (also the first check after load or after the rules change); every machine with a reaction to that event acts at once.
  - Conditions: output full = the output inventory has no room for one more cycle of the recipe's products (`UFGRecipe::GetProducts` + `HasEnoughSpaceForStacks`); producing = `IsProducing()`.
  - Actions: `SetIsProductionPaused(true / false)`, the game's own standby.
  - Logs every fire and action (`... fires EVENT (Output full)`, `... paused`).
- `UAMRemoteCallObject`: clients send `ServerSetRules`; `RequestSetRules` calls the subsystem directly on the server / in single player.
- UI: `UAMAutomationTab` keeps a copy of the machine's rules and sends them on every change; `UAMRuleRow` is one row (its widgets and click handlers); `UAMEventChoice` is one event-name button under a row (suggestions while typing, or the list of all events for a reaction). Buttons and fields use plain colour brushes (`AMUIHelpers`), no engine style textures.
- Event names become uppercase when the field is committed (Enter or clicking elsewhere), not while typing (changing a field's text while typing can move the cursor).

## Marker, tab icon, tab layout (4 Nov 2026, first try)

- Marker: `AAMMarkers` (local actor spawned by the subsystem, transient, not on dedicated servers): one `UInstancedStaticMeshComponent`, one instance per machine with rules, rebuilt on `OnRulesChanged`. Position: top of the machine's colliding bounds + 1.5 m. Cull distance 300 m.
- `M_AM_Marker`: unlit, masked, two-sided, `used_with_instanced_static_meshes`. World position offset = (instance origin + camera right * x + camera up * y) - vertex position, with x, y from the vertex UV (so the picture is always upright, whatever the OBJ importer does with V). Instance origin = TransformPosition(instance space -> world) of (0,0,0). Parameters: `Size` (cm, 100), `Color` (white), `Brightness` (2).
- Tab icon: `AddTabEntry` sets the copied entry's texture variable to `T_AM_TabIcon`.
- Tab layout: page 1230 wide = Production page + the inventory panel. Left: title; two tables side by side, ALARMS (ALARM NAME | MACHINE EVENT) and ACTIONS (DO | ON ALARM); right: an INFO panel. Cell buttons open a list of choices under the row; pointing at a choice explains it in the info panel (descriptions in `AMRules::GetConditionDescription` / `GetActionDescription`).
- The window's inventory panel is hidden on our tab (frame `SetInventoryVisibility(false, true)`, one frame after the switch so it comes after the game's own handler) and shown again when going back to Production.

## Tab look (second version, 4 Nov 2026)

Tested before: marker, tab icon, two tables, info panel and inventory hiding all work. The developer asked for the look of the game's own windows (train Stop Settings) and the see-through page of the other tabs:

- Page background like the recipe tab's list area: sRGB 15,15,15 at ~94% (`AMUI::PageBackground`); each tab page has its own background, the window has none. A full-height side panel on the right like the recipe tab's info panel: sRGB 56,57,61 at ~92% (`AMUI::SidePanel`).
- Panel widths are fixed (sum of the row cells), so adding a row does not move anything.
- Alarm names are written in capitals while typing: `UAMUppercaseText` (a `UEditableText` subclass) sets the capitalised text straight on its Slate widget and moves the cursor to the end, from OnTextChanged. (UMG `SetText` on commit did not change what was shown.)
- Rows, dropdown lists and "+ Add" buttons use the font of the tab buttons' titles (`mTitleObject` of the first `Widget_SlidingTabs_Button_C`), read at runtime; rows are rebuilt once a frame after construct, when the buttons exist. Fallback: game font size 12.
- Info panel: darker (sRGB 40,41,44), title and text in the font's "Regular" face (`AMUI::RegularFont`).
- Panels "Alarms", "Actions", "Info": title above (white 15), body `AMUI::PanelBody` (sRGB 67,67,68, slightly see-through), column names inside (grey 13), rows scroll, full-width "+ Add ..." button (sRGB 102) at the bottom.
- Rows: field and dropdowns sRGB 87 with a white arrow icon (`T_AM_DropArrow`), bin button (`T_AM_Trash`). Square corners everywhere (`FSlateColorBrush`).
- Choices open as a dropdown list under the row (full width, sRGB 110); the current one is orange.
- Colours are the game's sRGB values sampled from a screenshot, converted with `FLinearColor(FColor)`.

## Every machine event and action for manufacturers (4 Nov 2026)

Machine events (`EAMCondition`, values saved: add new ones at the end only): output full / not full, starts / stops producing, loses / gets power (`HasPower`), input missing / available (input slots hold one batch of every ingredient, `UFGRecipe::GetIngredients` + `HasItems`), paused / resumed (`IsProductionPaused`), productivity below / above N% (`GetProductivity`), recipe changed (compared with the last check, not saved), input full / not full (some ingredient's slot has no room for one more: `HasEnoughSpaceForStack`).

Actions (`EAMAction`): pause, resume, pause or resume (toggle), set clock speed N% (`SetPendingPotential`, clamped to `GetCurrentMinPotential` .. `GetCurrentMaxPotential`, i.e. what the machine's power shards allow), delete output / delete input (our own: `RemoveAllFromIndex` on every slot, not `Empty()`, so the slots keep their item settings), change recipe (`FAMReactionRule::Recipe`, saved; on the server: only if `GetAvailableRecipes` has it, then delete input + output, then `SetRecipe`, which needs empty slots), show message (`FAMReactionRule::Message`; the server calls `MulticastShowMessage` on the subsystem, every game instance calls `UFGGameUI::ShowTextNotification` for its local players; untested how the game shows it).

Rules have a `Value` (float, saved) for the ones that need a number; `AMRules::GetConditionValueInfo` / `GetActionValueInfo` give the label, range and default. The row shows a "% field" under the cell when needed.

Not offered: production boost (Somersloops are items, not a setting).

## World actions in the HUB terminal (4 Nov 2026)

- The HUB terminal (`/Game/FactoryGame/Buildable/Factory/TradingPost/UI/BPW_Hub.BPW_Hub_C`) uses the same window frame and sliding tabs as the machine window: tabs "Select Milestone" / "Active Milestone", pages in `HorizontalBox_25` (offsets 0, -1310). A second widget hook puts `UAMAutomationTab` there.
- `UAMAutomationTab` is in world mode when its host window is `BPW_Hub_C`: no Alarms panel, a "World Actions" panel offering only world actions (`AMRules::IsWorldAction`: Show message), rules read from / sent to `AAMAutomationSubsystem::mWorldActions` (saved, replicated; `UAMRemoteCallObject::RequestSetWorldActions`).
- Machines no longer accept world actions (`SetRules` drops them); `MoveWorldActionsFromMachines` moves them from older saves on load.
- `FireEvent(Event, Ringer)` also runs the world actions.
- The HUB page also has the Alarms panel ("Alarm Name | World Event"), without an add button for now: world events (night, fuse blown...) come later.
- Show message: `MulticastShowMessage(Title = alarm, Body = own text or "Rung by <machine>")`; each game instance creates `UAMNotification` (a `UFGPushNotificationWidget`: Wi-Fi icon, orange alarm name, text size 19, removed after 7 s) and gives it to `UFGGameUI::PushNotificationWidget`, the game's notification list. If it is not on screen a frame later, `ShowTextNotification` is used instead (logged: "Notification shown in the game's list" / "not taken").
- Sounds: the game uses Wwise. The starter project has no sound events (placeholders drop them), so a development dump (`AMWidgetDump::InstallSoundDump`, `[SoundDump]` lines) lists the game's `AkAudioEvent` assets with UI / alert-like names from the asset registry, and the `PostEvent` functions of `AkGameplayStatics`, to pick 5 sounds for a Play sound action. Result (4 Nov 2026): 2503 events; chosen Blip `Play_UI_CodexMessage_Blip`, Ping `Play_EQ_Resource_Ping_Fire`, Warning `Play_P_Suit_Damage_Warning`, Alert `Play_FrackerSmasher_Alarm` (was Error `Play_Train_Signals_Error`: too faint over running machines; same slot, so saved rules keep working), Success `Play_MAM_Scanner_Complete_Fanfare` (all under `/Game/WwiseAudio/Events/...`, see `AMRules::GetSounds`). The dump is switched off in the module now.
- Play sound: the event is loaded by path and played with its own UFUNCTION `UAkAudioEvent::PostOnActor(Actor, delegate, CallbackMask, bStop)`, called by name (`AMSound`), so the mod does not depend on the Wwise plugin. The server calls `MulticastPlaySound(index)`; each game instance plays it on its local players' pawns. The sound's number is stored in the rule's `Value`.
- Show message tested 4 Nov 2026: our notifications stack at the side of the screen with the game's.

## Wrong input and "Pass wrong items through" (4-5 Nov 2026, works)

- Wrong input (`AAMAutomationSubsystem::FindWrongInput`): for each `FCD_INPUT` connection of the machine (`GetConnectionComponents`), its connected belt end (`GetConnection`) is asked `Factory_PeekOutput`: the first item waiting. Wrong if its class is not in `UFGRecipe::GetIngredients`. Pipes: every `UFGPipeConnectionComponent` with `PCT_CONSUMER`, connected, whose `GetFluidDescriptor` is not an ingredient. No recipe: never wrong.
- Options: `FAMMachineOptions` (SaveGame) inside `FAMMachineRules`; `IsEmpty()` counts it, so the entry and the marker stay while an option is on. `SetRules`, `ServerSetRules` and `RequestSetRules` carry it.
- Pass through, second version (5 Nov 2026, untested; the first one put the item in the output slot, which stopped production and made Output full ring): `TakeWrongItem` (server, every check, game thread) takes one wrong item off the input belt (`Factory_GrabOutput` on the belt end) if the machine has an output belt and holds none yet. The item waits in a small table (`AMAS_Held`, one item per machine, not in any inventory). Hooks on `UFGFactoryConnectionComponent::Factory_GrabOutput` and `Factory_PeekOutput` (SML `SUBSCRIBE_METHOD`, installed at startup): the original runs first; only if it has nothing and the connection is an output of a machine holding an item, that item is given. So the machine's products always go first, and no slot filter is changed. Belts may grab on worker threads: the table has a lock, and an atomic count lets every other grab in the factory skip it. Saved items: copied to `mHeldItems` (SaveGame) in `PreSaveGame`, back to the table in `PostLoadGame`. If the mod is removed, at most one item per machine is lost; a dismantled machine's waiting item is lost too.
- To test: do belts call the hooked `Factory_GrabOutput` (if the game inlined the call, nothing comes out: then hook elsewhere)? With the option on, a mixed belt should flow, the copper should leave on the output belt between ingots, and no other alarm should ring.
- UI: an Options strip at the bottom of the machine page (not in the HUB), as wide as the Alarms panel. Each option is one button (the whole row clicks), coloured like "+ Add alarm": the name on the left, a switch picture on the right (`T_AM_SwitchOff` / `T_AM_SwitchOn`, 64 x 32, drawn by `make_icons.py` after the game's power switch window, in their own colours: off = white outline, see-through, white knob left; on = white, dark knob right; `...Disabled` versions in grey (110 / 64), not used yet). Hovering or clicking shows the explanation in the Info panel. Tested 5 Nov 2026: passing wrong items works (first version of the strip).

## Miners (5 Nov 2026)

- The miner (Mk1-3) window is `/Game/FactoryGame/Interface/UI/InGame/Widget_ResourceExtractorUI.Widget_ResourceExtractorUI_C` (from `mInteractWidgetSoftClass` in `Build_MinerMk1`). Its uasset has a `Widget_Window_DarkMode` frame but no `Widget_SlidingTabs` and no tab pages: one canvas page (`Widget_Overclock`, `Widget_StandbyButton`, `Widget_Manufacturing_Warning`, an inventory slot). Variables include `mResourceExtractor`, `mExtractorNode`, `mWantProdPaused`. So the Automation tab cannot go into a pages box as in the production window; the dump (now pointed at this window) shows its tree to decide how.
- Dump result (5 Nov 2026): the same frame `Widget_Window_DarkMode_C` (one tab entry, "Miner"; `Widget_TabsContainer` with one `Widget_SlidingTabs_Button_C`, its `mSlidingTabsWidget` is None). Frame body: `WindowHBox` = [`WindowBody` (NamedSlot) holding `CanvasPanel_3` = the miner's page, `mInventorySizeBox` (inventory)]. The tabs container's `IfButtonClicked` / `SetActiveButton` highlight the clicked button and the frame broadcasts `OnTabButtonClicked(index)`.
- Single-page mode (`UAMAutomationTab::bSinglePageMode`: the host window has no `Widget_SlidingTabs` variable): the SML hook puts our widget into `CanvasPanel_3` (collapsed); on the tick after construct `SetupSinglePage` moves **only our widget** into `WindowHBox` (the parent of `WindowBody`), and `OnTabButtonClicked` shows `WindowBody` or us (`ShowSinglePage`, inventory hidden on ours). Our page keeps its own fixed width; its height is set to the machine page's (measured while that shows, `mMachinePageHeight`), so switching tabs only changes the window's width. Tested 5 Nov 2026: the tabs switch.
- First try (5 Nov 2026) moved the miner's page into an overlay: its widgets were rebuilt and showed their designer defaults ("999 Nuclear Uranium Waste", placeholder icon). Never reparent the game's page.
- The miner window's `mTabs` is still empty when our construct runs (the window fills it in its own construct): the tab entry is added in `SetupSinglePage`, retried on the next ticks while empty. `InitTabs` alone did not make a second button (the single "Miner" button stretched over the bar), so `RegenerateTabButtons` also copies the frame's `mTabs` into the tabs container's `mButtons` and calls its `GenerateButtons`. That made 2 buttons, but the first stayed stretched over the bar and was not clickable (`HitTestInvisible`: in a one-tab window it is a title). So the buttons are then laid side by side in the container's canvas (`mButtonSize`, 220 x 36) and made `SelfHitTestInvisible`; their slots are logged. Every button got the window's title ("Miner") and the default icon, so ours gets `SetTitle` / `SetIcon` itself. Clicks: the frame did not broadcast `OnTabButtonClicked` for these buttons, and the tab button's own `OnClicked(Instigator)` did not fire either (the click sound played), so we bind the plain `UButton` inside each tab button (`mButton`, `OnClicked.AddUniqueDynamic`: `OnMachineTabPressed` / `OnAutomationTabPressed`) and call the container's `SetActiveButton` for the highlight.
- Machines: `CanAutomate` also accepts `AFGBuildableResourceExtractor` (miners, and water / oil / well extractors, same class). `AMRules::IsConditionAvailable` / `IsActionAvailable` list what each machine type has; the dropdowns show only those (the current choice stays), and `SetRules` drops the rest. `HasPassWrongItems`: production machines only; the Options section is hidden otherwise.
- Output full on an extractor: the output inventory has no room for one more cycle (`GetNumExtractedItemsPerCycle` of the node's `GetResourceClass`). Delete output: its output inventory.

## Storage containers (5 Nov 2026, tab works)

- Window: `/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_Storage.Widget_Storage_C` (from `mInteractWidgetSoftClass` of `Build_StorageContainerMk1` and `Mk2`, both `AFGBuildableStorage`). The asset has `Widget_Window_DarkMode`, `WindowBody`, no `Widget_SlidingTabs`: a one-page window (kind B in `GAME-UI-GUIDE.md`). Its variables include `mInventoryVertBox`, `mScrollableContent`, `mSortbuttonHbox`, `mSortButton`, `mGrabAllButton`, `mDumpAllButton`, `mStorageActor`. The hook goes into `mInventoryVertBox` (confirmed by the dump: `WindowBody` > `StoragePanel` (CanvasPanel) > `mInventoryVertBox`). First try: no tab, because the frame variable is `WidgetWindowDarkMode` here; `GetWindowFrame` now finds the frame by class. First-first try crashed at start: SML's generic slot setup on a VerticalBox parent (see the guide).
- `CanAutomate`: `AFGBuildableStorage` except `AFGCentralStorageContainer` (Dimensional Depot uploader). Events `StorageFull`, `StorageNotFull`, `StorageEmpty`, `StorageNotEmpty`, `FillAbove`, `FillBelow` (fill level = sum of items / slot size over slots, `GetSlotSize`), action `DeleteContents` (`DeleteAllItems(GetStorageInventory())`). Machines and containers each see only their own events and actions; a new row starts on the first one the machine has. Full = fill level 100% **or** `IsBeltBlocked` (the item waiting at an input belt's end, `Factory_PeekOutput`, has no room: `HasEnoughSpaceForItem`). Belt blocked / Belt flowing were separate events for a few minutes, never released: removed from the enum (they were its last values).
- Item count above / below: `FAMFireRule::Item` (SaveGame), `Value` = the count (label "#", no unit), counted with `GetNumItems`. On the alarm's number line: "# [500] [icon item v]" (the line starts under the alarm name when an item is needed).
- Dropdown window (second version, 5 Nov 2026, untested; first version tested: opened at the top of the page because it was moved above the dropdown when the page was too short): every dropdown of the rows (Machine Event, Do, On Alarm, Recipe, Sound, Item) opens `UAMAutomationTab::OpenPicker`. The tab's root is SizeBox > Overlay [page, `mPopupLayer` canvas]; the window is a see-through full-page button (click outside closes) plus a box placed from cached geometry (`AbsoluteToLocal`): a search field exactly over the clicked dropdown (focused on the next tick; typing filters by label, any case; Enter picks the first shown), and under it a scrolling list (up to 320 high, shorter if the page ends sooner) of buttons with an optional 28 px icon (items: `GetSmallIcon`). Choosing closes it on the next tick (`ClosePicker(true)`), not inside its own click. Items: the container's contents first, then products and ingredients of `AFGRecipeManager::GetAllAvailableRecipes`, solids only, no building descriptors, by name. The list under the alarm name while typing (suggestions) stays inline.

## The widget dump

Now pointed at the HUB terminal (`BPW_Hub_C`): its tree, plus variables and functions of every widget in it whose class name contains Tab, Menu or Hub, to find where an Automation tab goes next to Milestones.


`AMWidgetDump` now writes, once per session:

- the full tree of `Widget_Manufacturing_C` (between `===== begin` and `===== end`);
- for `Widget_Manufacturing_C`, `Widget_Window_DarkMode_C`, `Widget_TabsContainer_C`, `Widget_SlidingTabs_C` and `Widget_SlidingTabs_Button_C`: every Blueprint variable with its value, and every function with its parameters (between `===== reflection` and `===== end reflection`);
- one line per new widget class seen (`new widget class: ...`), useful later for other buildings.

Test of the Automation tab: open a Smelter and a Constructor, click Automation, Production, Automation, check that the other tabs still work, quit, and let Claude read the log (`Automation tab:` lines).
