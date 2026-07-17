# Development, tests, and playtesting

[Back to the README](../README.md) · [Architecture](architecture.md) ·
[Stage authoring](stage-authoring.md) · [Assets and audio](assets-and-audio.md)

## Configure and build

The supported Windows configuration is Visual Studio 2022 / MSVC with vcpkg
manifest mode. `CMakePresets.json` supplies the toolchain and the local raylib
overlay.

```powershell
cmake --preset default
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Use `Release` for a normal playable build. CMake copies `assets/` to the
target directory after each successful game build. If MSBuild fails in
`vcpkg.exe z-applocal`, close the running game first: Windows has a DLL or
executable handle open beside the target.

## Test suite

CTest runs focused C tests for gameplay, stage progression, title/menu,
settings/input, and bosses. On Unix, it also includes a short headless game
smoke run. The asset and generator tests are Python scripts:

```bash
python3 tests/test_stage_table.py
python3 tests/test_sprite_assets.py
python3 tests/test_xm_assets.py
python3 tests/test_sfx_assets.py
python3 tests/test_icon_asset.py
```

Run the relevant generator test whenever changing its source asset or tool.
Run the full CTest suite after gameplay, menu, boss, stage, or build changes.

## Manual playtest pass

- Confirm the ocean presents continuously and pixel art stays sharp after a
  display or raylib change.
- Check weapon-class rules: gun versus air, torpedo versus surface, mortar
  versus land; ensure terrain clamps torpedoes without blocking the player.
- Play both stages through their boss and salvage transitions, including
  Stage 2 lead torpedoes.
- Check pause, title, controls, Escape back/quit confirmation, fullscreen,
  and persisted audio settings.
- Review terrain at game scale for density, coastline continuity, hardpoint
  alignment, and joined-island seams.

Open work is tracked in [TODO.md](../TODO.md).
