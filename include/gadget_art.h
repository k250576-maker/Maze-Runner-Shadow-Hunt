#ifndef SHADOW_HUNT_GADGET_ART_H
#define SHADOW_HUNT_GADGET_ART_H

#include "gadgets.h"
#include <string>
#include <string_view>

namespace Gadgets
{
using PixelArt = std::array<std::string_view, 16>;

constexpr PixelArt BombPixels{
    "..........33....",
    ".........344....",
    "........34......",
    ".......33.......",
    "......12211.....",
    "....112333211...",
    "...12555534521..",
    "..1556653455521.",
    "..1565534555221.",
    ".15555345552221.",
    ".12223455522121.",
    ".13444352221221.",
    "..132222222221..",
    "..11222222211...",
    "....1111111.....",
    "................"
};
constexpr PixelArt LurePixels{
    ".......33.......",
    "......3333......",
    "......3..3......",
    ".......44.......",
    ".....114411.....",
    "....12444421....",
    "...1245665421...",
    "...1456566541...",
    "...1456765541...",
    "...1245655421...",
    "....12444421....",
    ".....112211.....",
    ".......44.......",
    "......5345......",
    "......5.35......",
    ".....2..2.2....."
};
constexpr Color BombColors[]{
    BLANK, {23, 19, 21, 255}, {75, 54, 43, 255}, {144, 105, 62, 255},
    {195, 156, 92, 255}, {88, 88, 78, 255}, {128, 126, 102, 255}
};
constexpr Color LureColors[]{
    BLANK, {32, 17, 25, 255}, {103, 47, 57, 255}, {145, 102, 64, 255},
    {198, 148, 84, 255}, {141, 42, 57, 255}, {182, 67, 78, 255}, {231, 167, 103, 255}
};

inline void DrawPixels(const PixelArt& art, const Color* palette, Vector2 topLeft, Vector2 pixel, float alpha)
{
    for (int row = 0; row < 16; ++row)
        for (int column = 0; column < 16; ++column)
        {
            const char index = art[row][column];
            if (index != '.') DrawRectangleRec({topLeft.x + column * pixel.x, topLeft.y + row * pixel.y, pixel.x, pixel.y},
                                               ColorAlpha(palette[index - '0'], alpha * palette[index - '0'].a / 255.0f));
        }
}

inline void DrawIcon(Type type, Vector2 topLeft, float pixel, float alpha = 1.0f)
{
    const bool bomb = type == Type::SmokeBomb;
    DrawPixels(bomb ? BombPixels : LurePixels, bomb ? BombColors : LureColors, topLeft, {pixel, pixel}, alpha);
}

class Art
{
public:
    void Load(const std::string& root)
    {
        const std::string vfx = root + "enviroment/vfx/";
        deployment = LoadSheet(vfx + "VFX2/sprite-sheet.png", 5);
        cloud = LoadSheet(vfx + "VFX3/sprite-sheet.png", 9);
        impact = LoadSheet(vfx + "VFX4/sprite-sheet.png", 6);
    }

    void Unload()
    {
        for (auto* sheet : {&deployment, &cloud, &impact})
        {
            if (IsTextureValid(*sheet)) UnloadTexture(*sheet);
            *sheet = {};
        }
    }

    void DrawPickups(const Run& run, const Maze::Layout& level, Vector2 player,
                     Rectangle visible, float time) const
    {
        for (const auto& pickup : run.Pickups())
        {
            if (pickup.cell == Enemies::NoCell || pickup.collected) continue;
            const Vector2 at = Enemies::Center(pickup.cell);
            if (!CheckCollisionPointRec(at, visible) || !Maze::HasLineOfSight(level, player, at)) continue;
            const float bob = std::round(1.6f * std::sin(time * 2.8f + pickup.cell));
            DrawEllipse(static_cast<int>(at.x), static_cast<int>(at.y + 4), 8, 2, {16, 12, 16, 150});
            DrawIcon(pickup.type, {at.x - 10, at.y - 15 + bob}, 1.25f);
        }
    }

    void DrawEffects(const Run& run, const Maze::Layout& level, Vector2 player, Rectangle visible) const
    {
        const auto observable = [&](const Effect& effect) {
            return effect.remaining > 0 && CheckCollisionPointRec(effect.position, visible)
                && Maze::HasLineOfSight(level, player, effect.position);
        };
        for (const auto& effect : run.Smoke())
            if (observable(effect)) DrawSmoke(effect);
        for (const auto& effect : run.Lures())
        {
            if (!observable(effect)) continue;
            const float age = LureSeconds - effect.remaining;
            const float pulse = std::fmod(age * 1.34f, 1.0f);
            const float fade = std::min(1.0f, effect.remaining * 3.0f);
            const auto at = effect.position;
            DrawCircleGradient(at, 14 + 4 * pulse, Fade({160, 44, 53, 255}, 0.32f * fade), BLANK);
            DrawPolyLinesEx(at, 4, 7 + 19 * pulse, 0, 1.0f,
                            Fade({186, 106, 79, 255}, (1 - pulse) * 0.55f * fade));
            DrawIcon(Type::SoulLure, {at.x - 10, at.y - 15}, 1.25f, fade);
        }
    }

    void DrawMeleeImpact(Vector2 feet, float age) const
    {
        constexpr float duration = 0.11f;
        if (!IsTextureValid(impact) || age >= duration) return;
        const float progress = age / duration;
        DrawFrame(impact, std::min(4, static_cast<int>(progress * 5)),
                  {feet.x, feet.y - 10}, 21, 0.5f, Fade({230, 174, 130, 255}, (1 - progress) * 0.72f));
    }

private:
    static constexpr float FrameSize = 128.0f;

    static Texture2D LoadSheet(const std::string& path, int frames)
    {
        if (!FileExists(path.c_str())) return {};
        Texture2D sheet = LoadTexture(path.c_str());
        if (!IsTextureValid(sheet)) return {};
        if (sheet.width != FrameSize * frames || sheet.height != FrameSize)
        {
            TraceLog(LOG_WARNING, "Unexpected smoke sprite sheet dimensions: %s", path.c_str());
            UnloadTexture(sheet);
            return {};
        }
        SetTextureFilter(sheet, TEXTURE_FILTER_POINT);
        return sheet;
    }

    static void DrawFrame(Texture2D texture, int frame, Vector2 at, float size, float anchorY,
                          Color tint, bool mirrored = false)
    {
        DrawTexturePro(texture, {frame * FrameSize, 0, mirrored ? -FrameSize : FrameSize, FrameSize},
            {at.x, at.y, size, size}, {size * 0.5f, size * anchorY}, 0, tint);
    }

    void DrawSmoke(const Effect& effect) const
    {
        const float age = SmokeSeconds - effect.remaining;
        const float alpha = std::min(1.0f, effect.remaining / 0.72f);
        if (IsTextureValid(deployment) && age < 0.42f)
            DrawFrame(deployment, std::min(4, static_cast<int>(age * 12)), effect.position, 104, 0.66f,
                      Fade({158, 151, 145, 255}, 0.66f * (1 - age / 0.42f)));

        if (!IsTextureValid(cloud))
        {
            // Keep the active area visible even in checkouts without the local pack.
            constexpr PixelArt mist{
                "................", "................", ".......1....1...", "...11.111..111..",
                "..11211211.1211.", ".112221222112211", ".12222222222221.", "122222222222221.",
                "112222222222211.", ".1122221222211..", "..11121111211...", "...111....11....",
                "................", "................", "................", "................"
            };
            constexpr Color tones[]{BLANK, {116, 108, 120, 49}, {141, 132, 144, 92}};
            const float spread = std::clamp(age * 3 + 0.25f, 0.25f, 1.0f) * SmokeRadius;
            DrawPixels(mist, tones, {effect.position.x - spread, effect.position.y - spread * 0.64f},
                       {spread / 8, spread / 10}, alpha);
            return;
        }

        const Color gray{151, 144, 150, 255};
        if (age < 0.60f)
        {
            const int frame = std::min(4, static_cast<int>(age / 0.12f));
            DrawFrame(cloud, frame, effect.position, 110 + age * 32, 0.70f, Fade(gray, 0.73f));
            return;
        }

        if (effect.remaining <= 0.72f)
        {
            const int frame = 6 + std::min(2, static_cast<int>((0.72f - effect.remaining) / 0.24f));
            DrawFrame(cloud, frame, effect.position, 132, 0.53f, Fade(gray, alpha * 0.66f));
            return;
        }

        const float blend = (1 + std::sin(age * 4.8f)) * 0.5f;
        DrawFrame(cloud, 5, {effect.position.x - 8, effect.position.y + 9}, 130, 0.59f,
                  Fade(gray, 0.59f - blend * 0.13f));
        DrawFrame(cloud, 6, {effect.position.x + 11, effect.position.y + 19}, 115, 0.56f,
                  Fade(gray, 0.22f + blend * 0.21f), true);
    }

    Texture2D deployment{}, cloud{}, impact{};
};
}
#endif
