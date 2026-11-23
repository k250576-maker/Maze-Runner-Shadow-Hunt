#ifndef SHADOW_HUNT_PARCHMENT_MAP_H
#define SHADOW_HUNT_PARCHMENT_MAP_H

#include "exploration_map.h"
#include "game_hud.h"

namespace Parchment
{
constexpr int CompactCells = 17;
constexpr int CompactCellPixels = 6;
constexpr int ExpandedMargin = 22;
constexpr Color Ink{87, 57, 36, 255};
constexpr Color Border{137, 95, 56, 255};
constexpr Color Corridor{163, 122, 74, 255};
constexpr Color Wall{115, 78, 48, 255};
constexpr Color Marker{157, 38, 33, 255};

struct GridView { int column; int row; int columns; int rows; int pitch; };

inline void DrawPaper(int x, int y, int width, int height, float scale)
{
    if (width < 4 || height < 4) return;
    const Rectangle bounds{static_cast<float>(x), static_cast<float>(y),
                           static_cast<float>(width), static_cast<float>(height)};
    DrawRectangleGradientEx(bounds, Color{228, 198, 148, 255}, Color{196, 157, 106, 255},
                            Color{181, 139, 91, 255}, Color{220, 188, 137, 255});
    DrawRectangleLines(x, y, width, height, Ink);
    const int inset = Hud::Pixels(scale, 4);
    DrawRectangleLines(x + inset, y + inset, width - 2 * inset, height - 2 * inset, Border);
    const int corner = Hud::Pixels(scale, 6);
    for (const int dx : {-1, 1})
        for (const int dy : {-1, 1})
        {
            const int cx = dx > 0 ? x + inset : x + width - inset - 1;
            const int cy = dy > 0 ? y + inset : y + height - inset - 1;
            DrawLine(cx, cy + dy * corner, cx + dx * corner, cy, Ink);
        }
}

inline void DrawChart(const Exploration::Map& discovery, Vector2 player, GridView grid, int x, int y)
{
    for (int dy = 0; dy < grid.rows; ++dy)
        for (int dx = 0; dx < grid.columns; ++dx)
        {
            const auto tile = discovery.At(grid.column + dx, grid.row + dy);
            if (tile == Exploration::Tile::Unknown) continue;
            const int left = x + dx * grid.pitch;
            const int top = y + dy * grid.pitch;
            const bool passage = tile == Exploration::Tile::Passage;
            DrawRectangle(left, top, grid.pitch, grid.pitch, passage ? Corridor : Wall);
            // Fine ink flecks give the stone a hand-shaded edge at readable scales.
            if (!passage && grid.pitch >= 5 && (dx + grid.column + dy + grid.row) % 2 == 0)
                DrawLine(left + 1, top + grid.pitch - 2, left + grid.pitch - 2, top + 1, Ink);
        }

    const Vector2 center{x + (player.x / Maze::CellSize - grid.column) * grid.pitch,
                         y + (player.y / Maze::CellSize - grid.row) * grid.pitch};
    const float radius = std::max(2.0f, grid.pitch * 0.45f);
    DrawCircleV(center, radius + 1.0f, Color{246, 212, 141, 255});
    DrawCircleV(center, radius, Marker);
}

inline void DrawCompact(const Exploration::Map& discovery, Vector2 player, Maze::Cell cell, float scale)
{
    const int pitch = Hud::Pixels(scale, CompactCellPixels);
    const int columns = std::min(CompactCells, Maze::Columns);
    const int rows = std::min(CompactCells, Maze::Rows);
    const GridView grid{std::clamp(cell.column - columns / 2, 0, Maze::Columns - columns),
                        std::clamp(cell.row - rows / 2, 0, Maze::Rows - rows), columns, rows, pitch};
    const int margin = Hud::Pixels(scale, 16);
    const int padding = Hud::Pixels(scale, 10);
    const int header = Hud::Pixels(scale, 21);
    const int font = Hud::Pixels(scale, 14);
    const int width = columns * pitch + 2 * padding;
    const int height = rows * pitch + 2 * padding + header;
    DrawPaper(margin, margin, width, height, scale);
    DrawText("Map", margin + padding, margin + padding, font, Ink);
    DrawText("M", margin + width - padding - MeasureText("M", font), margin + padding, font, Ink);
    DrawLine(margin + padding, margin + padding + header - 3, margin + width - padding,
             margin + padding + header - 3, Border);
    DrawChart(discovery, player, grid, margin + padding, margin + padding + header);
}

inline void DrawExpanded(const Exploration::Map& discovery, Vector2 player, int width, int height, float scale)
{
    const int margin = Hud::Pixels(scale, ExpandedMargin);
    const int padding = Hud::Pixels(scale, 16);
    const int header = Hud::Pixels(scale, 35);
    const int pitch = std::min((width - 2 * (margin + padding)) / Maze::Columns,
                               (height - 2 * (margin + padding) - header) / Maze::Rows);
    if (pitch < 1) return;
    const int paperWidth = Maze::Columns * pitch + 2 * padding;
    const int paperHeight = Maze::Rows * pitch + 2 * padding + header;
    const int x = (width - paperWidth) / 2;
    const int y = (height - paperHeight) / 2;
    const int font = Hud::Pixels(scale, 20);
    const int keyFont = Hud::Pixels(scale, 14);
    DrawRectangle(0, 0, width, height, Color{8, 7, 9, 153});
    DrawPaper(x, y, paperWidth, paperHeight, scale);
    DrawText("Exploration map", x + padding, y + padding, font, Ink);
    DrawText("M / Esc", x + paperWidth - padding - MeasureText("M / Esc", keyFont),
             y + padding + (font - keyFont) / 2, keyFont, Ink);
    DrawLine(x + padding, y + padding + header - 4,
             x + paperWidth - padding, y + padding + header - 4, Border);
    DrawChart(discovery, player, {0, 0, Maze::Columns, Maze::Rows, pitch},
              x + padding, y + padding + header);
}
}
#endif
