# Seavious

Seavious is a compact horizontal-scrolling pixel-art shooter in C and
[raylib](https://www.raylib.com/). You fly a small surface craft through two
authored stages: first the open-ocean, dual-weapon tutorial; then an
archipelago where the salvaged mortar is needed to clear shore installations.
Each stage ends in a distinct boss and awards a run-carrying upgrade.

## Play it

The game opens on a title screen. Start a run, clear Stage 1 to salvage the
mortar, then clear Stage 2's fortress atoll to obtain the targeting computer.
Stage clear continues the same run; game over starts a fresh Stage 1 run.

| Action | Keys |
| --- | --- |
| Move | Arrow keys or WASD |
| Fire torpedo | Space |
| Fire mortar (after Stage 1) | Left Shift or X |
| Pause / menu | P |
| Screenshot (during gameplay) | Print Screen |
| Restart after game over or stage clear | R, Enter, or keypad Enter |
| Fullscreen | F11 |

Print Screen saves the clean presented gameplay frame as the next numbered PNG
in the `screenshots` folder beside the executable. Escape backs out of menus
and modals. During active gameplay it opens a quit confirmation rather than
closing the game immediately. The in-game Controls screen is the authoritative
binding list.

## Build on Windows

Install Visual Studio 2022 (or Build Tools) with the Desktop development with
C++ workload, install [vcpkg](https://github.com/microsoft/vcpkg), and set
`VCPKG_ROOT`. The project can be opened through its WSL UNC path or cloned to
a native Windows path.

```powershell
cmake --preset default
cmake --build build --config Release
.\build\Release\seavious-dev.exe
```

`seavious-dev.exe` is a temporary output name while a stale Windows handle
blocks the normal executable name. Close any running game before rebuilding:
vcpkg's deployment step must be able to update the DLLs beside it.

The checked-in raylib overlay is intentional. It works around a current
vcpkg/raylib configuration that can produce a blank window; the CMake preset
enables it through `VCPKG_OVERLAY_PORTS`.

## Documentation

- [Game design and progression](docs/game-design.md)
- [Stage authoring](docs/stage-authoring.md)
- [Assets and audio](docs/assets-and-audio.md)
- [Architecture and runtime structure](docs/architecture.md)
- [Development, tests, and playtesting](docs/development.md)
- [Working task list](TODO.md)

## License

MIT. See [LICENSE](LICENSE).
