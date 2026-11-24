#ifndef SHADOW_HUNT_MAIN_MENU_H
#define SHADOW_HUNT_MAIN_MENU_H

#include "raylib.h"
#include "game_hud.h"
#include <cmath>

// Medieval/dark-fantasy main menu. Matches the dungeon HUD palette.
namespace MainMenu
{
// Palette — all colors use the same muted gold and stone tones as the HUD.
constexpr Color BackgroundDark{7, 6, 9, 255};
constexpr Color StoneBand{11, 10, 14, 255};
constexpr Color BorderOuter{70, 55, 46, 255};
constexpr Color BorderInner{45, 38, 32, 255};
constexpr Color PanelFill{13, 11, 16, 215};
constexpr Color Divider{75, 60, 50, 255};
constexpr Color TitleGold{229, 205, 167, 255};  // Hud::Gold
constexpr Color SubtitleTone{150, 128, 104, 255};
constexpr Color ItemSelected{248, 224, 174, 255};
constexpr Color ItemNormal{148, 133, 115, 255};
constexpr Color ItemHighlight{32, 26, 18, 210};
constexpr Color ItemHighlightBorder{88, 68, 46, 190};
constexpr Color HintColor{82, 72, 60, 200};

enum class Item { StartGame = 0, Controls = 1, Quit = 2, Count = 3 };

struct State
{
    Item    selected     = Item::StartGame;
    float   time         = 0.0f;
    bool    controlsOpen = false;
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
    if (state.controlsOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I) || IsKeyPressed(KEY_ENTER)
            || IsKeyPressed(KEY_SPACE))
            state.controlsOpen = false;
        return false;
    }
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) NavigateUp(state);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) NavigateDown(state);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        if (state.selected == Item::Controls)
        {
            state.controlsOpen = true;
            return false;
        }
        confirmed = state.selected;
        return true;
    }
    return false;
}

// Draws animated torch flicker at world position (x, y).
inline void DrawTorchFlame(float x, float y, float time, float seed, float scale)
{
    const float f1 = std::sin(time * 7.3f  + seed)        * 0.5f + 0.5f;
    const float f2 = std::sin(time * 11.7f + seed * 2.3f) * 0.5f + 0.5f;
    const float f3 = std::sin(time * 5.1f  + seed * 0.7f) * 0.5f + 0.5f;

    const float outerR = scale * (38.0f + f1 * 10.0f + f2 * 6.0f);
    const float midR   = scale * (18.0f + f2 *  5.0f + f3 * 4.0f);
    const float coreR  = scale * (7.0f  + f1 *  2.0f);
    const Vector2 center{x, y};
    const int     ix = static_cast<int>(x);
    const int     iy = static_cast<int>(y);

    // Broad ambient glow.
    DrawCircleGradient(center, outerR,
        Color{160, 72, 10, static_cast<unsigned char>(28 + f1 * 18)},
        Color{0, 0, 0, 0});
    // Orange mid-flame.
    DrawCircleGradient(center, midR,
        Color{230, 110, 20, static_cast<unsigned char>(170 + f2 * 50)},
        Color{140, 40, 0, 0});
    // Bright core.
    DrawCircleGradient(center, coreR,
        Color{255, 230, 140, 230},
        Color{255, 140, 30, 60});
    // Bracket stub below the flame.
    const int bx = ix - Hud::Pixels(scale, 3);
    const int by = iy + Hud::Pixels(scale, 4);
    DrawRectangle(bx, by, Hud::Pixels(scale, 6), Hud::Pixels(scale, 10), Color{90, 72, 60, 255});
    DrawRectangle(bx - Hud::Pixels(scale, 2), by - Hud::Pixels(scale, 3),
                  Hud::Pixels(scale, 10), Hud::Pixels(scale, 4), Color{70, 55, 45, 255});
}

// Draws the main menu screen (not the controls overlay).
inline void Draw(int width, int height, const State& state, float scale)
{
    // Stone background: solid dark base with subtle alternating vertical bands.
    ClearBackground(BackgroundDark);
    const int band = Hud::Pixels(scale, 56);
    for (int x = 0; x < width; x += band * 2)
        DrawRectangle(x, 0, band, height, StoneBand);

    // Edge vignette to focus the eye on the centre.
    const int vigW = width / 5;
    DrawRectangleGradientH(0,          0, vigW,    height, Color{0,0,0,150}, Color{0,0,0,0});
    DrawRectangleGradientH(width-vigW, 0, vigW,    height, Color{0,0,0,0},   Color{0,0,0,150});
    DrawRectangleGradientV(0,           0, width,  height/5, Color{0,0,0,120}, Color{0,0,0,0});
    DrawRectangleGradientV(0, height*4/5, width,  height/5, Color{0,0,0,0},   Color{0,0,0,120});

    // Torches flanking the central panel.
    const float torchY  = height * 0.40f;
    const float torchLX = width  * 0.17f;
    const float torchRX = width  * 0.83f;
    DrawTorchFlame(torchLX, torchY, state.time, 0.0f,  scale);
    DrawTorchFlame(torchRX, torchY, state.time, 3.14f, scale);

    // Central panel.
    const int panelW  = Hud::Pixels(scale, 330);
    const int panelH  = Hud::Pixels(scale, 272);
    const int panelX  = (width  - panelW) / 2;
    const int panelY  = (height - panelH) / 2;
    const int border  = Hud::Pixels(scale, 2);
    DrawRectangle(panelX, panelY, panelW, panelH, PanelFill);
    DrawRectangleLines(panelX,          panelY,          panelW,          panelH,          BorderOuter);
    DrawRectangleLines(panelX + border, panelY + border, panelW - border*2, panelH - border*2, BorderInner);

    // Title.
    const int titleSz    = Hud::Pixels(scale, 34);
    const int subtitleSz = Hud::Pixels(scale, 17);
    const char* line1    = "MAZE RUNNER";
    const char* line2    = "Shadow Hunt";
    const int l1w        = MeasureText(line1, titleSz);
    const int l2w        = MeasureText(line2, subtitleSz);
    const int titleY     = panelY + Hud::Pixels(scale, 26);
    // Drop shadow then text.
    DrawText(line1, (width - l1w) / 2 + 2, titleY + 2,                           titleSz,    Color{0,0,0,180});
    DrawText(line1, (width - l1w) / 2,     titleY,                               titleSz,    TitleGold);
    DrawText(line2, (width - l2w) / 2 + 1, titleY + titleSz + Hud::Pixels(scale,3) + 1, subtitleSz, Color{0,0,0,150});
    DrawText(line2, (width - l2w) / 2,     titleY + titleSz + Hud::Pixels(scale,3),    subtitleSz, SubtitleTone);

    // Horizontal rule.
    const int ruleY   = titleY + titleSz + subtitleSz + Hud::Pixels(scale, 14);
    const int ruleX0  = panelX + Hud::Pixels(scale, 22);
    const int ruleW   = panelW - Hud::Pixels(scale, 44);
    DrawRectangle(ruleX0, ruleY, ruleW, Hud::Pixels(scale, 1), Divider);

    // Menu items.
    const char* labels[] = {"START GAME", "CONTROLS", "QUIT"};
    const int   itemSz   = Hud::Pixels(scale, 19);
    const int   spacing  = Hud::Pixels(scale, 44);
    const int   firstY   = ruleY + Hud::Pixels(scale, 22);
    const int   pad      = Hud::Pixels(scale, 8);

    for (int i = 0; i < 3; ++i)
    {
        const bool sel    = (static_cast<int>(state.selected) == i);
        const int  lw     = MeasureText(labels[i], itemSz);
        const int  itemX  = (width - lw) / 2;
        const int  itemY  = firstY + i * spacing;

        if (sel)
        {
            // Subtle highlight box.
            DrawRectangle(itemX - pad*2, itemY - pad/2, lw + pad*4, itemSz + pad,
                          ItemHighlight);
            DrawRectangleLines(itemX - pad*2, itemY - pad/2, lw + pad*4, itemSz + pad,
                               ItemHighlightBorder);
            // Arrow indicator.
            DrawText(">", itemX - Hud::Pixels(scale, 20), itemY, itemSz, ItemSelected);
        }
        // Shadow then label.
        DrawText(labels[i], itemX + 1, itemY + 1, itemSz, Color{0,0,0,150});
        DrawText(labels[i], itemX,     itemY,     itemSz, sel ? ItemSelected : ItemNormal);
    }

    // Navigation hint.
    const int   hintSz = Hud::Pixels(scale, 11);
    const char* hint   = "W/S or UP/DOWN to navigate   ENTER to select";
    const int   hintW  = MeasureText(hint, hintSz);
    DrawText(hint, (width - hintW) / 2,
             panelY + panelH - Hud::Pixels(scale, 20), hintSz, HintColor);
}

// Draws the controls overlay (reuses the HUD panel, adds a return prompt).
inline void DrawControlsScreen(int width, int height, float scale)
{
    Hud::DrawControls(width, height, scale);
    const int   sz  = Hud::Pixels(scale, 13);
    const char* msg = "Press Esc or Enter to return to menu";
    const int   mw  = MeasureText(msg, sz);
    // Position below the standard panel.
    DrawText(msg, (width - mw) / 2, height / 2 + Hud::Pixels(scale, 110), sz,
             Color{180, 165, 148, 220});
}

} // namespace MainMenu
#endif
