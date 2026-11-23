#ifndef SHADOW_HUNT_EXPLORATION_MAP_H
#define SHADOW_HUNT_EXPLORATION_MAP_H

#include "maze_generation.h"

namespace Exploration
{
constexpr int RevealRadius = 3;
enum class Tile { Unknown, Passage, Wall };

class Map
{
public:
    explicit Map(const Maze::Layout& level) : level(level) {}

    Tile At(int column, int row) const
    {
        return Maze::Contains(column, row) ? discovered[row][column] : Tile::Unknown;
    }

    void Visit(Maze::Cell cell)
    {
        if ((cell.column == last.column && cell.row == last.row) || !level.IsWalkable(cell.column, cell.row))
            return;
        last = cell;

        struct VisitStep { Maze::Cell cell; int distance; };
        // The Manhattan diamond holds at most 1 + 2r(r + 1) reachable candidates.
        constexpr int Capacity = 1 + 2 * RevealRadius * (RevealRadius + 1);
        constexpr int Span = 2 * RevealRadius + 1;
        bool queued[Span][Span]{};
        Maze::Queue<VisitStep, Capacity> frontier;
        frontier.Push({cell, 0});
        queued[RevealRadius][RevealRadius] = true;

        while (!frontier.Empty())
        {
            const VisitStep step = frontier.Pop();
            discovered[step.cell.row][step.cell.column] = Tile::Passage;
            for (int direction = 0; direction < 4; ++direction)
            {
                const Maze::Cell next = Maze::Neighbor(step.cell, direction);
                if (!Maze::Contains(next.column, next.row)) continue;
                // Record the face of a bordering wall. Only reachable floor enters the queue.
                if (!level.IsWalkable(next.column, next.row))
                {
                    discovered[next.row][next.column] = Tile::Wall;
                    continue;
                }
                if (step.distance == RevealRadius) continue;
                bool& visited = queued[next.row - cell.row + RevealRadius][next.column - cell.column + RevealRadius];
                if (visited) continue;
                visited = true;
                frontier.Push({next, step.distance + 1});
            }
        }
    }

private:
    const Maze::Layout& level;
    Tile discovered[Maze::Rows][Maze::Columns]{};
    Maze::Cell last{-1, -1};
};
}
#endif
