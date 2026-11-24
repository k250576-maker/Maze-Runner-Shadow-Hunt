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

## Audio and exploration map

The game streams `dungeon002.ogg` by default. Use `--music dark-cavern` to
select the other ambience or `--music off` to mute gameplay music. Deep Dungeon
plays on the main menu. The old `--music deep-dungeon` option selects the
default gameplay ambience for compatibility.

Put optional local recordings in the following paths under the project root.
OGG, MP3, FLAC and WAV with the same basename are supported.

| Track | Local path | Use |
|---|---|---|
| Deep Dungeon | `assets/audio/music/deep_dungeon.mp3` | Main menu |
| Rich House | `assets/audio/music/rich_house.mp3` | Exploration |
| Elmshore | `assets/audio/music/elmshore.mp3` | Exploration |

Raylib uses the audio-only exports of the supplied WebM recordings. To convert
a locally available WebM to MP3, use
`ffmpeg -n -i "input.webm" -vn -c:a libmp3lame -q:a 2 "output.mp3"`.
The game alternates the available exploration tracks after 2–4 quiet minutes.
During danger the melody fades and pauses, then resumes after a short quiet
period. Menu playback falls back to bundled ambience when Deep Dungeon is
missing. See [audio credits](assets/audio/CREDITS.txt) and the in-game Credits
screen for attribution. Keep the optional recordings local.

```powershell
& ./build/game.exe --seed 42 --music dark-cavern
```

Footsteps prefer a 240 ms excerpt of the optional local recording and fall back
to the bundled stone step. The quiet mix (0.075) uses a 78-pixel stride,
a 28-pixel first step, a 520 ms minimum interval and short sample fades.
Damage has a separate synthesized impact at volume 0.32.

Press **M** to expand the parchment map. **M** or **Esc** closes it.
The **I** controls panel, expanded map, and Mobcyclopedia pause player
movement and enemy updates.
The compact map shows the nearby portion of the chart.

`include/exploration_map.h` stores a 2D array of unknown, passage, and
wall states for the current run. When the player changes cells, a bounded
BFS reuses the maze's custom queue. It follows four-directional walkable
neighbors up to `RevealRadius` steps away and records the walls bordering
those passages. Walls never enter the queue, so adjacent disconnected
corridors stay hidden. Each new run starts with an empty chart.
`CompactCells` and `CompactCellPixels` in `include/parchment_map.h` set
the minimap's coverage and size. `ExpandedMargin` controls the larger map's
padding, and the expanded tile size fits the available window.

## Enemies and the Mobcyclopedia

Open **Mobcyclopedia** from the main menu or press **B** while playing.
Press **B** or **Esc** to close the book. Non-release builds show live AI
diagnostics with **F3**. Each row reports the state, remaining waypoints,
nodes removed from the search queue in the latest run, total searches
and simulation updates. Builds with `NDEBUG` omit the overlay.

Turn the bestiary pages with Left/Right or A/D. Wide windows show a spread
per creature. Narrow windows show individual leaves. Body text uses a local
serif font when available.

- The Angel sees the player across clear floor within six tiles. It chases at
  45 px/s. Beam direction, range and wall occlusion determine whether it freezes.
  Its BFS uses the custom FIFO queue, visited bits and parent links to reconstruct
  a shortest legal path in O(V + E) time and O(V) space.
- The Ghost senses motion through walls and detects a nearby still player.
  It chases at 38 px/s. Dijkstra uses cost 1 for floor and 5 for walls, an
  indexed binary min-heap and parent links. Search takes O((V + E) log V) time
  and O(V) space. Active wards and the entrance buffer remove forbidden edges.
- Each supernatural enemy spawns at least 32 floor steps from the starting tile. After
  12 seconds dormant, it patrols within three tiles of home. Patrol,
  investigation and return use 55% of chase speed. Detection expires after
  three seconds of investigating the last known position. A ten-tile leash
  limits pursuit. Targets and newly activated torches invalidate cached routes.
- The Entrance Hall is permanently safe. Lit torches protect visible floor
  within two tiles against supernatural enemies. The Sentinel and Stalker
  can cross torchlight and still respect the Entrance Hall. Navigation caches four-pixel ward patches, allows
  clearance for the body, and checks actual movement segments. A torch that
  ignites around an enemy forces a harmless retreat via a local escape BFS.
- The first flashlight sighting discovers the creature for that run. A sight
  line is still required to draw it or deliver a hit. Hidden creatures give
  proximity warnings. A discovered Angel gives one second of warning before
  a lethal contact. The Ghost telegraphs for 0.46 seconds, removes one of five
  hearts and grants 1.5 seconds of immunity to nonlethal hits. Restart rebuilds
  the same seed and resets detection, health, exploration and torches.

- Press **Space** for a short melee swing in the facing direction. A swing
  can hit each physical enemy once and checks for walls. The cooldown is
  0.45 seconds. The Sentinel takes three hits, while the Stalker takes one.
- The Sentinel uses A* with unit floor costs, Manhattan distance, and the
  same indexed min-heap used by Dijkstra. The Stalker reuses the BFS queue
  and alternates fast chases with recovery or retreat. Both wind up before
  dealing one damage and check range, direction, and sight when they strike.
- A fixed pool starts with up to three Sentinels and four Stalkers behind a
  12-second grace period. Replacements are attempted one at a time every 45–75
  seconds and wait 2.5 seconds before pursuit. Up to two physical enemies
  pursue at once. BFS distances, viewport and flashlight exclusion, and spacing
  checks guide placement. BFS takes O(V + E) time and O(V) space. Scanning
  candidates and comparing up to the fixed cap adds O(V) work per replacement.
- The Sentinel detects within seven cells and the Stalker within six, subject
  to line of sight and the ten-cell home leash. Both remember the last seen
  position for up to three seconds. A* has O((V + E) log V) worst-case time
  with the indexed heap and O(V) space. The Stalker's BFS takes O(V + E) time
  and O(V) space.
- A Sentinel drops 15 souls and a Stalker drops five. A fixed tile-indexed grid
  combines drops on the same tile. Drops and nearby collection take O(1) time,
  and storage takes O(V) space. The HUD shows collected souls and active time.
  Game Over reports the time, score, and seed.
- Fourteen decorative bats rest, flutter between walkable tiles, and sometimes
  flee when approached. The game uses two animated poses drawn in code.
  An optional local two-frame `assets/characters/bat.png` overrides that art.
  The supplied demon, blood monster, and staff-wielding wizard sheets depict
  different creatures. The two physical enemies use code-drawn placeholders
  until matching art is available.

### Reproduce the navigation checks

From the project root with a working Raylib toolchain:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic tests/enemy_logic.cpp -o build/enemy-logic.exe -lraylib -lopengl32 -lgdi32 -lwinmm
& ./build/enemy-logic.exe
```

The test covers fixed seeds, pursuit, flashlight occlusion, wall collision,
weighted routes, torch intersection and retreat, damage, and grace.
