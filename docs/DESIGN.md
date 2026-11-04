# Automated Machines — design

A Satisfactory mod that lets the player automate the machines they already have. Machines **fire events** when their status changes, and other machines **react** to those events. No programming: the player types event names and picks actions from lists.

The name starts with "Automated" on purpose: players who come back after months away should recognise the mod in their mod list at once. It automates existing machines; it does not add new "automatic machines".

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

**We build one machine type at a time.** The first one proves the whole system (events, UI, save, multiplayer); the others then reuse it.

### UI

- **No new windows.** The Automation section is **added to the game's existing window** of the machine (a new tab, or a panel inside the window, whichever the game's widget allows).
- Only buildings that have **no window at all** (possibly rail signals, power switches...) get a small window of our own.
- How: SML widget hooks can insert our widget into a named panel of a game widget. The game's widget trees are not visible in the editor (placeholder assets, like the textures in Road Maker), so first we **dump the widget tree of an open machine window to the log** in game, find the right panel, then hook into it.

### Core rule: safe to uninstall, small saves

If a player stops using the mod, **their machines must stay exactly where they are**, and the save must not be bloated while they use it.

- **We never replace or change the game's machines.** No new machine classes, no "automated smelter" building. The game's own Smelter stays a vanilla Smelter.
- **All mod data lives in one save object of our own** (a world subsystem saved with the game), not in the machines. If the mod is removed, the game simply drops that one object; every machine loads as normal.
- **Compact data:** the event names are stored once; each automated machine stores only a reference to the building plus a few small numbers per rule (status, event number, action, value). Roughly tens of bytes per machine. Machines without automation store nothing.
- Dismantled machines are removed from our data automatically, so nothing piles up.
- After uninstalling, machines keep the last state the automation gave them (e.g. paused, or a changed clock speed), like any setting the player changes by hand. The mod page should say so.

### Marker: seeing what is automated

In a crowd of machines the player must see at a glance which ones are automated, cheaply:

- The marker is **never saved**. It is created when the world loads, from our own data, and removed when a machine loses its last rule. Without the mod there is simply no marker.
- **One shared instanced mesh** for all markers (one instance per automated machine), not one actor per machine. Thousands of markers then cost about one draw call.
- It sits in the same place on every machine of a type (position per machine type, e.g. on the roof), and should be readable from a distance.
- Optional later: the instance colour shows the state (idle, just fired, paused by automation), through per-instance data, still without saving anything.
- The mesh is made in code or by our asset script, so no external 3D tools are needed.

### Code approach (inspired by Road Maker, not copied)

- Logic in C++; Blueprint assets generated by Python scripts where possible, as in Road Maker.
- Event list and per-machine automation settings saved in the mod's own save object (see "Core rule"), never in the game's buildings.
- Statuses read from the game's factory classes; actions use the game's own functions (e.g. pause / standby, clock speed), so machines behave exactly as if the player had done it by hand.
- Multiplayer from the start: clients send their changes to the server through a remote call object; the server runs all events and actions.

## Open questions

1. **First machine:** which machine do we start with? Suggestion: the **Smelter** (one input, one output, simple statuses).
2. **Where in the machine window:** a new tab, or a panel inside the existing window? Depends on what the game's widget allows (see UI).
3. ~~Multiple events~~ — decided: every row acts on its own (see "On a machine").
4. **3D model:** shape (antenna, beacon, small box with lights...), size, and where it sits on each machine.
5. **MAM research:** which research tree, which node, what cost.
6. **Statuses and actions for the first machine:** the exact list (see below, to confirm against the game's code).

## Candidate statuses and actions for production machines

To check against the game's headers before we commit to them.

Statuses (fire an event when it changes):

- starts / stops producing
- power lost / power back
- output full / output not full
- input missing / input available
- paused / resumed (by the player or by an action)
- productivity below / above a threshold
- recipe changed

Actions:

- pause / resume
- toggle pause
- set clock speed (a percentage)
- later: change recipe
