#ifndef SHADOW_HUNT_TORCHES_H
#define SHADOW_HUNT_TORCHES_H

#include "lighting.h"
#include "torch_state.h"
#include <cmath>
#include <string>

namespace Torches
{
constexpr float GlowRadiusCells = 4.25f;
constexpr float GlowBrightness = 1.0f;
constexpr Color GlowTint{255, 129, 52, 255};
constexpr float GlowColorStrength = 0.31f;
constexpr float GlowBounceStrength = 0.075f;
constexpr int FrameCount = 4;
constexpr float FramesPerSecond = 8.0f;

class Fixtures
{
public:
    explicit Fixtures(const State& state) : state(state) {}

    bool Load(const std::string& assetRoot)
    {
        for (int i = 0; i < FrameCount; ++i)
        {
            const std::string path = assetRoot + "enviroment/torch_" + std::to_string(i + 1) + ".png";
            frames[i] = LoadTexture(path.c_str());
            if (!IsTextureValid(frames[i]))
            {
                Unload();
                return false;
            }
            SetTextureFilter(frames[i], TEXTURE_FILTER_POINT);
        }
        return true;
    }

    void Unload()
    {
        for (Texture2D& frame : frames)
        {
            if (frame.id != 0) UnloadTexture(frame);
            frame = {};
        }
    }

    void UpdateAnimation(float dt)
    {
        animationTime = std::fmod(animationTime + dt, FrameCount / FramesPerSecond);
    }

    int AppendLights(Lighting::PointLight* lights) const
    {
        int count = 0;
        for (int i = 0; i < state.Count(); ++i)
        {
            if (state.IsLit(i))
                lights[count++] = {state.WorldPosition(i), GlowRadiusCells * Maze::CellSize, GlowTint,
                                   GlowBrightness, GlowColorStrength, GlowBounceStrength};
        }
        return count;
    }

    void DrawBrackets() const
    {
        for (int i = 0; i < state.Count(); ++i)
        {
            const Vector2 position = state.WorldPosition(i);
            const int x = static_cast<int>(position.x);
            const int y = static_cast<int>(position.y);
            DrawRectangle(x - 5, y - 5, 10, 10, Color{91, 75, 67, 255});
            DrawRectangle(x - 3, y, 6, 10, Color{123, 91, 67, 255});
        }
    }

    void DrawFlames() const
    {
        const int frame = static_cast<int>(animationTime * FramesPerSecond);
        for (int i = 0; i < state.Count(); ++i)
        {
            if (!state.IsLit(i)) continue;
            const Vector2 position = state.WorldPosition(i);
            const Texture2D texture = frames[(frame + i) % FrameCount];
            DrawTexture(texture, static_cast<int>(position.x) - texture.width / 2,
                        static_cast<int>(position.y) + 8 - texture.height, WHITE);
        }
    }

private:
    const State& state;
    Texture2D frames[FrameCount]{};
    float animationTime = 0;
};
}
#endif
