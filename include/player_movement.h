#ifndef SHADOW_HUNT_PLAYER_MOVEMENT_H
#define SHADOW_HUNT_PLAYER_MOVEMENT_H

#include "maze.h"
#include <algorithm>
#include <cmath>

namespace Movement
{
// Adjust this to change travel speed; 4 means 0.25 seconds per cell.
constexpr float CellsPerSecond = 4.0f;
constexpr float AnimationFramesPerSecond = 8.0f;
enum class Direction { None, Left, Right, Up, Down };

struct Cell { int column; int row; };
struct Position { float x; float y; };

inline Position Center(Cell cell)
{
    return {(cell.column + 0.5f) * Maze::CellSize,
            (cell.row + 0.5f) * Maze::CellSize};
}

struct Player
{
    Cell current{Maze::SpawnColumn, Maze::SpawnRow};
    Cell target = current;
    Position visual = Center(current);
    float progress = 0;
    bool moving = false;
    Direction facing = Direction::Down;
    Direction queued = Direction::None;
    float animationTime = 0;
    int animationFrame = 1; // Middle column is the idle pose.
};

inline Cell Adjacent(Cell cell, Direction direction)
{
    switch (direction)
    {
        case Direction::Left: --cell.column; break;
        case Direction::Right: ++cell.column; break;
        case Direction::Up: --cell.row; break;
        case Direction::Down: ++cell.row; break;
        case Direction::None: break;
    }
    return cell;
}

inline bool CanMove(Cell cell, Direction direction)
{
    const Cell next = Adjacent(cell, direction);
    return direction != Direction::None && Maze::IsWalkable(next.column, next.row);
}

inline bool StartStep(Player& player, Direction direction)
{
    if (direction == Direction::None) return false;
    player.facing = direction;
    if (!CanMove(player.current, direction)) return false;
    player.target = Adjacent(player.current, direction);
    player.progress = 0;
    player.moving = true;
    return true;
}

// Input is passed in separately so transitions can be checked without a window.
// A tapped turn is buffered for the next cell; a held key repeats movement.
inline void Update(Player& player, float dt, Direction held, Direction pressed)
{
    if (player.moving && pressed != Direction::None && pressed != player.facing)
        player.queued = pressed;

    float remaining = std::max(dt, 0.0f);
    while (remaining > 0)
    {
        if (!player.moving)
        {
            const Direction queued = player.queued;
            player.queued = Direction::None;
            // A blocked buffered turn is discarded; a held direction can continue.
            if (!StartStep(player, queued) && !StartStep(player, held)) break;
        }
        const float needed = (1.0f - player.progress) / CellsPerSecond;
        const float elapsed = std::min(remaining, needed);
        player.progress += elapsed * CellsPerSecond;
        player.animationTime = std::fmod(player.animationTime + elapsed, 0.5f);
        remaining -= elapsed;
        const Position start = Center(player.current);
        const Position end = Center(player.target);
        player.visual = {start.x + (end.x - start.x) * player.progress,
                         start.y + (end.y - start.y) * player.progress};
        if (elapsed >= needed)
        {
            player.current = player.target;
            player.visual = end; // Exact center: avoid accumulating position drift.
            player.progress = 0;
            player.moving = false;
        }
    }

    const bool continuing = CanMove(player.current, player.queued)
                         || CanMove(player.current, held);
    if (!player.moving && !continuing)
    {
        player.animationTime = 0;
        player.animationFrame = 1;
    }
    else
    {
        // Left stride, neutral, right stride, neutral, using inspected columns.
        constexpr int WalkCycle[] = {0, 1, 2, 1};
        player.animationFrame = WalkCycle[static_cast<int>(player.animationTime
                                      * AnimationFramesPerSecond) % 4];
    }
}
}
#endif
