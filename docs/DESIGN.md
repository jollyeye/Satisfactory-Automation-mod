# Automated Machines — design

A Satisfactory mod that lets the player automate the machines they already have. Machines **fire events** when their status changes, and other machines **react** to those events. No programming: the player types event names and picks actions from lists.

The name starts with "Automated" on purpose: players who come back after months away should recognise the mod in their mod list at once. It automates existing machines; it does not add new "automatic machines".

## Words the player sees

No programming words in the game. Decided 4 Nov 2026:

| In the game | In the code and these notes |
|-------------|-----------------------------|
| **alarm** (e.g. `PLATES BACKED UP`) | event |
| **ring** an alarm ("Ring PLATES BACKED UP when Output full") | fire an event (fire rule) |
| **action** ("Pause when alarm PLATES BACKED UP") | reaction |

Sections of the Automation tab: **ALARMS** and **ACTIONS**; buttons **+ Add alarm**, **+ Add action**.

## Decisions so far

### Events

- An event is just a **name the player types**, e.g. `PLATES BACKED UP`.
- Event names are **always uppercase**. Whatever the player types is converted to uppercase as they type.
- While typing, a **dropdown of suggestions** shows existing event names that match, so the same event is not created twice with small differences.
- Event names live in **one world-wide list**, saved with the game. Any machine anywhere can fire or listen to any event.
- **No renaming.** To fix a name, the player creates a new event with the correct name and uses it instead.
- **Unused events are deleted automatically:** an event that no machine fires or listens to any more disappears from the list.

### On a machine: two lists

Every row reads as a plain sentence: **what happens first, the cause second.** That reads more naturally for non-programmers than "when X, do Y".

**Fire events (fire... when...)** — the machine watches its own status and fires an event when the status changes:

```
AUTOMATION — Constructor (Iron Plate)

Fire event                     when
[PLATES BACKED UP      ]       [Output full      ▼]
[PLATES FLOWING        ]       [Output not full  ▼]
[+ add]
```

**Reactions (do... when...)** — the machine listens to events and acts:

```
AUTOMATION — Smelter (Iron Ingot)

Do                     when event
[Pause         ▼]      [PLATES BACKED UP    ▼]
[Resume        ▼]      [PLATES FLOWING      ▼]
[Pause         ▼]      [POWER LOW           ▼]
[+ add]
```

- **Every row acts on its own.** In the example the Smelter pauses when `PLATES BACKED UP` fires *or* when `POWER LOW` fires; each row simply tries to pause it. Pausing a machine that is already paused does nothing.
- No "all of these at once" combinations for now.
- A machine can have **any number** of rows. No limit for now; if many automated machines cause lag, we add a limit or optimise later.

### Freight platforms (7 Nov 2026)

- Item platform: the container's alarms (Full / Not full, Empty / Not empty, Filled above / below, Item count above / below); fluid platform: the same without item counts (like the fluid buffer).
- Both: **Busy** / **Not busy** (a docked train being loaded / unloaded, not finished yet).
- Actions: **Load mode** with the on / off switch (on = load into trains, off = unload them), Delete contents.

### Truck stations (7 Nov 2026, not tested yet)

- Automation tab in the truck station window (one page). The window has **no standby button**: no Running action, no Paused / Resumed.
- Alarms: the container's (fluid stations: without item counts), **Busy** / **Not busy** (loading / unloading a docked vehicle), **Vehicle arrives** / **Vehicle leaves** (trucks, tractors, explorers), the fuel slot (**Out of fuel**, **Has fuel**, **Fuel below** N items; the generator alarms, reworded for both), power (Loses / Gets power).
- Transfer alarms (7 Nov 2026): **Transfer rate above / below** (items/min) and **Max transfer above / below** (stacks/min), the two rates of its window.
- Actions: **Load mode** (on = load into vehicles, off = unload them), **Replace differing fuel** (the window's "Replace Differing Fuel" switch: on swaps a docked vehicle's other fuel for the station's), Delete contents.

### Train stations (7 Nov 2026)

- Automation tab in the station window (sliding pages, like the production window).
- Alarms: Train arrives (docks or stops), Train leaves, Docking done (its platforms finished; renamed from "Transfer done" 7 Nov 2026: the transfer itself is tracked on the freight platforms).
- Action: **Display text** (a text field): the station's display (its 3D sign, `UFGTextRenderer`) shows the text; empty shows its name again. The station is **not renamed** (map, timetables keep the name; decided 7 Nov 2026). Only the picture changes, re-drawn every second on every machine from our replicated entry: nothing in the game's data.
- No on / off: stations have no standby.
- Train arrives / leaves: a train with `IsDocked()` whose `mDockedAtStation` is this station (the station's own docking status and docked vehicle stay empty: tested 7 Nov 2026). A station without freight platforms has no docking (trains only stop): a train standing (under 1 m/s) on the station (its bounds + 2 m) counts as arrived.
- **States left by actions are released when their actions go** (7 Nov 2026): a signal with no Blocked action left is unblocked, a station with no Display text action left shows its name (`ReleaseActionStates`, on every rules change and alarm removal).

### Railway signals (started 7 Nov 2026)

- Alarms: Turns red, Turns green (green = clear or dock), Train on track, Track empty (the track the signal guards; renamed from "block" so it is not mixed up with the Block action). Tested 7 Nov 2026: blocking works; a train already far enough into the guarded track has it reserved and is not stopped.
- Action: **Blocked** with the on / off switch (on = Block, off = Unblock), kept per signal and saved in our data only. A blocked signal shows red and trains entering its block through it wait; a train already past or committed is not stopped. Without the mod every signal works normally (nothing written into the game's data).
- How: hooks on the game's block (`HasOverlappingTrains` / `HasOverlappingApprovedReservations` answer "busy" for reservations entering through a blocked signal; `GetAspectFor` answers red). `AMSignal` in `Automation/AMSignal.*`.

### Actions grouped by alarm (decided 6 Nov 2026, option A)

- The Actions panel shows one block per alarm: a header with the alarm (a dropdown: changing it moves all the block's actions) and a bin, the alarm's actions under it (moved right, no alarm column), and "+ action" under them. The panel's button is "+ Listen to an alarm" (a new block, its alarm chosen in its header). Same in the HUB.
- The bin of a block with several actions must be held down (the big bin, 2.5 s); with one action a click removes it.
- Second version (6 Nov 2026, after testing the first): every action is one line, as high as the value lines: `[ Set clock speed   [100] % ] [bin]`. An action cannot be changed, only its value (number, recipe, sound, message text) or removed; "+ action" opens the list of actions to add. Pause / Resume are one action **Running** with the options' on / off switch (on = runs, off = standby: like the game's standby lever, where 1 runs and 0 stands by; renamed from "Standby", whose on meant paused and read backwards); Turn on / Turn off are one **Power** with the switch. The alarm in the header can still be changed.
- One alarm cannot have the same action twice (two messages) or two of a kind (Pause / Resume / Pause or resume; Turn on / Turn off / Turn on or off): the Do list hides them, "+ action" picks the first free one.
- Saved as before (each action with its alarm): only the drawing changed. A header cannot take an alarm that already has its own block here.
- The Alarms panel stays as it is.

### World actions: the Automation tab of the HUB terminal (decided 4 Nov 2026)

Some actions have nothing to do with a machine: they only need to hear an alarm (Show message; later maybe a sound, ringing another alarm, "A and B rang"). They live in an **Automation tab of the HUB terminal**, next to Milestones.

- The HUB **listens**: "when alarm X rings, do Y". It also **rings** alarms on **world events** (decided 5 Nov 2026): its Alarms section has "+ Add alarm" and the World Event column offers world events only (machines never offer them). First world event: **Milestone completed** (any milestone, by any player). The **MAM** has its own Automation tab too (decided 5 Nov 2026): alarms only, on its world event **Research completed**; what happens when they ring is set on machines, signs or the HUB. World events are rung by the world itself (the message says "Rung by the MAM (research name)"), so the HUB and the MAM have no locate button in the overview, and neither does a machine you are already standing next to. More later (night falls, a fuse blows, power storage low). The Alarms Overview counts the HUB as a ringer ("The HUB (world events)").
- World actions: **Show message** (our notification: Wi-Fi icon, the alarm's name, your text or "Rung by <machine>"; stacked at the side with the game's notifications) and **Play sound** (one of 5 short game sounds: Blip, Ping, Warning, Alert, Success), for every player.
- Show message moves there from the machines; machine rules with Show message from older saves are moved to the HUB on load.

### Alarms Overview: a second HUB tab (decided 5 Nov 2026)

A table of every alarm in the world, to find alarms that are only half made (someone listens but nobody rings it, or the other way round):

| Name | Rung by | Listening |
|---|---|---|
| PLATES BACKED UP | 3 [bin] | 2 [bin] |

- **Rung by**: how many machines ring the alarm. Its bin removes the alarm's rows from every machine that rings it.
- **Listening**: how many machines, signs and the HUB do something when it rings. Its bin removes every action on that alarm, and the signs' designs for it (a sign with no designs left goes back to its normal design).
- The alarm name takes the rest of the row; the two count columns are fixed.
- A count of 0 shows "nobody" in orange. **Bins are held down for 2.5 seconds**: a big bin in the middle of the screen fills up from the bottom, from transparent to solid white, with "Hold to remove ..." under it; letting go earlier removes nothing. Pointing at a bin says what it will remove. Pointing at a name explains the alarm (who rings it, who listens); names are not clickable.
- **Two regions** (5 Nov 2026, second look): the page is as wide as the HUB's Select Milestone page; left the Alarms table, right a **Machines** region, empty at first with a line saying how to fill it. **Machine lists**: clicking a count (Rung by / Listening) shows in the Machines region the list of who rings / listens to it, nearest first: `[machine icon] [name] [distance] [locate] [bin]` (signs and the HUB too). Clicking the same cell again closes it; another count cell swaps to its list (one list at a time). The bin removes the alarm from that machine only (hold 2.5 s like the others). Built for thousands of machines: the rows are made 50 per frame behind a progress bar, the list scrolls in the region. **Locate** (decided 5 Nov 2026): like the resource scanner, without a scan: the scanner's ping sound, the machine's icon on the compass and the map (only for the player who asked), and a ring effect pulsing on the machine, until the player gets within 8 m of it (a foundation; or 15 minutes, or locate clicked again); on the compass it behaves like an ore (top row, stays at the edge when not ahead). The bin of a single machine removes at once (no hold).
- Same class as the Automation tab (`UAMAutomationTab`), hooked a second time into the HUB with the name `AM_OverviewTab`; the name switches it to overview mode. Clearing goes to the server (`ServerClearAlarm`).

### Unlock

- Unlocked by a **MAM research, as early in the game as possible**, so players can use it from the start.
- Exact research tree and cost: to decide (see open questions).

### What can be automated (eventually)

Everything that has a switch or an internal state that can change:

- every production machine (Smelter, Constructor, Assembler, Foundry, Manufacturer, Refinery, Blender, Packager, Particle Accelerator, Converter, Quantum Encoder, ...)
- generators and extractors (miners, water extractors, oil pumps, resource well pressurizers)
- storage containers (fill level: empty, full, above / below a level)
- railway signals
- power switches and other switchable buildings

**We build one machine type at a time**, starting with **the Smelter and the Constructor**. Both are the same kind of building in the game's code (manufacturers), so one implementation proves the whole system (events, UI, save, multiplayer) on two machines, and a Smelter → Constructor chain can be tested at once. The others then reuse it.

### UI

- **Dropdowns** (decided 5 Nov 2026): every dropdown (Machine Event, Do, On Alarm, Recipe, Sound, Item) opens a window floating over the page, starting where the dropdown is: a search box on top (type to filter, Enter picks the first), the choices under it in a scrolling list, with icons for items. A click outside closes it.

- **No new windows.** The Automation section is **added to the game's existing window** of the machine (a new tab, or a panel inside the window, whichever the game's widget allows).
- Only buildings that have **no window at all** (possibly rail signals, power switches...) get a small window of our own.
- How: SML widget hooks can insert our widget into a named panel of a game widget. The game's widget trees are not visible in the editor (placeholder assets, like the textures in Road Maker), so first we **dump the widget tree of an open machine window to the log** in game, find the right panel, then hook into it.

### Core rule: safe to uninstall, small saves

If a player stops using the mod, **their machines must stay exactly where they are**, and the save must not be bloated while they use it.

- **We never replace or change the game's machines.** No new machine classes, no "automated smelter" building. The game's own Smelter stays a vanilla Smelter.
- **All mod data lives in one save object of our own** (a world subsystem saved with the game), not in the machines. If the mod is removed, the game simply drops that one object; every machine loads as normal.
- **Compact data:** each automated machine stores a reference to the building and its rules; a rule is its event name plus one small number (status or action). Roughly tens of bytes per rule. Machines without automation store nothing.
- **The event list is not stored separately:** it is every event name used by some rule. So an event that no rule uses any more is gone by itself (the "unused events are deleted" rule, for free).
- Dismantled machines are removed from our data automatically, so nothing piles up.
- After uninstalling, machines keep the last state the automation gave them (e.g. paused, or a changed clock speed), like any setting the player changes by hand. The mod page should say so.

### Underglow: seeing what is automated (decided 5 Nov 2026, replaces the marker)

In a crowd of machines the player must see at a glance which ones are automated, without spoiling the factory's look:

- **A glow on the floor under the machine, like a tuned car's underglow**: cyan / light blue like the build hologram, emissive so it blooms, brightest at the machine's edge and fading out about 60 cm around it with a rounded shape. Subtle and gentle, also at night: it must not wash the floor out or draw a bright rim (tuned 5 Nov 2026: fade 1.5 m wide, ending 45 cm past the machine's visible edge).
- **Projected onto the floor** (a decal), not a flat plane: it lies on whatever is below (foundations, ground, the machine underneath in a stack), and leaves the floor's own colour alone (it only adds light).
- **Signs never glow** (nothing may change how a sign looks except its own designs).
- **Never saved**: made when the world loads, from our own data; gone with the mod.
- Earlier ideas dropped: the floating Wi-Fi marker (in the way, especially on signs and stacks); a see-through overlay of the machine (would not follow animated parts such as the Constructor's arms); making the secondary colour glow (the game's paint material has no glow input; it would mean replacing the game's factory material).
- Later: the colour shows the state (e.g. flashes when an alarm rings); maybe only while the build gun is out.

### Code approach (inspired by Road Maker, not copied)

- Logic in C++; Blueprint assets generated by Python scripts where possible, as in Road Maker.
- Event list and per-machine automation settings saved in the mod's own save object (see "Core rule"), never in the game's buildings.
- Statuses read from the game's factory classes; actions use the game's own functions (e.g. pause / standby, clock speed), so machines behave exactly as if the player had done it by hand.
- Multiplayer from the start: clients send their changes to the server through a remote call object; the server runs all events and actions.

## Open questions

1. ~~First machine~~ — decided: the Smelter and the Constructor.
2. **Where in the machine window:** a new tab, or a panel inside the existing window? Depends on what the game's widget allows (see UI).
3. ~~Multiple events~~ — decided: every row acts on its own (see "On a machine").
4. **Marker:** shape decided (flat Wi-Fi icon facing the camera, white). Still open: size, height above each machine, and the state colours.
5. **MAM research:** which research tree, which node, what cost.
6. **Statuses and actions for the Smelter and Constructor:** the exact list (see below, to confirm against the game's code).

## Machine events and actions for production machines (Smelter, Constructor, ...)

Machine events (an alarm rings when one becomes true): Output full, Output not full, Starts producing, Stops producing, Loses power, Gets power, Input missing, Input available, Paused, Resumed, Productivity below N%, Productivity above N%, Recipe changed, Input full, Input not full, Wrong input, Input correct.

Actions: Pause, Resume, Pause or resume, Set clock speed N% (within what the machine's power shards allow), Delete output, Delete input (our own functions: the items are destroyed), Change recipe (empties the input and output slots, then switches to the chosen recipe; only recipes the machine can make and the player has unlocked), Show message and Play sound (world actions, in the HUB's Automation tab).

Each one has an explanation in the tab's info panel.

### Miners (started 5 Nov 2026)

Miners have no input and no recipe, so they get fewer machine events and actions; the lists in the tab only show what the machine has.

- Machine events: Output full, Output not full, Starts producing, Stops producing, Loses power, Gets power, Paused, Resumed, Productivity below N%, Productivity above N%.
- Actions: Pause, Resume, Pause or resume, Set clock speed N%, Delete output.
- No Options (Output overflow needs input belts).
- The same rules cover the other extractors (water extractor, oil extractor, resource well extractor): they are the same kind of machine in the game. Their windows come one by one.
- The miner's window has no tabs (one page), unlike the production machines' window, so where the Automation page goes is worked out from a dump of that window.

### Storage containers (started 5 Nov 2026)

Storage Container and Industrial Storage Container (same window, `Widget_Storage`). The game has no way to empty a container except taking everything out by hand, so the mod adds one.

- Machine events: Full, Not full, Empty, Not empty, Filled above N%, Filled below N% (a slot with half a stack counts as half a slot).
- Full = the belt into it stops: every slot holds a full stack, or a belt brings an item it has no room for (in a mixed container that can happen before every slot is full). A separate "Belt blocked" event was tried and dropped (decided 5 Nov 2026): for one kind of item it is the same moment as Full, and a blocked output belt is the next machine's "Input full" / "Full".
- Item count above N / Item count below N, for one item picked under the alarm (decided 5 Nov 2026): for containers with several kinds of items, e.g. restart only the Constructor making Iron Plates when fewer than 200 are left. The item is picked in a dropdown window with the items' icons: items in this container first, then every item of the unlocked recipes. The number line reads "# [500] [icon Iron Plate]".
- Actions: Delete contents (destroyed for good, like Delete output). With Full or Filled above on the same container it becomes an overflow that throws away what nobody uses.
- The container's own events are the useful part: e.g. "Filled below 10%" rings an alarm that resumes the machines filling it, "Filled above 90%" pauses them.
- Not included for now: the Dimensional Depot uploader (its own window), fluid buffers (other kind of building).

### Copy and paste (decided 5 Nov 2026)

Automation is copied together with the game's own machine settings:

- **Ctrl+C / Ctrl+V** while looking at a machine, and **middle click** then build: everything (the game's settings and the automation).
- The window's **Copy Settings / Paste Settings** buttons: only the game's settings (e.g. a sign's design, a Smelter's recipe), as without the mod (decided 5 Nov 2026: people want to copy only a design).
- Our **Copy automation settings / Paste automation settings** buttons at the bottom of every Automation page: only the automation.
- **Middle click** (sample a building with the build gun) on an automated machine, then build: the new buildings get the same automation.
- Only between the same machine (Constructor to Constructor, Miner Mk1 to Miner Mk1): the game's own grouping puts all production machines together, which is not wanted. Pasting replaces the machine's automation; copying a machine without automation and pasting it removes the other machine's automation (decided 5 Nov 2026).
- A notification says what was copied / pasted ("AUTOMATION COPIED: 2 alarms, 1 action").

### Fuel generators (started 5 Nov 2026: Biomass Burner first)

Biomass Burner, Coal Generator, Fuel Generator and Nuclear Power Plant are the same kind of machine in the game (one input: fuel); their windows come one by one, the Biomass Burner first.

- Machine events: Starts / Stops producing, Paused / Resumed, Out of fuel / Has fuel, Fuel below N (items in the fuel slot), Load above / below N% (how much of what it can make the grid uses), Fuse blown / Fuse reset (its grid's fuse), Input full / not full (fuel slot), Wrong input / Input correct (a belt brings something that is not fuel).
- Actions: Pause, Resume, Pause or resume, Set clock speed N%, Delete input (its fuel).
- Ideas: Load above 90% on the main generators starts backup burners; Load below 20% pauses them; Fuse blown shows a message.

### Signs: a design per alarm (decided 5 Nov 2026)

Signs only react (no machine events). The Sign Designer gets a list of designs on its left (first tabs in the title bar: crowded with many designs and too close to the close button):
`Designs: Default / PLATES BACKED UP [bin] / PLATES FLOWING [bin] / ... (scrolls) / + Auto Settings`.

- "Default" (was "Normal") is the sign's normal design; each other row is the design shown when its alarm rings. The game's own designer edits the selected row (the preview shows it). The bin on a row removes that design.
- "+ Auto Settings" opens, inside the list under it, a search field and the alarms without a design (the button reads "Cancel" meanwhile); typing a name that does not exist yet and pressing Enter makes a design for a new alarm. A new design starts as a copy of the one being edited.
- When an alarm rings, the sign shows its design and keeps it until another of its alarms rings (an alarm has no "stopped ringing"; use pairs: BACKED UP red, FLOWING green). The design showing is saved.
- Copy: Ctrl+C / Ctrl+V and middle click copy the design and the alarm designs; the window's Copy / Paste Settings buttons only the design open in the editor (any row).
- Uninstall-safe: without the mod the sign keeps the design it was showing.

### Power switches (done 5 Nov 2026)

- Automation tab in the switch window (Power Switch and Priority Power Switch share it).
- Alarms: Switched on / off; A usage > / < B usage; A power > / < B power (power = the generators' capacity on that side; each side counts its own buildings, also while the switch joins them).
- Actions: Turn on, Turn off, Turn on or off; Set priority 0-8 (priority switches only).
- An orange warning in the tab while a cable elsewhere joins grid A and B into one grid.

### Lights and light control panels: settings per alarm (started 6 Nov 2026)

- Like signs: a side list in the light window (Street Light, other lights and the Light Control Panel share one window): `Settings: Default / ALARM [bin] / ... / + Auto Settings`.
- A row's settings are the window's own controls: colour slot, intensity, night mode, on / standby. Selecting a row loads its settings into the window (and onto the light); the game's controls edit them.
- When an alarm rings, the light (a panel: all its lights) takes that alarm's settings; Default before any alarm rang. Closing the window puts back the settings it should have.
- A light with its own settings for an alarm wins over its control panel's settings for the same alarm (panels are applied first, then single lights; tested 6 Nov 2026: the street light won, and obeyed the panel again once its own alarm row was gone).
- No underglow on lights (like signs). Copy / paste and middle-click sampling carry the alarm settings.

### Wrong items on a belt (decided 4 Nov 2026)

A machine refuses an item that is not an ingredient of its recipe, so the belt stops behind it. Deleting that item was rejected: it would be a cheat. Instead:

- **Machine events "Wrong input" / "Input correct":** the first item waiting at an input belt is not an ingredient, or an input pipe carries a fluid that is not one.
- **Option "Pass wrong items through"** (section Options, at the bottom of the machine's tab; saved per machine; a machine with only this option on still gets the marker): the wrong item is taken off the belt and goes **straight onto the output belt**, never into the output slot, so production does not stop and no other machine event rings. The machine's own products have priority: the wrong item goes out only when the output has nothing to give. Nothing is created or destroyed. (First version, 4 Nov 2026, put it in the output slot: that stopped production and rang Output full; changed on 5 Nov.)
- **Option "Pass extra items through"** (6 Nov 2026, its own switch under the first one): a right item (an ingredient) waiting on an input belt while its input slot is full goes the same way, straight onto the output belt. Lets machines be chained on one belt: what one cannot take goes on to the next. Same rules: products first, an output belt needed, one waiting item per machine shared with the wrong-items option (a wrong item first).
- **Option "Output overflow"** (7 Nov 2026, replaces the two switches above with one): everything that would block an input belt, a wrong item or an ingredient whose slot is full, goes straight onto the output belt. Same rules as above. Saved as the two old flags (both set together), so old saves keep working; the switch shows on when either is set.
- **Action "Output overflow"** (7 Nov 2026): a compact switch row on every machine that has the option (on = Output overflow on). When the alarm rings it switches the machine's option on or off; the change is saved and shows in the Options switch. One per alarm (conflict family 6).
- Belts only. A pipe with the wrong fluid stays stuck (the alarm can tell you).
- This changes how factories can be built (mixed "sushi" belts, machines as filters). Idea, not decided: unlock the option together with the Smart Splitter.

### Underglow: circuit lines (7 Nov 2026, to test)

The glow under automated machines is now a **circuit-board pattern**: cyan traces (40 cm grid, and a fainter 20 cm one) with 45-degree bends and round pads, glowing over a faint cyan light, with the same soft fade at the edge. It reaches up to 1.3 m and fades out between 40 cm and 1.1 m, so the lines climb a little onto the machine (also on its sides: the wall limit is gone). Made so machines whose mesh hides the old flat glow (the truck station) still show it.

