#include "raylib.h"
// Use this project's headers even when VS Code opens the shared parent folder.
#include "../include/maze_generation.h"
#include "../include/dungeon_art.h"
#include "../include/game_hud.h"
#include "../include/game_audio.h"
#include "../include/parchment_map.h"
#include "../include/player_movement.h"
#include "../include/lighting.h"
#include "../include/torches.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

namespace
{
constexpr int DefaultWindowWidth = 720;
constexpr int DefaultWindowHeight = 608;
constexpr float BaseCameraZoom = 2.0f;
constexpr float CameraFollowSpeed = 10.0f;
constexpr float MaxVisibleWorldSpan = 20.0f * Maze::CellSize;
constexpr Color HudBackground{13, 15, 20, 255};
constexpr int FrameWidth = 16;
constexpr int FrameHeight = 32;

using Movement::Direction;
using Movement::Player;
static_assert(Maze::MaxTorches <= Lighting::MaxStaticLights,
              "Lighting storage must cover every torch.");

struct Viewport
{
    int width;
    int height;
    float hudScale;
    float zoom;
};

Viewport MakeViewport(int width, int height)
{
    const float heightScale = static_cast<float>(height) / DefaultWindowHeight;
    const float hudScale = std::min(heightScale, static_cast<float>(width) / DefaultWindowWidth);
    // Whole-number zoom maps world pixels to equal-sized blocks of screen pixels.
    const float localZoom = std::ceil(std::max(width, height) / MaxVisibleWorldSpan);
    const float zoom = std::max({1.0f, localZoom, std::round(BaseCameraZoom * heightScale)});
    return {width, height, hudScale, zoom};
}

Vector2 ClampCameraCenter(Vector2 center, const Viewport& view)
{
    constexpr float worldWidth = Maze::Columns * Maze::CellSize;
    constexpr float worldHeight = Maze::Rows * Maze::CellSize;
    const float halfWidth = std::min(view.width / view.zoom, worldWidth) * 0.5f;
    const float halfHeight = std::min(view.height / view.zoom, worldHeight) * 0.5f;
    return {
        std::clamp(center.x, halfWidth, worldWidth - halfWidth),
        std::clamp(center.y, halfHeight, worldHeight - halfHeight)};
}

Camera2D FollowCamera(Vector2& center, Vector2 player, const Viewport& view, float dt)
{
    const Vector2 target = ClampCameraCenter(player, view);
    const float blend = 1.0f - std::exp(-CameraFollowSpeed * dt);
    center.x += (target.x - center.x) * blend;
    center.y += (target.y - center.y) * blend;
    center = ClampCameraCenter(center, view);
    const float screenX = view.width * 0.5f;
    const float screenY = view.height * 0.5f;
    // Snap the translation to a screen pixel while retaining the smooth center.
    return {{std::round(screenX - center.x * view.zoom), std::round(screenY - center.y * view.zoom)},
            {0, 0}, 0, view.zoom};
}

Rectangle VisibleDungeon(const Viewport& view, Camera2D camera)
{
    const Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
    return {topLeft.x, topLeft.y, view.width / view.zoom, view.height / view.zoom};
}

bool DirectionKeyDown(Direction direction)
{
    switch (direction)
    {
        case Direction::Left: return IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
        case Direction::Right: return IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
        case Direction::Up: return IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
        case Direction::Down: return IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
        default: return false;
    }
}

Direction PressedDirection()
{
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) return Direction::Left;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) return Direction::Right;
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) return Direction::Up;
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) return Direction::Down;
    return Direction::None;
}

Vector2 FacingVector(Direction direction)
{
    switch (direction)
    {
        case Direction::Left: return {-1, 0};
        case Direction::Right: return {1, 0};
        case Direction::Up: return {0, -1};
        case Direction::Down: return {0, 1};
        default: return {0, 0};
    }
}

std::string AssetRoot()
{
    if (FileExists("assets/characters/player.png")) return "assets/";
    return std::string(GetApplicationDirectory()) + "../assets/";
}

void UpdatePlayer(Player& player, const Maze::Layout& level, float dt, bool controlsOpen)
{
    if (controlsOpen)
    {
        Movement::Update(player, level, dt, Direction::None);
        return;
    }

    Direction direction = PressedDirection();
    // A fresh press takes over immediately. Keep that direction while it is held.
    if (direction == Direction::None && DirectionKeyDown(player.facing)) direction = player.facing;
    if (direction == Direction::None)
    {
        for (Direction candidate : {Direction::Left, Direction::Right, Direction::Up, Direction::Down})
        {
            if (!DirectionKeyDown(candidate)) continue;
            direction = candidate;
            break;
        }
    }
    Movement::Update(player, level, dt, direction);
}

enum class Overlay { None, Controls, Map };

bool HandleOverlayInput(Overlay& overlay, bool controls, bool map, bool escape)
{
    if (escape)
    {
        if (overlay == Overlay::None) return false;
        overlay = Overlay::None;
    }
    else if (controls || map)
    {
        const Overlay requested = controls ? Overlay::Controls : Overlay::Map;
        overlay = overlay == requested ? Overlay::None : requested;
    }
    return true;
}

void DrawPlayer(Texture2D texture, const Player& player)
{
    const int facingRow = player.facing == Direction::Up ? 32
                        : (player.facing == Direction::Left || player.facing == Direction::Right ? 64 : 0);
    Rectangle source{static_cast<float>(player.animationFrame * FrameWidth), static_cast<float>(facingRow),
                     static_cast<float>(FrameWidth), static_cast<float>(FrameHeight)};
    if (player.facing == Direction::Right) source.width = -source.width;
    // 2x scale; transparent padding is below the feet. Lift the frame so
    // the visible feet remain inside the occupied floor cell.
    const Rectangle destination{
        player.position.x - Maze::CellSize * 0.5f,
        player.position.y - Maze::CellSize * 0.5f - 10, 32, 64};
    const Vector2 offsets[] = {{-2, 0}, {2, 0}, {0, -2}, {0, 2}};
    for (Vector2 offset : offsets)
    {
        Rectangle edge = destination;
        edge.x += offset.x;
        edge.y += offset.y;
        DrawTexturePro(texture, source, edge, {0, 0}, 0, Color{235, 198, 118, 190});
    }
    DrawTexturePro(texture, source, destination, {0, 0}, 0, WHITE);
}

}

int main(int argc, char* argv[])
{
    auto seed = static_cast<std::uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    auto soundtrack = GameAudio::DefaultSoundtrack;
    for (int i = 1; i < argc; i += 2)
    {
        const std::string_view option = argv[i];
        if (i + 1 >= argc || (option != "--seed" && option != "--music"))
        {
            std::fputs("Usage: game.exe [--seed 0..4294967295] [--music dungeon002|dark-cavern|deep-dungeon|off]\n", stderr);
            return 2;
        }
        const std::string_view value = argv[i + 1];
        if (option == "--music")
        {
            if (GameAudio::ParseSoundtrack(value, soundtrack)) continue;
            std::fputs("Music must be dungeon002, dark-cavern, deep-dungeon, or off.\n", stderr);
            return 2;
        }
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seed);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        {
            std::fputs("Seed must be an integer from 0 to 4294967295.\n", stderr);
            return 2;
        }
    }

    Maze::GeneratedLevel generated;
    if (!Maze::Generate(seed, generated))
    {
        std::fprintf(stderr, "Maze generation failed reachability validation for seed %lu.\n",
                     static_cast<unsigned long>(seed));
        return 1;
    }
    const Maze::Layout& level = generated.layout;
    TraceLog(LOG_INFO, "Maze seed: %lu", static_cast<unsigned long>(seed));

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DefaultWindowWidth, DefaultWindowHeight, "Maze Runner - Shadow Hunt");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    const std::string root = AssetRoot();
    const Texture2D dungeon = LoadTexture((root + "enviroment/mainlevbuild.png").c_str());
    const Texture2D playerTexture = LoadTexture((root + "characters/player.png").c_str());
    if (dungeon.id == 0 || playerTexture.id == 0 || dungeon.width != 1024
        || dungeon.height != 640 || playerTexture.width != 48 || playerTexture.height != 192)
    {
        TraceLog(LOG_ERROR, "Expected dungeon 1024x640 and player 48x192 in assets/.");
        if (dungeon.id != 0) UnloadTexture(dungeon);
        if (playerTexture.id != 0) UnloadTexture(playerTexture);
        CloseWindow();
        return 1;
    }
    SetTextureFilter(dungeon, TEXTURE_FILTER_POINT);
    SetTextureFilter(playerTexture, TEXTURE_FILTER_POINT);
    Player player{Movement::Center(level.spawn)};
    Lighting::DungeonLight light(level);
    if (!light.Initialize())
    {
        TraceLog(LOG_ERROR, "Could not create dungeon lighting render texture.");
        UnloadTexture(playerTexture);
        UnloadTexture(dungeon);
        CloseWindow();
        return 1;
    }
    Torches::State torchState(level);
    Torches::Fixtures torches(torchState);
    if (!torches.Load(root))
    {
        TraceLog(LOG_ERROR, "Could not load torch animation frames.");
        light.Unload();
        UnloadTexture(playerTexture);
        UnloadTexture(dungeon);
        CloseWindow();
        return 1;
    }
    GameAudio::System audio;
    audio.Initialize(root, soundtrack);
    Exploration::Map exploration(level);
    exploration.Visit(level.spawn);
    Overlay overlay = Overlay::None;
    Vector2 cameraCenter{player.position.x, player.position.y};
    while (!WindowShouldClose())
    {
        if (!HandleOverlayInput(overlay, IsKeyPressed(KEY_I), IsKeyPressed(KEY_M), IsKeyPressed(KEY_ESCAPE))) break;
        // Raylib fits the current monitor and restores the previous window bounds.
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        const Viewport view = MakeViewport(GetScreenWidth(), GetScreenHeight());

        const float dt = GetFrameTime();
        const auto previous = player.position;
        UpdatePlayer(player, level, dt, overlay != Overlay::None);
        audio.Update(std::abs(player.position.x - previous.x) + std::abs(player.position.y - previous.y));

        const Vector2 playerPosition{player.position.x, player.position.y};
        const auto playerCell = Movement::CellAt(player.position);
        exploration.Visit(playerCell);
        torches.UpdateAnimation(dt);
        if (torchState.ActivateNearby(playerPosition))
        {
            Lighting::PointLight sources[Maze::MaxTorches];
            const int count = torches.AppendLights(sources);
            light.SetStaticLights(sources, count);
        }
        light.Update(playerPosition, FacingVector(player.facing));

        BeginDrawing();
        ClearBackground(HudBackground);
        if (view.width > 0 && view.height > 0)
        {
            const Camera2D camera = FollowCamera(cameraCenter, playerPosition, view, dt);
            const Rectangle visible = VisibleDungeon(view, camera);
            BeginMode2D(camera);
            DungeonArt::Draw(dungeon, level, visible, Lighting::AmbientMaskColor);
            torches.DrawBrackets();
            light.DrawDarkness();
            torches.DrawFlames();
            DrawPlayer(playerTexture, player);
            EndMode2D();
            Hud::DrawLocation(view.width, view.hudScale, level.LocationName(playerCell));
            if (overlay == Overlay::Controls) Hud::DrawControls(view.width, view.height, view.hudScale);
            else if (overlay == Overlay::Map)
                Parchment::DrawExpanded(exploration, playerPosition, view.width, view.height, view.hudScale);
            else Parchment::DrawCompact(exploration, playerPosition, playerCell, view.hudScale);
        }
        EndDrawing();
    }
    audio.Shutdown();
    torches.Unload();
    light.Unload();
    UnloadTexture(playerTexture);
    UnloadTexture(dungeon);
    CloseWindow();
    return 0;
}
