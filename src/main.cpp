#include "raylib.h"
#include "../include/maze_generation.h"
#include "../include/dungeon_art.h"
#include "../include/bats.h"
#include "../include/enemy_art.h"
#include "../include/enemy_debug.h"
#include "../include/physical_enemies.h"
#include "../include/game_hud.h"
#include "../include/game_audio.h"
#include "../include/game_over.h"
#include "../include/main_menu.h"
#include "../include/pause_menu.h"
#include "../include/escape_objective.h"
#include "../include/victory_screen.h"
#include "../include/dungeon_exploration.h"
#include "../include/exploration_art.h"
#include "../include/audio_settings_ui.h"
#include "../include/parchment_map.h"
#include "../include/player_movement.h"
#include "../include/run_progress.h"
#include "../include/lighting.h"
#include "../include/torches.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
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
constexpr float PlayerVisualScale = 0.80f; // Multiplies the existing 2x sprite scale.

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

enum class GameOverlay { None, Controls, Map, Book, Pause, Settings };

void DrawInteractionPrompt(const char* prompt,const Viewport& view)
{
    if(!prompt || !*prompt)return;
    const int size=std::clamp(Hud::Pixels(view.hudScale,22),18,30);
    const int available=std::max(60,view.width-48);
    std::string first=prompt,second;
    if(MeasureText(prompt,size)>available)
    {
        auto split=first.find_last_of(' ');
        while(split!=std::string::npos && MeasureText(first.substr(0,split).c_str(),size)>available)
            split=split==0?std::string::npos:first.find_last_of(' ',split-1);
        if(split!=std::string::npos){second=first.substr(split+1);first.resize(split);}
    }
    const int lines=second.empty()?1:2;
    const int width=std::min(view.width-24,std::max(MeasureText(first.c_str(),size),MeasureText(second.c_str(),size))+28);
    const int height=lines*(size+6)+16;
    const Rectangle panel{(view.width-width)/2.0f,static_cast<float>(view.height-100-height),static_cast<float>(width),static_cast<float>(height)};
    DrawRectangleRounded(panel,0.18f,5,{12,10,15,235});
    DrawRectangleRoundedLines(panel,0.18f,5,Hud::Brass);
    Hud::DrawCenteredText(first.c_str(),view.width/2,static_cast<int>(panel.y)+8,size,Hud::Gold);
    if(!second.empty())Hud::DrawCenteredText(second.c_str(),view.width/2,static_cast<int>(panel.y)+14+size,size,Hud::Gold);
}

void DrawMelee(const Maze::Layout& level, Vector2 player, const Enemies::Melee& attack)
{
    if (attack.effect <= 0) return;
    const float progress = 1 - attack.effect / Enemies::MeleeEffect;
    const float facing = std::atan2(attack.direction.y, attack.direction.x);
    const unsigned char alpha = static_cast<unsigned char>(210 * (1 - progress));
    const Color tint{255, 216, 141, alpha};
    const float end = std::min(1.0f, -0.7f + 2.4f * progress);
    const float start = std::max(-1.0f, end - 0.92f);
    Vector2 previousInner{}, previousOuter{};
    constexpr int segments = 14;
    for (int i = 0; i <= segments; ++i)
    {
        const float t = float(i) / segments;
        const float angle = facing + (start + (end-start)*t) * Enemies::MeleeHalfArc;
        const Vector2 direction{std::cos(angle), std::sin(angle)};
        const float wall = Maze::FirstWallDistance(level, player, direction, Enemies::MeleeReach);
        const float reach = std::clamp(wall - 1, 0.0f, Enemies::MeleeReach);
        const float inner = reach * (1 - 0.25f * std::sin(3.1415927f * t));
        const Vector2 a{player.x + direction.x*inner, player.y + direction.y*inner};
        const Vector2 b{player.x + direction.x*reach, player.y + direction.y*reach};
        if (i > 0)
        {
            DrawTriangle(previousInner, b, previousOuter, tint);
            DrawTriangle(previousInner, a, b, tint);
        }
        previousInner = a;
        previousOuter = b;
    }
}

void DrawSouls(const Run::Souls& souls, Rectangle visible, Vector2 player, const Maze::Layout& level, double elapsed)
{
    const int left = std::max(0, int(visible.x / Maze::CellSize));
    const int top = std::max(0, int(visible.y / Maze::CellSize));
    const int right = std::min(Maze::Columns-1, int((visible.x+visible.width) / Maze::CellSize));
    const int bottom = std::min(Maze::Rows-1, int((visible.y+visible.height) / Maze::CellSize));
    for (int row = top; row <= bottom; ++row)
        for (int column = left; column <= right; ++column)
        {
            const int id = Enemies::CellId({column,row});
            if (!souls.At(id)) continue;
            Vector2 point = Enemies::Center(id);
            if (!Maze::HasLineOfSight(level, player, point)) continue;
            point.y -= 8 + 2.5f * std::sin(float(elapsed) * 4 + id);
            DrawCircleGradient(point,12,{107,229,203,120},BLANK);
            DrawPoly(point,4,5,0,{142,241,209,240});
            DrawPoly({point.x,point.y-1},4,2,0,{237,251,208,255});
        }
}

void DrawPlayer(Texture2D texture, const Player& player, Color tint)
{
    const int facingRow = player.facing == Direction::Up    ? 32
                        : (player.facing == Direction::Left
                           || player.facing == Direction::Right ? 64 : 0);
    Rectangle source{static_cast<float>(player.animationFrame * FrameWidth),
                     static_cast<float>(facingRow),
                     static_cast<float>(FrameWidth),
                     static_cast<float>(FrameHeight)};
    if (player.facing == Direction::Right) source.width = -source.width;
    const float pixelScale=2.0f*PlayerVisualScale;
    // Neutral frames end at source row 19, with transparent padding below.
    // Retain the old foot anchor (world Y + 12) when changing visual size.
    constexpr float feetRow=19.0f;
    constexpr float groundOffset=12.0f;
    const Rectangle destination{
        player.position.x - FrameWidth*pixelScale*0.5f,
        player.position.y + groundOffset - feetRow*pixelScale,
        FrameWidth*pixelScale,FrameHeight*pixelScale};
    const Vector2 offsets[] = {{-2, 0}, {2, 0}, {0, -2}, {0, 2}};
    for (Vector2 offset : offsets)
    {
        Rectangle edge = destination;
        edge.x += offset.x*PlayerVisualScale;
        edge.y += offset.y*PlayerVisualScale;
        DrawTexturePro(texture, source, edge, {0, 0}, 0, Color{235, 198, 118, 190});
    }
    DrawTexturePro(texture, source, destination, {0, 0}, 0, tint);
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
    Enemies::Manager       enemies;
    Enemies::PhysicalEnemies physical;
    Bats::Flock            bats;
    Enemies::Health        health;
    Run::Clock             clock;
    Run::Souls             souls;
    Escape::Objective      escape;
    Dungeon::Exploration   dungeon;
    unsigned               defeated=0;
    int                    victoryChoice=0;
    float                  shownHealth = 1;
    Enemies::Melee         melee;
    GameOverlay            overlay    = GameOverlay::None;
    PauseMenu::State        pauseMenu;
    bool                   controlsFromPause = false;
    GameOver::Choice       deathChoice = GameOver::Choice::Restart;
    int                    bookLeaf = 0;
    float                  enemyAnimation = 0;
#ifndef NDEBUG
    bool                   debugAI = false;
#endif
    Vector2                cameraCenter{};
    bool                   resourcesReady = false;

    // Constructors store references into generated.layout / torchState.
    // Maze::Generate() must be called before Initialize().
    PlaySession()
        : light(generated.layout)
        , torchState(generated.layout)
        , torches(torchState)
        , exploration(generated.layout)
        , enemies(generated.layout, torchState)
        , physical(generated.layout)
        , bats(generated.layout)
        , escape(generated.layout)
        , dungeon(generated.layout)
    {}

    PlaySession(const PlaySession&) = delete;
    PlaySession& operator=(const PlaySession&) = delete;

    bool Initialize(const std::string& assetRoot)
    {
        if (!escape.Initialize()) return false;
        if (!dungeon.Initialize(generatedSeed)) return false;
        escape.SetUnlocked(false);
        if (!enemies.Spawn())       return false;
        if (!light.Initialize())    return false;
        if (!torches.Load(assetRoot)) { light.Unload(); return false; }
        player        = {Movement::Center(generated.layout.spawn)};
        cameraCenter  = {player.position.x, player.position.y};
        exploration.Visit(generated.layout.spawn);
        RefreshLights();
        resourcesReady = true;
        return true;
    }
    unsigned generatedSeed=0;

    void RefreshLights()
    {
        Lighting::PointLight sources[Maze::MaxTorches+1];
        int count=torches.AppendLights(sources);
        sources[count++]=escape.Light();
        light.SetStaticLights(sources,count);
    }

    ~PlaySession()
    {
        if (!resourcesReady) return;
        torches.Unload();
        light.Unload();
    }
};

std::unique_ptr<PlaySession> StartSession(const std::string& root, std::uint32_t seed)
{
    auto session = std::make_unique<PlaySession>();
    if (!Maze::Generate(seed, session->generated))
    {
        TraceLog(LOG_ERROR, "Maze generation failed reachability validation for seed %lu.",
                 static_cast<unsigned long>(seed));
        return {};
    }
    session->generatedSeed=seed;
    if (!session->Initialize(root))
    {
        TraceLog(LOG_ERROR, "Could not initialize game session resources.");
        return {};
    }
    session->physical.SetSeed(seed);
    session->clock.seed = seed;
    session->bats.Place(seed);
    TraceLog(LOG_INFO, "Maze seed: %lu", static_cast<unsigned long>(seed));
    return session;
}

} // anonymous namespace

int main(int argc, char* argv[])
{
    auto seed       = static_cast<std::uint32_t>(
                          std::chrono::system_clock::now().time_since_epoch().count());
    auto soundtrack = GameAudio::DefaultSoundtrack;
    bool seedSpecified = false;

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
        seedSpecified = true;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DefaultWindowWidth, DefaultWindowHeight, "Maze Runner - Shadow Hunt");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    const std::string root = AssetRoot();

    // The menu and gameplay both use the dungeon atlas and player sheet.
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
    Enemies::Sprites enemySprites;
    if (!enemySprites.Load(root))
    {
        TraceLog(LOG_ERROR, "Could not load the Angel and Ghost artwork.");
        UnloadTexture(playerTex);
        UnloadTexture(dungeonTex);
        CloseWindow();
        return 1;
    }
    Bats::Art batArt;
    batArt.Load(root);
    Dungeon::Art explorationArt;explorationArt.Load(root);
    Mobcyclopedia::Type bookType;
    bookType.Load();
    MenuBackdrop::Castle menuBackdrop;
    menuBackdrop.LoadTorches(root);

    GameAudio::System audio;
    // AssetRoot may be relative to the workspace or absolute beside build/.
    const std::string localSettingsPath=std::string(GetApplicationDirectory())+"../settings.cfg";
    auto audioPreferences=AudioPreferences::Load(localSettingsPath);
    AudioSettings::State audioPanel;
    audio.ApplySettings(audioPreferences);
    audio.Initialize(root, soundtrack);
    const auto settingsInput=[&]()
    {
        const auto result=AudioSettings::Input(audioPanel,audioPreferences);
        if(result.changed)audio.ApplySettings(audioPreferences);
        if(result.save)
        {
            audioPanel.saveFailed=!AudioPreferences::Save(localSettingsPath,audioPreferences);
            audioPanel.dirty=audioPanel.saveFailed;
            if(audioPanel.saveFailed)TraceLog(LOG_WARNING,"Could not save audio settings: %s",localSettingsPath.c_str());
        }
        return result.closed;
    };

    // ------------------------------------------------------------------ //
    //  Application state machine: Menu ↔ Playing                         //
    // ------------------------------------------------------------------ //
    enum class AppState { Menu, Playing };
    AppState       appState  = AppState::Menu;
    MainMenu::State menuState{};
    std::unique_ptr<PlaySession> session;
    const auto drawMenuFrame = [&](const Viewport& view) {
        audio.Update(0.0f);
        BeginDrawing();
        if (view.width > 0 && view.height > 0)
            MainMenu::Draw(view.width, view.height, menuState, menuBackdrop, dungeonTex, playerTex, enemySprites, bookType);
        if(menuState.overlay==MainMenu::Overlay::Settings)
            AudioSettings::Draw(audioPanel,audioPreferences,view.width,view.height);
        EndDrawing();
    };
    const auto returnToMenu = [&](const Viewport& view) {
        session.reset();
        if (!seedSpecified)
            seed = static_cast<std::uint32_t>(
                std::chrono::system_clock::now().time_since_epoch().count());
        audio.TransitionToMenu();
        audio.SetPaused(false);
        UI::ScreenChanged();
        appState = AppState::Menu;
        menuState = {};
        // EndDrawing polls input so the confirmation key is consumed before the menu accepts keys.
        drawMenuFrame(view);
    };

    bool quitRequested=false;
    while (!WindowShouldClose() && !quitRequested)
    {
        UI::BeginFrame();
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        const Viewport view = MakeViewport(GetScreenWidth(), GetScreenHeight());
        const float dt      = GetFrameTime();

        // ---- MENU STATE ---- //
        if (appState == AppState::Menu)
        {
            menuState.time += dt;
            if(menuState.overlay==MainMenu::Overlay::Settings)
            {
                if(settingsInput())menuState.overlay=MainMenu::Overlay::None;
                drawMenuFrame(view);continue;
            }
            MainMenu::Item confirmed{};
            if (MainMenu::HandleInput(menuState, confirmed))
            {
                if (confirmed == MainMenu::Item::Quit)
                    break;

                // Start Game: generate maze, initialise session, transition audio.
                if (confirmed == MainMenu::Item::StartGame)
                {
                    session = StartSession(root, seed);
                    if (session)
                    {
                        audio.TransitionToGame(seed);
                        audio.SetPaused(false);
                        UI::ScreenChanged();
                        appState = AppState::Playing;
                    }
                }
            }

            drawMenuFrame(view);
            continue;
        }

        // ---- PLAYING STATE ---- //
#ifndef NDEBUG
        if (IsKeyPressed(KEY_F3)) session->debugAI = !session->debugAI;
#endif
        const bool wasOverlayOpen = session->overlay != GameOverlay::None;
        if(session->escape.Completed())
        {
            const auto action=Victory::Input(session->victoryChoice);
            if(action==Victory::Action::Menu) { returnToMenu(view); continue; }
            if(action==Victory::Action::Quit) { quitRequested=true; continue; }
            if(action==Victory::Action::SameSeed || action==Victory::Action::NewMaze)
            {
                auto nextSeed=session->clock.seed;
                if(action==Victory::Action::NewMaze)
                {
                    nextSeed=static_cast<std::uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
                    if(nextSeed==session->clock.seed) ++nextSeed;
                }
                if(auto restarted=StartSession(root,nextSeed))
                {
                    seed=nextSeed; session=std::move(restarted);
                    audio.SetPaused(false); audio.TransitionToGame(seed); UI::ScreenChanged();
                }
                continue;
            }
        }
        else if (session->health.Dead())
        {
            session->health.Update(dt);
            if (session->health.flash == 0 && GameOver::Confirm(session->deathChoice))
            {
                if (session->deathChoice == GameOver::Choice::Menu)
                {
                    returnToMenu(view);
                    continue;
                }
                if (auto restarted = StartSession(root, seed))
                {
                    session = std::move(restarted);
                    audio.SetPaused(false);
                    audio.TransitionToGame(seed);
                    UI::ScreenChanged();
                }
                continue;
            }
        }
        else if(session->overlay==GameOverlay::Pause)
        {
            const auto action=PauseMenu::Input(session->pauseMenu);
            if(action==PauseMenu::Action::Resume) session->overlay=GameOverlay::None;
            else if(action==PauseMenu::Action::Controls) { session->overlay=GameOverlay::Controls; session->controlsFromPause=true; }
            else if(action==PauseMenu::Action::Settings)session->overlay=GameOverlay::Settings;
            else if(action==PauseMenu::Action::Menu) { returnToMenu(view); continue; }
            else if(action==PauseMenu::Action::Quit) { quitRequested=true; continue; }
            else if(action==PauseMenu::Action::Restart)
            {
                if(auto restarted=StartSession(root,session->clock.seed))
                {
                    session=std::move(restarted); audio.SetPaused(false); audio.TransitionToGame(session->clock.seed);
                    UI::ScreenChanged();
                }
                continue;
            }
        }
        else if(session->overlay==GameOverlay::Settings)
        {
            if(settingsInput())session->overlay=GameOverlay::Pause;
        }
        else if (IsKeyPressed(KEY_ESCAPE))
        {
            if (session->overlay != GameOverlay::None)
            {
                session->overlay = session->controlsFromPause && session->overlay==GameOverlay::Controls
                    ? GameOverlay::Pause : GameOverlay::None;
                session->controlsFromPause=false;
            }
            else
            {
                session->overlay=GameOverlay::Pause;
            }
            UI::ScreenChanged();
        }
        else if(session->overlay==GameOverlay::Controls &&
                (IsKeyPressed(KEY_I) || UI::ConfirmKey() || UI::BackPressed()))
        {
            session->overlay=session->controlsFromPause?GameOverlay::Pause:GameOverlay::None;
            session->controlsFromPause=false; UI::ScreenChanged();
        }
        else if(session->overlay==GameOverlay::Book && Mobcyclopedia::ClosePressed())
        { session->overlay=GameOverlay::None; UI::ScreenChanged(); }
        else if(session->overlay==GameOverlay::Map && UI::BackPressed())
        { session->overlay=GameOverlay::None; UI::ScreenChanged(); }
        else if (IsKeyPressed(KEY_B) && !session->controlsFromPause)
        {
            session->bookLeaf = 0;
            session->overlay = session->overlay == GameOverlay::Book ? GameOverlay::None : GameOverlay::Book;
        }
        else if (session->overlay != GameOverlay::Book && IsKeyPressed(KEY_I))
        {
            session->overlay = (session->overlay == GameOverlay::Controls)
                               ? GameOverlay::None : GameOverlay::Controls;
        }
        else if (session->overlay != GameOverlay::Book && !session->controlsFromPause && IsKeyPressed(KEY_M))
        {
            session->overlay = (session->overlay == GameOverlay::Map)
                               ? GameOverlay::None : GameOverlay::Map;
        }

        if (!session->escape.Completed() && session->overlay == GameOverlay::Book) Mobcyclopedia::Navigate(session->bookLeaf);
        const Maze::Layout& level = session->generated.layout;
        const bool overlayOpen = session->health.Dead() || session->escape.Completed() || session->overlay != GameOverlay::None || IsWindowMinimized() || wasOverlayOpen;
        audio.SetPaused(overlayOpen);
        session->clock.Update(dt, overlayOpen);

        // Player movement — blocked while an overlay is open.
        const auto previous = session->player.position;
        if (!overlayOpen && !session->dungeon.hidden)
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

        const float moved = std::abs(session->player.position.x - previous.x)
                          + std::abs(session->player.position.y - previous.y);

        const Vector2 playerPos{session->player.position.x, session->player.position.y};
        const auto    playerCell = Movement::CellAt(session->player.position);
        if(!overlayOpen)
        {
            auto& exploration=session->dungeon;
            exploration.Update(dt);
            if(!exploration.hidden)
            {
                const auto oldGold=exploration.gold;
                const bool collected=exploration.Collect(playerPos);
                if(exploration.gold>oldGold)audio.CollectGold();
                else if(collected)audio.CollectSoul();
            }
            if(IsKeyPressed(KEY_E))
            {
                if(exploration.hidden)
                {
                    bool clear=true;
                    for(const auto* enemy:{&session->enemies.Angel(),&session->enemies.Ghost()})
                        if(Enemies::DistanceSquared(enemy->position,playerPos)<36*36)clear=false;
                    for(const auto& actor:session->physical.Actors())
                        if(actor.active&&actor.health>0&&Enemies::DistanceSquared(actor.body.position,playerPos)<36*36)clear=false;
                    if(clear){exploration.hidden=false;exploration.hidingBarrel=-1;}
                }
                else
                {
                    const auto action=exploration.Nearest(playerPos);
                    if(exploration.Activate(action))
                    {
                        if(action.kind==Dungeon::Kind::Chest)audio.OpenChest();else audio.CollectSoul();
                        session->escape.SetUnlocked(exploration.exitOpen);
                        if(action.kind==Dungeon::Kind::Door||action.kind==Dungeon::Kind::Exit)
                        {session->physical.InvalidateRoutes();session->enemies.InvalidateRoutes();session->RefreshLights();}
                    }
                }
            }
        }
        session->light.SetObserverEnabled(!session->dungeon.hidden);
        if(!overlayOpen) session->exploration.Visit(playerCell);
        if(!overlayOpen) session->torches.UpdateAnimation(dt);
        if (!overlayOpen && session->torchState.ActivateNearby(playerPos))
        {
            session->RefreshLights();
        }
        session->light.Update(playerPos, FacingVector(session->player.facing));
        const Camera2D camera = FollowCamera(session->cameraCenter, playerPos, view, overlayOpen?0:dt);
        const Rectangle visible = VisibleDungeon(view, camera);
        GameAudio::EnemyAudio cues;
        bool quiet = false;
        if (!overlayOpen)
        {
            session->bats.Update(dt, playerPos);
            const Vector2 facing = FacingVector(session->player.facing);
            const bool swing = session->melee.Update(dt, !session->dungeon.hidden && !wasOverlayOpen && !UI::keysBlocked && IsKeyPressed(KEY_SPACE), facing);
            session->enemyAnimation = std::fmod(session->enemyAnimation + dt, 1.0f);
            const int previousHealth = session->health.points;
            session->enemies.Update(dt, {previous.x, previous.y}, playerPos, session->light, session->health,session->dungeon.hidden);
            session->physical.Update(dt, playerPos, visible, session->light, session->exploration,
                                     swing, facing, session->health,session->dungeon.hidden);
            const auto distanceTo = [&](const Enemies::Enemy& enemy) {
                return std::sqrt(Enemies::DistanceSquared(playerPos, enemy.position));
            };
            const auto& angel = session->enemies.Angel();
            const auto& ghost = session->enemies.Ghost();
            for (const auto* enemy : {&angel, &ghost})
                if (enemy->traveled > 0) cues.Movement(enemy->kind, distanceTo(*enemy));
            for (const auto& actor : session->physical.Actors())
            {
                if (!actor.active) continue;
                const float distance = distanceTo(actor.body);
                if (actor.body.traveled > 0) cues.Movement(actor.body.kind, distance);
                if (actor.swung) cues.Action(actor.body.kind == Enemies::Kind::Sentinel ?
                                            GameAudio::Cue::Sword : GameAudio::Cue::Lunge, distance);
                if (actor.struck)
                {
                    cues.Action(actor.health > 0 ? GameAudio::Cue::EnemyHit : GameAudio::Cue::EnemyDeath, distance);
                    if (actor.health == 0)
                    {
                        ++session->defeated;
                        session->souls.Drop(actor.body.kind, actor.body.position, session->clock.elapsed);
                        session->dungeon.Drop(actor.body.kind,actor.body.position);
                    }
                }
            }
            if (!session->health.Dead() && session->souls.Collect(level, playerPos, session->clock.elapsed)) audio.CollectSoul();
            if (session->health.points < previousHealth) audio.Damage(session->health.lastHit);
            if(session->dungeon.hidden && (ghost.state==Enemies::State::Chasing || ghost.windup>0 || session->health.points<previousHealth))session->dungeon.WarnGhost();
            quiet = !session->health.Dead() && session->melee.cooldown == 0
                && session->health.flash == 0 && !session->physical.Dangerous()
                && angel.state != Enemies::State::Chasing && ghost.state != Enemies::State::Chasing;
            // Resolve damage first: a dead player cannot win in the same update.
            if(session->escape.TryComplete({previous.x,previous.y},playerPos,!session->health.Dead(),
                {session->clock.seed,session->clock.elapsed,session->souls.Total(),session->health.points,session->defeated,session->dungeon.gold}))
            {
                UI::ScreenChanged(); audio.SetPaused(true); audio.PlayVictory();
            }
        }

        const float actualHealth = float(session->health.points) / Enemies::StartingHealth;
        if(!overlayOpen) session->shownHealth = std::max(actualHealth, session->shownHealth - dt * 1.8f);
        audio.SetPaused(overlayOpen || session->escape.Completed());
        audio.Update(moved, quiet, cues);
        BeginDrawing();
        ClearBackground(HudBackground);
        if (view.width > 0 && view.height > 0)
        {
            BeginMode2D(camera);
            DungeonArt::Draw(dungeonTex, level, visible, Lighting::AmbientMaskColor);
            session->escape.Draw(dungeonTex,session->clock.elapsed);
            explorationArt.Draw(session->dungeon,visible);
            for (const auto* e : {&session->enemies.Angel(), &session->enemies.Ghost()})
            {
                if (!e->discovered || !CheckCollisionPointRec(e->position, visible)
                    || !Maze::HasLineOfSight(level, playerPos, e->position)) continue;
                enemySprites.DrawActor(*e, session->enemyAnimation);
            }
            for (const auto& actor : session->physical.Actors())
            {
                if (!actor.active || !CheckCollisionPointRec(actor.body.position, visible)
                    || !Maze::HasLineOfSight(level, playerPos, actor.body.position)) continue;
                enemySprites.DrawPhysical(actor);
            }
            batArt.Draw(session->bats, level, playerPos, visible);
            session->torches.DrawBrackets();
            session->light.DrawDarkness();
            session->escape.DrawLabel(playerPos);
            DrawSouls(session->souls, visible, playerPos, level, session->clock.elapsed);
            const auto& ghostActor = session->enemies.Ghost();
            if (ghostActor.discovered && CheckCollisionPointRec(ghostActor.position, visible)
                && (ghostActor.windup > 0 || ghostActor.attackPulse > 0)
                && Maze::HasLineOfSight(level, playerPos, ghostActor.position))
                enemySprites.DrawGhostPulse(ghostActor);
            session->torches.DrawFlames();
            const float recoil = session->health.invulnerable / Enemies::HitCooldown;
            Color tint = session->health.flash > 0 ? Color{255, 124, 111, 255} : WHITE;
            if (recoil > 0) tint.a = static_cast<unsigned char>(232 + 22 * std::cos(recoil * 12));
            if(!session->dungeon.hidden)DrawPlayer(playerTex, session->player, tint);
            if (!overlayOpen && !session->dungeon.hidden && !session->escape.Completed()) DrawMelee(level, playerPos, session->melee);
            EndMode2D();
            const auto& angel = session->enemies.Angel();
            const auto& ghost = session->enemies.Ghost();
            const char* status = nullptr;
            bool sanctuary = session->enemies.Safe(playerPos);
            if (sanctuary) status = Enemies::InEntrance(level, playerCell) ? "Entrance sanctuary" : "Torch ward (spirits only)";
            for (const auto& actor : session->physical.Actors())
            {
                if (actor.active && actor.body.state == Enemies::State::WindingUp
                    && Maze::HasLineOfSight(level, playerPos, actor.body.position))
                {
                    status = actor.body.kind == Enemies::Kind::Sentinel ? "Wizard charging a spell" : "Demon about to lunge";
                    sanctuary = false;
                }
            }
            if (!status && ghost.windup > 0) status = "Spectral strike";
            else if (!status && angel.nearby && !angel.discovered) status = "Stone scrapes nearby";
            else if (!status && ghost.nearby && !ghost.discovered) status = "A chill in the air";
            else if (!status && angel.windup > 0) status = "Stone grinds closer";
            Hud::DrawLocation(view.width, view.hudScale, level.LocationName(playerCell));
            Hud::DrawStatus(view.width, view.hudScale, status, sanctuary);
            Hud::DrawHealth(view.width, view.height, view.hudScale, session->health.points,
                            Enemies::StartingHealth, session->shownHealth);
            char time[24];
            session->clock.Format(time);
            Hud::DrawRun(view.width, view.height, view.hudScale, Hud::Pixels(view.hudScale, 220), time, session->souls.Total(),session->dungeon.gold);
            if(!overlayOpen)
            {
                const char* prompt=session->dungeon.hidden?"Press E to Exit":session->dungeon.Prompt(session->dungeon.Nearest(playerPos));
                DrawInteractionPrompt(prompt,view);
                Hud::DrawCenteredText(TextFormat("Keys: Bronze %s  Silver %s  Escape %s | Exit %s",session->dungeon.HasKey(0)?"yes":"-",session->dungeon.HasKey(1)?"yes":"-",session->dungeon.HasKey(2)?"yes":"-",session->dungeon.exitOpen?"open":"locked"),view.width/2,view.height-73,12,Hud::DimText);
                if(session->dungeon.ghostWarning>0)Hud::DrawCenteredText("YOU CANNOT HIDE FROM GHOSTS!",view.width/2,view.height/2-40,20,Fade(RED,std::min(1.0f,session->dungeon.ghostWarning)));
                if(session->dungeon.pickupFlash>0)
                {
                    const char* names[]={"Bronze Key","Silver Key","Escape Key"};
                    Hud::DrawCenteredText(TextFormat("Picked up %s",names[session->dungeon.lastKey]),view.width/2,view.height/2+30,18,Fade(Hud::Gold,std::min(1.0f,session->dungeon.pickupFlash)));
                }
            }
            Hud::DrawHitFlash(view.width, view.height, session->health.flash / Enemies::HitFlash,
                              session->health.Dead() && session->health.lastHit == Enemies::Kind::Angel);
            if(session->escape.Completed())
                Victory::Draw(session->escape.FinalResult(),session->victoryChoice,view.width,view.height);
            else if (session->health.Dead() && session->health.flash == 0)
                GameOver::Draw(view.width, view.height, session->deathChoice, session->health.lastHit, session->clock, session->souls.Total(),session->dungeon.gold);
            else if (session->overlay == GameOverlay::Controls)
            {
                Hud::DrawControls(view.width, view.height, "Press Esc or I to close this panel");
                UI::DrawBack(view.width,view.height);
            }
            else if(session->overlay==GameOverlay::Pause)
                PauseMenu::Draw(session->pauseMenu,view.width,view.height);
            else if(session->overlay==GameOverlay::Settings)
                AudioSettings::Draw(audioPanel,audioPreferences,view.width,view.height);
            else if (session->overlay == GameOverlay::Map)
            {
                Parchment::DrawExpanded(session->exploration, playerPos,
                                        view.width, view.height, view.hudScale,level,session->dungeon.exitOpen);
                UI::DrawBack(view.width,view.height);
            }
            else if (session->overlay == GameOverlay::Book)
                Mobcyclopedia::Draw(view.width, view.height, enemySprites, bookType, session->bookLeaf, static_cast<float>(GetTime()));
            else
            {
                Parchment::DrawCompact(session->exploration, playerPos,
                                       playerCell, view.hudScale,level);
#ifndef NDEBUG
                if (session->debugAI) Enemies::DrawDebug(view.width, view.height, session->enemies, session->physical);
#endif
            }
        }
        EndDrawing();
    }

    // ---- Cleanup ---- //
    session.reset();
    if(audioPanel.dirty)AudioPreferences::Save(localSettingsPath,audioPreferences);
    audio.Shutdown();
    menuBackdrop.UnloadTorches();
    bookType.Unload();
    batArt.Unload();
    explorationArt.Unload();
    enemySprites.Unload();
    UnloadTexture(playerTex);
    UnloadTexture(dungeonTex);
    CloseWindow();
    return 0;
}
