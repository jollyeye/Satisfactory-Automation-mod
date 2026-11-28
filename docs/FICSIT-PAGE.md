# ficsit.app page — Automated Machines 1.2.0

Copy each part into the matching field on ficsit.app.

- **Name:** Automated Machines
- **Mod reference:** AutomatedMachines
- **Logo:** `Tools/IconSource/mod_icon_512.png`
- **Source URL:** https://github.com/jollyeye/Satisfactory-Automation-mod
- **Short description** (fits the 128-character limit):

> Machines trigger events when something happens to them, and other machines react. Automate your factory, no programming.

---

## Full description (Markdown)

![Automated Machines in a Constructor](https://raw.githubusercontent.com/jollyeye/Satisfactory-Automation-mod/main/docs/images/construct.jpg)

**Automated Machines** adds an **Automation** tab to your machines' own windows. A machine watches itself and **triggers an event** when something happens to it. Any machine that **listens** to that event then **acts**.

> *The Storage Container is 82% full → trigger `CONTAINER FULL` → the Constructors feeding it go on standby.*
> *A fuel generator is running low → trigger `FUEL LOW` → every player gets a message, and a backup generator starts.*

No logic gates, no wiring, no programming: name an event on one machine, then pick it on another.

### How it works

- **Conditions** (left card): what this machine watches. Examples: *output full*, *productivity below 50%*, *fuel below 20*, *input full*, *loses power*, *train arrives*, *battery below 30%*. Each condition triggers the event you name next to it.
- **Actions** (right card): what this machine does when an event is triggered by any machine, this one included. Examples: *standby on/off*, *set clock speed*, *change recipe*, *turn a power switch on/off*, *show a message to every player*, *play a sound*, *show a text on a sign or station*, *delete the items blocking a buffer*.
- **AND:** an action can wait for several events: `CONTAINER FULL and POWER LOW`. It runs when one of them is triggered while the others are still active.
- **Somersloop amplification**: an action sets a production machine's amplification (1x to 2x, in its own steps), using only the Somersloops you placed in it.
- **Point to the machine**: every player gets a machine on the compass and the map, with its own icon or one of the game's sign icons (warning, power, droplet, trash...). In the HUB it points to the machine that triggered the event.
- **Live state**: a condition's arrow turns green while the condition is met.
- **Hold vehicles here** (truck stations): a truck that reaches the station stops there (its autopilot off, as you can do in its window) until the event releases it. For example: hold trucks at the source while the destination is full.
- **Percentages** are bars you can drag. Item counts let you pick the item.
- Pointing at anything explains it at the bottom of the window.

![A Storage Container that stops its suppliers when it fills up](https://raw.githubusercontent.com/jollyeye/Satisfactory-Automation-mod/main/docs/images/container.jpg)

### Signs, lights, splitters

Signs, lights and light control panels, Smart and Programmable Splitters and the Priority Merger get a list of **designs / settings per event** at the side of their own window. A sign can turn green with *WORKING* while the line runs, and red with *NO FUEL* when a generator empties. A splitter can send iron to another line when a container fills up.

![A sign with one design per event](https://raw.githubusercontent.com/jollyeye/Satisfactory-Automation-mod/main/docs/images/signs.jpg)

### The HUB

- The **HUB's Automation tab** has world conditions (*milestone completed*) and world actions (*show a message*, *play a sound*) that need no machine. The MAM has *research completed*.
- The **Events Overview** in the HUB lists every event of your world. It shows how many machines trigger it, how many listen, and when it was last triggered. You can search, sort by any column, and see each event's machines, nearest first, with a Locate button. Hold the bin to delete an event everywhere.

![The Events Overview in the HUB](https://raw.githubusercontent.com/jollyeye/Satisfactory-Automation-mod/main/docs/images/hub.jpg)

### Good to know

- **Unlock it in the MAM:** a new research tab, *Automated Machines*, costs **6 AI Limiters**. Under it, *Output Overflow* (2 High-Speed Connectors) lets production machines push items that would block their input belt onto their output belt.
- **You can see which machines are automated:** they get a soft cyan circuit-board glow on the floor under them.
- **No cheats:** the mod only does what you could do by hand. It creates no items. Delete actions only remove what you could throw in the trash yourself.
- **Copy & paste:** the game's own copy and paste (Ctrl+C / Ctrl+V, middle click) copies the automation too. The tab also has its own Copy / Paste buttons.
- **Safe to uninstall:** the mod stores its rules only in its own save data. Remove it and your machines simply keep the last state the automation gave them (clock speed, standby, switches). If a machine's Somersloop amplification was lowered, take a Somersloop out and put it back to restore it. A truck held at a station keeps its autopilot off: switch it on in its window.

### Supported buildings

Production machines (Smelter, Constructor, Assembler, Foundry, Manufacturer, Refinery, Blender, Packager, Particle Accelerator, Converter, Quantum Encoder) · Miners · Water and Oil Extractors · Resource Well Pressurizer and Extractors · Storage Containers · Fluid Buffers · Biomass Burner, Coal, Fuel and Nuclear generators · Alien Power Augmenter · Power Storage · Power Switches and Priority Switches · Valves · AWESOME Sink · Train Stations, Freight Platforms and Railway Signals · Truck Stations · Drone Ports · Dimensional Depot Uploader · Signs · Lights and Light Control Panels · Smart / Programmable Splitters and Priority Merger · HUB · MAM

### Limits in 1.0

- Machines built from a **blueprint** (Blueprint Designer) start without automation. Copy it with Ctrl+C / Ctrl+V or middle click instead.
- **Multiplayer:** made for it (the server runs the rules, every player sees and edits them), but not tested with several players yet. Please report anything odd.
- Not automated: pipeline pumps, trains / locomotives, doors, Geothermal Generators, elevators.

### For mod makers

Your mod's buildings can add their own conditions and actions to the Automation tab just by naming functions `AM_Condition_<Name>` / `AM_Action_<Name>`, with no dependency on this mod. Road Maker's boom barrier already does. See [MODDING.md](https://github.com/jollyeye/Satisfactory-Automation-mod/blob/main/docs/MODDING.md).

### Feedback

Bug reports and ideas are welcome on the [GitHub repository](https://github.com/jollyeye/Satisfactory-Automation-mod/issues).
