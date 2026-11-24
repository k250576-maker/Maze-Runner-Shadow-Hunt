#ifndef SHADOW_HUNT_PAUSE_MENU_H
#define SHADOW_HUNT_PAUSE_MENU_H

#include "ui_interaction.h"

namespace PauseMenu
{
enum class Action { None, Resume, Restart, Controls, Settings, Menu, Quit };
struct State { int selected=0; Action pending=Action::None; int confirmation=1; };
constexpr float Width=380, Height=418;
inline Rectangle Button(int i) { return {32,93.0f+i*48,Width-64,38}; }
inline Camera2D Layout() { return UI::Centered(GetScreenWidth(),GetScreenHeight(),Width,Height); }
inline Rectangle ConfirmationButton(int i) { return {32.0f+i*162,190,148,40}; }

inline Action Input(State& state)
{
    if(state.pending!=Action::None)
    {
        if(IsKeyPressed(KEY_ESCAPE)) { state.pending=Action::None; UI::ScreenChanged(); return Action::None; }
        if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) state.confirmation=1-state.confirmation;
        if(UI::Select(state.confirmation,2,ConfirmationButton,Layout()))
        {
            const Action result=state.confirmation==0?state.pending:Action::None;
            state.pending=Action::None; UI::ScreenChanged(); return result;
        }
        return Action::None;
    }
    if(IsKeyPressed(KEY_ESCAPE)) { UI::ScreenChanged(); return Action::Resume; }
    if(!UI::Select(state.selected,6,Button,Layout())) return Action::None;
    const Action action=static_cast<Action>(state.selected+1);
    UI::ScreenChanged();
    if(action==Action::Restart || action==Action::Menu)
    {
        state.pending=action; state.confirmation=1; return Action::None;
    }
    return action;
}
inline void Draw(const State& state,int width,int height)
{
    DrawRectangle(0,0,width,height,{7,5,10,188});
    const Camera2D layout=UI::Centered(width,height,Width,Height);
    BeginMode2D(layout);
    Hud::DrawStonePanel({0,0,Width,Height});
    if(state.pending!=Action::None)
    {
        Hud::DrawCenteredText("Leave this run?",190,54,28,Hud::Gold);
        Hud::DrawCenteredText(state.pending==Action::Restart?"Restart with the same maze seed?":"Return to the main menu?",190,112,17,Hud::Text);
        Hud::DrawCenteredText("Your current progress will be lost.",190,142,16,Hud::DimText);
        UI::Button(ConfirmationButton(0),"Confirm",state.confirmation==0,layout);
        UI::Button(ConfirmationButton(1),"Cancel",state.confirmation==1,layout);
    }
    else
    {
        Hud::DrawCenteredText("Run Paused",190,29,30,Hud::Gold);
        Hud::DrawDivider(32,73,316);
        constexpr const char* labels[]={"Resume Game","Restart Run","Controls","Settings","Return to Main Menu","Quit Game"};
        for(int i=0;i<6;++i) UI::Button(Button(i),labels[i],state.selected==i,layout);
        Hud::DrawCenteredText("Esc to resume",190,388,14,Hud::DimText);
    }
    EndMode2D();
}
}
#endif
