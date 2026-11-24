#ifndef SHADOW_HUNT_MAIN_MENU_H
#define SHADOW_HUNT_MAIN_MENU_H

#include "raylib.h"
#include "game_hud.h"
#include "menu_backdrop.h"
#include "credits.h"
#include <algorithm>
#include <cmath>

namespace MainMenu
{
enum class Item { StartGame, Controls, Mobcyclopedia, Credits, Quit, Count };
enum class Overlay { None, Controls, Book, Credits };

struct State
{
    int bookLeaf = 0;
    Item    selected     = Item::StartGame;
    float   time         = 0.0f;
    Overlay overlay      = Overlay::None;
};

inline void NavigateUp(State& state)
{
    int i = static_cast<int>(state.selected);
    state.selected = static_cast<Item>((i - 1 + static_cast<int>(Item::Count))
                                       % static_cast<int>(Item::Count));
}

inline void NavigateDown(State& state)
{
    int i = static_cast<int>(state.selected);
    state.selected = static_cast<Item>((i + 1) % static_cast<int>(Item::Count));
}

// Returns true when an action item (StartGame or Quit) was confirmed.
// 'confirmed' is set only on return true; Controls is handled internally.
inline bool HandleInput(State& state, Item& confirmed)
{
    if (state.overlay == Overlay::Book)
    {
        Mobcyclopedia::Navigate(state.bookLeaf);
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
            state.overlay = Overlay::None;
        return false;
    }
    if (state.overlay == Overlay::Credits)
    {
        if (Credits::CloseOrOpenLink()) state.overlay = Overlay::None;
        return false;
    }
    if (state.overlay == Overlay::Controls)
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I) || IsKeyPressed(KEY_ENTER)
            || IsKeyPressed(KEY_SPACE))
            state.overlay = Overlay::None;
        return false;
    }
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) NavigateUp(state);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) NavigateDown(state);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        if (state.selected == Item::Controls)
        {
            state.overlay = Overlay::Controls;
            return false;
        }
        if (state.selected == Item::Mobcyclopedia)
        {
            state.bookLeaf = 0;
            state.overlay = Overlay::Book;
            return false;
        }
        if (state.selected == Item::Credits)
        {
            state.overlay = Overlay::Credits;
            return false;
        }
        confirmed = state.selected;
        return true;
    }
    return false;
}

inline void DrawOptions(int width, int height, const State& state, float scale, bool stacked)
{
    constexpr int panelWidth = 316;
    constexpr int panelHeight = 419;
    constexpr int centerX = panelWidth / 2;
    constexpr int buttonHeight = 40;
    constexpr int textSize = 19;
    constexpr int stackedArtClearance = 90;
    constexpr const char* labels[] = {"START GAME", "CONTROLS", "MOBCYCLOPEDIA", "CREDITS", "QUIT"};
    static_assert(sizeof(labels) / sizeof(labels[0]) == static_cast<int>(Item::Count));
    const float panelX = width * (stacked ? 0.50f : 0.29f) - centerX * scale;
    const float panelY = stacked
        ? std::max(height * 0.64f - panelHeight * scale * 0.5f,
                   height * MenuBackdrop::StackedFloor + stackedArtClearance * scale)
        : (height - panelHeight * scale) * 0.5f;
    const Camera2D layout{{std::round(panelX), std::round(panelY)}, {0, 0}, 0, scale};
    BeginMode2D(layout);
    Hud::DrawStonePanel({0, 0, panelWidth, panelHeight});
    Hud::DrawCenteredText("MAZE RUNNER", centerX + 2, 34, 37, Color{9, 7, 10, 255});
    Hud::DrawCenteredText("MAZE RUNNER", centerX, 32, 37, Hud::Gold);
    Hud::DrawCenteredText("SHADOW HUNT", centerX, 75, 17, Color{187, 151, 103, 255});
    Hud::DrawDivider(29, 107, panelWidth - 58);

    for (int i = 0; i < static_cast<int>(Item::Count); ++i)
    {
        const bool selected = static_cast<int>(state.selected) == i;
        const int y = 123 + i * 49;
        const Rectangle button{29, static_cast<float>(y), panelWidth - 58, buttonHeight};
        DrawRectangleRounded({button.x, button.y + 2, button.width, button.height},
                             0.24f, 4, Color{12, 10, 14, 255});
        DrawRectangleRounded(button, 0.24f, 4,
                             selected ? Color{65, 47, 32, 255} : Color{35, 32, 37, 255});
        if (selected)
        {
            const auto shimmer = static_cast<unsigned char>(22 + 8 * std::sin(state.time * 2.0f));
            DrawRectangleRounded({button.x - 3, button.y - 3, button.width + 6, button.height + 6},
                                 0.24f, 4, Color{193, 137, 69, shimmer});
            DrawRectangleRoundedLinesEx(button, 0.24f, 4, 2, Color{176, 133, 78, 255});
            DrawPoly({49, button.y + buttonHeight * 0.5f}, 4, 4, 0, Hud::Gold);
            DrawPoly({panelWidth - 49, button.y + buttonHeight * 0.5f}, 4, 4, 0, Hud::Gold);
        }
        else DrawRectangleRoundedLines(button, 0.24f, 4, Hud::Border);
        Hud::DrawCenteredText(labels[i], centerX, y + (buttonHeight - textSize) / 2,
                              textSize, selected ? Color{255, 229, 180, 255} : Hud::DimText);
    }

    Hud::DrawCenteredText("W/S or arrows to move", centerX, 377, 13, Hud::DimText);
    Hud::DrawCenteredText("Enter / Space to select", centerX, 394, 13, Hud::DimText);
    EndMode2D();
}

inline void Draw(int width, int height, const State& state, const MenuBackdrop::Castle& castle,
                 Texture2D stone, Texture2D player, const Enemies::Sprites& enemySprites, const Mobcyclopedia::Type& type)
{
    const bool stacked = width < height;
    const float scale = std::min(width / 690.0f, height / 558.0f);
    castle.Draw(stone, player, width, height, scale * (stacked ? MenuBackdrop::StackedZoom : 2.38f),
                stacked, state.time);
    if (state.overlay == Overlay::Book)
        Mobcyclopedia::Draw(width, height, enemySprites, type, state.bookLeaf, state.time);
    else if (state.overlay == Overlay::Credits)
        Credits::Draw(width, height, type.face);
    else if (state.overlay == Overlay::Controls)
        Hud::DrawControls(width, height, "Esc / Enter / Space / I to return to menu");
    else
        DrawOptions(width, height, state, scale, stacked);
}

} // namespace MainMenu
#endif
