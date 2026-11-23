#ifndef SHADOW_HUNT_ROOMS_H
#define SHADOW_HUNT_ROOMS_H

namespace Rooms
{
enum class Id { Entrance, Library, Storage, Special, Count };
inline constexpr const char* Names[] = {"Entrance Hall", "Library", "Storage Room", "Sealed Chamber"};

// Inclusive physical tile bounds.
struct CellArea { int left; int top; int right; int bottom; };

struct Room
{
    CellArea bounds{};
    CellArea doorway{};
};
}
#endif
