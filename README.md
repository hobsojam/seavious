# Seavious

Retro vertical-scrolling shooter, built in C with [raylib](https://www.raylib.com/).

Renders internally at 240x320 to a `RenderTexture2D` with point (nearest-neighbor)
filtering, then upscales 3x to the window — the standard trick for a crisp
pixel-art look at native resolution.

## Building on Windows (MSVC + vcpkg)

Prerequisites:
- Visual Studio 2022 (Desktop development with C++ workload) or the standalone
  Build Tools
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` set

This project is on the WSL filesystem. Open it from Windows via the UNC path
(`\\wsl$\<distro>\home\james\dev\seavious` or `\\wsl.localhost\...`), or clone
it to a native Windows path for faster builds — either works with CMake.

```powershell
cmake --preset default    # or: cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
.\build\Release\seavious.exe
```

Or just open the folder in Visual Studio 2022 — it detects `CMakeLists.txt`
and `vcpkg.json` automatically (manifest mode) and configures itself.

## Controls

Arrow keys / WASD to move. (No shooting yet — this is the render/input skeleton.)
