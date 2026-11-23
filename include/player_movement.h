#ifndef SHADOW_HUNT_PLAYER_MOVEMENT_H
#define SHADOW_HUNT_PLAYER_MOVEMENT_H

#include "maze.h"
#include "furniture.h"
#include <algorithm>
#include <cmath>

namespace Movement
{
// World pixels per second. 128 keeps the previous travel speed.
constexpr float PixelsPerSecond = 128.0f;
constexpr float CollisionHalfSize = 9.0f;
constexpr float AnimationFramesPerSecond = 8.0f;
enum class Direction { None, Left, Right, Up, Down };

using Cell = Maze::Cell;
struct Position { float x; float y; };

inline Position Center(Cell cell)
{
    return {(cell.column + 0.5f) * Maze::CellSize,
            (cell.row + 0.5f) * Maze::CellSize};
}

inline int CellIndex(float coordinate)
{
    return static_cast<int>(std::floor(coordinate / Maze::CellSize));
}

// The cell containing the player's ground position, independent of the collider's footprint.
inline Cell CellAt(Position position)
{
    return {CellIndex(position.x), CellIndex(position.y)};
}

struct Player
{
    Position position{};
    Direction facing = Direction::Down;
    float animationTime = 0;
    int animationFrame = 1; // Middle column is the idle pose.
};

inline int LastOverlappedCell(float edge)
{
    // An edge exactly on a grid line touches, but does not overlap, the next cell.
    return static_cast<int>(std::ceil(edge / Maze::CellSize)) - 1;
}

inline float SweepWalls(const Maze::Layout& level, float center, float across, float displacement, bool horizontal)
{
    const int axisCells = horizontal ? Maze::Columns : Maze::Rows;
    const float destination = std::clamp(center + displacement, 0.0f,
                                         static_cast<float>(axisCells * Maze::CellSize));
    if (destination == center) return center;

    const int step = destination > center ? 1 : -1;
    const float leadingOffset = step * CollisionHalfSize;
    const float startEdge = center + leadingOffset;
    const float endEdge = destination + leadingOffset;
    const int first = step > 0 ? CellIndex(startEdge) : LastOverlappedCell(startEdge);
    const int last = step > 0 ? LastOverlappedCell(endEdge) : CellIndex(endEdge);
    const int acrossFirst = CellIndex(across - CollisionHalfSize);
    const int acrossLast = LastOverlappedCell(across + CollisionHalfSize);

    // Check every crossed strip across the full collider to catch walls even during a long frame.
    for (int tile = first; step > 0 ? tile <= last : tile >= last; tile += step)
    {
        for (int other = acrossFirst; other <= acrossLast; ++other)
        {
            if (level.IsWalkable(horizontal ? tile : other, horizontal ? other : tile)) continue;
            const int wallFace = step > 0 ? tile : tile + 1;
            return wallFace * Maze::CellSize - leadingOffset;
        }
    }
    return destination;
}

inline float SweepAxis(const Maze::Layout& level, float center, float across, float displacement, bool horizontal)
{
    float destination = SweepWalls(level, center, across, displacement, horizontal);
    if (destination == center) return center;

    for (const Furniture::Placement prop : Furniture::Placements)
    {
        if (!Furniture::IsSolid(prop)) continue;
        const Furniture::Footprint base = Furniture::Base(prop);
        const float acrossMin = horizontal ? base.top : base.left;
        const float acrossMax = horizontal ? base.bottom : base.right;
        if (across + CollisionHalfSize <= acrossMin || across - CollisionHalfSize >= acrossMax) continue;

        // Sweep the center against the footprint expanded by the player's hitbox.
        if (displacement > 0)
        {
            const float contact = (horizontal ? base.left : base.top) - CollisionHalfSize;
            if (center <= contact) destination = std::min(destination, contact);
        }
        else
        {
            const float contact = (horizontal ? base.right : base.bottom) + CollisionHalfSize;
            if (center >= contact) destination = std::max(destination, contact);
        }
    }
    return destination;
}

inline void Update(Player& player, const Maze::Layout& level, float dt, Direction direction)
{
    const Position start = player.position;
    if (direction != Direction::None) player.facing = direction;
    const float distance = PixelsPerSecond * std::max(dt, 0.0f);
    switch (direction)
    {
        case Direction::Left: player.position.x = SweepAxis(level, start.x, start.y, -distance, true); break;
        case Direction::Right: player.position.x = SweepAxis(level, start.x, start.y, distance, true); break;
        case Direction::Up: player.position.y = SweepAxis(level, start.y, start.x, -distance, false); break;
        case Direction::Down: player.position.y = SweepAxis(level, start.y, start.x, distance, false); break;
        case Direction::None: break;
    }

    const float traveled = std::abs(player.position.x - start.x) + std::abs(player.position.y - start.y);
    if (traveled == 0)
    {
        player.animationTime = 0;
        player.animationFrame = 1;
        return;
    }

    constexpr int WalkCycle[] = {0, 1, 2, 1};
    constexpr float cycleDuration = 4.0f / AnimationFramesPerSecond;
    player.animationTime = std::fmod(player.animationTime + traveled / PixelsPerSecond, cycleDuration);
    player.animationFrame = WalkCycle[static_cast<int>(player.animationTime * AnimationFramesPerSecond) % 4];
}
}
#endif
