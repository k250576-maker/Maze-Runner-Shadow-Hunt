#ifndef SHADOW_HUNT_ENEMY_ART_H
#define SHADOW_HUNT_ENEMY_ART_H

#include "physical_enemies.h"
#include <algorithm>
#include <string>

namespace Enemies
{
class Sprites
{
public:
    bool Load(const std::string& root)
    {
        angel = LoadTexture((root + "characters/angel_transparent.png").c_str());
        Image sheet = LoadImage((root + "characters/ghost.png").c_str());
        if (!IsTextureValid(angel) || !IsImageValid(sheet)
            || angel.width != 306 || angel.height != 139 || sheet.width != 6144 || sheet.height != 4352)
        {
            if (IsImageValid(sheet)) UnloadImage(sheet);
            Unload();
            return false;
        }

        // Four frames from the front-facing row. Upload only the row segment we use.
        ImageCrop(&sheet, {0, 5 * FrameSize, FrameCount * FrameSize, FrameSize});
        ImageFormat(&sheet, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        auto* pixels = static_cast<Color*>(sheet.data);
        for (int i = 0; i < sheet.width * sheet.height; ++i)
        {
            Color& pixel = pixels[i];
            if (!pixel.a) continue;
            // Lift the nearly black silhouette while preserving the magenta details.
            pixel.r = static_cast<unsigned char>(std::max<int>(pixel.r, 42));
            pixel.g = static_cast<unsigned char>(std::max<int>(pixel.g, 35));
            pixel.b = static_cast<unsigned char>(std::max<int>(pixel.b, 73));
        }
        ghost = LoadTextureFromImage(sheet);
        UnloadImage(sheet);
        if (!IsTextureValid(ghost)) { Unload(); return false; }
        knight = LoadPortrait(root + "characters/knight.png");
        stalker = LoadPortrait(root + "characters/hollow_stalker.png");
        SetTextureFilter(angel, TEXTURE_FILTER_POINT);
        SetTextureFilter(ghost, TEXTURE_FILTER_POINT);
        return true;
    }

    void Unload()
    {
        if (IsTextureValid(angel)) UnloadTexture(angel);
        if (IsTextureValid(ghost)) UnloadTexture(ghost);
        if (IsTextureValid(knight)) UnloadTexture(knight);
        if (IsTextureValid(stalker)) UnloadTexture(stalker);
        angel = ghost = knight = stalker = {};
    }

    void DrawActor(const Enemy& enemy, float time) const
    {
        const bool angelActor = enemy.kind == Kind::Angel;
        const bool frozen = enemy.state == State::Frozen;
        const float cycle = enemy.stride * 6.2831853f / 24;
        const float hover = angelActor ? 0 : 3.0f + 2.0f * std::sin(time * 6.2831853f);
        const float effort = enemy.windup > 0 ? std::min(1.0f, enemy.windup / 0.7f) : 0;
        const float angle = angelActor && !frozen ? (enemy.traveled > 0 ? 1.7f * std::sin(cycle) : 0) : 0;
        const float x = enemy.position.x;
        const float y = enemy.position.y;
        DrawEllipse(static_cast<int>(x), static_cast<int>(y), 10, 4, {12, 8, 18, static_cast<unsigned char>(angelActor ? 160 : 85)});
        Draw(enemy.kind, {x + enemy.facing.x * effort * 2, y - hover},
             (angelActor ? 43 : 44) * (1 + 0.045f * effort),
             !frozen && (enemy.state == State::Chasing || enemy.state == State::Investigating),
             time, enemy.facing.x > 0 && angelActor, angle, effort);
    }

    void DrawGhostPulse(const Enemy& ghost) const
    {
        const Vector2 ground = ghost.position;
        if (ghost.windup > 0)
        {
            const float charge = std::clamp(ghost.windup / GhostWarning, 0.0f, 1.0f);
            const float size = 14 - 8 * charge;
            DrawRing({ground.x,ground.y-10},size,size+1.2f,0,360,24,Fade({203,170,255,255},0.30f+0.55f*charge));
        }
        if (ghost.attackPulse > 0)
        {
            const float fade = ghost.attackPulse / GhostPulse;
            const float range = 10 + 21*(1-fade);
            DrawRing(ground,range,range+2.5f,0,360,28,Fade({188,122,247,255},fade*0.72f));
            DrawCircleGradient({ground.x,ground.y-10},range,{207,165,255,static_cast<unsigned char>(97*fade)},BLANK);
        }
    }

    void DrawPhysical(const PhysicalEnemy& actor) const
    {
        const auto& enemy = actor.body;
        const auto& stats = Stats(enemy.kind);
        const bool dying = enemy.state == State::Dying;
        const float alpha = dying ? actor.timer / DeathDuration : 1;
        const float intent = enemy.state == State::WindingUp ? 1 - actor.timer / stats.windup : 0;
        const float gait = enemy.traveled > 0 ? std::sin(enemy.stride * 6.2831853f / 24) : 0;
        const Vector2 p = enemy.position;
        const float lunge = actor.swung || (enemy.state == State::Recovering && actor.hurt <= 0
            && actor.timer > stats.recovery - 0.14f) ? 1 : 0;
        const Color tint = actor.hurt > 0 ? Color{255, 157, 146, 255} : WHITE;
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), 11, 4, Fade({15,10,16,255}, alpha * 0.65f));
        Draw(enemy.kind, p, (enemy.kind == Kind::Sentinel ? 43.0f : 36.0f) * (0.76f + 0.24f * alpha),
             false, gait, enemy.facing.x < 0, dying ? (1 - alpha) * 26 : (intent * -8 + lunge * 13) * (enemy.facing.x < 0 ? -1 : 1), intent, Fade(tint, alpha));
        if (intent > 0)
        {
            const float start = std::atan2(enemy.facing.y, enemy.facing.x) * 57.29578f;
            DrawRing(p, stats.reach - 2, stats.reach, start - 37, start + 37, 18,
                     Fade({249, 153, 89, 255}, 0.2f + 0.65f * intent));
        }
        if (actor.swung || (enemy.state == State::Recovering && actor.hurt <= 0 && actor.timer > stats.recovery - 0.16f))
        {
            const float start = std::atan2(enemy.facing.y, enemy.facing.x) * 57.29578f;
            DrawRing(p, stats.reach - 6, stats.reach + 2, start - 44, start + 44, 20, {255, 181, 115, 185});
        }
    }

    void Draw(Kind kind, Vector2 feet, float height, bool pursuing = false, float time = 0, bool mirrored = false,
              float angle = 0, float effort = 0, Color tint = WHITE) const
    {
        if (kind == Kind::Angel)
        {
            Rectangle pose = pursuing ? Rectangle{177, 6, 105, 127} : Rectangle{21, 8, 86, 125};
            // The plinth stays centered under the body when the reach pose extends left.
            float anchor = pursuing ? 56.0f : 39.0f;
            if (mirrored) { anchor = pose.width - anchor; pose.width = -pose.width; }
            DrawFrame(angel, pose, feet, height, anchor, Color{255, static_cast<unsigned char>(255-44*effort),
                      static_cast<unsigned char>(255-54*effort), 255}, angle);
            return;
        }
        if (kind == Kind::Sentinel || kind == Kind::Stalker)
        {
            const Texture2D texture = kind == Kind::Sentinel ? knight : stalker;
            if (!IsTextureValid(texture))
                DrawPlaceholder(kind, feet, height, time, mirrored, angle, tint);
            else
            {
                const float width = static_cast<float>(texture.width);
                DrawFrame(texture, {0, 0, mirrored ? -width : width, float(texture.height)},
                          feet, height, width / 2, tint, angle + 2 * time);
            }
            return;
        }
        const int cycle = static_cast<int>(std::fmod(time, 1.0f) * 6);
        const int frame = cycle < FrameCount ? cycle : 6 - cycle;
        const Rectangle pose{56.0f + frame * FrameSize, 60, 126, 151};
        DrawFrame(ghost, pose, feet, height, 75.0f, Color{235, static_cast<unsigned char>(224-64*effort), 255, 235});
    }

private:
    static Texture2D LoadPortrait(const std::string& path)
    {
        if (!FileExists(path.c_str())) return {};
        Image image = LoadImage(path.c_str());
        if (!IsImageValid(image)) return {};
        const Rectangle bounds = GetImageAlphaBorder(image, 0.04f);
        if (bounds.width > 0 && bounds.height > 0) ImageCrop(&image, bounds);
        Texture2D texture = bounds.width > 0 && bounds.height > 0 ? LoadTextureFromImage(image) : Texture2D{};
        UnloadImage(image);
        if (IsTextureValid(texture)) SetTextureFilter(texture, TEXTURE_FILTER_POINT);
        return texture;
    }

    static void DrawPlaceholder(Kind kind, Vector2 feet, float height, float gait, bool mirrored, float lean, Color tint)
    {
        const bool armored = kind == Kind::Sentinel;
        const float unit = height / 40;
        const float flip = mirrored ? -1.0f : 1.0f;
        const auto point = [&](float x, float y) {
            return Vector2{feet.x + (flip * x - y * lean * 0.01745f) * unit, feet.y + y * unit};
        };
        const auto color = [&](Color value) { return ColorTint(value, tint); };
        const auto line = [&](float x, float y, float x2, float y2, float width, Color value) {
            DrawLineEx(point(x, y), point(x2, y2), width * unit, color(value));
        };
        const auto patch = [&](float x, float y, float x2, float y2, float x3, float y3, Color value) {
            const Vector2 a = point(x, y), b = point(x2, y2), c = point(x3, y3);
            if ((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x) > 0) DrawTriangle(a, c, b, color(value));
            else DrawTriangle(a, b, c, color(value));
        };
        const Color outline{21,19,27,255}, shadow{38,34,53,255}, steel{102,109,124,255};
        const Color red{184,42,47,255}, eyes{255,101,54,255};
        patch(-6,-30, 2,-27, -13,-3, armored ? red : shadow);
        line(-4,-12, -5-gait*2,-1, 5, outline);
        line(4,-12, 5+gait*2,-1, 5, outline);
        line(-6,-23, 5,-23, 15, outline);
        line(-3,-24, 4,-24, 11, armored ? steel : shadow);
        line(-7,-25, -10,-13, 5, armored ? steel : shadow);
        line(8,-24, 11,-13, 5, armored ? steel : shadow);
        line(-5,-14, 6,-14, 3, armored ? red : outline);
        if (armored)
        {
            line(10,-13, 15,-28 + gait*2, 2.4f, {181,187,188,255});
            line(8,-17, 15,-15, 2, {145,109,66,255});
            line(-3,-37, 6,-37, 11, outline);
            line(-2,-36, 5,-36, 9, steel);
            line(5,-42, -2,-44, 3, red);
        }
        else
        {
            patch(-9,-27, 0,-43, 9,-27, shadow);
            line(-3,-33, 5,-33, 9, outline);
            line(9,-13, 13,-12, 2, red);
        }
        line(-2,-34, 0,-34, 2, eyes);
        line(4,-34, 6,-34, 2, eyes);
    }

    static constexpr int FrameSize = 256;
    static constexpr int FrameCount = 4;

    static void DrawFrame(Texture2D texture, Rectangle source, Vector2 feet,
                          float height, float anchor, Color tint, float angle = 0)
    {
        const float scale = height / source.height;
        const Rectangle destination{feet.x, feet.y, std::abs(source.width) * scale, height};
        DrawTexturePro(texture, source, destination, {anchor * scale, height}, angle, tint);
    }

    Texture2D angel{};
    Texture2D ghost{};
    Texture2D knight{}, stalker{};
};
}
#endif
