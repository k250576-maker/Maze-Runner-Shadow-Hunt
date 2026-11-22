#ifndef SHADOW_HUNT_LIGHTING_H
#define SHADOW_HUNT_LIGHTING_H

#include "raylib.h"
#include "rlgl.h"
#include "maze.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Lighting
{
// Tuning values in maze cells, degrees, and normalized opacity.
constexpr float FlashlightRangeCells = 5.0f;
constexpr float BeamAngleDegrees = 70.0f;
constexpr float AmbientRadiusCells = 1.5f;
constexpr float DarknessOpacity = 1.0f; // No maze structure outside active lights.
constexpr float BeamFullStrengthFraction = 0.60f;
constexpr float RaySpacingDegrees = 2.0f;
constexpr int MaxStaticLights = 16;
constexpr Color CandleTint{255, 195, 110, 255};
constexpr float CandleWallFaceDepth = 6.0f;

struct PointLight
{
    Vector2 position{};
    float radius = 0;
    Color tint = CandleTint;
};

class DungeonLight
{
public:
    bool Initialize()
    {
        mask = LoadRenderTexture(Maze::Columns * Maze::CellSize, Maze::Rows * Maze::CellSize);
        if (mask.id == 0) return false;
        staticMask = LoadRenderTexture(Maze::Columns * Maze::CellSize, Maze::Rows * Maze::CellSize);
        if (staticMask.id == 0) { Unload(); return false; }
        SetTextureFilter(mask.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(staticMask.texture, TEXTURE_FILTER_POINT);
        BeginTextureMode(staticMask);
        ClearBackground(BLACK);
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
        ClearBackground(BLACK);
        BeginLightBlend();
        for (int i = 0; i < staticCount; ++i)
        {
            const PointLight& light = staticLights[i];
            if (light.radius <= 0) continue;
            DrawLightFan(-PI, PI, light.radius, 0.15f, light.position, light.tint);
            DrawWallFaces(light);
        }
        EndBlendMode();
        EndTextureMode();
    }

    // Maze-local pixel coordinates, with (0,0) at the maze's top-left.
    // No movement dependency: any observer can supply a position and direction.
    void Update(Vector2 position, Vector2 direction)
    {
        const float newAngle = direction.x != 0 || direction.y != 0
            ? std::atan2(direction.y, direction.x) : facingAngle;
        const bool changed = position.x != origin.x || position.y != origin.y || newAngle != facingAngle;
        origin = position;
        facingAngle = newAngle;
        if (mask.id == 0) return;
        if (maskReady && !changed) return;
        BeginTextureMode(mask);
        const unsigned char shade = Brightness(0);
        ClearBackground(Color{shade, shade, shade, 255});
        BeginLightBlend();
        if (staticCount > 0)
            DrawTextureRec(staticMask.texture,
                           {0, 0, static_cast<float>(staticMask.texture.width), -static_cast<float>(staticMask.texture.height)},
                           {0, 0}, WHITE);
        DrawLightFan(-PI, PI, AmbientRadiusCells * Maze::CellSize, 0.25f, origin, WHITE);
        const float halfAngle = BeamAngleDegrees * DEG2RAD * 0.5f;
        // MAX blending keeps ambient, beam, and candle overlap non-destructive.
        DrawLightFan(facingAngle - halfAngle, facingAngle + halfAngle,
                     FlashlightRangeCells * Maze::CellSize, BeamFullStrengthFraction, origin, WHITE);
        EndBlendMode();
        EndTextureMode();
        maskReady = true;
    }

    // Player light (beam + ambient) only; candles never affect this query.
    bool IsWorldPositionVisible(Vector2 worldPosition) const
    {
        return IsPlayerLit(worldPosition, true);
    }

    // Future Angel observation must use this: ONLY the directional flashlight,
    // with range/cone/LOS checks, never ambient glow or placed candlelight.
    bool IsWorldPositionObserved(Vector2 worldPosition) const
    {
        return IsPlayerLit(worldPosition, false);
    }

    // Visual illumination is deliberately distinct from active observation.
    bool IsWorldPositionIlluminated(Vector2 worldPosition) const
    {
        if (IsWorldPositionVisible(worldPosition)) return true;
        if (!IsWalkablePosition(worldPosition)) return false;
        for (int i = 0; i < staticCount; ++i)
        {
            const PointLight& light = staticLights[i];
            const float dx = worldPosition.x - light.position.x;
            const float dy = worldPosition.y - light.position.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < light.radius && distance <= CastRay(std::atan2(dy, dx), distance, light.position) + 0.0001f)
                return true;
        }
        return false;
    }

    void DrawDarkness(Vector2 screenOrigin) const
    {
        BeginBlendMode(BLEND_MULTIPLIED);
        DrawTextureRec(mask.texture,
                       {0, 0, static_cast<float>(mask.texture.width), -static_cast<float>(mask.texture.height)},
                       screenOrigin, WHITE);
        EndBlendMode();
    }

private:
    RenderTexture2D mask{};
    RenderTexture2D staticMask{};
    PointLight staticLights[MaxStaticLights]{};
    int staticCount = 0;
    Vector2 origin{};
    float facingAngle = PI * 0.5f;
    bool maskReady = false;

    static bool IsWalkablePosition(Vector2 worldPosition)
    {
        if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.y)) return false;
        const int column = static_cast<int>(std::floor(worldPosition.x / Maze::CellSize));
        const int row = static_cast<int>(std::floor(worldPosition.y / Maze::CellSize));
        return Maze::IsWalkable(column, row);
    }

    bool IsPlayerLit(Vector2 worldPosition, bool includeAmbient) const
    {
        if (!IsWalkablePosition(worldPosition)) return false;
        const float dx = worldPosition.x - origin.x;
        const float dy = worldPosition.y - origin.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < 0.001f) return true;
        const bool ambient = includeAmbient && distance <= AmbientRadiusCells * Maze::CellSize;
        const float dot = (dx * std::cos(facingAngle) + dy * std::sin(facingAngle)) / distance;
        const bool beam = distance <= FlashlightRangeCells * Maze::CellSize
            && dot >= std::cos(BeamAngleDegrees * DEG2RAD * 0.5f) - 0.00001f;
        if (!ambient && !beam) return false;
        return distance <= CastRay(std::atan2(dy, dx), distance, origin) + 0.0001f;
    }

    static void BeginLightBlend()
    {
        // Component-wise maximum: overlapping lights blend without darkening
        // one another or washing out the dark dungeon with repeated additions.
        rlSetBlendFactors(RL_ONE, RL_ONE, RL_MAX);
        BeginBlendMode(BLEND_CUSTOM);
    }

    // Grid traversal: visit crossed cells rather than taking tiny pixel steps.
    // Returns the first wall/boundary distance, capped by the requested range.
    float CastRay(float angle, float range, Vector2 source) const
    {
        int column = static_cast<int>(std::floor(source.x / Maze::CellSize));
        int row = static_cast<int>(std::floor(source.y / Maze::CellSize));
        if (!Maze::IsWalkable(column, row)) return 0;
        const float dx = std::cos(angle);
        const float dy = std::sin(angle);
        const int stepX = dx >= 0 ? 1 : -1;
        const int stepY = dy >= 0 ? 1 : -1;
        const float infinity = std::numeric_limits<float>::infinity();
        const float deltaX = std::fabs(dx) > 0.000001f ? Maze::CellSize / std::fabs(dx) : infinity;
        const float deltaY = std::fabs(dy) > 0.000001f ? Maze::CellSize / std::fabs(dy) : infinity;
        float nextX = std::isfinite(deltaX)
            ? ((column + (stepX > 0 ? 1 : 0)) * Maze::CellSize - source.x) / dx : infinity;
        float nextY = std::isfinite(deltaY)
            ? ((row + (stepY > 0 ? 1 : 0)) * Maze::CellSize - source.y) / dy : infinity;
        while (true)
        {
            const float distance = std::min(nextX, nextY);
            if (distance > range) return range;
            if (std::fabs(nextX - nextY) < 0.0001f)
            {
                // Supercover: a ray may not slip between two touching walls.
                if (!Maze::IsWalkable(column + stepX, row)
                    || !Maze::IsWalkable(column, row + stepY)) return distance;
                column += stepX;
                row += stepY;
                nextX += deltaX;
                nextY += deltaY;
            }
            else if (nextX < nextY)
            {
                column += stepX;
                nextX += deltaX;
            }
            else
            {
                row += stepY;
                nextY += deltaY;
            }
            if (!Maze::IsWalkable(column, row)) return distance;
        }
    }

    static unsigned char Brightness(float light)
    {
        return static_cast<unsigned char>(255 * (1 - DarknessOpacity + DarknessOpacity * light));
    }

    void DrawWallFaces(const PointLight& light) const
    {
        // Light only the near surface of an exposed wall, never the floor on
        // its far side. Small strips stay strictly inside the solid wall cell.
        const Vector2 normals[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        const auto colorAt = [&](Vector2 point)
        {
            const float dx = point.x - light.position.x;
            const float dy = point.y - light.position.y;
            const float strength = std::clamp((1 - std::sqrt(dx * dx + dy * dy) / light.radius) / 0.85f, 0.0f, 1.0f);
            return Color{static_cast<unsigned char>(light.tint.r * strength),
                         static_cast<unsigned char>(light.tint.g * strength),
                         static_cast<unsigned char>(light.tint.b * strength), 255};
        };
        for (int row = 0; row < Maze::Rows; ++row)
        {
            for (int column = 0; column < Maze::Columns; ++column)
            {
                if (Maze::IsWalkable(column, row)) continue;
                const float left = static_cast<float>(column * Maze::CellSize);
                const float top = static_cast<float>(row * Maze::CellSize);
                for (int side = 0; side < 4; ++side)
                {
                    const Vector2 normal = normals[side];
                    if (!Maze::IsWalkable(column + static_cast<int>(normal.x), row + static_cast<int>(normal.y))) continue;
                    const Vector2 face{left + (side == 1 ? Maze::CellSize : 0),
                                       top + (side == 3 ? Maze::CellSize : 0)};
                    if ((light.position.x - face.x) * normal.x + (light.position.y - face.y) * normal.y <= 0) continue;
                    constexpr int Segments = 4;
                    constexpr float length = static_cast<float>(Maze::CellSize) / Segments;
                    for (int segment = 0; segment < Segments; ++segment)
                    {
                        Vector2 sample{face.x + (side >= 2 ? (segment + 0.5f) * length : 0) + normal.x * 0.01f,
                                       face.y + (side < 2 ? (segment + 0.5f) * length : 0) + normal.y * 0.01f};
                        const float dx = sample.x - light.position.x;
                        const float dy = sample.y - light.position.y;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance >= light.radius || distance > CastRay(std::atan2(dy, dx), distance, light.position) + 0.0001f) continue;
                        const Rectangle strip{
                            face.x + (side >= 2 ? segment * length : 0) - (side == 1 ? CandleWallFaceDepth : 0),
                            face.y + (side < 2 ? segment * length : 0) - (side == 3 ? CandleWallFaceDepth : 0),
                            side < 2 ? CandleWallFaceDepth : length,
                            side < 2 ? length : CandleWallFaceDepth};
                        DrawRectangleGradientEx(strip,
                            colorAt({strip.x, strip.y}), colorAt({strip.x, strip.y + strip.height}),
                            colorAt({strip.x + strip.width, strip.y + strip.height}),
                            colorAt({strip.x + strip.width, strip.y}));
                    }
                }
            }
        }
    }

    void DrawLightFan(float startAngle, float endAngle, float range, float fullStrengthFraction,
                      Vector2 source, Color tint) const
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
        for (int row = 0; row <= Maze::Rows; ++row)
        {
            for (int column = 0; column <= Maze::Columns; ++column)
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
        const float brightRange = range * fullStrengthFraction;
        const auto point = [&](float angle, float distance)
        {
            return Vector2{source.x + std::cos(angle) * distance,
                           source.y + std::sin(angle) * distance};
        };
        const auto vertex = [tint](Vector2 position, unsigned char shade)
        {
            rlColor4ub(static_cast<unsigned char>(shade * tint.r / 255),
                       static_cast<unsigned char>(shade * tint.g / 255),
                       static_cast<unsigned char>(shade * tint.b / 255), 255);
            rlVertex2f(position.x, position.y);
        };
        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);
        for (int i = 0; i + 1 < count; ++i)
        {
            const float distanceA = CastRay(angles[i], range, source);
            const float distanceB = CastRay(angles[i + 1], range, source);
            const unsigned char shadeA = Brightness(1 - std::max(0.0f, distanceA - brightRange) / (range - brightRange));
            const unsigned char shadeB = Brightness(1 - std::max(0.0f, distanceB - brightRange) / (range - brightRange));
            const Vector2 innerA = point(angles[i], std::min(distanceA, brightRange));
            const Vector2 innerB = point(angles[i + 1], std::min(distanceB, brightRange));
            const Vector2 outerA = point(angles[i], distanceA);
            const Vector2 outerB = point(angles[i + 1], distanceB);
            // Bright inner triangle, then a fading outer band. This keeps the
            // already-dark artwork readable without custom shaders.
            vertex(source, 255); vertex(innerB, 255); vertex(innerA, 255);
            vertex(innerA, 255); vertex(innerB, 255); vertex(outerB, shadeB);
            vertex(innerA, 255); vertex(outerB, shadeB); vertex(outerA, shadeA);
        }
        rlEnd();
    }
};
}
#endif
