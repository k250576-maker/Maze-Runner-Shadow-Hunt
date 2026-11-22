#include "raylib.h"
#include "maze.h"
#include "player_movement.h"
#include "lighting.h"
#include "candles.h"
#include "candle_renderer.h"
#include <string>

namespace
{
constexpr int Margin = 24;
constexpr int MazeTop = 80;
constexpr int ScreenWidth = Maze::Columns * Maze::CellSize + Margin * 2;
constexpr int ScreenHeight = MazeTop + Maze::Rows * Maze::CellSize + 48;
// Individually inspected swatches, not an atlas-wide grid.
constexpr Rectangle FloorSource{736, 208, 32, 32};
constexpr Rectangle WallSource{736, 272, 32, 32};
constexpr int FrameWidth = 16;
constexpr int FrameHeight = 32;

using Movement::Direction;
using Movement::Player;
static_assert(Candles::StartingInventory <= Lighting::MaxStaticLights,
              "Lighting storage must cover all placeable candles.");

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

void UpdatePlayer(Player& player, float dt)
{
    const Direction pressed = PressedDirection();
    Direction held = Direction::None;
    // Favor a fresh press, then a buffered turn, then the current direction.
    if (pressed != Direction::None) held = pressed;
    else if (DirectionKeyDown(player.queued)) held = player.queued;
    else if (DirectionKeyDown(player.facing)) held = player.facing;
    else
    {
        for (Direction direction : {Direction::Left, Direction::Right, Direction::Up, Direction::Down})
            if (DirectionKeyDown(direction)) { held = direction; break; }
    }
    Movement::Update(player, dt, held, pressed);
}

void DrawMaze(Texture2D dungeon)
{
    for (int row = 0; row < Maze::Rows; ++row)
    {
        for (int column = 0; column < Maze::Columns; ++column)
        {
            const bool floor = Maze::IsWalkable(column, row);
            const Rectangle destination{
                static_cast<float>(Margin + column * Maze::CellSize),
                static_cast<float>(MazeTop + row * Maze::CellSize),
                static_cast<float>(Maze::CellSize), static_cast<float>(Maze::CellSize)};
            DrawTexturePro(dungeon, floor ? FloorSource : WallSource, destination,
                           {0, 0}, 0, floor ? Color{170, 180, 195, 255} : WHITE);
            if (!floor) DrawRectangleLinesEx(destination, 1, Color{81, 72, 62, 255});
        }
    }
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
        Margin + player.visual.x - Maze::CellSize * 0.5f,
        MazeTop + player.visual.y - Maze::CellSize * 0.5f - 10, 32, 64};
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

int main()
{
    InitWindow(ScreenWidth, ScreenHeight, "Maze Runner - Shadow Hunt | Asset Prototype");
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
    Player player;
    Lighting::DungeonLight light;
    if (!light.Initialize())
    {
        TraceLog(LOG_ERROR, "Could not create dungeon lighting render texture.");
        UnloadTexture(playerTexture);
        UnloadTexture(dungeon);
        CloseWindow();
        return 1;
    }
    Candles::State candles;
    Candles::Renderer candleRenderer;
    if (!candleRenderer.Load(root))
    {
        TraceLog(LOG_ERROR, "Could not load candleA animation frames.");
        light.Unload();
        UnloadTexture(playerTexture);
        UnloadTexture(dungeon);
        CloseWindow();
        return 1;
    }
    const char* candleFeedback = "";
    float feedbackTime = 0;
    while (!WindowShouldClose())
    {
        const float dt = GetFrameTime();
        UpdatePlayer(player, dt);
        candles.Update(dt);
        feedbackTime = std::max(0.0f, feedbackTime - dt);
        if (IsKeyPressed(KEY_E))
        {
            // During a step, 'current' is the last cell center reached.
            const Candles::PlacementResult result = candles.TryPlace(player.current.column, player.current.row);
            switch (result)
            {
                case Candles::PlacementResult::Placed: candleFeedback = "Candle placed"; break;
                case Candles::PlacementResult::Duplicate: candleFeedback = "A candle is already here"; break;
                case Candles::PlacementResult::Empty: candleFeedback = "No candles remaining"; break;
                case Candles::PlacementResult::InvalidCell: candleFeedback = "Cannot place a candle here"; break;
            }
            feedbackTime = 2.0f;
            if (result == Candles::PlacementResult::Placed)
            {
                Lighting::PointLight sources[Candles::StartingInventory];
                for (int i = 0; i < candles.count; ++i)
                    sources[i] = {{(candles.placed[i].column + 0.5f) * Maze::CellSize,
                                   (candles.placed[i].row + 0.5f) * Maze::CellSize},
                                  Candles::GlowRadiusCells * Maze::CellSize, Lighting::CandleTint};
                light.SetStaticLights(sources, candles.count);
            }
        }
        light.Update({player.visual.x, player.visual.y}, FacingVector(player.facing));
        BeginDrawing();
        ClearBackground(Color{13, 15, 20, 255});
        DrawText("MAZE RUNNER - SHADOW HUNT", Margin, 18, 24, Color{232, 211, 165, 255});
        DrawText("Dungeon asset prototype | Fixed maze", Margin, 49, 16, LIGHTGRAY);
        DrawText(TextFormat("Candles: %d / %d", candles.Remaining(), Candles::StartingInventory),
                 ScreenWidth - 170, 49, 16, Color{232, 211, 165, 255});
        if (feedbackTime > 0) DrawText(candleFeedback, Margin, 66, 12, Color{232, 211, 165, 255});
        DrawMaze(dungeon);
        light.DrawDarkness({Margin, MazeTop});
        candleRenderer.Draw(candles, {Margin, MazeTop});
        DrawPlayer(playerTexture, player);
        DrawText("Move: WASD / Arrows     E: Place candle     Esc: Quit",
                 Margin, ScreenHeight - 30, 16, LIGHTGRAY);
        EndDrawing();
    }
    candleRenderer.Unload();
    light.Unload();
    UnloadTexture(playerTexture);
    UnloadTexture(dungeon);
    CloseWindow();
    return 0;
}
