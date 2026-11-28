# Automated Machines — what is left (18 Nov 2026)

Done and tested so far: see the table in `DEV-NOTES.md` ("What works").

## Release 1.2.0 (28 Nov 2026)

- **Other mods** (done and tested 28 Nov 2026 with Road Maker's boom barrier: hooks found, enum dropdown, action run): naming convention `AM_Condition_` / `AM_Action_` / `AM_Info_` on buildings, and a Blueprint library for events. See `docs/MODDING.md`. Automation window for buildings with no game window: `OpenAutomationWindow(Building)`, callable by name; Road Maker's boom barrier will use it.
- **Hold vehicles here** (truck stations, GitHub issue #2): done and tested 27 Nov 2026 (docked or within 3 m of the parking spot; no saved list; stops the vehicle before loading).

## Release 1.1.0 (24 Nov 2026)

- **Point to the machine** (HUB) and **Point to this machine** (machines) actions: done and tested 24 Nov 2026, with an icon choice (machine icon or 12 of the game's sign icons, made black).
- **Live state of conditions**: done and tested 24 Nov 2026 (a condition row's arrow turns green while its condition is met).

- Built on official SML 3.12.1 (headers CL520140) with the engine 5.6.1-css-87 (SML needs it: the game update changed the engine's UNetConnection, SML issue #432). Requires SML ^3.12.1, game >=520140.

- **Somersloop amplification** action (GitHub issue #1): done and tested 21 Nov 2026 (levels per machine from the Somersloop slot size, limited to the Somersloops placed; uninstall keeps the level, a Somersloop out and back in restores it).

## Release 1.0.1 (20 Nov 2026)

- Fixes the crash on load after the game update 1.2.6 (build 520140): rebuilt on the community headers for CL520140 (SML PR #433, unofficial; SML had none yet). Tested: the save that crashed loads and runs (24 machines, 29 events), the game update check says the 62 classes match.
- New: game update guard (pauses the mod with a message instead of crashing when a game update changes the classes it reads).
- To do when SML publishes official CL520140 headers: rebuild on them, check the log says "match", release again if anything differs.

## Release 1.0.0 (released 18 Nov 2026)

Ready: every feature tested in game. Done for the release:
- MAM research tested (own tab, 6 AI Limiter, then Output Overflow for 2 High-Speed Connector).
- Clean-up: widget dump off, sign copy / paste, "removing action" and "[Research]" logs Verbose, old Wi-Fi marker assets removed from `Tools/create_assets.py` (it deletes them from the project if present).
- Version 1.0.0 in `AutomatedMachines.uplugin`.
- Mod icon: isometric chip on a glowing circuit board with the white Wi-Fi (`Resources/Icon128.png`; sources and the 512 px upload logo in `Tools/IconSource/`).
- ficsit.app page: `docs/FICSIT-PAGE.md` (screenshots in `docs/images/`, shown from GitHub).
- Released on ficsit.app on 18 Nov 2026 (tag `v1.0.0`).

Known limits, said on the page:
1. **Multiplayer**: made for it (server-side rules, replicated), not tested with several players.
2. **Blueprints (Blueprint Designer)**: machines built from a blueprint get no automation; Ctrl+C / Ctrl+V and middle click do copy it.
3. Not automated: pumps, trains, doors, Geothermal Generators, elevators.

## Settings (decided 5 Nov 2026: none for now)

No settings tab in the HUB. Nothing in the mod needs a world setting: every behaviour is per machine, in its Automation tab. The only candidates are personal display preferences (underglow on / off / only with the build gun, brightness; notification sound on / off). If players ask for them, they go in SML's own mod configuration (main menu > Mods > Automated Machines), per player, not in the save or the HUB.

## More machines (list of 6 Nov 2026)

Done and tested: production machines (production window), miners, Water / Oil Extractor, storage containers, Biomass Burner, power switches and priority switches, signs, lights and light control panels, valves, fluid buffers, AWESOME Sink, HUB, MAM, railway signals (with Track / Block actions), train stations, freight platforms (items and fluids), truck station (with transfer rates), Power Storage (14 Nov 2026: grid alarms, one shared list per grid), the Output overflow option and action. Not automated (decided): pipeline pumps, trains / locomotives.

**Should already work, to check** (same class or window as something done):
- Coal Generator, Fuel Generator, Nuclear Power Plant (fuel generators like the Biomass Burner; the Coal one also takes water, the Nuclear one has a waste output).
- Assembler, Foundry, Manufacturer, Refinery, Blender, Packager, Particle Accelerator, Converter, Quantum Encoder (production window; fluid outputs and variable power not tried yet).
- Industrial Fluid Buffer (same window as the Fluid Buffer).

**Power:**
- Geothermal Generator: excluded (decided 17 Nov 2026).
- Alien Power Augmenter: done and tested (17 Nov 2026). Alarms: starts / stops producing, fuel empty / available / below, power, fuse. Action: delete fuel (it has no standby button).

**Extraction:**
- Resource Well Pressurizer: code in and tab hooked on `bg` (17 Nov 2026), waiting for the in-game test. Alarms: starts / stops producing, paused / resumed, power, fuse. Actions: pause / resume / toggle, set clock speed. Its extractors (Resource Well Extractor) had no tab (their own window): hooked too; they have no standby or clock speed (their window has none), so their only action is delete output.

**Logistics:**
- Dimensional Depot uplink: tab works (17 Nov 2026); depot item count alarms still to confirm. Alarms: input full / not full / empty / not empty (its one slot), depot items above / below (the shared depot's count of an item), power. Action: delete input (same name as on other machines).

**Transport:**
- Drone Port: done and tested (14 Nov 2026). Alarms: drone arrives / leaves, busy / idle, input / output full, outgoing / incoming item counts, outgoing / incoming rate (items per minute), fuel, power. Actions: delete outgoing items, delete incoming items.

**Other buildings:**
- Doors: not automated (decided 17 Nov 2026: too buggy in the game).
- Personnel Elevator: arrives at a floor (action: call to a floor).
- Space Elevator: phase completed, parts delivered (world events, like the MAM).

**Power switch display (17 Nov 2026, done and tested, no flicker):** the switches get the train stations' "Display text" action: the switch's sign shows the text; empty = its own name (tag) again. Only the sign's picture changes, the tag stays.

**Words (18 Nov 2026, done):** Alarm -> Event, ring -> trigger, machine / world event -> condition, in every text players see (tabs, tooltips, condition and action descriptions, messages, research, the `.uplugin`). 

**Machine tab card layout (18 Nov 2026, code in, waiting for the in-game test):** see DESIGN "Machine Automation tab: card layout". Conditions / Actions cards, one-line condition rows with value bars, event headers with "+ action", Copy / Paste at the top, Overflow and the info line at the bottom, explanations in empty cards. Tested and working (18 Nov 2026). HUB and MAM moved to it too (18 Nov 2026, waiting for the test); Overview unchanged.

**Events Overview rework (18 Nov 2026, done and tested):** search, sortable columns, Last triggered column, one bin per event held 1.2 s with a ring, card style. See DESIGN "Events Overview".

**AND between events (18 Nov 2026, done and tested):** "+ and" on event headers; see DESIGN "AND between events".

## Useful optional features

- **Colours for point icons** (asked 24 Nov 2026): the compass took the tint, the map drew them white (as an ore, also with the beacon kind). Look at how the game's own map markers / stamps get their colour.

- **Find an alarm's machines**: clicking an alarm name in the HUB overview lists the machines that ring it and listen to it (name, distance, direction), optionally with a map marker.
- **More world events in the HUB** (Milestone completed is the first, 5 Nov 2026): night falls / day breaks, a power circuit's fuse blows / is reset, power storage low, Space Elevator phase completed, MAM research completed.
- **More alarm logic**: ring another alarm (chains); "A and B rang"; a delay ("N seconds after"); a cooldown so a flapping machine does not ring 10 times a second; "every Nth time".
- **Glow shows the state**: flashes when the machine rings an alarm, another colour while the automation keeps it paused; option to show glows only while the build gun is out.
- **Alarm history in the HUB**: when each alarm last rang and who rang it, how often in the last hour.
- **Machine names**: give an automated machine a name, shown in messages and in the overview.
- **Alarm sets**: save a machine's alarms and actions as a named preset and apply it to others (beyond copy / paste).
- **Per-alarm notification sound** (the Show message action with a chosen sound).

## Batch: Smart Splitter, Programmable Splitter, Priority Merger (done and tested with flowing items, 14 Nov 2026)
Like signs and lights: a side list in each window, `Settings: Default / ALARM [bin] / ... / + Auto Settings`. A row holds the machine's own settings (splitters: the sort rules; merger: the input priorities). When an alarm rings the machine takes that alarm's settings and keeps them until another of its alarms rings. No conditions, no Automation tab: they only react. To test: the list shows, selecting a row changes the game's controls, an alarm switches the settings, deleting an alarm, copy / paste, middle-click sampling, saving and loading.
