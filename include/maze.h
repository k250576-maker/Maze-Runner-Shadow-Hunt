#ifndef SHADOW_HUNT_MAZE_H
#define SHADOW_HUNT_MAZE_H

namespace Maze
{
constexpr int Columns = 21;
constexpr int Rows = 15;
constexpr int CellSize = 32;
constexpr int SpawnColumn = 1;
constexpr int SpawnRow = 1;
// '#' is solid; '.' is walkable. Layout has no Raylib dependency.
inline constexpr char Cells[Rows][Columns + 1] = {
    "#####################",
    "#.......#...........#",
    "#.###.#.#.#####.###.#",
    "#...#.#...#.....#...#",
    "###.#.#####.###.#.#.#",
    "#...#.......#...#.#.#",
    "#.#######.#.#.###.#.#",
    "#.........#.#.....#.#",
    "#.###.#####.#####.#.#",
    "#...#.....#.......#.#",
    "###.#####.#.#######.#",
    "#...#.....#.........#",
    "#.###.###########.#.#",
    "#.................#.#",
    "#####################"
};
constexpr bool IsWalkable(int column, int row)
{
    return column >= 0 && column < Columns && row >= 0 && row < Rows
        && Cells[row][column] == '.';
}
static_assert(IsWalkable(SpawnColumn, SpawnRow), "Spawn must be walkable.");
}
#endif
