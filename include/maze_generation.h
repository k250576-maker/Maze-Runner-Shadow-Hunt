#ifndef SHADOW_HUNT_MAZE_GENERATION_H
#define SHADOW_HUNT_MAZE_GENERATION_H

#include "maze.h"
#include <algorithm>
#include <cstdint>
#include <random>

namespace Maze
{
constexpr int ExtraPassages = 8;
constexpr int ChamberCount = static_cast<int>(Rooms::Id::Count);
constexpr int LogicalCount = LogicalColumns * LogicalRows;
constexpr Cell Directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
static_assert(LogicalColumns >= 12 && LogicalRows >= 12, "Leave space between the four chamber bays.");

constexpr Cell Neighbor(Cell cell, int direction)
{
    return {cell.column + Directions[direction].column, cell.row + Directions[direction].row};
}

constexpr bool ContainsLogical(Cell cell)
{
    return cell.column >= 0 && cell.column < LogicalColumns && cell.row >= 0 && cell.row < LogicalRows;
}

struct PassageGraph
{
    // Four neighbor bits per vertex. Add() keeps each connection bidirectional.
    std::uint8_t edges[LogicalRows][LogicalColumns]{};

    bool Has(Cell cell, int direction) const
    {
        return (edges[cell.row][cell.column] & (1 << direction)) != 0;
    }

    void Add(Cell cell, int direction)
    {
        const Cell next = Neighbor(cell, direction);
        edges[cell.row][cell.column] |= 1 << direction;
        edges[next.row][next.column] |= 1 << ((direction + 2) % 4);
    }
};

template<class T, int Capacity>
struct Stack
{
    T items[Capacity]{};
    int size = 0;
    void Push(T item) { items[size++] = item; }
    T Top() const { return items[size - 1]; }
    void Pop() { --size; }
    bool Empty() const { return size == 0; }
};

template<class T, int Capacity>
struct Queue
{
    // BFS marks tiles on enqueue, so no tile uses more than one slot.
    T items[Capacity]{};
    int head = 0;
    int tail = 0;
    void Push(T item) { items[tail++] = item; }
    T Pop() { return items[head++]; }
    bool Empty() const { return head == tail; }
};

struct Distances
{
    int value[Rows][Columns]{};
    int reached = 0;
};

inline Distances MeasureDistances(const Layout& layout, Cell start)
{
    Distances result;
    for (auto& row : result.value) std::fill_n(row, Columns, -1);
    if (!layout.IsWalkable(start.column, start.row)) return result;
    Queue<Cell, Rows * Columns> queue;
    queue.Push(start);
    result.value[start.row][start.column] = 0;
    while (!queue.Empty())
    {
        const Cell current = queue.Pop();
        ++result.reached;
        for (int direction = 0; direction < 4; ++direction)
        {
            const Cell next = Neighbor(current, direction);
            if (!layout.IsWalkable(next.column, next.row) || result.value[next.row][next.column] >= 0) continue;
            result.value[next.row][next.column] = result.value[current.row][current.column] + 1;
            queue.Push(next);
        }
    }
    return result;
}

inline Distances MeasureDistances(const Layout& layout)
{
    return MeasureDistances(layout, layout.spawn);
}

struct GeneratedLevel
{
    Layout layout;
    PassageGraph graph;
    int exitDistance = -1;
};

struct Edge { Cell from; int direction; };
struct Band { int first; int width; };

template<int Count>
inline void MakeBands(Band (&bands)[Count], std::mt19937& random)
{
    int order[Count];
    for (int i = 0; i < Count; ++i) order[i] = i;
    std::shuffle(order, order + Count, random);
    for (Band& band : bands) band.width = 2;
    for (int i = 0; i < WideBands; ++i) bands[order[i]].width = 3;
    int start = 1;
    for (Band& band : bands)
    {
        band.first = start;
        start += band.width + 1;
    }
}

inline PassageGraph CarveGraph(const int (&bays)[LogicalRows][LogicalColumns], std::mt19937& random)
{
    PassageGraph graph;
    bool visited[LogicalRows][LogicalColumns]{};
    Stack<Cell, LogicalCount> stack;
    const Cell start{2, 0};
    stack.Push(start);
    visited[start.row][start.column] = true;
    while (!stack.Empty())
    {
        const Cell cell = stack.Top();
        int options[4];
        int count = 0;
        for (int direction = 0; direction < 4; ++direction)
        {
            const Cell next = Neighbor(cell, direction);
            if (ContainsLogical(next) && bays[next.row][next.column] < 0 && !visited[next.row][next.column])
                options[count++] = direction;
        }
        if (count == 0)
        {
            stack.Pop(); // Backtrack to the nearest vertex with an unexplored neighbor.
            continue;
        }
        const int direction = options[std::uniform_int_distribution<int>(0, count - 1)(random)];
        const Cell next = Neighbor(cell, direction);
        graph.Add(cell, direction);
        visited[next.row][next.column] = true;
        stack.Push(next);
    }

    Edge candidates[2 * LogicalCount];
    int count = 0;
    for (int row = 0; row < LogicalRows; ++row)
        for (int column = 0; column < LogicalColumns; ++column)
            if (bays[row][column] < 0)
                for (int direction = 0; direction < 2; ++direction)
                {
                    const Cell next = Neighbor({column, row}, direction);
                    if (ContainsLogical(next) && bays[next.row][next.column] < 0 && !graph.Has({column, row}, direction))
                        candidates[count++] = {{column, row}, direction};
                }
    std::shuffle(candidates, candidates + count, random);
    for (int i = 0; i < std::min(count, ExtraPassages); ++i)
        graph.Add(candidates[i].from, candidates[i].direction);
    return graph;
}

inline Rooms::CellArea Gate(Cell from, Cell to, const Band* columns, const Band* rows)
{
    const Band x = columns[std::min(from.column, to.column)];
    const Band y = rows[std::min(from.row, to.row)];
    if (from.column != to.column)
        return {x.first + x.width, y.first, x.first + x.width, y.first + y.width - 1};
    return {x.first, y.first + y.width, x.first + x.width - 1, y.first + y.width};
}

inline void Open(Layout& layout, Rooms::CellArea area)
{
    for (int row = area.top; row <= area.bottom; ++row)
        for (int column = area.left; column <= area.right; ++column)
            layout.floor[row][column] = true;
}

inline void PlaceTorches(Layout& layout, std::mt19937& random)
{
    // Each chamber gets a north-wall fixture. The remaining fixtures are spread along passages.
    for (const Rooms::Room& room : layout.rooms)
    {
        for (int column = (room.bounds.left + room.bounds.right) / 2; column >= room.bounds.left; --column)
        {
            if (layout.IsWalkable(column, room.bounds.top - 1)) continue;
            layout.torchSites[layout.torchCount++] = {column, room.bounds.top};
            break;
        }
    }
    Cell candidates[Rows * Columns];
    int count = 0;
    for (int row = 1; row < Rows - 1; ++row)
        for (int column = 1; column < Columns - 1; ++column)
            if (layout.IsWalkable(column, row) && !layout.IsWalkable(column, row - 1))
                candidates[count++] = {column, row};
    std::shuffle(candidates, candidates + count, random);
    constexpr int minimumSpacing = 6;
    for (int i = 0; i < count && layout.torchCount < MaxTorches; ++i)
    {
        const Cell point = candidates[i];
        bool spaced = true;
        for (int j = 0; j < layout.torchCount && spaced; ++j)
        {
            const int dx = point.column - layout.torchSites[j].column;
            const int dy = point.row - layout.torchSites[j].row;
            spaced = dx * dx + dy * dy >= minimumSpacing * minimumSpacing;
        }
        if (spaced) layout.torchSites[layout.torchCount++] = point;
    }
}

inline bool Generate(std::uint32_t seed, GeneratedLevel& result)
{
    result = {};
    std::mt19937 random(seed);
    const auto choose = [&random](int low, int high) { return std::uniform_int_distribution<int>(low, high)(random); };
    const Cell origins[ChamberCount] = {
        {0, 0},
        {choose(LogicalColumns / 2 + 1, LogicalColumns - 2), choose(1, LogicalRows / 2 - 3)},
        {choose(1, LogicalColumns / 2 - 3), choose(LogicalRows / 2 + 1, LogicalRows - 2)},
        {choose(LogicalColumns / 2 + 1, LogicalColumns - 2), choose(LogicalRows / 2 + 1, LogicalRows - 2)}
    };
    int bays[LogicalRows][LogicalColumns];
    for (auto& row : bays) std::fill_n(row, LogicalColumns, -1);
    for (int i = 0; i < ChamberCount; ++i)
        for (int row = origins[i].row; row < origins[i].row + 2; ++row)
            for (int column = origins[i].column; column < origins[i].column + 2; ++column)
                bays[row][column] = i;

    result.graph = CarveGraph(bays, random);
    Layout& layout = result.layout;
    Band columns[LogicalColumns];
    Band rows[LogicalRows];
    MakeBands(columns, random);
    MakeBands(rows, random);

    for (int i = 0; i < ChamberCount; ++i)
    {
        const Cell origin = origins[i];
        Edge doors[8];
        int doorCount = 0;
        for (int row = origin.row; row < origin.row + 2; ++row)
            for (int column = origin.column; column < origin.column + 2; ++column)
                for (int direction = 0; direction < 4; ++direction)
                {
                    const Cell next = Neighbor({column, row}, direction);
                    if (!ContainsLogical(next)) continue;
                    if (bays[next.row][next.column] < 0) doors[doorCount++] = {{column, row}, direction};
                    else if (direction < 2 && bays[next.row][next.column] == i)
                        result.graph.Add({column, row}, direction);
                }
        if (doorCount == 0) return false;
        const Edge door = doors[choose(0, doorCount - 1)];
        result.graph.Add(door.from, door.direction);
        const Band endX = columns[origin.column + 1];
        const Band endY = rows[origin.row + 1];
        layout.rooms[i] = {{columns[origin.column].first, rows[origin.row].first,
                            endX.first + endX.width - 1, endY.first + endY.width - 1},
                           Gate(door.from, Neighbor(door.from, door.direction), columns, rows)};
        Open(layout, layout.rooms[i].bounds);
    }

    // Rasterize only graph edges. Unconnected vertices always retain a stone separator.
    for (int row = 0; row < LogicalRows; ++row)
        for (int column = 0; column < LogicalColumns; ++column)
        {
            const Band x = columns[column];
            const Band y = rows[row];
            Open(layout, {x.first, y.first, x.first + x.width - 1, y.first + y.width - 1});
            for (int direction = 0; direction < 2; ++direction)
            {
                if (!result.graph.Has({column, row}, direction)) continue;
                const Cell next = Neighbor({column, row}, direction);
                auto gate = Gate({column, row}, next, columns, rows);
                if (bays[row][column] < 0 && bays[next.row][next.column] < 0 && choose(0, 15) == 0)
                {
                    if (direction == 0) gate.top = gate.bottom = (gate.top + gate.bottom) / 2;
                    else gate.left = gate.right = (gate.left + gate.right) / 2;
                }
                Open(layout, gate);
            }
        }

    const auto entrance = layout.rooms[0].bounds;
    layout.spawn = {(entrance.left + entrance.right) / 2, (entrance.top + entrance.bottom) / 2};
    const Distances distances = MeasureDistances(layout);
    int walkable = 0;
    for (const auto& row : layout.floor)
        for (bool open : row) walkable += open;
    if (distances.reached != walkable) return false;

    // The farthest tile in the three non-entrance chambers reserves a reachable future exit.
    int farthestRoom = 1;
    for (int i = 1; i < ChamberCount; ++i)
    {
        const auto bounds = layout.rooms[i].bounds;
        for (int row = bounds.top; row <= bounds.bottom; ++row)
            for (int column = bounds.left; column <= bounds.right; ++column)
                if (distances.value[row][column] > result.exitDistance)
                {
                    result.exitDistance = distances.value[row][column];
                    layout.futureExit = {column, row};
                    farthestRoom = i;
                }
    }
    std::swap(layout.rooms[farthestRoom], layout.rooms[static_cast<int>(Rooms::Id::Special)]);
    PlaceTorches(layout, random);
    return result.exitDistance > 0;
}
}
#endif
