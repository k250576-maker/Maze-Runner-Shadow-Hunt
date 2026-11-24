#ifndef SHADOW_HUNT_FURNITURE_RENDERER_H
#define SHADOW_HUNT_FURNITURE_RENDERER_H

#include "furniture.h"
#include "raylib.h"
#include <algorithm>
#include <array>
#include <string>

namespace Furniture
{
class Renderer
{
public:
    bool Load(const std::string& assetRoot)
    {
        for (int i = 0; i < static_cast<int>(Kind::Count); ++i)
        {
            const Asset& asset = Assets[i];
            const std::string path = assetRoot + "Furniture/" + asset.filename;
            textures[i] = LoadTexture(path.c_str());
            if (!IsTextureValid(textures[i]) || textures[i].width != asset.width || textures[i].height != asset.height)
            {
                TraceLog(LOG_ERROR, "Furniture texture is missing or has unexpected dimensions: %s", path.c_str());
                Unload();
                return false;
            }
            SetTextureFilter(textures[i], TEXTURE_FILTER_POINT);
        }
        for (int i = 0; i < PlacementCount; ++i) drawOrder[i] = i;
        std::stable_sort(drawOrder.begin(), drawOrder.end(), [](int a, int b) {
            return Placements[a].y < Placements[b].y;
        });

        // Apply the existing world light mask to each prop so it can draw on either side of the player.
        constexpr const char* fragment = R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform sampler2D lightMask;
uniform vec4 propRect;
uniform vec2 worldSize;
out vec4 finalColor;
void main()
{
    vec4 sprite = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec2 world = propRect.xy + fragTexCoord * propRect.zw;
    vec2 uv = world / worldSize;
    vec4 illumination = texture(lightMask, vec2(uv.x, 1.0 - uv.y));
    sprite.rgb = sprite.rgb * illumination.a + illumination.rgb;
    finalColor = sprite;
}
)glsl";
        shader = LoadShaderFromMemory(nullptr, fragment);
        maskLocation = GetShaderLocation(shader, "lightMask");
        rectLocation = GetShaderLocation(shader, "propRect");
        const int sizeLocation = GetShaderLocation(shader, "worldSize");
        if (maskLocation < 0 || rectLocation < 0 || sizeLocation < 0)
        {
            TraceLog(LOG_ERROR, "Could not load furniture lighting shader.");
            Unload();
            return false;
        }
        const float size[] = {Maze::Columns * Maze::CellSize, Maze::Rows * Maze::CellSize};
        SetShaderValue(shader, sizeLocation, size, SHADER_UNIFORM_VEC2);
        return true;
    }

    void Unload()
    {
        for (Texture2D& texture : textures)
        {
            if (texture.id != 0) UnloadTexture(texture);
            texture = {};
        }
        if (shader.id != 0) UnloadShader(shader);
        shader = {};
    }

    void DrawDecals(Rectangle visible) const
    {
        for (const Placement prop : Placements)
        {
            if (IsSolid(prop)) continue;
            const Rectangle rect = DrawBounds(prop);
            if (CheckCollisionRecs(visible, rect))
                DrawTextureV(textures[static_cast<int>(prop.kind)], {rect.x, rect.y}, WHITE);
        }
    }

    void DrawSolids(Rectangle visible, Texture2D mask, float playerY, bool inFront) const
    {
        for (const int index : drawOrder)
        {
            const Placement prop = Placements[index];
            if (!IsSolid(prop) || (prop.y > playerY) != inFront) continue;
            const Rectangle rect = DrawBounds(prop);
            if (!CheckCollisionRecs(visible, rect)) continue;
            const float worldRect[] = {rect.x, rect.y, rect.width, rect.height};
            BeginShaderMode(shader);
            SetShaderValue(shader, rectLocation, worldRect, SHADER_UNIFORM_VEC4);
            SetShaderValueTexture(shader, maskLocation, mask);
            DrawTextureV(textures[static_cast<int>(prop.kind)], {rect.x, rect.y}, WHITE);
            EndShaderMode();
        }
    }

private:
    static Rectangle DrawBounds(Placement prop)
    {
        const Asset& asset = Sprite(prop.kind);
        const int overhang = IsSolid(prop) ? 4 : 0;
        return {static_cast<float>(prop.x - asset.width / 2),
                static_cast<float>(prop.y + overhang - asset.height),
                static_cast<float>(asset.width), static_cast<float>(asset.height)};
    }

    Texture2D textures[static_cast<int>(Kind::Count)]{};
    std::array<int, PlacementCount> drawOrder{};
    Shader shader{};
    int maskLocation = -1;
    int rectLocation = -1;
};
}
#endif
