#ifndef SHADOW_HUNT_LIGHTING_H
#define SHADOW_HUNT_LIGHTING_H

#include "raylib.h"
#include "rlgl.h"
#include "maze_visibility.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Lighting
{
// Tuning values in maze cells, degrees, and normalized opacity.
constexpr float FlashlightRangeCells = 5.0f;
constexpr float BeamAngleDegrees = 70.0f;
constexpr float BeamHalfAngle = BeamAngleDegrees * DEG2RAD * 0.5f;
constexpr float BeamFeatherDegrees = 13.0f;
constexpr float BeamNearBlendCells = 0.65f;
constexpr Color FlashlightCoreTint{255, 190, 113, 255};
constexpr Color FlashlightFalloffTint{255, 139, 54, 255};
constexpr float FlashlightColorStrength = 0.27f;
constexpr float FlashlightBounceStrength = 0.045f;
constexpr Color PlayerGlowTint{255, 193, 126, 255};
constexpr float PlayerGlowColorStrength = 0.15f;
constexpr float PlayerGlowBrightness = 0.68f;
constexpr float AmbientRadiusCells = 1.5f;
constexpr float DarknessOpacity = 0.82f; // Lower this to brighten the dungeon outside local lights.
constexpr Color AmbientTint{232, 248, 255, 255};
constexpr float AmbientBrightness = 1.0f - DarknessOpacity;
constexpr unsigned char AmbientGain = static_cast<unsigned char>(AmbientBrightness * 255 + 0.5f);
// Tint for stone outside the light mask's world bounds.
constexpr Color AmbientMaskColor{
    static_cast<unsigned char>(AmbientTint.r * AmbientBrightness + 0.5f),
    static_cast<unsigned char>(AmbientTint.g * AmbientBrightness + 0.5f),
    static_cast<unsigned char>(AmbientTint.b * AmbientBrightness + 0.5f), 255};
constexpr float BeamFullStrengthFraction = 0.60f;
constexpr float PointFullStrengthFraction = 0.15f;
constexpr float RaySpacingDegrees = 2.0f;
constexpr int MaxStaticLights = 24;
constexpr float WallFaceDepth = 9.0f;
constexpr float WallBounceDepth = 14.0f;
constexpr float WallInteriorFalloff = 0.22f;
static_assert(WallBounceDepth < Maze::CellSize, "Reflected light must stay in the floor tile next to the wall.");

struct PointLight
{
    Vector2 position{};
    float radius = 0;
    Color tint = WHITE;
    float brightness = 1.0f;
    float colorStrength = 0.0f;
    float bounceStrength = 0.0f;
};

struct LightProfile
{
    Color core;
    Color falloff;
    float fullStrengthFraction;
    float brightness = 1.0f;
    float feather = 0.0f;
    float colorStrength = 0.0f;
    float bounceStrength = 0.0f;
};

inline float SmoothFade(float progress)
{
    const float t = std::clamp(progress, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

inline Color LightColor(const LightProfile& profile, float distanceFraction, float angularWeight)
{
    const float fade = SmoothFade((distanceFraction - profile.fullStrengthFraction)
                                  / (1 - profile.fullStrengthFraction));
    const float gain = std::clamp(profile.brightness, 0.0f, 1.0f) * angularWeight * (1 - fade);
    const float colorGain = gain * std::clamp(profile.colorStrength, 0.0f, 1.0f);
    const auto channel = [fade, colorGain](unsigned char core, unsigned char falloff) {
        return static_cast<unsigned char>((core + (falloff - core) * fade) * colorGain + 0.5f);
    };
    // RGB adds colored light and alpha scales the stone's existing texture.
    return {channel(profile.core.r, profile.falloff.r), channel(profile.core.g, profile.falloff.g),
            channel(profile.core.b, profile.falloff.b), static_cast<unsigned char>(255 * gain + 0.5f)};
}

inline float AngularWeight(const LightProfile& profile, float angle, float startAngle, float endAngle)
{
    const float edgeDistance = std::min(angle - startAngle, endAngle - angle);
    if (edgeDistance < 0.0f) return 0.0f;
    return profile.feather > 0.0f ? SmoothFade(edgeDistance / profile.feather) : 1.0f;
}

class DungeonLight
{
public:
    explicit DungeonLight(const Maze::Layout& level) : level(level) {}

    bool Initialize()
    {
        mask = LoadRenderTexture(Maze::Columns * Maze::CellSize, Maze::Rows * Maze::CellSize);
        if (mask.id == 0) return false;
        staticMask = LoadRenderTexture(Maze::Columns * Maze::CellSize, Maze::Rows * Maze::CellSize);
        if (staticMask.id == 0) { Unload(); return false; }
        SetTextureFilter(mask.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(staticMask.texture, TEXTURE_FILTER_POINT);
        BeginTextureMode(staticMask);
        ClearBackground({0, 0, 0, AmbientGain});
        EndTextureMode();
        return true;
    }

    void Unload()
    {
        if (mask.id != 0) UnloadRenderTexture(mask);
        if (staticMask.id != 0) UnloadRenderTexture(staticMask);
        mask = {};
        staticMask = {};
        maskReady = false;
    }

    // Only rebuild when fixed light sources change, not when flames animate.
    // Positions are copied so the caller's temporary array need not stay alive.
    void SetStaticLights(const PointLight* lights, int count)
    {
        staticCount = lights != nullptr ? std::clamp(count, 0, MaxStaticLights) : 0;
        for (int i = 0; i < staticCount; ++i) staticLights[i] = lights[i];
        maskReady = false;
        if (staticMask.id == 0) return;
        BeginTextureMode(staticMask);
        ClearBackground({0, 0, 0, AmbientGain});
        BeginLightBlend();
        for (int i = 0; i < staticCount; ++i)
        {
            const PointLight& light = staticLights[i];
            if (light.radius <= 0) continue;
            const LightProfile profile{light.tint, light.tint, PointFullStrengthFraction,
                                       light.brightness, 0.0f, light.colorStrength, light.bounceStrength};
            DrawLightFan(-PI, PI, light.radius, light.position, profile);
            DrawStoneLighting(light.position, light.radius, profile, 0.0f, PI);
        }
        EndBlendMode();
        DrawBouncePatches();
        EndTextureMode();
    }

    // Maze-local pixel coordinates, with (0,0) at the maze's top-left.
    // No movement dependency: any observer can supply a position and direction.
    void Update(Vector2 position, Vector2 direction)
    {
        const float newAngle = direction.x != 0 || direction.y != 0
            ? std::atan2(direction.y, direction.x) : facingAngle;
        const Vector2 previous = origin;
        const bool changed = position.x != origin.x || position.y != origin.y || newAngle != facingAngle;
        origin = position;
        facingAngle = newAngle;
        if (mask.id == 0) return;
        if (maskReady && !changed) return;
        BeginTextureMode(mask);
        RestoreStaticLighting(previous);
        BeginLightBlend();
        const LightProfile halo{PlayerGlowTint, PlayerGlowTint, 0.25f,
                                PlayerGlowBrightness, 0.0f, PlayerGlowColorStrength};
        DrawLightFan(-PI, PI, AmbientRadiusCells * Maze::CellSize, origin, halo);
        const LightProfile beam{FlashlightCoreTint, FlashlightFalloffTint, BeamFullStrengthFraction,
                                1.0f, BeamFeatherDegrees * DEG2RAD,
                                FlashlightColorStrength, FlashlightBounceStrength};
        // MAX blending makes ambient a floor and keeps overlapping lights from stacking.
        DrawLightFan(facingAngle - BeamHalfAngle, facingAngle + BeamHalfAngle,
                     FlashlightRangeCells * Maze::CellSize, origin, beam);
        DrawStoneLighting(origin, FlashlightRangeCells * Maze::CellSize, beam, facingAngle, BeamHalfAngle);
        EndBlendMode();
        DrawBouncePatches();
        EndTextureMode();
        maskReady = true;
    }

    // Tests the flashlight and the player's local glow.
    bool IsWorldPositionVisible(Vector2 worldPosition) const
    {
        return IsPlayerLit(worldPosition, true);
    }

    // Future Angel observation must use this: ONLY the directional flashlight,
    // with range, cone, and line-of-sight checks.
    bool IsWorldPositionObserved(Vector2 worldPosition) const
    {
        return IsPlayerLit(worldPosition, false);
    }

    // Tests local light sources for gameplay, independent of the ambient rendering floor.
    bool IsWorldPositionIlluminated(Vector2 worldPosition) const
    {
        if (IsWorldPositionVisible(worldPosition)) return true;
        if (!Maze::IsWalkablePosition(level, worldPosition)) return false;
        for (int i = 0; i < staticCount; ++i)
        {
            const PointLight& light = staticLights[i];
            const float dx = worldPosition.x - light.position.x;
            const float dy = worldPosition.y - light.position.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < light.radius && Maze::HasLineOfSight(level, light.position, worldPosition))
                return true;
        }
        return false;
    }

    Texture2D MaskTexture() const { return mask.texture; } // RGB light, alpha texture gain.

    void DrawDarkness() const
    {
        // scene.rgb = scene.rgb * mask.a + mask.rgb; keep framebuffer alpha intact.
        rlSetBlendFactorsSeparate(RL_ONE, RL_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        DrawTextureRec(mask.texture,
                       {0, 0, static_cast<float>(mask.texture.width), -static_cast<float>(mask.texture.height)},
                       {0, 0}, WHITE);
        EndBlendMode();
    }

private:
    struct BouncePatch { Rectangle bounds; int edge; Color tint; };
    const Maze::Layout& level;
    RenderTexture2D mask{};
    RenderTexture2D staticMask{};
    PointLight staticLights[MaxStaticLights]{};
    int staticCount = 0;
    Vector2 origin{};
    float facingAngle = PI * 0.5f;
    bool maskReady = false;
    std::vector<BouncePatch> bouncePatches;

    void RestoreStaticLighting(Vector2 previous) const
    {
        const float width = static_cast<float>(staticMask.texture.width);
        const float height = static_cast<float>(staticMask.texture.height);
        Rectangle dirty{0, 0, width, height};
        if (maskReady)
        {
            // Once torches are cached, restore only the old and new flashlight footprints.
            constexpr float padding = FlashlightRangeCells * Maze::CellSize + WallFaceDepth + WallBounceDepth + 2;
            const float x = std::max(0.0f, std::floor(std::min(previous.x, origin.x) - padding));
            const float y = std::max(0.0f, std::floor(std::min(previous.y, origin.y) - padding));
            const float right = std::min(width, std::ceil(std::max(previous.x, origin.x) + padding));
            const float bottom = std::min(height, std::ceil(std::max(previous.y, origin.y) + padding));
            dirty = {x, y, right - x, bottom - y};
        }
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
        DrawTextureRec(staticMask.texture, {dirty.x, height - dirty.y - dirty.height, dirty.width, -dirty.height},
                       {dirty.x, dirty.y}, WHITE);
        EndBlendMode();
    }

    bool IsPlayerLit(Vector2 worldPosition, bool includeAmbient) const
    {
        if (!Maze::IsWalkablePosition(level, worldPosition)) return false;
        const float dx = worldPosition.x - origin.x;
        const float dy = worldPosition.y - origin.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < 0.001f) return Maze::HasLineOfSight(level, origin, worldPosition);
        const bool ambient = includeAmbient && distance <= AmbientRadiusCells * Maze::CellSize;
        const float dot = (dx * std::cos(facingAngle) + dy * std::sin(facingAngle)) / distance;
        const bool beam = distance <= FlashlightRangeCells * Maze::CellSize
            && dot >= std::cos(BeamHalfAngle) - 0.00001f;
        return (ambient || beam) && Maze::HasLineOfSight(level, origin, worldPosition);
    }

    static void BeginLightBlend()
    {
        // Component-wise maximum: overlapping lights blend without darkening
        // one another or washing out the dark dungeon with repeated additions.
        rlSetBlendFactors(RL_ONE, RL_ONE, RL_MAX);
        BeginBlendMode(BLEND_CUSTOM);
    }

    static void DrawEdgeGradient(Rectangle bounds, int nearSide, Color nearColor, Color farColor)
    {
        const bool nearLowEdge = nearSide % 2 == 0;
        const Color low = nearLowEdge ? nearColor : farColor;
        const Color high = nearLowEdge ? farColor : nearColor;
        if (nearSide < 2) DrawRectangleGradientEx(bounds, low, low, high, high);
        else DrawRectangleGradientEx(bounds, low, high, high, low);
    }

    void DrawBouncePatches()
    {
        if (bouncePatches.empty()) return;
        // Only RGB is added. Zero source alpha leaves the direct texture gain intact.
        BeginBlendMode(BLEND_ADD_COLORS);
        for (const BouncePatch& patch : bouncePatches)
            DrawEdgeGradient(patch.bounds, patch.edge, patch.tint, BLANK);
        EndBlendMode();
        bouncePatches.clear();
    }

    void DrawStoneLighting(Vector2 source, float radius, const LightProfile& profile,
                           float direction, float halfAngle)
    {
        const Vector2 normals[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        const Vector2 forward{std::cos(direction), std::sin(direction)};
        const bool directional = halfAngle < PI;
        LightProfile reflected = profile;
        reflected.colorStrength = profile.bounceStrength;
        const auto scaleChannel = [](unsigned char channel) {
            return static_cast<unsigned char>(channel * WallInteriorFalloff + 0.5f);
        };
        const int firstRow = std::max(0, static_cast<int>(std::floor((source.y - radius) / Maze::CellSize)));
        const int lastRow = std::min(Maze::Rows - 1, static_cast<int>((source.y + radius) / Maze::CellSize));
        const int firstColumn = std::max(0, static_cast<int>(std::floor((source.x - radius) / Maze::CellSize)));
        const int lastColumn = std::min(Maze::Columns - 1, static_cast<int>((source.x + radius) / Maze::CellSize));
        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                if (level.IsWalkable(column, row)) continue;
                const float left = static_cast<float>(column * Maze::CellSize);
                const float top = static_cast<float>(row * Maze::CellSize);
                for (int side = 0; side < 4; ++side)
                {
                    const Vector2 normal = normals[side];
                    if (!level.IsWalkable(column + static_cast<int>(normal.x), row + static_cast<int>(normal.y))) continue;
                    const Vector2 face{left + (side == 1 ? Maze::CellSize : 0),
                                       top + (side == 3 ? Maze::CellSize : 0)};
                    if ((source.x - face.x) * normal.x + (source.y - face.y) * normal.y <= 0) continue;
                    constexpr int Segments = 4;
                    constexpr float length = static_cast<float>(Maze::CellSize) / Segments;
                    for (int segment = 0; segment < Segments; ++segment)
                    {
                        const bool verticalWall = side < 2;
                        const Vector2 stripStart{face.x + (verticalWall ? 0 : segment * length),
                                                 face.y + (verticalWall ? segment * length : 0)};
                        const Vector2 sample{stripStart.x + (verticalWall ? 0 : length * 0.5f) + normal.x * 0.01f,
                                             stripStart.y + (verticalWall ? length * 0.5f : 0) + normal.y * 0.01f};
                        const float dx = sample.x - source.x;
                        const float dy = sample.y - source.y;
                        const float distanceSquared = dx * dx + dy * dy;
                        if (distanceSquared >= radius * radius) continue;

                        const float angle = directional
                            ? std::atan2(forward.x * dy - forward.y * dx, forward.x * dx + forward.y * dy) : 0.0f;
                        const float weight = AngularWeight(profile, angle, -halfAngle, halfAngle);
                        if (weight == 0 || !Maze::HasLineOfSight(level, source, sample)) continue;

                        const float fraction = std::sqrt(distanceSquared) / radius;
                        const Color lit = LightColor(profile, fraction, weight);
                        const Color shaded{scaleChannel(lit.r), scaleChannel(lit.g), scaleChannel(lit.b), scaleChannel(lit.a)};
                        const Rectangle wallStrip{
                            stripStart.x - (side == 1 ? WallFaceDepth : 0),
                            stripStart.y - (side == 3 ? WallFaceDepth : 0),
                            verticalWall ? WallFaceDepth : length, verticalWall ? length : WallFaceDepth};
                        DrawEdgeGradient(wallStrip, side, lit, shaded);

                        Color bounce = LightColor(reflected, fraction, weight);
                        if (bounce.r == 0 && bounce.g == 0 && bounce.b == 0) continue;
                        bounce.a = 0;
                        // Confine the reflected band to the one floor tile sharing this visible face.
                        const Rectangle floorStrip{
                            stripStart.x - (side == 0 ? WallBounceDepth : 0),
                            stripStart.y - (side == 2 ? WallBounceDepth : 0),
                            verticalWall ? WallBounceDepth : length, verticalWall ? length : WallBounceDepth};
                        bouncePatches.push_back({floorStrip, side ^ 1, bounce});
                    }
                }
            }
        }
    }

    void DrawLightFan(float startAngle, float endAngle, float range,
                      Vector2 source, const LightProfile& profile) const
    {
        // Fixed storage covers uniform rays plus three rays at every grid vertex.
        constexpr int MaxAngles = 2048;
        float angles[MaxAngles];
        int count = 0;
        const auto addAngle = [&](float angle)
        {
            while (angle < startAngle) angle += 2 * PI;
            while (angle > startAngle + 2 * PI) angle -= 2 * PI;
            if (angle <= endAngle && count < MaxAngles) angles[count++] = angle;
        };
        angles[count++] = startAngle;
        angles[count++] = endAngle;
        for (float angle = startAngle + RaySpacingDegrees * DEG2RAD;
             angle < endAngle; angle += RaySpacingDegrees * DEG2RAD) addAngle(angle);
        // Rays on either side of corners prevent triangles bridging past wall
        // edges. Uniform rays alone can visibly leak at narrow corner angles.
        const int firstRow = std::max(0, static_cast<int>(std::ceil((source.y - range) / Maze::CellSize)));
        const int lastRow = std::min(Maze::Rows, static_cast<int>((source.y + range) / Maze::CellSize));
        const int firstColumn = std::max(0, static_cast<int>(std::ceil((source.x - range) / Maze::CellSize)));
        const int lastColumn = std::min(Maze::Columns, static_cast<int>((source.x + range) / Maze::CellSize));
        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                const float dx = column * Maze::CellSize - source.x;
                const float dy = row * Maze::CellSize - source.y;
                if (dx * dx + dy * dy > range * range) continue;
                const float angle = std::atan2(dy, dx);
                addAngle(angle - 0.0001f);
                addAngle(angle);
                addAngle(angle + 0.0001f);
            }
        }
        std::sort(angles, angles + count);
        struct Ray
        {
            Vector2 direction;
            float stop;
            float weight;
        };
        const auto rayAt = [&](float angle) {
            const Vector2 direction{std::cos(angle), std::sin(angle)};
            const float stop = std::min(range, Maze::FirstWallDistance(level, source, direction, range));
            return Ray{direction, stop, AngularWeight(profile, angle, startAngle, endAngle)};
        };

        constexpr int FadeBands = 3;
        const float brightRange = range * profile.fullStrengthFraction;
        const float nearBlend = profile.feather > 0.0f
            ? std::min(BeamNearBlendCells * Maze::CellSize, brightRange) : 0.0f;
        struct Vertex { Vector2 position; Color color; };
        const auto sample = [&](Ray ray, float radius) {
            const float distance = std::min(radius, ray.stop);
            const float nearWeight = nearBlend > 0.0f ? std::min(1.0f, distance / nearBlend) : 1.0f;
            return Vertex{{source.x + ray.direction.x * distance, source.y + ray.direction.y * distance},
                          LightColor(profile, distance / range, ray.weight * nearWeight)};
        };
        const auto vertex = [](Vertex point) {
            rlColor4ub(point.color.r, point.color.g, point.color.b, point.color.a);
            rlVertex2f(point.position.x, point.position.y);
        };
        const auto bandBetween = [&](Vertex innerA, Vertex innerB, Vertex outerA, Vertex outerB) {
            vertex(innerA); vertex(innerB); vertex(outerB);
            vertex(innerA); vertex(outerB); vertex(outerA);
        };

        // Extra rings sample the curved falloff without expanding the cone or the wall-clipped rays.
        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);
        Ray previous = rayAt(angles[0]);
        for (int i = 1; i < count; ++i)
        {
            const Ray next = rayAt(angles[i]);
            Vertex innerA = sample(previous, brightRange);
            Vertex innerB = sample(next, brightRange);
            if (nearBlend > 0.0f)
            {
                // Let the halo carry the color at the apex and blend into the direct cone.
                const Vertex nearA = sample(previous, nearBlend);
                const Vertex nearB = sample(next, nearBlend);
                vertex({source, BLANK}); vertex(nearB); vertex(nearA);
                bandBetween(nearA, nearB, innerA, innerB);
            }
            else
            {
                vertex({source, LightColor(profile, 0.0f, 1.0f)});
                vertex(innerB);
                vertex(innerA);
            }
            for (int band = 1; band <= FadeBands; ++band)
            {
                const float radius = brightRange + (range - brightRange) * band / FadeBands;
                const Vertex outerA = sample(previous, radius);
                const Vertex outerB = sample(next, radius);
                bandBetween(innerA, innerB, outerA, outerB);
                innerA = outerA;
                innerB = outerB;
            }
            previous = next;
        }
        rlEnd();
    }
};
}
#endif
