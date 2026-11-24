#ifndef SHADOW_HUNT_MAZE_H
#define SHADOW_HUNT_MAZE_H

#include "rooms.h"

namespace Maze
{
constexpr int LogicalColumns = 19;
constexpr int LogicalRows = 19;
constexpr int WideBands = 3; // Three bands per axis get three tiles instead of two.
constexpr int Columns = 3 * LogicalColumns + 1 + WideBands;
constexpr int Rows = 3 * LogicalRows + 1 + WideBands;
constexpr int CellSize = 32;
constexpr int MaxTorches = 24;

struct Cell { int column; int row; };

constexpr bool Contains(int column, int row)
{
    return column >= 0 && column < Columns && row >= 0 && row < Rows;
}

struct Layout
{
    bool floor[Rows][Columns]{};
    bool locked[Rows][Columns]{}; // Runtime side-room gates; base topology stays intact.
    Rooms::Room rooms[static_cast<int>(Rooms::Id::Count)]{};
    Cell spawn{};
    Cell futureExit{};
    Cell torchSites[MaxTorches]{};
    int torchCount = 0;

    constexpr bool IsWalkable(int column, int row) const
    {
        return Contains(column, row) && floor[row][column] && !locked[row][column];
    }

    const char* LocationName(Cell cell) const
    {
        for (int i = 0; i < static_cast<int>(Rooms::Id::Count); ++i)
        {
            const auto room = rooms[i].bounds;
            if (cell.column >= room.left && cell.column <= room.right
                && cell.row >= room.top && cell.row <= room.bottom) return Rooms::Names[i];
        }
        return "The Forsaken Keep";
    }
};
}
#endif
