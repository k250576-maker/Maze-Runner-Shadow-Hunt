#ifndef SHADOW_HUNT_ENEMY_H
#define SHADOW_HUNT_ENEMY_H

#include "maze.h"
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Enemies
{
enum class Kind { Angel, Ghost, Sentinel, Stalker };
inline const char* Name(Kind kind)
{
    switch (kind)
    {
        case Kind::Angel: return "Weeping Angel";
        case Kind::Ghost: return "Phasing Ghost";
        case Kind::Sentinel: return "Fallen Sentinel";
        case Kind::Stalker: return "Hollow Stalker";
    }
    return "";
}

enum class State { Dormant, Patrolling, Investigating, Chasing, Returning, Frozen, Repelled, WindingUp, Recovering, Dying };

constexpr float DetectionRadius = 6 * Maze::CellSize;
constexpr float CloseSense = 2 * Maze::CellSize;
constexpr float LeashRadius = 10 * Maze::CellSize;
constexpr float PatrolRadius = 3 * Maze::CellSize;
constexpr float PatrolSpeedFactor = 0.55f;
constexpr float DisengageSeconds = 3.0f;

constexpr int CellCount = Maze::Columns * Maze::Rows;
constexpr int NoCell = -1;
constexpr float GraceSeconds = 12.0f;
constexpr float AngelSpeed = 45.0f;
constexpr float GhostSpeed = 38.0f;
constexpr float AngelRadius = 9.0f;
constexpr float GhostRadius = 8.0f;

inline int CellId(Maze::Cell cell) { return cell.row * Maze::Columns + cell.column; }
inline Maze::Cell CellAt(int id) { return {id % Maze::Columns, id / Maze::Columns}; }
inline int CellAt(Vector2 world)
{
    const int x = static_cast<int>(std::floor(world.x / Maze::CellSize));
    const int y = static_cast<int>(std::floor(world.y / Maze::CellSize));
    return Maze::Contains(x, y) ? CellId({x, y}) : NoCell;
}
inline Vector2 Center(int id)
{
    const Maze::Cell cell = CellAt(id);
    return {(cell.column + 0.5f) * Maze::CellSize, (cell.row + 0.5f) * Maze::CellSize};
}
inline float DistanceSquared(Vector2 a, Vector2 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}
inline Vector2 PointAlong(Vector2 from, Vector2 to, float fraction)
{
    return {from.x + (to.x - from.x) * fraction, from.y + (to.y - from.y) * fraction};
}
inline float ClosestFraction(Vector2 from, Vector2 to, Vector2 point)
{
    const float length = DistanceSquared(from, to);
    if (length == 0) return 0;
    const float dot = (point.x - from.x) * (to.x - from.x) + (point.y - from.y) * (to.y - from.y);
    return std::clamp(dot / length, 0.0f, 1.0f);
}

struct Route
{
    std::array<int, CellCount> cells{};
    int length = 0;
    int next = 0;
    int nodesExplored = 0;
    int searches = 0;
    int Remaining() const { return length - next; }
};

struct Enemy
{
    Kind kind;
    float speed;
    float radius;
    Vector2 position{};
    Vector2 target{};
    Vector2 facing{1, 0};
    float stride = 0;
    float traveled = 0;
    State state = State::Dormant;
    Route route{};
    Vector2 home{}, perch{}, patrolGoal{};
    float memory = 0;
    bool discovered = false;
    bool nearby = false;
    float windup = 0;
    float attackPulse = 0;
    int lastGoal = NoCell;
    unsigned updates = 0;
};
}
#endif
