#ifndef SHADOW_HUNT_CANDLE_RENDERER_H
#define SHADOW_HUNT_CANDLE_RENDERER_H

#include "raylib.h"
#include "candles.h"
#include <string>

namespace Candles
{
constexpr int SpriteScale = 2;

class Renderer
{
public:
    bool Load(const std::string& assetRoot)
    {
        for (int i = 0; i < FrameCount; ++i)
        {
            const std::string path = assetRoot + "enviroment/candleA_0" + std::to_string(i + 1) + ".png";
            frames[i] = LoadTexture(path.c_str());
            if (frames[i].id == 0)
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

    void Draw(const State& candles, Vector2 mazeScreenOrigin) const
    {
        for (int i = 0; i < candles.count; ++i)
        {
            const Candle& candle = candles.placed[i];
            const Texture2D texture = frames[candle.Frame()];
            const float width = static_cast<float>(texture.width * SpriteScale);
            const float height = static_cast<float>(texture.height * SpriteScale);
            const float centerX = (candle.column + 0.5f) * Maze::CellSize;
            const float bottomY = (candle.row + 1) * Maze::CellSize - 2.0f;
            // The frames differ in height: anchor their bases, not their tops.
            DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                           {mazeScreenOrigin.x + centerX - width * 0.5f,
                            mazeScreenOrigin.y + bottomY - height, width, height},
                           {0, 0}, 0, WHITE);
        }
    }

private:
    Texture2D frames[FrameCount]{};
};
}
#endif
