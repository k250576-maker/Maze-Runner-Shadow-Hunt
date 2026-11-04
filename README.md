# Maze Runner – Shadow Hunt

University C++ DSA Lab project using the normal installed Raylib 6.2 library.
The existing Raylib window test is unchanged; no game features have been added.

## Structure

- `src/main.cpp`: application entry point.
- `include/`: future project headers.
- `assets/`: future game resources.
- `build/`: generated executables and machine-local runtime DLLs (ignored by Git).
- `.vscode/`: shared build, run, IntelliSense, and debug configuration.
- `run.ps1`: launch from any working directory, with Mesa environment variables.

The project proposal PDF is retained in the root. `build/game-original.exe` is
the preserved original executable. `build/Debug/raylibtest.o` is an existing
experimental object file; no experimental executable or custom Raylib is used.

## Build and run on Windows

Requires an existing MSYS2 UCRT64 installation with g++, normal Raylib 6.2,
and (for debugging) GDB. Configuration assumes `C:/msys64/ucrt64`; adjust the
paths in `.vscode/` and `run.ps1` if your installation differs.

Open this folder in VS Code. Press **Ctrl+Shift+B** to build, or choose
**Terminal > Run Task > Build and Run (Mesa)** to build and launch.
**Run (Mesa)** launches the last build. With Microsoft's C/C++ extension,
**F5** builds and starts GDB using the `Debug (Mesa)` configuration.
The build task creates `build/` when needed.

Equivalent PowerShell commands from the project root:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
& C:/msys64/ucrt64/bin/g++.exe -std=c++17 -g -Wall -Wextra -I include src/main.cpp -o build/game.exe -lraylib -lopengl32 -lgdi32 -lwinmm
.\run.ps1
```

## Local Mesa workaround

Mesa is only a workaround for this laptop's AMD OpenGL driver issue.
The local `opengl32.dll` and `libgallium_wgl.dll` live beside the executable
in `build/`. The launcher sets `GALLIUM_DRIVER=llvmpipe` and
`MESA_LOADER_DRIVER_OVERRIDE=llvmpipe`, and adds UCRT64's bin directory to
the process PATH for the installed Raylib and runtime dependencies.
It restores the caller's environment afterward. No system files or execution
policies are changed.

Teammates with working OpenGL drivers need no Mesa DLLs: build normally and
run `build/game.exe` with `C:/msys64/ucrt64/bin` on PATH. The Mesa variables
are relevant only when Mesa is present. No OpenGL 2.1 override or custom Raylib
build is required. Runtime DLLs and executables are local and excluded from Git.
