#ifndef SHADOW_HUNT_MAZE_VISIBILITY_H
#define SHADOW_HUNT_MAZE_VISIBILITY_H

#include "maze.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Maze
{
inline bool IsWalkablePosition(const Layout& level, Vector2 position)
{
    if (!(position.x >= 0 && position.x < Columns * CellSize
          && position.y >= 0 && position.y < Rows * CellSize)) return false;
    return level.IsWalkable(static_cast<int>(position.x / CellSize),
                            static_cast<int>(position.y / CellSize));
}

// Step through each grid boundary touched by a unit ray. At a shared corner,
// both side cells must be open before the ray can continue diagonally.
inline float FirstWallDistance(const Layout& level, Vector2 source, Vector2 direction, float range)
{
    if (!IsWalkablePosition(level, source)) return 0.0f;
    int column = static_cast<int>(source.x / CellSize);
    int row = static_cast<int>(source.y / CellSize);
    const int stepX = direction.x >= 0 ? 1 : -1;
    const int stepY = direction.y >= 0 ? 1 : -1;
    constexpr float infinity = std::numeric_limits<float>::infinity();
    const float deltaX = std::fabs(direction.x) > 0.000001f ? CellSize / std::fabs(direction.x) : infinity;
    const float deltaY = std::fabs(direction.y) > 0.000001f ? CellSize / std::fabs(direction.y) : infinity;
    float nextX = std::isfinite(deltaX)
        ? ((column + (stepX > 0 ? 1 : 0)) * CellSize - source.x) / direction.x : infinity;
    float nextY = std::isfinite(deltaY)
        ? ((row + (stepY > 0 ? 1 : 0)) * CellSize - source.y) / direction.y : infinity;
    while (true)
    {
        const float distance = std::min(nextX, nextY);
        if (distance > range) return infinity;
        if (std::fabs(nextX - nextY) < 0.0001f)
        {
            if (!level.IsWalkable(column + stepX, row) || !level.IsWalkable(column, row + stepY))
                return distance;
            column += stepX;
            row += stepY;
            nextX += deltaX;
            nextY += deltaY;
        }
        else if (nextX < nextY)
        {
            column += stepX;
            nextX += deltaX;
        }
        else
        {
            row += stepY;
            nextY += deltaY;
        }
        if (!level.IsWalkable(column, row)) return distance;
    }
}

inline bool HasLineOfSight(const Layout& level, Vector2 source, Vector2 target)
{
    if (!IsWalkablePosition(level, source) || !IsWalkablePosition(level, target)) return false;
    const float dx = target.x - source.x;
    const float dy = target.y - source.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance == 0.0f) return true;
    return FirstWallDistance(level, source, {dx / distance, dy / distance}, distance) > distance;
}
}
#endif
