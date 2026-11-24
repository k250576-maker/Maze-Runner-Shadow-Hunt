#ifndef SHADOW_HUNT_ENEMY_PATHFINDING_H
#define SHADOW_HUNT_ENEMY_PATHFINDING_H

#include "enemy.h"
#include "maze_generation.h"
#include "torch_state.h"
#include <algorithm>
#include <limits>
#include <vector>

namespace Enemies
{
inline void Reconstruct(Route& route, const std::array<int, CellCount>& parent, int goal)
{
    for (int id = goal; id != NoCell; id = parent[id])
        route.cells[route.length++] = id;
    std::reverse(route.cells.begin(), route.cells.begin() + route.length);
}

inline void BeginSearch(Route& route)
{
    route.next = route.length = route.nodesExplored = 0;
    ++route.searches;
}

constexpr int FloorCost = 1;
constexpr int WallCost = 5;

class TorchBarrier
{
public:
    static constexpr float Radius = Torches::ProtectionRadiusCells * Maze::CellSize;
    static constexpr int Patch = 4;
    static constexpr int Width = Maze::Columns * Maze::CellSize / Patch;
    static constexpr int Height = Maze::Rows * Maze::CellSize / Patch;

    explicit TorchBarrier(const Maze::Layout& layout) : level(layout), lit(Width * Height, false) {}

    bool Room(Vector2 point, float margin = 0) const
    {
        const auto room = level.rooms[static_cast<int>(Rooms::Id::Entrance)].bounds;
        return point.x >= room.left * Maze::CellSize - margin && point.y >= room.top * Maze::CellSize - margin &&
               point.x <= (room.right + 1) * Maze::CellSize + margin && point.y <= (room.bottom + 1) * Maze::CellSize + margin;
    }

    bool Refresh(const Torches::State& torches)
    {
        if (ready && revision == torches.Revision()) return false;
        ready = true;
        revision = torches.Revision();
        for (int i = 0; i < torches.Count(); ++i)
        {
            if (!torches.IsLit(i) || mapped[i]) continue;
            mapped[i] = true;
            const Vector2 source = torches.WorldPosition(i);
            for (int y = std::max(0, int((source.y - Radius) / Patch)); y <= std::min(Height-1, int((source.y + Radius) / Patch)); ++y)
                for (int x = std::max(0, int((source.x - Radius) / Patch)); x <= std::min(Width-1, int((source.x + Radius) / Patch)); ++x)
                {
                    const Vector2 center{(x + 0.5f) * Patch, (y + 0.5f) * Patch};
                    if (DistanceSquared(source, center) > (Radius + 3) * (Radius + 3)) continue;
                    // Four-pixel patches align with tile edges. Only illuminated floor contributes.
                    if (!Maze::IsWalkablePosition(level, center)) continue;
                    for (float dy : {-1.99f, 0.0f, 1.99f})
                        for (float dx : {-1.99f, 0.0f, 1.99f})
                            if (Maze::HasLineOfSight(level, source, {center.x + dx, center.y + dy}))
                                lit[y * Width + x] = true;
                }
        }
        blocked.fill(0);
        for (int id = 0; id < CellCount; ++id)
            for (int d = 0; d < 2; ++d)
            {
                const auto c = Maze::Neighbor(CellAt(id), d);
                if (!Maze::Contains(c.column, c.row)) continue;
                const int next = CellId(c);
                if (SegmentClear(Center(id), Center(next))) continue;
                blocked[id] |= 1 << d;
                blocked[next] |= 1 << (d + 2);
            }
        return true;
    }

    bool BlocksEdge(int id, int d) const { return (blocked[id] & (1 << d)) != 0; }
    bool IsClear(Vector2 point) const { return SegmentClear(point, point); }

    bool SegmentClear(Vector2 a, Vector2 b) const
    {
        const auto room = level.rooms[static_cast<int>(Rooms::Id::Entrance)].bounds;
        constexpr float unit = Maze::CellSize;
        constexpr float buffer = unit + AngelRadius;
        if (Intersects(a, b, room.left*unit-buffer, room.top*unit-buffer,
                       (room.right+1)*unit+buffer, (room.bottom+1)*unit+buffer)) return false;
        if (!revision) return true;
        const int left = std::max(0, int((std::min(a.x,b.x)-AngelRadius)/Patch));
        const int top = std::max(0, int((std::min(a.y,b.y)-AngelRadius)/Patch));
        const int right = std::min(Width-1, int((std::max(a.x,b.x)+AngelRadius)/Patch));
        const int bottom = std::min(Height-1, int((std::max(a.y,b.y)+AngelRadius)/Patch));
        for (int y = top; y <= bottom; ++y)
            for (int x = left; x <= right; ++x)
                if (lit[y*Width+x] && Intersects(a,b,x*Patch-AngelRadius,y*Patch-AngelRadius,
                    (x+1)*Patch+AngelRadius,(y+1)*Patch+AngelRadius)) return false;
        return true;
    }

private:
    static bool Intersects(Vector2 a, Vector2 b, float left, float top, float right, float bottom)
    {
        float enter = 0, leave = 1;
        const auto clip = [&](float start, float delta, float lo, float hi) {
            if (delta == 0) return start >= lo && start <= hi;
            float first = (lo - start) / delta, last = (hi - start) / delta;
            if (first > last) std::swap(first, last);
            enter = std::max(enter, first);
            leave = std::min(leave, last);
            return enter <= leave;
        };
        return clip(a.x,b.x-a.x,left,right) && clip(a.y,b.y-a.y,top,bottom);
    }

    const Maze::Layout& level;
    std::vector<bool> lit;
    std::array<bool,Maze::MaxTorches> mapped{};
    std::array<unsigned char, CellCount> blocked{};
    unsigned revision = 0;
    bool ready = false;
};

template <typename BlocksEdge>
inline void FindFloorRoute(const Maze::Layout& level, int start, int goal, Route& route,
                           Vector2 home, BlocksEdge blocked)
{
    BeginSearch(route);
    if (start == NoCell || goal == NoCell) return;
    std::array<int, CellCount> parent;
    parent.fill(NoCell);
    std::array<bool, CellCount> visited{};
    Maze::Queue<int, CellCount> queue;
    visited[start] = true;
    queue.Push(start);
    while (!queue.Empty())
    {
        const int current = queue.Pop();
        ++route.nodesExplored;
        if (current == goal)
        {
            Reconstruct(route, parent, goal);
            return;
        }
        for (int direction = 0; direction < 4; ++direction)
        {
            const Maze::Cell cell = Maze::Neighbor(CellAt(current), direction);
            if (!level.IsWalkable(cell.column, cell.row) || blocked(current, direction, cell)) continue;
            const int next = CellId(cell);
            if (DistanceSquared(Center(next), home) > LeashRadius * LeashRadius || visited[next]) continue;
            visited[next] = true;
            parent[next] = current;
            queue.Push(next);
        }
    }
}

inline void FindAngelRoute(const Maze::Layout& level, int start, int goal, Route& route,
                           const TorchBarrier& barrier, Vector2 home)
{
    FindFloorRoute(level, start, goal, route, home,
        [&](int from, int direction, Maze::Cell) { return barrier.BlocksEdge(from, direction); });
}

// Indexed binary min-heap. Each cell occupies at most one slot, so decreasing
// a key sifts its existing entry up instead of inserting a duplicate.
class MinHeap
{
public:
    explicit MinHeap(const std::array<int, CellCount>& cost) : cost(cost) { slots.fill(NoCell); }
    bool Empty() const { return size == 0; }

    void Improve(int id)
    {
        int slot = slots[id];
        if (slot == NoCell)
        {
            slot = size++;
            heap[slot] = id;
            slots[id] = slot;
        }
        while (slot > 0)
        {
            const int parent = (slot - 1) / 2;
            if (!Less(slot, parent)) break;
            Swap(slot, parent);
            slot = parent;
        }
    }

    int Pop()
    {
        const int id = heap[0];
        Swap(0, --size);
        slots[id] = NoCell;
        for (int slot = 0, child = 1; child < size; child = 2 * slot + 1)
        {
            if (child + 1 < size && Less(child + 1, child)) ++child;
            if (!Less(child, slot)) break;
            Swap(slot, child);
            slot = child;
        }
        return id;
    }

private:
    bool Less(int a, int b) const
    {
        return cost[heap[a]] < cost[heap[b]] || (cost[heap[a]] == cost[heap[b]] && heap[a] < heap[b]);
    }

    void Swap(int a, int b)
    {
        std::swap(heap[a], heap[b]);
        slots[heap[a]] = a;
        slots[heap[b]] = b;
    }

    const std::array<int, CellCount>& cost;
    std::array<int, CellCount> heap{};
    std::array<int, CellCount> slots{};
    int size = 0;
};

inline bool InEntrance(const Maze::Layout& level, Maze::Cell cell)
{
    const auto room = level.rooms[static_cast<int>(Rooms::Id::Entrance)].bounds;
    return cell.column >= room.left && cell.column <= room.right
        && cell.row >= room.top && cell.row <= room.bottom;
}

inline int Manhattan(int from, int goal)
{
    const Maze::Cell a = CellAt(from), b = CellAt(goal);
    return std::abs(a.column - b.column) + std::abs(a.row - b.row);
}

inline void FindSentinelRoute(const Maze::Layout& level, int start, int goal, Route& route, Vector2 home)
{
    BeginSearch(route);
    if (start == NoCell || goal == NoCell) return;
    std::array<int, CellCount> cost, rank, parent;
    cost.fill(std::numeric_limits<int>::max());
    parent.fill(NoCell);
    MinHeap frontier(rank);
    cost[start] = 0;
    rank[start] = Manhattan(start, goal);
    frontier.Improve(start);
    while (!frontier.Empty())
    {
        const int current = frontier.Pop();
        ++route.nodesExplored;
        if (current == goal)
        {
            Reconstruct(route, parent, goal);
            return;
        }
        for (int direction = 0; direction < 4; ++direction)
        {
            const auto cell = Maze::Neighbor(CellAt(current), direction);
            if (!level.IsWalkable(cell.column, cell.row) || InEntrance(level, cell)) continue;
            const int next = CellId(cell);
            const int candidate = cost[current] + FloorCost;
            if (candidate >= cost[next] || DistanceSquared(Center(next), home) > LeashRadius * LeashRadius) continue;
            cost[next] = candidate;
            // Cardinal, unit-cost steps make Manhattan distance a consistent lower bound.
            rank[next] = candidate + Manhattan(next, goal);
            parent[next] = current;
            frontier.Improve(next);
        }
    }
}

inline void FindGhostRoute(const Maze::Layout& level, const TorchBarrier& barrier,
                           int start, int goal, Route& route, Vector2 home)
{
    BeginSearch(route);
    if (start == NoCell || goal == NoCell || !barrier.IsClear(Center(start))) return;
    std::array<int, CellCount> cost, parent;
    cost.fill(std::numeric_limits<int>::max());
    parent.fill(NoCell);
    MinHeap queue(cost);
    cost[start] = 0;
    queue.Improve(start);
    const Vector2 target = Center(goal);
    int nearest = start;
    float nearestDistance = DistanceSquared(Center(start), target);
    while (!queue.Empty())
    {
        const int current = queue.Pop();
        ++route.nodesExplored;
        const float distance = DistanceSquared(Center(current), target);
        if (distance < nearestDistance || (distance == nearestDistance && cost[current] < cost[nearest]))
        {
            nearest = current;
            nearestDistance = distance;
        }
        if (current == goal) break;
        for (int direction = 0; direction < 4; ++direction)
        {
            const auto cell = Maze::Neighbor(CellAt(current), direction);
            if (!Maze::Contains(cell.column, cell.row) || barrier.BlocksEdge(current, direction)) continue;
            const int next = CellId(cell);
            if (DistanceSquared(Center(next), home) > LeashRadius * LeashRadius) continue;
            const int candidate = cost[current] + (level.IsWalkable(cell.column, cell.row) ? FloorCost : WallCost);
            if (candidate >= cost[next]) continue;
            cost[next] = candidate;
            parent[next] = current;
            queue.Improve(next);
        }
    }
    Reconstruct(route, parent, nearest);
}
}
#endif
