#ifndef SHADOW_HUNT_RUN_PROGRESS_H
#define SHADOW_HUNT_RUN_PROGRESS_H

#include "enemy.h"
#include "maze_visibility.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace Run
{
struct Clock
{
    std::uint32_t seed = 0;
    double elapsed = 0;

    void Update(float dt, bool paused)
    {
        if (!paused && dt > 0) elapsed += dt;
    }

    void Format(char (&out)[24]) const
    {
        const auto seconds = static_cast<unsigned long long>(elapsed);
        std::snprintf(out, sizeof(out), "%02llu:%02llu", seconds / 60, seconds % 60);
    }
};

using Score = unsigned long long;
constexpr float PickupRadius = 19.0f;
constexpr double PickupDelay = 0.25;

class Souls
{
public:
    void Drop(Enemies::Kind kind, Vector2 position, double elapsed)
    {
        using Enemies::Kind;
        const int cell = Enemies::CellAt(position);
        if (cell == Enemies::NoCell) return;
        if (kind != Kind::Sentinel && kind != Kind::Stalker) return;
        auto& pickup = pickups[cell];
        if (!pickup.value) pickup.readyAt = elapsed + PickupDelay;
        pickup.value += kind == Kind::Sentinel ? 15 : 5;
    }

    Score At(int cell) const { return pickups[cell].value; }
    Score Total() const { return total; }

    Score Collect(const Maze::Layout& level, Vector2 player, double elapsed)
    {
        const int id = Enemies::CellAt(player);
        if (id == Enemies::NoCell) return 0;
        const Maze::Cell origin = Enemies::CellAt(id);
        Score collected = 0;
        // Pickup reach is shorter than a tile. Only the nine nearby cells can contribute.
        for (int row = origin.row - 1; row <= origin.row + 1; ++row)
            for (int column = origin.column - 1; column <= origin.column + 1; ++column)
            {
                if (!Maze::Contains(column, row)) continue;
                const int cell = Enemies::CellId({column, row});
                auto& pickup = pickups[cell];
                if (!pickup.value || elapsed < pickup.readyAt) continue;
                const Vector2 position = Enemies::Center(cell);
                if (Enemies::DistanceSquared(position, player) > PickupRadius * PickupRadius
                    || !Maze::HasLineOfSight(level, player, position)) continue;
                collected += pickup.value;
                pickup = {};
            }
        total += collected;
        return collected;
    }

private:
    struct Pickup { Score value = 0; double readyAt = 0; };
    std::array<Pickup, Enemies::CellCount> pickups{};
    Score total = 0;
};
}
#endif
