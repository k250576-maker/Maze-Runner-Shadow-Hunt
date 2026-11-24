#include "raylib.h"
#include "../include/maze_generation.h"
#include "../include/dungeon_art.h"
#include "../include/game_hud.h"
#include "../include/game_audio.h"
#include "../include/main_menu.h"
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
constexpr int DefaultWindowWidth  = 720;
constexpr int DefaultWindowHeight = 608;
constexpr float BaseCameraZoom    = 2.0f;
constexpr float CameraFollowSpeed = 10.0f;
constexpr float MaxVisibleWorldSpan = 20.0f * Maze::CellSize;
constexpr Color HudBackground{13, 15, 20, 255};
constexpr int FrameWidth  = 16;
constexpr int FrameHeight = 32;

using Movement::Direction;
using Movement::Player;
static_assert(Maze::MaxTorches <= Lighting::MaxStaticLights,
              "Lighting storage must cover every torch.");

struct Viewport
{
    int   width;
    int   height;
    float hudScale;
    float zoom;
};

Viewport MakeViewport(int width, int height)
{
    const float heightScale = static_cast<float>(height) / DefaultWindowHeight;
    const float hudScale    = std::min(heightScale, static_cast<float>(width) / DefaultWindowWidth);
    const float localZoom   = std::ceil(std::max(width, height) / MaxVisibleWorldSpan);
    const float zoom        = std::max({1.0f, localZoom, std::round(BaseCameraZoom * heightScale)});
    return {width, height, hudScale, zoom};
}

Vector2 ClampCameraCenter(Vector2 center, const Viewport& view)
{
    constexpr float worldWidth  = Maze::Columns * Maze::CellSize;
    constexpr float worldHeight = Maze::Rows    * Maze::CellSize;
    const float halfWidth  = std::min(view.width  / view.zoom, worldWidth)  * 0.5f;
    const float halfHeight = std::min(view.height / view.zoom, worldHeight) * 0.5f;
    return {
        std::clamp(center.x, halfWidth,  worldWidth  - halfWidth),
        std::clamp(center.y, halfHeight, worldHeight - halfHeight)};
}

Camera2D FollowCamera(Vector2& center, Vector2 player, const Viewport& view, float dt)
{
    const Vector2 target = ClampCameraCenter(player, view);
    const float blend = 1.0f - std::exp(-CameraFollowSpeed * dt);
    center.x += (target.x - center.x) * blend;
    center.y += (target.y - center.y) * blend;
    center = ClampCameraCenter(center, view);
    const float screenX = view.width  * 0.5f;
    const float screenY = view.height * 0.5f;
    return {{std::round(screenX - center.x * view.zoom),
             std::round(screenY - center.y * view.zoom)},
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
        case Direction::Left:  return IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
        case Direction::Right: return IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
        case Direction::Up:    return IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
        case Direction::Down:  return IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
        default:               return false;
    }
}

Direction PressedDirection()
{
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  return Direction::Left;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) return Direction::Right;
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    return Direction::Up;
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  return Direction::Down;
    return Direction::None;
}

Vector2 FacingVector(Direction direction)
{
    switch (direction)
    {
        case Direction::Left:  return {-1,  0};
        case Direction::Right: return { 1,  0};
        case Direction::Up:    return { 0, -1};
        case Direction::Down:  return { 0,  1};
        default:               return { 0,  0};
    }
}

std::string AssetRoot()
{
    if (FileExists("assets/characters/player.png")) return "assets/";
    return std::string(GetApplicationDirectory()) + "../assets/";
}

enum class GameOverlay { None, Controls, Map };

void DrawPlayer(Texture2D texture, const Player& player)
{
    const int facingRow = player.facing == Direction::Up    ? 32
                        : (player.facing == Direction::Left
                           || player.facing == Direction::Right ? 64 : 0);
    Rectangle source{static_cast<float>(player.animationFrame * FrameWidth),
                     static_cast<float>(facingRow),
                     static_cast<float>(FrameWidth),
                     static_cast<float>(FrameHeight)};
    if (player.facing == Direction::Right) source.width = -source.width;
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

// All per-run gameplay state. Heap-allocated so that internal references to
// generated.layout stay valid when moving between menu and game.
struct PlaySession
{
    Maze::GeneratedLevel generated{};
    Player               player{};
    Lighting::DungeonLight light;
    Torches::State         torchState;
    Torches::Fixtures      torches;
    Exploration::Map       exploration;
    GameOverlay            overlay    = GameOverlay::None;
    Vector2                cameraCenter{};

    // Constructors store references into generated.layout / torchState.
    // Maze::Generate() must be called before Initialize().
    PlaySession()
        : light(generated.layout)
        , torchState(generated.layout)
        , torches(torchState)
        , exploration(generated.layout)
    {}

    PlaySession(const PlaySession&) = delete;
    PlaySession& operator=(const PlaySession&) = delete;

    bool Initialize(const std::string& assetRoot)
    {
        if (!light.Initialize())    return false;
        if (!torches.Load(assetRoot)) { light.Unload(); return false; }
        player        = {Movement::Center(generated.layout.spawn)};
        cameraCenter  = {player.position.x, player.position.y};
        exploration.Visit(generated.layout.spawn);
        return true;
    }

    void Teardown()
    {
        torches.Unload();
        light.Unload();
    }
};

} // anonymous namespace

int main(int argc, char* argv[])
{
    auto seed       = static_cast<std::uint32_t>(
                          std::chrono::system_clock::now().time_since_epoch().count());
    auto soundtrack = GameAudio::DefaultSoundtrack;

    for (int i = 1; i < argc; i += 2)
    {
        const std::string_view option = argv[i];
        if (i + 1 >= argc || (option != "--seed" && option != "--music"))
        {
            std::fputs("Usage: game.exe [--seed 0..4294967295] "
                       "[--music dungeon002|dark-cavern|deep-dungeon|off]\n", stderr);
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

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DefaultWindowWidth, DefaultWindowHeight, "Maze Runner - Shadow Hunt");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    const std::string root = AssetRoot();

    // Shared textures used by both menu (player sheet) and gameplay.
    const Texture2D dungeonTex = LoadTexture((root + "enviroment/mainlevbuild.png").c_str());
    const Texture2D playerTex  = LoadTexture((root + "characters/player.png").c_str());
    if (dungeonTex.id == 0 || playerTex.id == 0
        || dungeonTex.width != 1024 || dungeonTex.height != 640
        || playerTex.width  != 48   || playerTex.height  != 192)
    {
        TraceLog(LOG_ERROR, "Expected dungeon 1024x640 and player 48x192 in assets/.");
        if (dungeonTex.id != 0) UnloadTexture(dungeonTex);
        if (playerTex.id  != 0) UnloadTexture(playerTex);
        CloseWindow();
        return 1;
    }
    SetTextureFilter(dungeonTex, TEXTURE_FILTER_POINT);
    SetTextureFilter(playerTex,  TEXTURE_FILTER_POINT);

    GameAudio::System audio;
    audio.Initialize(root, soundtrack);

    // ------------------------------------------------------------------ //
    //  Application state machine: Menu ↔ Playing                         //
    // ------------------------------------------------------------------ //
    enum class AppState { Menu, Playing };
    AppState       appState  = AppState::Menu;
    MainMenu::State menuState{};
    PlaySession*   session   = nullptr;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        const Viewport view = MakeViewport(GetScreenWidth(), GetScreenHeight());
        const float dt      = GetFrameTime();

        // ---- MENU STATE ---- //
        if (appState == AppState::Menu)
        {
            menuState.time += dt;
            MainMenu::Item confirmed{};
            if (MainMenu::HandleInput(menuState, confirmed))
            {
                if (confirmed == MainMenu::Item::Quit)
                    break;

                // Start Game: generate maze, initialise session, transition audio.
                if (confirmed == MainMenu::Item::StartGame)
                {
                    session = new PlaySession();
                    if (!Maze::Generate(seed, session->generated))
                    {
                        std::fprintf(stderr,
                            "Maze generation failed reachability validation for seed %lu.\n",
                            static_cast<unsigned long>(seed));
                        delete session;
                        session = nullptr;
                        // Stay on the menu.
                    }
                    else if (!session->Initialize(root))
                    {
                        TraceLog(LOG_ERROR, "Could not initialise game session resources.");
                        delete session;
                        session = nullptr;
                    }
                    else
                    {
                        TraceLog(LOG_INFO, "Maze seed: %lu",
                                 static_cast<unsigned long>(seed));
                        audio.TransitionToGame();
                        appState = AppState::Playing;
                    }
                }
            }

            // Keep the menu music stream alive.
            audio.Update(0.0f);

            BeginDrawing();
            if (view.width > 0 && view.height > 0)
            {
                if (menuState.controlsOpen)
                    MainMenu::DrawControlsScreen(view.width, view.height, view.hudScale);
                else
                    MainMenu::Draw(view.width, view.height, menuState, view.hudScale);
            }
            EndDrawing();
            continue;
        }

        // ---- PLAYING STATE ---- //
        // ESC with no overlay → return to menu.
        if (IsKeyPressed(KEY_ESCAPE))
        {
            if (session->overlay != GameOverlay::None)
            {
                session->overlay = GameOverlay::None;
            }
            else
            {
                session->Teardown();
                delete session;
                session = nullptr;
                // Regenerate a fresh seed so the next game is different.
                seed = static_cast<std::uint32_t>(
                    std::chrono::system_clock::now().time_since_epoch().count());
                audio.TransitionToMenu();
                appState  = AppState::Menu;
                menuState = MainMenu::State{};
                continue;
            }
        }

        // I / M overlays (mirror original behaviour).
        if (IsKeyPressed(KEY_I))
        {
            session->overlay = (session->overlay == GameOverlay::Controls)
                               ? GameOverlay::None : GameOverlay::Controls;
        }
        else if (IsKeyPressed(KEY_M))
        {
            session->overlay = (session->overlay == GameOverlay::Map)
                               ? GameOverlay::None : GameOverlay::Map;
        }

        const Maze::Layout& level = session->generated.layout;
        const bool overlayOpen    = (session->overlay != GameOverlay::None);

        // Player movement — blocked while an overlay is open.
        const auto previous = session->player.position;
        if (!overlayOpen)
        {
            Direction direction = PressedDirection();
            if (direction == Direction::None && DirectionKeyDown(session->player.facing))
                direction = session->player.facing;
            if (direction == Direction::None)
            {
                for (Direction c : {Direction::Left, Direction::Right,
                                    Direction::Up,   Direction::Down})
                {
                    if (!DirectionKeyDown(c)) continue;
                    direction = c;
                    break;
                }
            }
            Movement::Update(session->player, level, dt, direction);
        }
        else
        {
            Movement::Update(session->player, level, dt, Direction::None);
        }

        const float moved = std::abs(session->player.position.x - previous.x)
                          + std::abs(session->player.position.y - previous.y);
        audio.Update(moved);

        const Vector2 playerPos{session->player.position.x, session->player.position.y};
        const auto    playerCell = Movement::CellAt(session->player.position);
        session->exploration.Visit(playerCell);
        session->torches.UpdateAnimation(dt);
        if (session->torchState.ActivateNearby(playerPos))
        {
            Lighting::PointLight sources[Maze::MaxTorches];
            const int count = session->torches.AppendLights(sources);
            session->light.SetStaticLights(sources, count);
        }
        session->light.Update(playerPos, FacingVector(session->player.facing));

        BeginDrawing();
        ClearBackground(HudBackground);
        if (view.width > 0 && view.height > 0)
        {
            const Camera2D camera = FollowCamera(session->cameraCenter, playerPos, view, dt);
            const Rectangle visible = VisibleDungeon(view, camera);
            BeginMode2D(camera);
            DungeonArt::Draw(dungeonTex, level, visible, Lighting::AmbientMaskColor);
            session->torches.DrawBrackets();
            session->light.DrawDarkness();
            session->torches.DrawFlames();
            DrawPlayer(playerTex, session->player);
            EndMode2D();
            Hud::DrawLocation(view.width, view.hudScale, level.LocationName(playerCell));
            if (session->overlay == GameOverlay::Controls)
                Hud::DrawControls(view.width, view.height, view.hudScale);
            else if (session->overlay == GameOverlay::Map)
                Parchment::DrawExpanded(session->exploration, playerPos,
                                        view.width, view.height, view.hudScale);
            else
                Parchment::DrawCompact(session->exploration, playerPos,
                                       playerCell, view.hudScale);
        }
        EndDrawing();
    }

    // ---- Cleanup ---- //
    if (session)
    {
        session->Teardown();
        delete session;
    }
    audio.Shutdown();
    UnloadTexture(playerTex);
    UnloadTexture(dungeonTex);
    CloseWindow();
    return 0;
}
