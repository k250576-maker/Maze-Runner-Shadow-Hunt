#ifndef SHADOW_HUNT_GAME_OVER_H
#define SHADOW_HUNT_GAME_OVER_H

#include "game_hud.h"
#include "enemy.h"
#include "run_progress.h"

namespace GameOver
{
enum class Choice { Restart, Menu };

inline bool Confirm(Choice& selected)
{
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S))
        selected = selected == Choice::Restart ? Choice::Menu : Choice::Restart;
    if (IsKeyPressed(KEY_ESCAPE))
    {
        selected = Choice::Menu;
        return true;
    }
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

inline void Draw(int width, int height, Choice choice, Enemies::Kind killer, const Run::Clock& clock, Run::Score souls)
{
    constexpr int panelWidth = 360;
    constexpr int panelHeight = 276;
    const float scale = std::min(width / 460.0f, height / 430.0f);
    DrawRectangle(0, 0, width, height, {9, 6, 11, 190});
    BeginMode2D({{(width - panelWidth * scale) / 2, (height - panelHeight * scale) / 2}, {0, 0}, 0, scale});
    Hud::DrawStonePanel({0, 0, panelWidth, panelHeight});
    Hud::DrawCenteredText("Game Over", 180, 26, 32, Hud::Gold);
    Hud::DrawCenteredText(TextFormat("Caught by the %s", Enemies::Name(killer)),
                          180, 65, 15, Hud::Text);
    char time[24];
    clock.Format(time);
    Hud::DrawCenteredText(TextFormat("Time %s   Souls %llu", time, souls), 180, 91, 15, Hud::Gold);
    Hud::DrawCenteredText(TextFormat("Seed %lu", static_cast<unsigned long>(clock.seed)), 180, 111, 13, Hud::DimText);
    Hud::DrawDivider(28, 132, 304);
    const char* labels[] = {"Restart", "Return to Main Menu"};
    for (int i = 0; i < 2; ++i)
    {
        const bool selected = i == static_cast<int>(choice);
        const Rectangle box{34, 146.0f + i * 47, 292, 39};
        DrawRectangleRounded(box, 0.18f, 4, selected ? Color{64, 46, 33, 255} : Hud::KeyBackground);
        DrawRectangleRoundedLinesEx(box, 0.18f, 4, selected ? 2 : 1, selected ? Hud::Brass : Hud::Border);
        Hud::DrawCenteredText(labels[i], 180, static_cast<int>(box.y) + 10, 18, selected ? Hud::Gold : Hud::Text);
    }
    Hud::DrawCenteredText("Enter / Space to confirm", 180, 246, 14, Hud::DimText);
    EndMode2D();
}
}
#endif
