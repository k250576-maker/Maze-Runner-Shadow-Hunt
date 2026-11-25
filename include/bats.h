#ifndef SHADOW_HUNT_BATS_H
#define SHADOW_HUNT_BATS_H

#include "enemy_pathfinding.h"
#include "player_movement.h"
#include <array>
#include <cstdint>
#include <string>

namespace Bats
{
constexpr int Capacity = 14;
constexpr float WingFramesPerSecond = 10.0f;
constexpr float WanderRadius = 4 * Maze::CellSize;
constexpr float StallSeconds = 0.75f;

struct Bat
{
    int home = Enemies::NoCell;
    int cell = Enemies::NoCell;
    int previous = Enemies::NoCell;
    int destination = Enemies::NoCell;
    Vector2 ground{}, target{}, progressAnchor{};
    float wing = 0;
    float cruise = 0, speed = 0, stalled = 0;
    std::uint32_t random = 0;
    bool left = false;

    Vector2 Ground() const { return ground; }
    Vector2 Position() const
    {
        return {ground.x + 1.4f * std::sin(wing * (2 * PI / 1.5f)),
                ground.y - 8 + 2.3f * std::sin(wing * (2 * PI / 1.25f))};
    }
};

class Flock
{
public:
    explicit Flock(const Maze::Layout& layout) : level(layout) {}
    const auto& Actors() const { return bats; }

    void Place(std::uint32_t seed)
    {
        std::uint32_t random = seed ^ 0xD6E8FEB8u;
        bats = {};
        const int start = static_cast<int>(Next(random) % Enemies::CellCount);
        int count = 0;
        for (int i = 0; i < Enemies::CellCount && count < Capacity; ++i)
        {
            const int id = (start + i * 43) % Enemies::CellCount;
            const auto tile = Enemies::CellAt(id);
            if (!level.IsWalkable(tile.column,tile.row) || Enemies::InEntrance(level,tile)) continue;
            const Vector2 p = Enemies::Center(id);
            bool near = false;
            constexpr float spacing = 7 * Maze::CellSize;
            for (int j = 0; j < count; ++j)
                near |= Enemies::DistanceSquared(p, Enemies::Center(bats[j].home)) < spacing * spacing;
            if (near) continue;
            Bat bat;
            bat.home = bat.cell = id;
            bat.ground = bat.progressAnchor = p;
            bat.random = Next(random);
            bat.wing = (Next(random) % 60) / 10.0f;
            bat.cruise = 28 + Next(random) % 14;
            Plan(bat);
            if (bat.destination != Enemies::NoCell) bats[count++] = bat;
        }
    }

    void Update(float dt)
    {
        if (dt <= 0) return;
        for (auto& bat : bats)
        {
            if (bat.cell == Enemies::NoCell) continue;
            bat.wing = std::fmod(bat.wing + dt, 60.0f);
            float remaining = dt;
            while (remaining > 0)
            {
                if (bat.destination == Enemies::NoCell || !Walkable(bat.destination))
                    Plan(bat, bat.destination);
                if (bat.destination == Enemies::NoCell) break;

                const float distance = std::sqrt(Enemies::DistanceSquared(bat.ground, bat.target));
                const float travelTime = distance / bat.speed;
                if (travelTime > remaining)
                {
                    bat.ground = Enemies::PointAlong(bat.ground, bat.target, remaining / travelTime);
                    break;
                }
                bat.ground = bat.target;
                remaining -= travelTime;
                if (bat.cell != bat.destination)
                {
                    bat.previous = bat.cell;
                    bat.cell = bat.destination;
                }
                Plan(bat);
            }
            if (Enemies::DistanceSquared(bat.progressAnchor, bat.ground) >= 4.0f)
            {
                bat.progressAnchor = bat.ground;
                bat.stalled = 0;
            }
            else if ((bat.stalled += dt) >= StallSeconds)
            {
                Plan(bat, bat.destination);
                bat.progressAnchor = bat.ground;
                bat.stalled = 0;
            }
        }
    }

private:
    static std::uint32_t Next(std::uint32_t& random) { return random = random * 1664525u + 1013904223u; }

    bool Walkable(int id) const
    {
        if (id == Enemies::NoCell) return false;
        const auto cell = Enemies::CellAt(id);
        return level.IsWalkable(cell.column, cell.row) && !Enemies::InEntrance(level, cell);
    }

    bool ClearLane(Vector2 from, Vector2 to) const
    {
        const bool horizontal = from.y == to.y;
        const float start = horizontal ? from.x : from.y;
        const float end = horizontal ? to.x : to.y;
        return Maze::HasLineOfSight(level, from, to)
            && Movement::SweepAxis(level, start, horizontal ? from.y : from.x, end - start, horizontal) == end;
    }

    void BeginLeg(Bat& bat, int cell, Vector2 target) const
    {
        bat.destination = cell;
        bat.target = target;
        bat.speed = bat.cruise * (0.90f + (Next(bat.random) >> 24) / 1275.0f);
        if (target.x != bat.ground.x) bat.left = target.x < bat.ground.x;
    }

    void Plan(Bat& bat, int rejected = Enemies::NoCell) const
    {
        bat.destination = Enemies::NoCell;
        if (!Walkable(bat.cell)) return;
        const auto center = Enemies::Center(bat.cell);
        if (Enemies::DistanceSquared(bat.ground, center) > 0.001f)
        {
            // Return along the same legal edge before choosing another turn.
            // This also lets a bat back out if the destination closed mid-leg.
            BeginLeg(bat, bat.cell, center);
            return;
        }

        int best = Enemies::NoCell, bestScore = -1;
        for (int d = 0; d < 4; ++d)
        {
            const auto adjacent = Maze::Neighbor(Enemies::CellAt(bat.cell), d);
            if (!Maze::Contains(adjacent.column, adjacent.row)) continue;
            const int next = Enemies::CellId(adjacent);
            const auto point = Enemies::Center(next);
            if (next == rejected || !Walkable(next)
                || Enemies::DistanceSquared(Enemies::Center(bat.home), point) > WanderRadius * WanderRadius
                || !ClearLane(center, point)) continue;
            const int score = static_cast<int>((Next(bat.random) >> 24) % 16) + (next == bat.previous ? 0 : 16);
            if (score > bestScore) { best = next; bestScore = score; }
        }
        if (best != Enemies::NoCell)
        {
            BeginLeg(bat, best, Enemies::Center(best));
            return;
        }

        // A temporarily isolated floor tile still leaves room to flutter.
        const unsigned offset = Next(bat.random) >> 30;
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto direction = Maze::Directions[(i + offset) % 4];
            const Vector2 target{center.x + direction.column * 6, center.y + direction.row * 6};
            if (!ClearLane(center, target)) continue;
            BeginLeg(bat, bat.cell, target);
            return;
        }
    }

    const Maze::Layout& level;
    std::array<Bat,Capacity> bats{};
};

class Art
{
public:
    void Load(const std::string& root)
    {
        const auto path = root + (FileExists((root+"characters/bat_clean.png").c_str())?"characters/bat_clean.png":"characters/bat.png");
        if (!FileExists(path.c_str())) return;
        texture = LoadTexture(path.c_str());
        if (!IsTextureValid(texture)) return;
        if (texture.width != 64 || texture.height != 32) { Unload(); return; }
        SetTextureFilter(texture,TEXTURE_FILTER_POINT);
    }

    void Unload()
    {
        if (IsTextureValid(texture)) UnloadTexture(texture);
        texture = {};
    }

    void Draw(const Flock& flock, const Maze::Layout& level, Vector2 player, Rectangle visible) const
    {
        for (const auto& bat : flock.Actors())
        {
            if (bat.cell == Enemies::NoCell) continue;
            const Vector2 ground = bat.Ground();
            const Vector2 air = bat.Position();
            const Vector2 p = Maze::HasLineOfSight(level, ground, air) ? air : ground;
            // Use the floor anchor for occlusion. The flight is purely decorative.
            if (!CheckCollisionPointRec(p,visible) || !Maze::HasLineOfSight(level,player,ground)) continue;
            const int frame = static_cast<int>(bat.wing * WingFramesPerSecond) % 2;
            if (!IsTextureValid(texture))
            {
                DrawFallback(p, frame);
                continue;
            }
            const Rectangle source{32.0f*frame,0,bat.left ? -32.0f : 32.0f,32};
            DrawTexturePro(texture,source,{p.x,p.y,32,32},{16,16},0,WHITE);
        }
    }

private:
    static void DrawFallback(Vector2 center, int frame)
    {
        const Color wings{116, 84, 148, 255};
        const float tip = center.y + (frame == 0 ? -5.0f : 3.0f);
        const Vector2 left{center.x - 11, tip}, right{center.x + 11, tip};
        const Vector2 tail{center.x, center.y + 4};
        DrawTriangle(left, tail, {center.x - 2, center.y - 2}, wings);
        DrawTriangle(right, {center.x + 2, center.y - 2}, tail, wings);
        DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), 3, 4, {66, 44, 92, 255});
        DrawRectangle(static_cast<int>(center.x) - 1, static_cast<int>(center.y) - 1, 2, 1, {202, 158, 223, 255});
    }

    Texture2D texture{};
};
}
#endif
