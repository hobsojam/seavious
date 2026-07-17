# Architecture and runtime structure

[Back to the README](../README.md) · [Stage authoring](stage-authoring.md) ·
[Development guide](development.md)

The game targets a 512×384 internal pixel canvas: 512×352 play area plus a
32-pixel HUD strip. `src/present.c` integer-scales and letterboxes that canvas
for the window; the art uses point filtering to stay crisp.

| Area | Main files | Responsibility |
| --- | --- | --- |
| App loop | `main.c` | Window, state transitions, menus, CLI devtools, frame orchestration |
| State and update | `game_state.*`, `game_update.*`, `gameplay.*` | Entity pools, input, movement, weapon and collision rules |
| Rendering | `game_render.*`, `present.*`, `assets.*` | World/HUD/menu drawing, terrain composition, presentation, asset loading |
| Stages | `stage.*`, `stage_data.h`, `stage*_data.c` | Scroll-driven events, terrain, upgrades, stage registration |
| Bosses | `boss.*` | Leviathan and fortress-atoll fight state, weapon-class collisions, salvage sequences |
| UI and settings | `menu.*`, `title.*`, `input.*`, `settings.*` | Modal navigation, shared bindings, persisted options |
| Audio | `audio.*` | Music state selection, sound loading, effects mix |

Stage tables are generated from ASCII maps, not hand-maintained C. The
`StageDescriptor` supplies events, terrain, hardpoints, length, and the
salvage award to the rest of the runtime. That keeps stage progression and
the development stage-select loadout on one source of truth.

`main.c` deliberately disables raylib's default Escape-to-exit binding. It
routes Escape to the active menu/modal first, then asks for confirmation
before ending a live run. The title and pause menus consume a small
`MenuInput` value, which keeps their state machines independently testable.

For quick playtests, launch with `--devtools` to expose stage select on the
title screen, `--stage N` to start directly in a stage with its canonical
prior upgrades, or `--boss` with a stage to jump to that stage's boss lock.
