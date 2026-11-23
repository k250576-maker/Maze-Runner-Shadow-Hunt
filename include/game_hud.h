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

inline int Pixels(float scale, int size)
{
    return std::max(1, static_cast<int>(std::round(scale * size)));
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

inline void DrawControls(int width, int height, float scale)
{
    struct Row { const char* action; const char* keys; };
    constexpr Row rows[] = {
        {"Move", "WASD / Arrow keys"},
        {"Exploration map", "M"},
        {"Toggle fullscreen", "F11"},
        {"Toggle controls", "I"},
        {"Close overlay / quit", "Esc"}
    };
    const int panelWidth = Pixels(scale, 426);
    const int panelHeight = Pixels(scale, 240);
    const int x = (width - panelWidth) / 2;
    const int y = (height - panelHeight) / 2;
    const int padding = Pixels(scale, 22);
    const int bodySize = Pixels(scale, 16);
    const int keyPadding = Pixels(scale, 7);
    const int rowHeight = Pixels(scale, 32);
    const int contentRight = x + panelWidth - padding;

    DrawRectangle(0, 0, width, height, Color{7, 7, 11, 145});
    if (panelWidth < 4 || panelHeight < 4) return;
    DrawRectangle(x, y, panelWidth, panelHeight, Panel);
    DrawRectangleLines(x, y, panelWidth, panelHeight, Border);
    DrawText("Controls", x + padding, y + padding, Pixels(scale, 24), Gold);
    int rowY = y + Pixels(scale, 60);
    DrawLine(x + padding, rowY - keyPadding, contentRight, rowY - keyPadding, Border);
    for (const Row& row : rows)
    {
        const int keyWidth = MeasureText(row.keys, bodySize) + 2 * keyPadding;
        const int keyX = contentRight - keyWidth;
        DrawText(row.action, x + padding, rowY + keyPadding / 2, bodySize, Text);
        DrawRectangle(keyX, rowY, keyWidth, bodySize + keyPadding, KeyBackground);
        DrawText(row.keys, keyX + keyPadding, rowY + keyPadding / 2, bodySize, Gold);
        rowY += rowHeight;
    }
}
}
#endif
