#ifndef SHADOW_HUNT_DUNGEON_ART_H
#define SHADOW_HUNT_DUNGEON_ART_H

#include "maze.h"
#include "raylib.h"
#include <cmath>

namespace DungeonArt
{
// Native-size swatches from mainlevbuild.png.
constexpr Rectangle Slate{832, 320, 32, 32};
constexpr Rectangle WornSlate{880, 320, 32, 32};
constexpr Rectangle Masonry{736, 272, 32, 32};
constexpr Rectangle WallTrim{512, 128, 32, 8};
constexpr Color WallTint{192, 181, 172, 255};

struct Runner { int column; int row; int columns; int rows; };

inline void DrawWall(Texture2D atlas, const Maze::Layout& level, int column, int row, Color unlitStone)
{
    constexpr int cell = Maze::CellSize;
    const int x = column * cell;
    const int y = row * cell;
    const Vector2 position{static_cast<float>(x), static_cast<float>(y)};
    if (!Maze::Contains(column, row))
    {
        DrawTextureRec(atlas, Masonry, position, unlitStone);
        return;
    }

    DrawTextureRec(atlas, Masonry, position, WallTint);
    constexpr Color edgeShadow{28, 25, 27, 255};
    constexpr Color edgeStone{82, 71, 65, 255};
    if (level.IsWalkable(column - 1, row))
    {
        DrawRectangle(x, y, 5, cell, edgeShadow);
        DrawRectangle(x + 1, y, 2, cell, edgeStone);
    }
    if (level.IsWalkable(column + 1, row))
    {
        DrawRectangle(x + cell - 5, y, 5, cell, edgeShadow);
        DrawRectangle(x + cell - 3, y, 2, cell, edgeStone);
    }
    if (level.IsWalkable(column, row - 1))
    {
        const Rectangle reverseTrim{WallTrim.x, WallTrim.y, WallTrim.width, -WallTrim.height};
        DrawTextureRec(atlas, reverseTrim, position, Color{189, 176, 162, 255});
    }
    if (level.IsWalkable(column, row + 1))
        DrawTextureRec(atlas, WallTrim, {position.x, position.y + cell - WallTrim.height}, WHITE);
}

inline void DrawRunner(Runner runner)
{
    constexpr int cell = Maze::CellSize;
    constexpr int inset = 5;
    const int x = runner.column * cell + inset;
    const int y = runner.row * cell + inset;
    const int width = runner.columns * cell - 2 * inset;
    const int height = runner.rows * cell - 2 * inset;
    constexpr Color darkHem{72, 37, 40, 255};
    constexpr Color gold{159, 115, 68, 255};
    constexpr Color burgundy{104, 32, 49, 255};
    constexpr Color pattern{139, 62, 64, 255};
    DrawRectangle(x, y, width, height, darkHem);
    DrawRectangle(x + 2, y + 2, width - 4, height - 4, gold);
    DrawRectangle(x + 4, y + 4, width - 8, height - 8, burgundy);
    DrawRectangleLines(x + 8, y + 8, width - 16, height - 16, pattern);

    const int centerX = x + width / 2;
    const int centerY = y + height / 2;
    const auto drawMotif = [centerY](int motifX) {
        DrawRectangle(motifX - 4, centerY - 2, 8, 4, pattern);
        DrawRectangle(motifX - 2, centerY - 4, 4, 8, pattern);
    };
    drawMotif(centerX);
    for (int offset = cell; offset <= width / 2 - 20; offset += cell)
    {
        drawMotif(centerX - offset);
        drawMotif(centerX + offset);
    }
    DrawRectangle(x + 11, y + 8, 2, height - 16, gold);
    DrawRectangle(x + width - 13, y + 8, 2, height - 16, gold);
}

inline void Draw(Texture2D atlas, const Maze::Layout& level, Rectangle visible, Color ambient)
{
    const Color unlitStone = ColorTint(WallTint, ambient);
    const int firstColumn = static_cast<int>(std::floor(visible.x / Maze::CellSize));
    const int lastColumn = static_cast<int>(std::ceil((visible.x + visible.width) / Maze::CellSize));
    const int firstRow = static_cast<int>(std::floor(visible.y / Maze::CellSize));
    const int lastRow = static_cast<int>(std::ceil((visible.y + visible.height) / Maze::CellSize));
    for (int row = firstRow; row < lastRow; ++row)
    {
        for (int column = firstColumn; column < lastColumn; ++column)
        {
            if (!level.IsWalkable(column, row))
            {
                DrawWall(atlas, level, column, row, unlitStone);
                continue;
            }
            const bool worn = (column * 17 + row * 23 + column * row) % 13 == 0;
            const Vector2 position{static_cast<float>(column * Maze::CellSize),
                                   static_cast<float>(row * Maze::CellSize)};
            DrawTextureRec(atlas, worn ? WornSlate : Slate, position, Color{216, 224, 227, 255});
        }
    }
    constexpr Rooms::Id carpeted[] = {Rooms::Id::Entrance, Rooms::Id::Library, Rooms::Id::Special};
    for (Rooms::Id id : carpeted)
    {
        const auto room = level.rooms[static_cast<int>(id)].bounds;
        DrawRunner({room.left + 1, (room.top + room.bottom) / 2 - 1, room.right - room.left - 1, 3});
    }
}
}
#endif
