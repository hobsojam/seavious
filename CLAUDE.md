# Seavious

Retro vertical-scrolling shooter in C + raylib, targeting native Windows.
See `README.md` for the full design (aesthetic, core mechanic, structure,
current scope) — keep that Design section up to date whenever a design
decision is made or changed in conversation, without waiting to be asked
again.

See `TODO.md` for the working task list — check items off as they're
completed and add new ones as they come up in conversation, same as the
README Design section.

## Build (Windows, MSVC + vcpkg)

```powershell
cmake --preset default
cmake --build build --config Release
.\build\Release\seavious.exe
```

Requires `VCPKG_ROOT` set and Visual Studio 2022 (or Build Tools) installed.
No compiler is available in the WSL dev environment — code changes here
can't be built or run locally; verification happens on the Windows side.

## Structure

- `src/main.c` — entry point: window/init, the frame loop (music → update
  → draw → present), and the CI smoke-test harness hooks.
- `src/gameplay.c/.h` — pure gameplay rules (movement, spawning, collision,
  scoring); no rendering or raylib state, unit-tested in `tests/`.
- `src/game_state.c/.h` — the `GameState` struct (all run state + entity
  pools), run reset/death flow, and effect-pool helpers.
- `src/game_update.c/.h` — one `UpdateGame` simulation step orchestrating
  the gameplay-layer functions.
- `src/game_render.c/.h` — one `DrawGame` frame draw (scene + HUD).
- `src/assets.c/.h` — `GameAssets` texture bundle, load/unload.
- `src/audio.c/.h` — music streams and the hard-cut track selection.
- `vcpkg.json` / `CMakeLists.txt` / `CMakePresets.json` — manifest-mode
  vcpkg + CMake build setup.

## Publishing Changes

- When the user asks for a PR, push the branch first, then create a draft PR
  with `gh pr create --draft --base main --head <branch> --title ... --body ...`.
- If `gh` reports a GitHub API or DNS error, retry after `gh auth setup-git`
  and use `gh pr create` directly; do not assume the branch push alone created
  the PR.
- Keep the PR body plain Markdown and include the change summary and the
  validation steps used.
