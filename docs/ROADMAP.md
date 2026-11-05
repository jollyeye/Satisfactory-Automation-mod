# Automated Machines — what is left (5 Nov 2026)

Done and tested so far: see the table in `DEV-NOTES.md` ("What works").

## Left to do (before a first release)

1. **Multiplayer test**: a client changing rules, copy / paste, sign designs, the HUB overview bins; and a dedicated server (no UI, no glows there, rules still run).
2. **Blueprints (the game's Blueprint Designer)**: a machine built from a blueprint gets no automation today (only Ctrl+C / Ctrl+V and middle click copy it). Decide: carry the rules inside blueprints, or say on the mod page that they don't.
3. **MAM research to unlock the mod** (as early as possible): which tree, node and cost. Until then everything is available from the start.
4. **Clean-up**:
   - switch off the widget dump (`AMWidgetDump::Install()` in the module), it writes many lines to the log;
   - remove the old Wi-Fi marker assets (`M_AM_Marker`, `SM_AM_Marker`, `T_AM_MarkerIcon`) from `Tools/create_assets.py`;
   - the `.uplugin` description still says "events": use the player's words (alarms, actions).
5. **Release**: mod icon, ficsit.app page text (what it does, "safe to uninstall: machines keep the last state the automation gave them"), version 0.1.0 → 1.0.0, upload.

## Settings (decided 5 Nov 2026: none for now)

No settings tab in the HUB. Nothing in the mod needs a world setting: every behaviour is per machine, in its Automation tab. The only candidates are personal display preferences (underglow on / off / only with the build gun, brightness; notification sound on / off). If players ask for them, they go in SML's own mod configuration (main menu > Mods > Automated Machines), per player, not in the save or the HUB.

## More machines (one at a time, like so far)

- Generators: Coal (fuel + water), Fuel (fluid fuel), Nuclear (fuel rods, waste output), Geothermal (nothing to feed: power events only).
- Extractors: Water Extractor, Oil Extractor, Resource Well Pressurizer.
- Power: **power switches and priority switches** (actions On / Off), **power storage** (events: charge above / below N%, full, empty).
- Logistics: programmable / smart splitters (action: change the filter), pipeline pumps / valves (action: set the flow limit), fluid buffers and industrial fluid buffers (fill level, like storage).
- Lights and light control panels (actions On / Off, change colour).
- Trains: stations, freight platforms, railway signals; truck and drone stations.

## Useful optional features

- **Find an alarm's machines**: clicking an alarm name in the HUB overview lists the machines that ring it and listen to it (name, distance, direction), optionally with a map marker.
- **World events in the HUB** (its Alarms panel is ready for them): night falls / day breaks, a power circuit's fuse blows / is reset, power storage low.
- **More alarm logic**: ring another alarm (chains); "A and B rang"; a delay ("N seconds after"); a cooldown so a flapping machine does not ring 10 times a second; "every Nth time".
- **Glow shows the state**: flashes when the machine rings an alarm, another colour while the automation keeps it paused; option to show glows only while the build gun is out.
- **Alarm history in the HUB**: when each alarm last rang and who rang it, how often in the last hour.
- **Machine names**: give an automated machine a name, shown in messages and in the overview.
- **Alarm sets**: save a machine's alarms and actions as a named preset and apply it to others (beyond copy / paste).
- **Per-alarm notification sound** (the Show message action with a chosen sound).
