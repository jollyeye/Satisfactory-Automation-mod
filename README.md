<p align="center"><img src="Tools/IconSource/mod_icon_512.png" width="160" alt="Automated Machines icon"></p>

# Automated Machines

A [Satisfactory](https://www.satisfactorygame.com/) mod. Machines **trigger events** when something happens to them, and other machines **react**. No programming, no wiring, no cheats.

> *The Storage Container is 90% full → trigger `CONTAINER FULL` → the Constructors feeding it go on standby.*
> *A fuel generator is running low → trigger `FUEL LOW` → every player gets a message, and a backup generator starts.*

![A Constructor's Automation tab](docs/images/construct.jpg)

## Install

Install it with the [Satisfactory Mod Manager](https://smm.ficsit.app/) from its [ficsit.app page](https://ficsit.app/mod/AutomatedMachines). It needs Satisfactory build 520140 or newer and SML 3.12.1 or newer.

In the game, research **Automated Machines** in the MAM's new *Automated Machines* tab (6 AI Limiters). After that, the windows of your machines get an **Automation** tab.

## How it works

- **Conditions** (left card): what this machine watches, e.g. *output full*, *productivity below 50%*, *fuel below 20*, *loses power*, *train arrives*, *battery below 30%*. Each condition triggers the event you name next to it.
- **Actions** (right card): what this machine does when an event is triggered by any machine, e.g. *standby on/off*, *set clock speed*, *change recipe*, *turn a power switch on/off*, *show a message*, *play a sound*, *show a text on a sign or station*, *delete the items blocking a buffer*.
- **AND**: an action can wait for several events, like `CONTAINER FULL and POWER LOW`.
- **Somersloop amplification**: set a production machine's amplification (1x to 2x, in its own steps), using only the Somersloops you placed in it.
- **Point to the machine**: show every player a machine on the compass and the map, with its own icon or one of the game's sign icons. In the HUB: the machine that triggered the event.
- **Live state**: a condition's arrow turns green while the condition is met.
- **Hold vehicles here** (truck stations): a truck that reaches the station stops there until the event releases it.
- **Signs, lights, splitters**: signs, lights, light control panels, Smart / Programmable Splitters and the Priority Merger get one design or setting per event.
- **The HUB**: world conditions and actions (*milestone completed*, *show a message*), and the **Events Overview**: every event in your world, who triggers it, who listens, when it was last triggered, with search, sorting and Locate.
- **Output Overflow** (MAM, 2 High-Speed Connectors): production machines can push items that would block their input belt onto their output belt.
- Automated machines show a soft cyan circuit glow on the floor under them.

![A Storage Container's Automation tab](docs/images/container.jpg)

![A sign with one design per event](docs/images/signs.jpg)

![The Events Overview in the HUB](docs/images/hub.jpg)

## Good to know

- **No cheats**: the mod only does what you could do by hand. It creates no items, and delete actions only remove what you could throw in the trash yourself.
- **Copy & paste**: the game's Ctrl+C / Ctrl+V and middle click copy the automation too.
- **Safe to uninstall**: the rules live only in the mod's own save data. Without the mod your machines keep the last state the automation gave them. If a machine's Somersloop amplification was lowered, take a Somersloop out and put it back to restore it. A truck held at a station keeps its autopilot off: switch it on in its window.
- **Game updates**: if a game update changes the game code the mod reads, the mod pauses with a message instead of crashing. Your automation stays saved until the mod is updated.
- **Limits**: machines built from a blueprint start without automation (copy it with Ctrl+C / Ctrl+V or middle click). Made for multiplayer, but not tested with several players yet. Not automated: pumps, trains, doors, Geothermal Generators, elevators.

## Supported buildings

Production machines (Smelter to Quantum Encoder) · Miners · Water and Oil Extractors · Resource Well Pressurizer and Extractors · Storage Containers · Fluid Buffers · Biomass Burner, Coal, Fuel and Nuclear generators · Alien Power Augmenter · Power Storage · Power Switches and Priority Switches · Valves · AWESOME Sink · Train Stations, Freight Platforms and Railway Signals · Truck Stations · Drone Ports · Dimensional Depot Uploader · Signs · Lights and Light Control Panels · Smart / Programmable Splitters and Priority Merger · HUB · MAM

## Building from source

The mod is all C++ (Unreal Engine 5.3, Coffee Stain's engine build) with [SML](https://github.com/satisfactorymodding/SatisfactoryModLoader) 3.12. Its few assets are made by a Python script, so there is nothing to edit in the editor.

1. Set up the [modding environment](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/index.html): the CSS Unreal Engine and SML's starter project. Use the starter project's game headers for **the game build you play** (see *Game updates* below).
2. Clone this repository into the starter project's `Mods/AutomatedMachines` folder.
3. Run `Tools/build-and-package.bat` (edit the paths at its top first). It builds the editor, makes the assets with `Tools/create_assets.py`, then packages the mod straight into your game folder.
4. For a release, use **Alpakit Release** in the editor with the Windows, Windows Server and Linux Server targets; upload `Saved/ArchivedPlugins/AutomatedMachines/AutomatedMachines.zip`.

Checks:
- `Tools/check_source.py`: quick source checks without building.
- `Tools/check_exports.py <mod dll> <game dll>`: finds game functions the mod uses that the real game does not export (the "module could not be loaded" error).

### Game updates

The mod reads the game's classes with the memory layout of the headers it was built with. After a game update, rebuild on headers for the new build, then look for this line in `FactoryGame.log`:

```
LogAutomatedMachines: Game update check: the 67 game classes the mod reads match this game version (CL520140)
```

Any class listed as different means the headers do not match the game.

## For mod makers

Your mod's buildings can add their own conditions and actions to the Automation tab just by naming functions `AM_Condition_<Name>` / `AM_Action_<Name>` (no dependency on this mod), and your code can trigger and hear events. See [docs/MODDING.md](docs/MODDING.md).

## Docs

- [Design](docs/DESIGN.md): what the mod does and why, decision by decision.
- [Game UI guide](docs/GAME-UI-GUIDE.md): how the mod hooks into the game's windows.
- [Dev notes](docs/DEV-NOTES.md): implementation notes, what was tested, known issues.
- [Roadmap](docs/ROADMAP.md): releases and ideas.
- [Modding](docs/MODDING.md): conditions and actions for other mods' buildings, events from other mods.

## Feedback

Bug reports and ideas are welcome in the [issues](https://github.com/jollyeye/Satisfactory-Automation-mod/issues). For a crash, please attach your `FactoryGame.log` (`%LOCALAPPDATA%\FactoryGame\Saved\Logs`).
