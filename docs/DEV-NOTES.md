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

## Current step: find where the Automation section goes

The editor only has placeholder versions of the game's widgets, so the layout of the Smelter / Constructor window cannot be seen there. `AMWidgetDump` runs in game, twice a second:

- every new widget class gets one line: `[WidgetDump] new widget class: <class> (inside: <outer widgets>)`;
- widgets whose class name contains `Manufactur`, `Smelter`, `Constructor`, `Interact`, `Factory` or `Production` get their whole tree written between `===== begin` and `===== end` lines: name, class, slot type, visibility and the text of text blocks.

Test: load a save, open a Smelter and then a Constructor, close the game, and let Claude read the `[WidgetDump]` lines of `FactoryGame.log`.
