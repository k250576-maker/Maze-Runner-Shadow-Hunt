#ifndef SHADOW_HUNT_GAME_OVER_H
#define SHADOW_HUNT_GAME_OVER_H

#include "game_hud.h"
#include "enemy.h"
#include "run_progress.h"
#include "ui_interaction.h"

namespace GameOver
{
enum class Choice { Restart, Menu };
inline Camera2D Layout(int width,int height)
{
    const float scale=std::min(width/460.0f,height/430.0f);
    return {{(width-360*scale)/2,(height-276*scale)/2},{0,0},0,scale};
}
inline Rectangle Button(int i) { return {34,146.0f+i*47,292,39}; }

inline bool Confirm(Choice& selected)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        selected = Choice::Menu;
        return true;
    }
    int index=static_cast<int>(selected);
    const bool activate=UI::Select(index,2,Button,Layout(GetScreenWidth(),GetScreenHeight()));
    selected=static_cast<Choice>(index);
    return activate;
}

inline void Draw(int width, int height, Choice choice, Enemies::Kind killer, const Run::Clock& clock, Run::Score souls,Run::Score gold=0)
{
    constexpr int panelWidth = 360;
    constexpr int panelHeight = 276;
    DrawRectangle(0, 0, width, height, {9, 6, 11, 190});
    const Camera2D layout=Layout(width,height);
    BeginMode2D(layout);
    Hud::DrawStonePanel({0, 0, panelWidth, panelHeight});
    Hud::DrawCenteredText("Game Over", 180, 26, 32, Hud::Gold);
    Hud::DrawCenteredText(TextFormat("Caught by the %s", Enemies::Name(killer)),
                          180, 65, 15, Hud::Text);
    char time[24];
    clock.Format(time);
    Hud::DrawCenteredText(TextFormat("Time %s   Souls %llu", time, souls), 180, 91, 15, Hud::Gold);
    Hud::DrawCenteredText(TextFormat("Gold %llu  Score %llu  Seed %lu",gold,souls+gold,static_cast<unsigned long>(clock.seed)), 180, 111, 13, Hud::DimText);
    Hud::DrawDivider(28, 132, 304);
    const char* labels[] = {"Restart", "Return to Main Menu"};
    for (int i = 0; i < 2; ++i)
    {
        const bool selected = i == static_cast<int>(choice);
        UI::Button(Button(i),labels[i],selected,layout);
    }
    Hud::DrawCenteredText("Enter / Space to confirm", 180, 246, 14, Hud::DimText);
    EndMode2D();
}
}
#endif
