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

- `src/main.c` — everything currently lives here (single-file skeleton).
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
