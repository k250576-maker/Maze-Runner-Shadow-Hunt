#ifndef SHADOW_HUNT_UI_INTERACTION_H
#define SHADOW_HUNT_UI_INTERACTION_H

#include "game_hud.h"

namespace UI
{
inline bool mouseBlocked = false, keysBlocked = false, consumed = false;
inline constexpr Camera2D Screen{{0,0},{0,0},0,1};

inline void BeginFrame()
{
    consumed = false;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) mouseBlocked = false;
    if (!IsKeyDown(KEY_ENTER) && !IsKeyDown(KEY_SPACE)) keysBlocked = false;
}
inline void ScreenChanged() { mouseBlocked = keysBlocked = consumed = true; }
inline bool Hover(Rectangle bounds, Camera2D layout = Screen)
{
    return CheckCollisionPointRec(GetScreenToWorld2D(GetMousePosition(),layout), bounds);
}
inline bool Click(Rectangle bounds, Camera2D layout = Screen)
{
    if (consumed || mouseBlocked || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !Hover(bounds,layout)) return false;
    consumed = true;
    return true;
}
inline bool ConfirmKey()
{
    return !keysBlocked && !consumed && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
}
inline Camera2D Centered(int width, int height, float panelWidth, float panelHeight)
{
    const float scale = std::max(0.1f,std::min(width/(panelWidth+56),height/(panelHeight+56)));
    return {{(width-panelWidth*scale)/2,(height-panelHeight*scale)/2},{0,0},0,scale};
}
template<class Bounds>
inline bool Select(int& selected, int count, Bounds bounds, Camera2D layout)
{
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected=(selected+count-1)%count;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected=(selected+1)%count;
    const Vector2 delta=GetMouseDelta();
    for(int i=0;i<count;++i)
    {
        if ((delta.x!=0 || delta.y!=0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) && Hover(bounds(i),layout)) selected=i;
        if (Click(bounds(i),layout)) { selected=i; return true; }
    }
    if (ConfirmKey()) { consumed=true; return true; }
    return false;
}
inline void Button(Rectangle bounds, const char* label, bool selected, Camera2D layout = Screen, int font = 18)
{
    const bool hover=Hover(bounds,layout);
    const bool pressed=hover && !mouseBlocked && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    DrawRectangleRounded(bounds,0.20f,6,pressed ? Color{98,70,39,255} :
                         (selected || hover ? Color{65,47,32,255} : Hud::KeyBackground));
    DrawRectangleRoundedLinesEx(bounds,0.20f,6,selected||hover?2.0f:1.0f,selected||hover?Hud::Gold:Hud::Border);
    Hud::DrawCenteredText(label,static_cast<int>(bounds.x+bounds.width/2),
                          static_cast<int>(bounds.y+(bounds.height-font)/2),font,selected||hover?Hud::Gold:Hud::Text);
}
inline Rectangle BackBounds(int width,int height) { return {width/2.0f-66,height-38.0f,132,28}; }
inline bool BackPressed() { return Click(BackBounds(GetScreenWidth(),GetScreenHeight())); }
inline void DrawBack(int width,int height) { Button(BackBounds(width,height),"Back",false,Screen,16); }
}
#endif
