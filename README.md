# Maze Runner - Shadow Hunt

University C++ DSA Lab project using the normal installed Raylib 6.2 library
and its OpenGL 3.3 renderer.

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

## Generated labyrinth

Each launch picks a new seed and logs it to the terminal. To replay a layout
with the same build, pass an unsigned 32-bit seed.

```powershell
& ./build/game.exe --seed 42
```

- `include/maze_generation.h` uses randomized depth-first search with a custom
  stack. It visits each passage vertex once, backtracks at dead ends, and
  opens eight additional connections to create alternate routes.
- The generator converts the logical graph to physical tiles only after
  selecting the connections. Most corridors are two tiles wide. Selected
  bands are three tiles wide, and occasional gates narrow to one tile. A
  stone separator stays in place wherever the graph has no connection.
- Each of the four chamber bays has a single doorway. Breadth-first search
  uses a custom queue to visit the physical floor tiles from spawn. It checks
  that every floor tile is reachable and reserves a future exit at the
  farthest reachable tile in the three non-entrance chambers.
- `include/maze.h` controls the graph dimensions. The default 19 × 19
  logical grid renders as 61 × 61 physical tiles. A future Kruskal generator
  can produce the same `PassageGraph` while reusing the tile carving and
  reachability checks.
