#ifndef SHADOW_HUNT_BATS_H
#define SHADOW_HUNT_BATS_H

#include "enemy_pathfinding.h"
#include <array>
#include <cstdint>
#include <string>

namespace Bats
{
constexpr int Capacity = 14;
constexpr float WakeRange = 10 * Maze::CellSize;
constexpr float FlightSeconds = 0.76f;
constexpr float WingFramesPerSecond = 10.0f;

struct Bat
{
    int perch = Enemies::NoCell;
    int destination = Enemies::NoCell;
    float nap = 0;
    float flight = 0;
    float wing = 0;
    bool left = false;

    Vector2 Ground() const
    {
        const Vector2 start = Enemies::Center(perch);
        if (destination == Enemies::NoCell) return start;
        const Vector2 end = Enemies::Center(destination);
        float progress = std::clamp(flight / FlightSeconds, 0.0f, 1.0f);
        progress=progress*progress*(3-2*progress);
        return Enemies::PointAlong(start,end,progress);
    }
    Vector2 Position() const
    {
        Vector2 center=Ground();
        const float progress=destination==Enemies::NoCell?0:std::clamp(flight/FlightSeconds,0.0f,1.0f);
        const float arc=std::sin(progress * 3.1415927f);
        center.y -= 9 + 5 * arc * arc; // Ease vertical takeoff/landing too.
        return center;
    }
};

class Flock
{
public:
    explicit Flock(const Maze::Layout& layout) : level(layout) {}
    const auto& Actors() const { return bats; }

    void Place(std::uint32_t seed)
    {
        random = seed ^ 0xD6E8FEB8u;
        bats = {};
        const int start = static_cast<int>(Next() % Enemies::CellCount);
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
                near |= Enemies::DistanceSquared(p,Enemies::Center(bats[j].perch)) < spacing * spacing;
            if (near) continue;
            bats[count++] = {id, Enemies::NoCell, 0.5f + (Next() % 30) / 10.0f,0,(Next()%60)/10.0f};
        }
    }

    void Update(float dt, Vector2 player)
    {
        if(dt<=0) return;
        for (auto& bat : bats)
        {
            if (bat.perch == Enemies::NoCell) continue;
            // Animation is independent of lighting and wake distance. Finish
            // flights even if the player walks away; never freeze in mid-air.
            bat.wing=std::fmod(bat.wing+dt,60.0f);
            float remaining=dt;
            while(remaining>0)
            {
            const Vector2 base = Enemies::Center(bat.perch);
            const float distance = Enemies::DistanceSquared(base,player);
            if (bat.destination != Enemies::NoCell)
            {
                const float elapsed=std::min(remaining,FlightSeconds-bat.flight);
                bat.flight += elapsed;
                remaining-=elapsed;
                if (bat.flight >= FlightSeconds)
                {
                    bat.perch = bat.destination;
                    bat.destination = Enemies::NoCell;
                    bat.flight=0;
                    bat.nap = 1.5f + (Next() % 26) / 10.0f;
                }
                continue;
            }
            if(distance>WakeRange*WakeRange) break; // Far-away perched bats rest intentionally.
            const bool startled = distance < 54 * 54 && bat.nap < 2.0f
                && Maze::HasLineOfSight(level,player,base);
            if(!startled)
            {
                const float elapsed=std::min(remaining,bat.nap);
                bat.nap-=elapsed; remaining-=elapsed;
                if(bat.nap>0) break;
            }
            bat.nap = 2.5f;
            const unsigned offset = Next() % 4;
            float farthest = startled ? distance : -1.0f;
            for (unsigned i = 0; i < 4; ++i)
            {
                const auto cell = Maze::Neighbor(Enemies::CellAt(bat.perch), (i+offset)%4);
                if (!level.IsWalkable(cell.column,cell.row) || Enemies::InEntrance(level,cell)) continue;
                const int id = Enemies::CellId(cell);
                const float nextDistance = Enemies::DistanceSquared(Enemies::Center(id),player);
                if (nextDistance <= farthest) continue;
                bat.destination = id;
                farthest = nextDistance;
                if (!startled) break;
            }
            if (bat.destination == Enemies::NoCell) break;
            bat.left = Enemies::Center(bat.destination).x < base.x;
            bat.flight = 0;
            }
        }
    }

private:
    std::uint32_t Next() { return random = random * 1664525u + 1013904223u; }
    const Maze::Layout& level;
    std::array<Bat,Capacity> bats{};
    std::uint32_t random = 0;
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
            if (bat.perch == Enemies::NoCell) continue;
            const Vector2 ground = bat.Ground();
            const Vector2 p = bat.Position();
            // Use the floor anchor for occlusion. The flight is purely decorative.
            if (!CheckCollisionPointRec(p,visible) || !Maze::HasLineOfSight(level,player,ground)) continue;
            const int frame = bat.destination == Enemies::NoCell ? 1 : static_cast<int>(bat.wing * WingFramesPerSecond) % 2;
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
