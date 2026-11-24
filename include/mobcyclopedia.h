#ifndef SHADOW_HUNT_MOBCYCLOPEDIA_H
#define SHADOW_HUNT_MOBCYCLOPEDIA_H

#include "enemy_art.h"
#include "game_hud.h"
#include "parchment_map.h"
#include <cstdlib>
#include <string>
#include <string_view>

namespace Mobcyclopedia
{
struct Entry
{
    Enemies::Kind kind;
    const char* name;
    const char* description;
    const char* escape;
    const char* algorithm;
    const char* search;
};

constexpr Entry Entries[] = {
    {Enemies::Kind::Angel, "Weeping Angel", "Listen for stone scraping. The statue patrols its own halls and gives chase when it sees you within six tiles.",
     "Face it with the flashlight to freeze it. Break sight or reach a lit torch or the Entrance Hall. Its touch can end the run.",
     "Breadth-first search", "Each open tile costs one step. A custom FIFO queue explores one distance at a time. Each visited tile remembers its parent. Following those parents backward gives a shortest safe route."},
    {Enemies::Kind::Ghost, "Phasing Ghost", "A sudden chill marks its approach. It senses nearby footsteps through walls and can also spot you in the open.",
     "Step away when the Ghost gathers strength. A hit costs one heart. Active torchlight wards the floor around you. Escape its territory or shelter in the Entrance Hall.",
     "Dijkstra's algorithm", "Floor costs one and walls cost five. A custom min-heap expands the cheapest route first. A cheaper arrival updates the tile's cost and parent. Warded crossings are forbidden."},
    {Enemies::Kind::Sentinel, "Fallen Sentinel",
     "An armored knight with three hearts. It raises its sword before a slash that costs one heart.",
     "Step out of its swing, then strike during recovery. Torchlight does not stop it. The Entrance Hall is safe.",
     "A* search", "Each floor step costs one. The shared min-heap ranks cells by steps taken plus Manhattan distance. That distance is a lower bound because steps are cardinal. Parents give the route back."},
    {Enemies::Kind::Stalker, "Hollow Stalker",
     "It sprints in bursts and crouches before lunging. One hit drops it. Its lunge costs one heart.",
     "Sidestep or back away from the lunge, then strike. Torches protect against spirits. This creature can cross torchlight.",
     "Breadth-first search", "The shared FIFO queue visits open cells in order of distance. Each cell remembers its parent. The chain of parents gives the shortest route through the corridors."}
};

constexpr int LeafCount = 2 * static_cast<int>(sizeof(Entries) / sizeof(Entries[0]));

class Type
{
public:
    void Load()
    {
        const char* windows = std::getenv("WINDIR");
        const std::string directory = windows ? std::string(windows) + "/Fonts/" : "";
        const std::string paths[] = {directory + "georgia.ttf", directory + "times.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", "/System/Library/Fonts/Supplemental/Georgia.ttf"};
        for (const auto& path : paths)
        {
            if (!FileExists(path.c_str())) continue;
            int glyphs[96];
            for (int i = 0; i < 95; ++i) glyphs[i] = 32+i;
            glyphs[95] = 0x2013;
            face = LoadFontEx(path.c_str(), 52, glyphs, 96);
            if (IsFontValid(face) && face.texture.id != GetFontDefault().texture.id)
            {
                SetTextureFilter(face.texture, TEXTURE_FILTER_BILINEAR);
                return;
            }
        }
        face = GetFontDefault();
    }
    void Unload() { if (face.texture.id && face.texture.id != GetFontDefault().texture.id) UnloadFont(face); }
    Font face{};
};

inline bool Spread(int width, int height) { return width >= 660 && width > height * 1.1f; }
inline void Navigate(int& leaf)
{
    const bool wide = Spread(GetScreenWidth(), GetScreenHeight());
    if (wide) leaf -= leaf % 2;
    const int step = wide ? 2 : 1;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) leaf = (leaf + LeafCount - step) % LeafCount;
    else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) leaf = (leaf + step) % LeafCount;
}

inline int Paragraph(Font font, std::string_view words, int x, int y, float width = 284)
{
    constexpr int size = 19;
    constexpr int leading = 24;
    const Color ink{55, 36, 29, 255};
    std::string line;
    while (!words.empty())
    {
        const auto end = words.find(' ');
        const std::string word(words.substr(0, end));
        const std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureTextEx(font, next.c_str(), size, 0).x > width)
        {
            DrawTextEx(font, line.c_str(), {float(x),float(y)}, size, 0, ink);
            line = word;
            y += leading;
        }
        else line = next;
        words.remove_prefix(end == words.npos ? words.size() : end + 1);
    }
    DrawTextEx(font, line.c_str(), {float(x),float(y)}, size, 0, ink);
    return y + leading;
}

inline void DrawLeaf(int leaf, int x, const Enemies::Sprites& sprites, Font face, float time)
{
    constexpr int width = 350, height = 476;
    const auto& entry = Entries[leaf / 2];
    Parchment::DrawPaper(x, 0, width, height, 1);
    const Color shade{87,49,26,75};
    if (leaf % 2) DrawRectangleGradientH(x, 0, 25, height, shade, BLANK);
    else DrawRectangleGradientH(x+width-25, 0, 25, height, BLANK, shade);
    const Color ink{72,42,30,255};
    if (leaf % 2 == 0)
    {
        Hud::DrawCenteredText("The Mobcyclopedia", x+175, 29, 20, ink);
        DrawEllipse(x+175, 173, 80, 89, Color{151,106,65,61});
        DrawEllipseLines(x+175, 173, 81, 90, Color{148,100,57,170});
        sprites.Draw(entry.kind, {float(x+175),246}, 144, false,
                     entry.kind == Enemies::Kind::Ghost ? time : 0.0f);
        Hud::DrawCenteredText(entry.name, x+175, 280, 26, ink);
        DrawRectangle(x+86, 320, 178, 1, Parchment::Border);
        Paragraph(face, entry.description, x+32, 341);
    }
    else
    {
        DrawText("Survival", x+32, 37, 25, ink);
        int y = Paragraph(face, entry.escape, x+32, 79) + 20;
        DrawRectangle(x+32, y, 286, 1, Parchment::Border);
        DrawText(entry.algorithm, x+32, y+18, 21, ink);
        Paragraph(face, entry.search, x+32, y+54);
    }
    Hud::DrawCenteredText(TextFormat("%d", leaf+1), x+175, 447, 15, ink);
}

inline void Draw(int width, int height, const Enemies::Sprites& sprites, const Type& type, int leaf, float time)
{
    const bool wide = Spread(width, height);
    const int w = wide ? 704 : 350;
    const float scale = std::min(width / float(w+34), height / 540.0f);
    DrawRectangle(0, 0, width, height, {12,8,12,200});
    BeginMode2D({{(width-w*scale)/2, (height-510*scale)/2}, {0,0}, 0, scale});
    DrawRectangleRounded({-9,-8,float(w+18),498},0.045f,5,{70,39,31,255});
    DrawRectangleRoundedLinesEx({-8,-7,float(w+16),496},0.045f,5,2,Hud::Brass);
    for (int y = 476; y < 484; y += 2) DrawRectangle(2,y,w-4,2,y%4 ? Parchment::Border : Hud::Gold);
    const int first = wide ? leaf - leaf%2 : leaf;
    DrawLeaf(first, 0, sprites, type.face, time);
    if (wide)
    {
        DrawRectangle(350,0,4,476,{92,56,38,255});
        DrawLeaf(first+1,354,sprites,type.face,time);
    }
    Hud::DrawCenteredText("Left / Right to turn    B / Esc to close",w/2,499,14,Hud::Gold);
    EndMode2D();
}
}
#endif
