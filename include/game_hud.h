#ifndef SHADOW_HUNT_GAME_HUD_H
#define SHADOW_HUNT_GAME_HUD_H

#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace Hud
{
constexpr Color Gold{229, 205, 167, 255};
constexpr Color Text{208, 199, 190, 255};
constexpr Color Border{100, 82, 75, 255};
constexpr Color Panel{25, 23, 27, 250};
constexpr Color KeyBackground{40, 35, 39, 255};
constexpr Color Brass{131, 105, 70, 255};
constexpr Color DimText{167, 153, 138, 255};

inline int Pixels(float scale, int size)
{
    return std::max(1, static_cast<int>(std::round(scale * size)));
}

inline void DrawStonePanel(Rectangle bounds)
{
    constexpr float roundness = 0.07f;
    const Rectangle shadow{bounds.x + 3, bounds.y + 5, bounds.width, bounds.height};
    const Rectangle inner{bounds.x + 5, bounds.y + 5, bounds.width - 10, bounds.height - 10};
    DrawRectangleRounded(shadow, roundness, 4, Color{3, 2, 5, 170});
    DrawRectangleRounded(bounds, roundness, 4, Panel);
    DrawRectangleRoundedLinesEx(bounds, roundness, 4, 2, Brass);
    DrawRectangleRoundedLines(inner, roundness, 4, Border);
}

inline void DrawCenteredText(const char* text, int centerX, int y, int size, Color color)
{
    DrawText(text, centerX - MeasureText(text, size) / 2, y, size, color);
}

inline void DrawDivider(int x, int y, int width)
{
    DrawRectangle(x, y, width, 2, Border);
    DrawRectangle(x + width / 2 - 14, y, 28, 2, Brass);
    DrawPoly({x + width * 0.5f, static_cast<float>(y + 1)}, 4, 4, 0, Brass);
}

inline void DrawLocation(int width, float scale, const char* name)
{
    const int font = Pixels(scale, 16);
    const int padding = Pixels(scale, 8);
    const int keySize = Pixels(scale, 24);
    const int nameWidth = MeasureText(name, font);
    const int stripWidth = nameWidth + 3 * padding + keySize;
    if (stripWidth + 2 * padding > width) return;
    const int x = width - padding * 2 - stripWidth;
    const int y = padding * 2;
    DrawRectangle(x, y, stripWidth, keySize + 2 * padding, Color{15, 15, 20, 190});
    DrawText(name, x + padding, y + padding + (keySize - font) / 2, font, Gold);
    const int keyX = x + nameWidth + 2 * padding;
    DrawRectangle(keyX, y + padding, keySize, keySize, KeyBackground);
    DrawRectangleLines(keyX, y + padding, keySize, keySize, Border);
    DrawText("I", keyX + (keySize - MeasureText("I", font)) / 2,
             y + padding + (keySize - font) / 2, font, Text);
}

inline void DrawStatus(int width, float scale, const char* text, bool sanctuary)
{
    if (!text) return;
    const int font = Pixels(scale, 14), pad = Pixels(scale, 6);
    const int w = MeasureText(text, font) + 2 * pad;
    const int x = width - Pixels(scale, 16) - w, y = Pixels(scale, 62);
    DrawRectangleRounded({float(x),float(y),float(w),float(font+2*pad)}, 0.25f, 4, Panel);
    DrawText(text, x+pad, y+pad, font, sanctuary ? Gold : Color{239,180,155,255});
}

inline void DrawHitFlash(int width, int height, float strength, bool angelDeath)
{
    if (strength <= 0) return;
    if (angelDeath)
        DrawRectangle(0, 0, width, height, {105,102,114, static_cast<unsigned char>(85 * strength)});
    const Color edge{static_cast<unsigned char>(angelDeath ? 109 : 187), 20, 32,
                     static_cast<unsigned char>((angelDeath ? 220 : 140) * strength)};
    const int w = width / 8, h = height / 8;
    DrawRectangleGradientH(0, 0, w, height, edge, BLANK);
    DrawRectangleGradientH(width-w, 0, w, height, BLANK, edge);
    DrawRectangleGradientV(0, 0, width, h, edge, BLANK);
    DrawRectangleGradientV(0, height-h, width, h, BLANK, edge);
}

inline void DrawHealth(int width, int height, float scale, int points, int capacity, float shown)
{
    const int size = Pixels(scale, 14);
    const int pad = Pixels(scale, 8);
    const int barHeight = Pixels(scale, 8);
    const int stripWidth = Pixels(scale, 194);
    const int stripHeight = size + barHeight + pad * 3;
    const int x = width - stripWidth - pad * 2, y = height - stripHeight - pad * 2;
    const Rectangle strip{float(x),float(y),float(stripWidth),float(stripHeight)};
    const float ratio = capacity > 0 ? float(points) / capacity : 0;
    const bool low = ratio > 0 && ratio < 0.30f;
    DrawRectangleRounded(strip, 0.20f, 4, {21,17,23,242});
    DrawRectangleRoundedLines(strip, 0.20f, 4, low ? Color{163,68,58,255} : Border);
    DrawText("Health", x+pad, y+pad, size, Gold);
    const char* reading = TextFormat("%d/%d   %d%%", points, capacity, static_cast<int>(std::lround(ratio*100)));
    DrawText(reading, x+stripWidth-pad-MeasureText(reading,size), y+pad, size, Text);
    const int left = x + pad, top = y + size + 2*pad, barWidth = stripWidth-2*pad;
    DrawRectangle(left,top,barWidth,barHeight,{59,25,33,255});
    const int filled = static_cast<int>(barWidth * std::clamp(shown, 0.0f, 1.0f));
    DrawRectangleGradientV(left,top,filled,barHeight,{224,93,83,255},{147,36,48,255});
    DrawRectangleLines(left,top,barWidth,barHeight,{119,70,61,220});
}

inline void DrawRun(int width, int height, float scale, int rightInset, const char* time, unsigned long long souls)
{
    const int size = Pixels(scale, 14), pad = Pixels(scale, 8);
    const char* label = TextFormat("%s    Souls %llu", time, souls);
    const int w = MeasureText(label,size) + pad*2 + size;
    const float x = float(width - rightInset - w);
    const float y = float(height - (size+pad*2) - pad*2);
    DrawRectangleRounded({x,y,float(w),float(size+pad*2)},0.20f,4,{21,17,23,235});
    DrawRectangleRoundedLines({x,y,float(w),float(size+pad*2)},0.20f,4,Border);
    DrawPoly({x+pad+size*0.28f,y+pad+size*0.50f},4,size*0.28f,0,{120,215,201,255});
    DrawText(label, int(x)+pad+size, int(y)+pad, size, Text);
}

inline void DrawControls(int width, int height, const char* footer)
{
    struct Row { const char* action; const char* keys; };
    constexpr Row rows[] = {
        {"Move", "WASD / Arrow keys"},
        {"Facing melee attack", "Space"},
        {"Exploration map", "M"},
        {"Mobcyclopedia", "B"},
        {"Toggle fullscreen", "F11"},
        {"Toggle controls", "I"},
        {"Close overlay / return to menu", "Esc"}
    };
    constexpr int panelWidth = 466;
    constexpr int padding = 26;
    constexpr int titleSize = 28;
    constexpr int titleRuleY = padding + titleSize + 11;
    constexpr int firstRowY = titleRuleY + 15;
    constexpr int rowHeight = 37;
    constexpr int bodySize = 18;
    constexpr int keySize = 16;
    constexpr int keyHeight = 28;
    constexpr int footerSize = 15;
    constexpr int footerRuleY = firstRowY + rowHeight * (sizeof(rows) / sizeof(rows[0])) + 4;
    constexpr int footerY = footerRuleY + 15;
    constexpr int panelHeight = footerY + footerSize + padding;
    constexpr int contentWidth = panelWidth - 2 * padding;
    const float scale = std::min(width / 580.0f, height / (panelHeight + 96.0f));
    const Camera2D layout{{(width - panelWidth * scale) * 0.5f,
                           (height - panelHeight * scale) * 0.5f}, {0, 0}, 0, scale};

    DrawRectangle(0, 0, width, height, Color{7, 7, 11, 176});
    BeginMode2D(layout);
    DrawStonePanel({0, 0, panelWidth, panelHeight});
    DrawCenteredText("Controls", panelWidth / 2, padding, titleSize, Gold);
    DrawDivider(padding, titleRuleY, contentWidth);
    int rowY = firstRowY;
    for (const Row& row : rows)
    {
        const int keyWidth = std::max(53, MeasureText(row.keys, keySize) + 22);
        const int keyX = panelWidth - padding - keyWidth;
        const Rectangle keyBox{static_cast<float>(keyX), static_cast<float>(rowY),
                               static_cast<float>(keyWidth), keyHeight};
        DrawText(row.action, padding, rowY + (keyHeight - bodySize) / 2, bodySize, Text);
        DrawRectangleRounded(keyBox, 0.20f, 4, KeyBackground);
        DrawRectangleRoundedLines(keyBox, 0.20f, 4, Brass);
        DrawCenteredText(row.keys, keyX + keyWidth / 2, rowY + (keyHeight - keySize) / 2,
                         keySize, Gold);
        rowY += rowHeight;
    }
    DrawDivider(padding, footerRuleY, contentWidth);
    DrawCenteredText(footer, panelWidth / 2, footerY, footerSize, DimText);
    EndMode2D();
}
}
#endif
