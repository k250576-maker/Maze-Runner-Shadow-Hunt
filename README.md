# Maze Runner - Shadow Hunt

University C++ DSA Lab project using the normal installed Raylib 6.2 library
and its OpenGL 3.3 renderer. The existing window test is unchanged.

## Structure

- `src/main.cpp`: application entry point.
- `include/`: project headers.
- `assets/`: game resources.
- `build/`: generated executables and local runtime files, excluded from Git.
- `.vscode/`: shared build, run, IntelliSense, and debug configuration.

The project proposal PDF is retained in the root.

## Build and run on Windows

Requires an existing MSYS2 UCRT64 installation with g++, normal Raylib 6.2,
and (for debugging) GDB. Configuration assumes `C:/msys64/ucrt64`; adjust
paths in `.vscode/` if your installation differs.

Open this folder in VS Code:

- **Ctrl+Shift+B** builds the project and creates `build/` when needed.
- **Terminal > Run Task > Build and Run** builds and launches the game.
- **Run** launches the last build directly, without a PowerShell launcher.
- **F5** builds and starts the `Debug` configuration with Microsoft's C/C++ extension.

Equivalent PowerShell commands from the project root:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
& C:/msys64/ucrt64/bin/g++.exe -std=c++17 -g -Wall -Wextra -I include src/main.cpp -o build/game.exe -lraylib -lopengl32 -lgdi32 -lwinmm
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
& ./build/game.exe
```

Teammates with working OpenGL drivers need no Mesa, custom Raylib build,
or additional run script. The shared configuration uses the normal installed
Raylib library and sets no Mesa environment variables.

A laptop-specific Mesa workaround may be kept locally in the ignored `build/`
folder, with an optional ignored `run.ps1` launcher. These files are not part
of the shared project.
