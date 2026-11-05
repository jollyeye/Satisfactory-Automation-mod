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

### World actions: the Automation tab of the HUB terminal (decided 4 Nov 2026)

Some actions have nothing to do with a machine: they only need to hear an alarm (Show message; later maybe a sound, ringing another alarm, "A and B rang"). They live in an **Automation tab of the HUB terminal**, next to Milestones.

- The HUB **listens**: "when alarm X rings, do Y". It has an Alarms section too, for world events to come later (e.g. night falls, a fuse blows); for now it rings nothing.
- World actions: **Show message** (our notification: Wi-Fi icon, the alarm's name, your text or "Rung by <machine>"; stacked at the side with the game's notifications) and **Play sound** (one of 5 short game sounds: Blip, Ping, Warning, Alert, Success), for every player.
- Show message moves there from the machines; machine rules with Show message from older saves are moved to the HUB on load.

### Automation Overview: a second HUB tab (decided 5 Nov 2026)

A table of every alarm in the world, to find alarms that are only half made (someone listens but nobody rings it, or the other way round):

| Alarm Name | Rung by | Listening |
|---|---|---|
| PLATES BACKED UP | 3 [bin] | 2 [bin] |

- **Rung by**: how many machines ring the alarm. Its bin removes the alarm's rows from every machine that rings it.
- **Listening**: how many machines, signs and the HUB do something when it rings. Its bin removes every action on that alarm, and the signs' designs for it (a sign with no designs left goes back to its normal design).
- A count of 0 shows "nobody" in orange. Bins need two clicks within 4 seconds; pointing at a bin says what it will remove. Pointing at a name explains the alarm (who rings it, who listens).
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

### Marker: seeing what is automated

In a crowd of machines the player must see at a glance which ones are automated, cheaply:

- The marker is **never saved**. It is created when the world loads, from our own data, and removed when a machine loses its last rule. Without the mod there is simply no marker.
- **Look: a flat Wi-Fi icon** (the game has none: its flat icon set `MonochromeIcons` has arrows, factory, power, train... but nothing signal-like, checked 4 Nov 2026; we draw our own with a Python script and use it for the tab button too) that always faces the camera, on every axis (also when looking from above or below). **White for now**; the colours come later.
- **One shared instanced mesh** for all markers (one instance per automated machine), not one actor per machine. Thousands of markers then cost about one draw call.
- How it stays cheap: the mesh is a single flat square; the material turns it towards the camera in the vertex shader (camera-facing billboard), so it works with instancing and nothing moves on the CPU. The Wi-Fi shape is a texture with transparency on that square, unlit so it reads in the dark.
- It floats in the same place above every machine of a type (position per machine type), and should be readable from a distance.
- Optional later: the instance colour shows the state (idle, just fired, paused by automation), through per-instance data, still without saving anything.
- The square mesh and the Wi-Fi texture are made by our scripts (Python, like Road Maker's icons), so no external 3D tools are needed.

- Stacks (5 Nov 2026): when something sits right on top of an automated machine (stacked containers), its marker floats halfway up in front of the side you are looking at (it follows you around), instead of above it; markers are placed again every 5 seconds.

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
- No Options (Pass wrong items through needs input belts).
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

Signs only react (no machine events). The Sign Designer's tab bar gets one tab per design:
`Sign Designer  [ Normal ][ PLATES BACKED UP | bin ][ PLATES FLOWING | bin ][ + AutoDesign ]` (our own tabs right of the window's title).

- "Normal" is the sign's normal design; each other tab is the design shown when its alarm rings. The game's own designer edits the selected tab (the preview shows it). The bin on a tab removes that design.
- "+ AutoDesign" opens a list of alarms with a search field; typing a name that does not exist yet and pressing Enter makes a design for a new alarm. A new design starts as a copy of the one being edited.
- When an alarm rings, the sign shows its design and keeps it until another of its alarms rings (an alarm has no "stopped ringing"; use pairs: BACKED UP red, FLOWING green). The design showing is saved.
- Copy: Ctrl+C / Ctrl+V and middle click copy the design and the alarm designs; the window's Copy / Paste Settings buttons only the design.
- Uninstall-safe: without the mod the sign keeps the design it was showing.

### Wrong items on a belt (decided 4 Nov 2026)

A machine refuses an item that is not an ingredient of its recipe, so the belt stops behind it. Deleting that item was rejected: it would be a cheat. Instead:

- **Machine events "Wrong input" / "Input correct":** the first item waiting at an input belt is not an ingredient, or an input pipe carries a fluid that is not one.
- **Option "Pass wrong items through"** (section Options, at the bottom of the machine's tab; saved per machine; a machine with only this option on still gets the marker): the wrong item is taken off the belt and goes **straight onto the output belt**, never into the output slot, so production does not stop and no other machine event rings. The machine's own products have priority: the wrong item goes out only when the output has nothing to give. Nothing is created or destroyed. (First version, 4 Nov 2026, put it in the output slot: that stopped production and rang Output full; changed on 5 Nov.)
- Belts only. A pipe with the wrong fluid stays stuck (the alarm can tell you).
- This changes how factories can be built (mixed "sushi" belts, machines as filters). Idea, not decided: unlock the option together with the Smart Splitter.
