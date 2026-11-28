# Handoff: how to continue Automated Machines (27 Nov 2026)

Read this first, then DESIGN.md, GAME-UI-GUIDE.md and ROADMAP.md. The repo also has docs/DEV-NOTES.md (technical notes per feature) and docs/MODDING.md (rules for other mods).

## Repo and build
- GitHub: jollyeye/Satisfactory-Automation-mod (branch main), public. Clone it in the session (add_repo, then clone).
- The user builds on Windows with `D:\SFMods\am-update-and-package.bat` (git pull + build + asset script + package into the game). Build log: `D:\SFMods\am-update-log.txt`. Game log: `C:\Users\magikgpdwin4\AppData\Local\FactoryGame\Saved\Logs\FactoryGame.log`. Read both through the linked computer (device_stage_files).
- Engine 5.6.1-css-87, starter project `D:\SFMods\SML` on SML master (3.12.1, official headers CL520140). Releases: Alpakit Release (Windows, Windows Server, Linux Server); the `.uplugin` "Version" must equal the SemVersion's major.
- After a game update: wait for SML's headers, pull, rebuild; the log line "Game update check: the N game classes ... match" must say match.
- "FAILED" = a compile error: read the build log, fix, push again.

## Rules for every change
- User constraint: no cheats. Only allow what the game itself allows; nothing creates items.
- Edit with small Python scripts that assert each target string occurs exactly once (watch tab indentation).
- New enum values (EAMCondition, EAMAction) go at the END, before Count: they are saved by index.
- Run `python3 Tools/check_source.py` before every push. Unreal treats shadowed names as errors (a local called `Owner` hides AActor::Owner).
- **Commit trailer (user's choice, 27 Nov 2026): only `Co-Authored-By: <model> <noreply@anthropic.com>`. No `Claude-Session:` link.**
- Document every working change in docs/DESIGN.md, docs/DEV-NOTES.md, docs/GAME-UI-GUIDE.md, docs/ROADMAP.md, and sync DESIGN / GAME-UI-GUIDE / ROADMAP to this project (claude/*.md).
- Words players see: Event (not alarm), trigger, Condition.

## Adding a new building (the usual loop)
1. Add the class to `CanAutomate` (AMAutomationSubsystem.cpp); conditions / actions in AMTypes (names, descriptions, value info, `IsConditionAvailable` / `IsActionAvailable` branch), evaluation in the subsystem, actions in `ApplyAction`; add the class to the game update guard (AMLayoutCheck.cpp).
2. To find a window's widgets, turn `AMWidgetDump::Install()` back on in the module (off for releases).
3. Hook the Automation tab in `AMWidgetHooks.cpp`; record it in GAME-UI-GUIDE.md.
4. On/off actions are a pair shown as one switch row: names in `AMRR_CompactName`, `AMRR_HasSwitch`, toggles in `OnStateClicked`, skip the "off" one in the picker, a conflict family in `ActionsConflict`.

## State right now
- Released: 1.0.0, 1.0.1 (game update), 1.1.0 (Somersloop amplification, point-to actions with sign icons, live state arrows), 1.2.0.
- 1.2.0 (28 Nov 2026): Hold vehicles here (truck stations, issue #2); other mods (docs/MODDING.md): `AM_Condition_` / `AM_Action_` / `AM_Info_` functions (number, on / off, enum dropdown), Blueprint library (events), the standalone Automation window. Road Maker's boom barrier uses them.
