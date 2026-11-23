#ifndef SHADOW_HUNT_TORCH_STATE_H
#define SHADOW_HUNT_TORCH_STATE_H

#include "maze_visibility.h"

namespace Torches
{
constexpr float ActivationRadius = 2.5f * Maze::CellSize;
constexpr float ProtectionRadiusCells = 1.75f;

class State
{
public:
    explicit State(const Maze::Layout& level) : level(level) {}

    int Count() const { return level.torchCount; }
    bool IsLit(int index) const { return lit[index]; }

    Vector2 WorldPosition(int index) const
    {
        const Maze::Cell site = level.torchSites[index];
        // The torch sits just inside the floor tile below its mounting wall.
        return {(site.column + 0.5f) * Maze::CellSize, site.row * Maze::CellSize + 6.0f};
    }

    bool ActivateNearby(Vector2 player)
    {
        bool changed = false;
        for (int i = 0; i < Count(); ++i)
        {
            if (lit[i]) continue;
            const Vector2 position = WorldPosition(i);
            if (DistanceSquared(player, position) > ActivationRadius * ActivationRadius
                || !Maze::HasLineOfSight(level, player, position)) continue;
            lit[i] = true;
            changed = true;
        }
        return changed;
    }

    // Enemy rules can opt in to this query. Only directly reachable floor is protected.
    bool IsPositionProtectedByTorch(Vector2 position) const
    {
        constexpr float radius = ProtectionRadiusCells * Maze::CellSize;
        for (int i = 0; i < Count(); ++i)
        {
            if (!lit[i]) continue;
            const Vector2 torch = WorldPosition(i);
            if (DistanceSquared(position, torch) <= radius * radius
                && Maze::HasLineOfSight(level, torch, position)) return true;
        }
        return false;
    }

private:
    static float DistanceSquared(Vector2 a, Vector2 b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    const Maze::Layout& level;
    bool lit[Maze::MaxTorches]{};
};
}
#endif
