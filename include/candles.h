#ifndef SHADOW_HUNT_CANDLES_H
#define SHADOW_HUNT_CANDLES_H

#include "maze.h"
#include <cmath>

namespace Candles
{
constexpr int StartingInventory = 5;
constexpr int FrameCount = 4;
constexpr float AnimationFramesPerSecond = 6.0f;
constexpr float GlowRadiusCells = 1.75f;

struct Candle
{
    int column = 0;
    int row = 0;
    float animationTime = 0;
    int Frame() const
    {
        return static_cast<int>(animationTime * AnimationFramesPerSecond) % FrameCount;
    }
};

enum class PlacementResult { Placed, Duplicate, Empty, InvalidCell };

// Small fixed storage for this prototype; independent of input, drawing, and
// lighting. This can later be replaced with the project's custom DSA storage.
struct State
{
    Candle placed[StartingInventory]{};
    int count = 0;

    int Remaining() const { return StartingInventory - count; }

    PlacementResult TryPlace(int column, int row)
    {
        if (!Maze::IsWalkable(column, row)) return PlacementResult::InvalidCell;
        if (Remaining() == 0) return PlacementResult::Empty;
        for (int i = 0; i < count; ++i)
            if (placed[i].column == column && placed[i].row == row)
                return PlacementResult::Duplicate;
        placed[count++] = {column, row, 0};
        return PlacementResult::Placed;
    }

    void Update(float dt)
    {
        if (dt <= 0) return;
        for (int i = 0; i < count; ++i)
            placed[i].animationTime = std::fmod(placed[i].animationTime + dt,
                                               FrameCount / AnimationFramesPerSecond);
    }
};
}
#endif
