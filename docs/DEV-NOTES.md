# Developer notes (Automated Machines)

Notes for the next session working on this mod. The design (what the mod does and why) is in `DESIGN.md`. Many lessons from Road Maker apply here too: see `docs/DEV-NOTES.md` in `jollyeye/Satisfactory-Road-Fence-Tool`.

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

Claude cannot compile; keep edits careful (count braces, check renames). Shadowing is an error (C4458): a local variable named like a member of a base class, e.g. `Padding` in a `UUserWidget`, fails the build. Unity builds merge `.cpp` files, so helpers in anonymous namespaces get file-specific prefixes (e.g. `AMWD_` in `AMWidgetDump.cpp`).

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

Machine events (`EAMCondition`, values saved: add new ones at the end only): output full / not full, starts / stops producing, loses / gets power (`HasPower`), input missing / available (input slots hold one batch of every ingredient, `UFGRecipe::GetIngredients` + `HasItems`), paused / resumed (`IsProductionPaused`), productivity below / above N% (`GetProductivity`), recipe changed (compared with the last check, not saved).

Actions (`EAMAction`): pause, resume, pause or resume (toggle), set clock speed N% (`SetPendingPotential`, clamped to `GetCurrentMinPotential` .. `GetCurrentMaxPotential`, i.e. what the machine's power shards allow).

Rules have a `Value` (float, saved) for the ones that need a number; `AMRules::GetConditionValueInfo` / `GetActionValueInfo` give the label, range and default. The row shows a "% field" under the cell when needed.

Not offered: changing the recipe (`SetRecipe` needs the inventories emptied first, which would lose items), production boost (Somersloops are items, not a setting).

## The widget dump

`AMWidgetDump` now writes, once per session:

- the full tree of `Widget_Manufacturing_C` (between `===== begin` and `===== end`);
- for `Widget_Manufacturing_C`, `Widget_Window_DarkMode_C`, `Widget_TabsContainer_C`, `Widget_SlidingTabs_C` and `Widget_SlidingTabs_Button_C`: every Blueprint variable with its value, and every function with its parameters (between `===== reflection` and `===== end reflection`);
- one line per new widget class seen (`new widget class: ...`), useful later for other buildings.

Test of the Automation tab: open a Smelter and a Constructor, click Automation, Production, Automation, check that the other tabs still work, quit, and let Claude read the log (`Automation tab:` lines).
