#ifndef SHADOW_HUNT_CREDITS_H
#define SHADOW_HUNT_CREDITS_H

#include "mobcyclopedia.h"

namespace Credits
{
struct Track
{
    const char* title;
    const char* credit;
    const char* source;
};

constexpr Track Music[] = {
    {"Rich House", "Music composed by Jeremy Soule.", "https://www.youtube.com/watch?v=8pZlkGHzTxc"},
    {"Elmshore", "Music composed by Justin Bell.", "https://www.youtube.com/watch?v=4Un4UIdo9HA"},
    {"Deep Dungeon", "Music created by Fog Saburo.", "https://www.youtube.com/watch?v=kA7c3SpzZDA"}
};
constexpr const char* Statement = "Maze Runner – Shadow Hunt is a noncommercial university DSA Lab project developed for personal and educational demonstration. Third-party music remains the work of its respective creators.";
constexpr int Width = 522, Height = 489;

inline Camera2D Layout(int screenWidth, int screenHeight)
{
    const float scale = std::min(screenWidth / 558.0f, screenHeight / 560.0f);
    return {{(screenWidth - Width * scale) / 2, (screenHeight - (Height + 35) * scale) / 2}, {0,0}, 0, scale};
}

inline Rectangle SourceButton(int row) { return {386, 115.0f + 61 * row, 102, 27}; }

inline bool CloseOrOpenLink()
{
    if (IsKeyPressed(KEY_ESCAPE) || UI::ConfirmKey() || UI::BackPressed()) return true;
    for (int i = 0; i < 3; ++i)
        if (IsKeyPressed(KEY_ONE + i) ||
            UI::Click(SourceButton(i), Layout(GetScreenWidth(), GetScreenHeight())))
            OpenURL(Music[i].source);
    return false;
}

inline void Draw(int width, int height, Font font)
{
    const Color ink{67,39,26,255};
    DrawRectangle(0,0,width,height,{11,7,12,196});
    BeginMode2D(Layout(width, height));
    DrawRectangleRounded({-8,-8,Width+16,Height+16},0.04f,5,{71,42,31,255});
    Parchment::DrawPaper(0,0,Width,Height,1);
    Hud::DrawCenteredText("Credits", Width/2, 25, 34, ink);
    DrawRectangle(34,73,Width-68,1,Parchment::Border);
    DrawText("Music Credits", 34, 89, 23, ink);
    for (int i = 0; i < 3; ++i)
    {
        const int y = 119 + 61*i;
        DrawText(Music[i].title,34,y,20,ink);
        Mobcyclopedia::Paragraph(font, Music[i].credit,34,y+25,330);
        const Rectangle box = SourceButton(i);
        UI::Button(box,TextFormat("Source %d",i+1),false,Layout(width,height),15);
    }
    DrawRectangle(34,301,Width-68,1,Parchment::Border);
    DrawText("Development",34,315,20,ink);
    Mobcyclopedia::Paragraph(font,"Maze Runner – Shadow Hunt project team",34,340,Width-68);
    Mobcyclopedia::Paragraph(font,Statement,34,373,Width-68);
    Hud::DrawCenteredText("1 / 2 / 3 for sources    Esc to return", Width/2, Height+15, 15, Hud::Gold);
    EndMode2D();
    UI::DrawBack(width,height);
}
}
#endif
