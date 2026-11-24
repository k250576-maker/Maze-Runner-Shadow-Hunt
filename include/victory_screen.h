#ifndef SHADOW_HUNT_VICTORY_SCREEN_H
#define SHADOW_HUNT_VICTORY_SCREEN_H

#include "escape_objective.h"
#include "ui_interaction.h"

namespace Victory
{
enum class Action { None, SameSeed, NewMaze, Menu, Quit };
constexpr float Width=450,Height=480;
inline Rectangle Button(int i) { return {45,264.0f+i*46,360,36}; }
inline Action Input(int& selected)
{
    if(UI::Select(selected,4,Button,UI::Centered(GetScreenWidth(),GetScreenHeight(),Width,Height)))
    { UI::ScreenChanged(); return static_cast<Action>(selected+1); }
    return Action::None;
}
inline void Draw(const Escape::Result& result,int selected,int width,int height)
{
    DrawRectangle(0,0,width,height,{6,6,10,218});
    const Camera2D layout=UI::Centered(width,height,Width,Height);
    BeginMode2D(layout);
    Hud::DrawStonePanel({0,0,Width,Height});
    Hud::DrawCenteredText("YOU ESCAPED THE SHADOWS",225,28,23,Hud::Gold);
    Hud::DrawCenteredText("Maze completed",225,65,20,Hud::Text);
    Hud::DrawDivider(40,100,370);
    Run::Clock clock{result.seed,result.elapsed}; char time[24]; clock.Format(time);
    Hud::DrawCenteredText(TextFormat("Completion time   %s",time),225,120,21,Hud::Gold);
    Hud::DrawCenteredText(TextFormat("Souls collected   %llu",result.souls),225,155,18,Hud::Text);
    Hud::DrawCenteredText(TextFormat("Enemies defeated   %u",result.defeated),225,183,18,Hud::Text);
    Hud::DrawCenteredText(TextFormat("Health   %d / %d     Seed   %lu",result.health,Enemies::StartingHealth,
                          static_cast<unsigned long>(result.seed)),225,216,16,Hud::DimText);
    constexpr const char* labels[]={"Play Again - Same Seed","New Maze","Return to Main Menu","Quit Game"};
    Hud::DrawCenteredText(TextFormat("Gold %llu    Score %llu",result.gold,result.souls+result.gold),225,242,14,Hud::Gold);
    for(int i=0;i<4;++i) UI::Button(Button(i),labels[i],selected==i,layout);
    Hud::DrawCenteredText("Mouse or arrows / Enter",225,454,14,Hud::DimText);
    EndMode2D();
}
}
#endif
