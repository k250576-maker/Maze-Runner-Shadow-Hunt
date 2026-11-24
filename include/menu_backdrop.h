#ifndef SHADOW_HUNT_MENU_BACKDROP_H
#define SHADOW_HUNT_MENU_BACKDROP_H

#include "dungeon_art.h"
#include "raylib.h"
#include <array>
#include <cmath>
#include <string>

namespace MenuBackdrop
{
constexpr float StackedFloor = 0.30f;
constexpr float StackedZoom = 1.84f;

class Castle
{
public:
    void LoadTorches(const std::string& assetRoot)
    {
        for (std::size_t i = 0; i < flames.size(); ++i)
        {
            const std::string path = assetRoot + "enviroment/torch_" + std::to_string(i + 1) + ".png";
            if (!FileExists(path.c_str())) continue;
            flames[i] = LoadTexture(path.c_str());
            if (IsTextureValid(flames[i])) SetTextureFilter(flames[i], TEXTURE_FILTER_POINT);
        }
    }

    void UnloadTorches()
    {
        for (Texture2D& flame : flames)
        {
            if (IsTextureValid(flame)) UnloadTexture(flame);
            flame = {};
        }
    }

    void Draw(Texture2D stone, Texture2D player, int width, int height,
              float zoom, bool stacked, float time) const
    {
        const float gateX = width * (stacked ? 0.50f : 0.745f);
        const float floorY = height * (stacked ? StackedFloor : 0.64f);
        const Camera2D scene{{std::round(gateX), std::round(floorY)}, {0, 0}, 0, zoom};
        const int left = static_cast<int>(std::floor(-gateX / zoom));
        const int right = static_cast<int>(std::ceil((width - gateX) / zoom));
        const int top = static_cast<int>(std::floor(-floorY / zoom));
        const int bottom = static_cast<int>(std::ceil((height - floorY) / zoom));

        ClearBackground({15, 15, 22, 255});
        BeginMode2D(scene);
        DrawMasonry(stone, left, right, top);
        DrawFloor(stone, left, right, bottom);
        DrawGate(stone);
        DrawTorch(-60, -63, time, 0);
        DrawTorch(60, -63, time, 2);
        DrawRunner(player);
        EndMode2D();

        DrawRectangleGradientV(0, 0, width, height / 3, Color{5, 5, 10, 180}, BLANK);
        DrawRectangleGradientV(0, height * 3 / 4, width, height - height * 3 / 4,
                               BLANK, Color{5, 5, 10, 200});
        if (stacked)
            DrawRectangleGradientH(0, 0, width / 8, height, Color{6, 6, 11, 175}, BLANK);
        else
            DrawRectangleGradientH(0, 0, width * 3 / 5, height, Color{6, 6, 11, 230}, BLANK);
        DrawRectangleGradientH(width * 7 / 8, 0, width - width * 7 / 8, height,
                               BLANK, Color{6, 6, 11, 175});
    }

private:
    static void DrawMasonry(Texture2D stone, int left, int right, int top)
    {
        for (int y = -32; y > top - 32; y -= 32)
        {
            const int stagger = ((-y / 32) % 2) * 16;
            const int firstX = left - 32 + stagger - left % 32;
            for (int x = firstX; x < right; x += 32)
            {
                const bool lighter = (x / 32 + y / 32) % 3 == 0;
                const Color tint = lighter ? Color{118, 113, 138, 255} : Color{95, 97, 120, 255};
                DrawTextureRec(stone, DungeonArt::Masonry,
                               {static_cast<float>(x), static_cast<float>(y)}, tint);
            }
        }

        for (int x : {-111, 98})
        {
            // Narrow windows in the wall behind the entrance.
            DrawRectangle(x - 5, -112, 21, 43, Color{38, 35, 44, 255});
            DrawRectangle(x - 2, -109, 15, 37, Color{12, 12, 20, 255});
            DrawRectangle(x + 4, -107, 3, 30, Color{42, 41, 55, 255});
            DrawRectangle(x - 8, -72, 27, 3, Color{47, 42, 48, 255});
            DrawRectangle(x - 5, -69, 21, 2, Color{10, 10, 15, 255});
        }
        DrawRectangle(left, -15, right - left, 2, Color{48, 43, 50, 255});
        DrawRectangle(left, -13, right - left, 13, Color{21, 20, 26, 255});
    }

    static void DrawFloor(Texture2D stone, int left, int right, int bottom)
    {
        DrawRectangle(left, 0, right - left, bottom, Color{24, 22, 28, 255});
        for (int y = 1, depth = 7; y < bottom; y += depth, depth += 3)
        {
            const int tileWidth = depth * 3;
            for (int x = -(1 + (-left / tileWidth)) * tileWidth; x < right; x += tileWidth)
            {
                const bool worn = (x / tileWidth + y) % 4 == 0;
                const Color tint = worn ? Color{118, 113, 124, 255} : Color{94, 93, 108, 255};
                const Rectangle tile{static_cast<float>(x + (y % 2) * depth),
                                     static_cast<float>(y), static_cast<float>(tileWidth - 1),
                                     static_cast<float>(depth - 1)};
                DrawTexturePro(stone, worn ? DungeonArt::WornSlate : DungeonArt::Slate,
                               tile, {0, 0}, 0, tint);
            }
        }
        DrawRectangleGradientV(left, 0, right - left, bottom,
                               BLANK, Color{8, 7, 14, 150});
        DrawCircleGradient({0, 6}, 66, Color{164, 86, 31, 35}, BLANK);
    }

    static void DrawGate(Texture2D stone)
    {
        constexpr Color shadow{10, 9, 15, 255};
        constexpr Color stoneTint{168, 150, 144, 255};
        constexpr Color edge{70, 57, 54, 255};
        DrawRectangle(-74, -111, 148, 114, shadow);
        for (int y = -106; y < -10; y += 32)
        {
            for (int x : {-65, -33, -1, 31})
                DrawTextureRec(stone, DungeonArt::Masonry,
                               {static_cast<float>(x), static_cast<float>(y)}, stoneTint);
        }
        DrawRectangle(-78, -113, 156, 8, Color{37, 32, 39, 255});
        DrawRectangle(-73, -114, 146, 2, edge);
        DrawRectangle(-81, -108, 162, 4, Color{15, 13, 19, 255});
        DrawRectangle(-42, -97, 84, 91, shadow);

        // Stairwell and frame from the dungeon atlas at their native pixel size.
        DrawTextureRec(stone, {880, 16, 64, 88}, {-32, -96}, Color{235, 212, 190, 255});
        DrawRectangle(-7, -108, 14, 8, Color{40, 30, 34, 255});
        DrawRectangle(-4, -107, 8, 2, Color{107, 80, 61, 255});
        for (int x : {-73, 52})
        {
            DrawRectangle(x, -105, 21, 106, Color{26, 23, 29, 255});
            DrawRectangle(x + 2, -106, 3, 105, Color{62, 50, 49, 255});
            DrawRectangle(x + 6, -104, 12, 104, Color{45, 37, 39, 255});
            for (int y = -92; y < 0; y += 15)
                DrawRectangle(x + 2, y, 16, 1, Color{22, 20, 25, 255});
            DrawRectangle(x - 3, -102, 27, 4, edge);
            DrawRectangle(x - 3, -7, 27, 7, edge);
        }
        for (int step = 0; step < 3; ++step)
        {
            const int x = -36 - step * 5;
            const int y = -8 + step * 5;
            DrawRectangle(x, y, -2 * x, 5, Color{37, 31, 35, 255});
            DrawRectangle(x + 1, y, -2 * x - 2, 1, Color{83, 62, 53, 255});
        }
    }

    void DrawTorch(int x, int y, float time, int phase) const
    {
        const float flicker = 0.5f + 0.5f * std::sin(time * 7.3f + phase * 1.6f);
        const Vector2 center{static_cast<float>(x), static_cast<float>(y)};
        DrawCircleGradient(center, 44 + 4 * flicker,
                           Color{180, 80, 28, static_cast<unsigned char>(58 + 13 * flicker)}, BLANK);
        DrawCircleGradient(center, 19 + 3 * flicker, Color{224, 98, 27, 106}, BLANK);

        const auto frame = (static_cast<int>(std::fmod(time, 0.5f) * 8) + phase) % flames.size();
        const Texture2D flame = flames[frame];
        if (IsTextureValid(flame))
            DrawTexture(flame, x - flame.width / 2, y + 6 - flame.height, WHITE);
        else
        {
            DrawRectangle(x - 3, y + 2, 6, 8, Color{79, 58, 45, 255});
            DrawRectangle(x - 2, y - 3, 5, 7, Color{200, 77, 24, 255});
            DrawRectangle(x, y - 2 - static_cast<int>(3 * flicker), 2, 5,
                          Color{249, 171, 69, 255});
        }
    }

    static void DrawRunner(Texture2D player)
    {
        constexpr int x = -11;
        constexpr int y = 18;
        constexpr int feetY = y + 19;
        constexpr Rectangle frame{0, 0, 16, 32};
        DrawCircleGradient({x + 8.0f, feetY + 2.0f}, 26, Color{173, 101, 46, 43}, BLANK);
        DrawRectangle(x - 2, feetY - 1, 20, 3, Color{7, 7, 13, 160});
        DrawRectangle(x + 1, feetY + 2, 14, 2, Color{8, 7, 12, 125});
        DrawTextureRec(player, frame, {x - 1, y}, Color{155, 110, 88, 225});
        DrawTextureRec(player, frame, {x, y}, WHITE);
    }

    std::array<Texture2D, 4> flames{};
};
}
#endif
