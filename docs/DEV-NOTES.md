# Developer notes (Automated Machines)

Notes for the next session working on this mod. The design (what the mod does and why) is in `DESIGN.md`. Many lessons from Road Maker apply here too: see `docs/DEV-NOTES.md` in `jollyeye/Satisfactory-Road-Fence-Tool`.

## Setup on the developer's PC (Windows)

| What                      | Where                                                                    |
|---------------------------|--------------------------------------------------------------------------|
| SML starter project       | `D:\SFMods\SML\FactoryGame.uproject` (shared with Road Maker)            |
| This mod (git clone)      | `D:\SFMods\SML\Mods\AutomatedMachines`                                   |
| Unreal Engine (CSS build) | `D:\UE-CSS` (or `D:\UE-CSS\Unreal Engine - CSS`)                         |
| Game install              | `D:\games\Satisfactory`                                                  |
| One-click update script   | `D:\SFMods\am-update-and-package.bat` (copy of `Tools/update-and-package.bat`) |
| Script log                | `D:\SFMods\am-update-log.txt`                                            |
| Game log                  | `C:\Users\magikgpdwin4\AppData\Local\FactoryGame\Saved\Logs\FactoryGame.log` |

GitHub: `jollyeye/Satisfactory-Automation-mod` (branch `main`). Use the capitalised URL `https://github.com/jollyeye/Satisfactory-Automation-mod.git`.

The update script and its log have an `am-` prefix so they do not overwrite Road Maker's. The mod's own log lines are `LogAutomatedMachines`.

## How we work

Same loop as Road Maker:

1. Claude edits the code in its own clone, commits and pushes to `main`.
2. The developer closes the game and runs `am-update-and-package.bat`: git pull, build the editor, package into the game.
3. If it prints "FAILED", Claude reads `D:\SFMods\am-update-log.txt` through the desktop app's file bridge and fixes the compile error.
4. The developer tests in game and reports. Claude reads `FactoryGame.log` (`LogAutomatedMachines` lines) when something is unclear.

Claude cannot compile; keep edits careful (count braces, check renames). Unity builds merge `.cpp` files, so helpers in anonymous namespaces get file-specific prefixes (e.g. `AMWD_` in `AMWidgetDump.cpp`).

## Project layout

- `Source/AutomatedMachines/...` C++. Class prefix `AM`.
- `Source/AutomatedMachines/.../Debug/AMWidgetDump.*`: development-only widget tree dump (see below). Remove before release.
- `Tools/update-and-package.bat`: the one-click update script.

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
  - on initialize: builds its page and appends a copy of the last `mTabs` entry titled "Automation" (so the frame makes a third button);
  - on construct: binds to the frame's `OnTabButtonClicked`; if the buttons were already made without ours, calls `InitTabs()` again; copies the game font from `mRecipeName`;
  - on its button: if `mIndexOffset` has no entry for our page, calls `SetupIndexOffset()`, else computes it from the previous page; then `SetActiveIndex(ours, true)` unless the game's handler already did.
  - Logs every step (`Automation tab: ...`).


## The widget dump

`AMWidgetDump` now writes, once per session:

- the full tree of `Widget_Manufacturing_C` (between `===== begin` and `===== end`);
- for `Widget_Manufacturing_C`, `Widget_Window_DarkMode_C`, `Widget_TabsContainer_C`, `Widget_SlidingTabs_C` and `Widget_SlidingTabs_Button_C`: every Blueprint variable with its value, and every function with its parameters (between `===== reflection` and `===== end reflection`);
- one line per new widget class seen (`new widget class: ...`), useful later for other buildings.

Test of the Automation tab: open a Smelter and a Constructor, click Automation, Production, Automation, check that the other tabs still work, quit, and let Claude read the log (`Automation tab:` lines).
