#ifndef SHADOW_HUNT_FURNITURE_H
#define SHADOW_HUNT_FURNITURE_H

#include "maze.h"
#include <array>

namespace Furniture
{
enum class Kind { EndTable, Shield, Bookshelf, LowBookshelf, WritingDesk, Crate, CrateStack, Barrel,
                  SmallBarrel, Basket, Web, Statue, Altar, Count };

struct Asset
{
    const char* filename;
    int width;
    int height;
    int baseWidth;
    int baseDepth;
};

inline constexpr Asset Assets[] = {
    {"Table - End 3 - Dark.png",                 37, 32, 29, 22},
    {"Weapon - Shield 1.png",                    29, 22,  0,  0},
    {"Storage - Bookshelf 1.a - Dark.png",        76, 38, 72, 17},
    {"Storage - Bookshelf 2 - Dark.png",          74, 32, 70, 17},
    {"Table - Writing desk - Dark.png",          72, 40, 64, 23},
    {"Storage - Crate 1 - Dark.png",             34, 36, 26, 23},
    {"Storage - Crates 2 - Dark.png",            61, 43, 53, 29},
    {"Storage - Barrel 1 - Dark.png",            33, 32, 25, 23},
    {"Storage - Barrel 3 - Dark.png",            31, 30, 23, 21},
    {"Storage - Wicker basket lid.png",           36, 37, 27, 24},
    {"Clutter - Webs 12 - Thin.png",              59, 48,  0,  0},
    {"Feature - Statue 2 - Stone.png",           36, 58, 25, 21},
    {"Feature - Altar - Gold.png",               42, 44, 35, 29}
};
static_assert(sizeof(Assets) / sizeof(Assets[0]) == static_cast<int>(Kind::Count));

struct Placement
{
    Rooms::Id room;
    Kind kind;
    // Ground anchor in world pixels. The bottom of a solid sprite overhangs it by four pixels.
    int x;
    int y;
};

// Reconnect the furniture renderer when a new layout is added.
inline constexpr std::array<Placement, 0> Placements{};
constexpr int PlacementCount = static_cast<int>(Placements.size());

struct Footprint { float left; float top; float right; float bottom; };

constexpr const Asset& Sprite(Kind kind) { return Assets[static_cast<int>(kind)]; }
constexpr bool IsSolid(Placement prop) { return Sprite(prop.kind).baseWidth > 0; }

constexpr Footprint Base(Placement prop)
{
    const Asset& sprite = Sprite(prop.kind);
    return {prop.x - sprite.baseWidth * 0.5f, prop.y - sprite.baseDepth * 0.5f,
            prop.x + sprite.baseWidth * 0.5f, prop.y + sprite.baseDepth * 0.5f};
}

constexpr bool Overlap(Footprint a, Footprint b)
{
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

constexpr Footprint WorldArea(Rooms::CellArea cells)
{
    const float unit = Maze::CellSize;
    return {cells.left * unit, cells.top * unit, (cells.right + 1) * unit, (cells.bottom + 1) * unit};
}

constexpr bool ValidLayout(const Maze::Layout& level)
{
    for (int i = 0; i < PlacementCount; ++i)
    {
        const Placement prop = Placements[i];
        if (!IsSolid(prop)) continue;
        const Footprint base = Base(prop);
        const Rooms::Room& room = level.rooms[static_cast<int>(prop.room)];
        const Footprint bounds = WorldArea(room.bounds);
        if (base.left < bounds.left || base.right > bounds.right
            || base.top < bounds.top || base.bottom > bounds.bottom) return false;
        for (int row = room.bounds.top; row <= room.bounds.bottom; ++row)
            for (int column = room.bounds.left; column <= room.bounds.right; ++column)
                if (!level.IsWalkable(column, row)
                    && Overlap(base, WorldArea({column, row, column, row}))) return false;
        if (Overlap(base, WorldArea(room.doorway))) return false;
        for (int j = 0; j < i; ++j)
            if (IsSolid(Placements[j]) && Overlap(base, Base(Placements[j]))) return false;
    }
    return true;
}

}
#endif
