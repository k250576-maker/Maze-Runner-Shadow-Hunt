#ifndef SHADOW_HUNT_ENEMY_MANAGER_H
#define SHADOW_HUNT_ENEMY_MANAGER_H

#include "enemy_pathfinding.h"
#include "enemy_contact.h"
#include "lighting.h"
#include "torch_state.h"
#include <algorithm>
#include <limits>

namespace Enemies
{
constexpr int MinimumSpawnSteps = 32;
constexpr int AngelSpawnSteps = 48;
constexpr int GhostSpawnSteps = 78;
constexpr float SpawnSeparation = 15.0f * Maze::CellSize;

inline int PickSpawn(const Maze::Layout& level, const Torches::State& torches,
                     const Maze::Distances& distances, int desiredSteps, int other = NoCell)
{
    constexpr float clearOfTorch = Torches::ProtectionRadiusCells * Maze::CellSize + 2 * AngelRadius;
    const Vector2 player = Center(CellId(level.spawn));
    int best = NoCell;
    int bestScore = std::numeric_limits<int>::max();
    for (int id = 0; id < CellCount; ++id)
    {
        const auto cell = CellAt(id);
        const int steps = distances.value[cell.row][cell.column];
        const Vector2 position = Center(id);
        const auto room = level.rooms[static_cast<int>(Rooms::Id::Entrance)].bounds;
        if (cell.column >= room.left - 5 && cell.column <= room.right + 5 && cell.row >= room.top - 5 && cell.row <= room.bottom + 5) continue;
        if (steps < MinimumSpawnSteps || DistanceSquared(position, player) < SpawnSeparation * SpawnSeparation
            || (other != NoCell && DistanceSquared(position, Center(other)) < SpawnSeparation * SpawnSeparation))
            continue;

        bool nearTorch = false;
        for (int i = 0; i < torches.Count(); ++i)
            nearTorch |= DistanceSquared(position, torches.WorldPosition(i)) <= clearOfTorch * clearOfTorch;
        if (nearTorch) continue;
        const int score = std::abs(steps - desiredSteps) * 4
            + static_cast<int>(std::abs(std::sqrt(DistanceSquared(position, player)) / Maze::CellSize - 24));
        if (score < bestScore)
        {
            best = id;
            bestScore = score;
        }
    }
    return best;
}

inline void MoveToward(Enemy& enemy, Vector2 destination, float dt)
{
    const Vector2 before = enemy.position;
    const float distance = std::sqrt(DistanceSquared(before, destination));
    const float step = enemy.speed * dt;
    if (distance <= step)
        enemy.position = destination;
    else if (step > 0)
    {
        enemy.position.x += (destination.x - enemy.position.x) * step / distance;
        enemy.position.y += (destination.y - enemy.position.y) * step / distance;
    }
    const float moved = std::min(distance, step);
    if (moved > 0)
    {
        enemy.facing = {(destination.x - before.x) / distance, (destination.y - before.y) / distance};
        enemy.traveled += moved;
        enemy.stride = std::fmod(enemy.stride + moved, 96.0f);
    }
}

inline bool FollowWaypoint(Enemy& enemy, float dt)
{
    Route& route = enemy.route;
    if (route.Remaining() && DistanceSquared(enemy.position, Center(route.cells[route.next])) == 0)
        ++route.next;
    if (!route.Remaining()) return false;
    const Vector2 waypoint = Center(route.cells[route.next]);
    MoveToward(enemy, waypoint, dt);
    if (DistanceSquared(enemy.position, waypoint) == 0) ++route.next;
    return true;
}

class Manager
{
public:
    Manager(const Maze::Layout& level, const Torches::State& torches) : level(level), torches(torches), barrier(level) {}

    bool Safe(Vector2 player) const { return barrier.Room(player) || torches.IsPositionProtectedByTorch(player); }

    bool Spawn()
    {
        const auto distances = Maze::MeasureDistances(level);
        const int a = PickSpawn(level, torches, distances, AngelSpawnSteps);
        const int g = PickSpawn(level, torches, distances, GhostSpawnSteps, a);
        if (a == NoCell || g == NoCell) return false;
        barrier.Refresh(torches);
        SetHome(angel, a);
        SetHome(ghost, g);
        return true;
    }

    const Enemy& Angel() const { return angel; }
    const Enemy& Ghost() const { return ghost; }

    void Update(float dt, Vector2 previous, Vector2 player, const Lighting::DungeonLight& light, Health& health,bool hidden=false)
    {
        const bool changed = barrier.Refresh(torches);
        if (changed)
        {
            for (Enemy* e : {&angel, &ghost})
            {
                e->lastGoal = NoCell;
                e->perch = PatrolPerch(CellAt(e->home));
                e->patrolGoal = e->perch;
            }
        }
        for (Enemy* e : {&angel, &ghost})
            e->discovered |= light.IsWorldPositionObserved(e->position);
        if (grace > 0)
        {
            grace = std::max(0.0f, grace - dt);
            return;
        }
        health.Update(dt);
        const bool moving = DistanceSquared(previous, player) > 0.01f;
        for (Enemy* actor : {&angel, &ghost})
        {
            Enemy& e = *actor;
            e.traveled = 0;
            e.attackPulse = std::max(0.0f, e.attackPulse - dt);
            const Vector2 before = e.position;
            Tick(e, dt, player, moving, light,hidden);
            const float contact = ContactFraction(previous, player, before, e);
            const bool armed = (e.discovered || (hidden && e.kind==Kind::Ghost)) && !(hidden && e.kind==Kind::Angel) && e.state == State::Chasing && !Safe(player)
                && Maze::HasLineOfSight(level, e.position, player)
                && DistanceSquared(e.position, player) <= (e.kind == Kind::Angel ? CloseSense * CloseSense : 28.0f * 28.0f);
            if (!armed || (e.kind == Kind::Ghost && health.invulnerable > 0))
            {
                e.windup = 0;
                continue;
            }
            e.windup += std::min(dt, 0.1f);
            const float warning = e.kind == Kind::Angel ? AngelWarning : GhostWarning;
            if (e.windup >= warning && contact >= 0 && !Safe(PointAlong(previous, player, contact)))
            {
                health.Hit(e.kind);
                if (e.kind == Kind::Ghost) e.attackPulse = GhostPulse;
                e.windup = 0;
            }
        }
    }

    void InvalidateRoutes(){barrier.Invalidate();angel.lastGoal=ghost.lastGoal=NoCell;}
private:
    void SetHome(Enemy& e, int id)
    {
        e.position = e.home = e.target = Center(id);
        e.perch = e.patrolGoal = PatrolPerch(id);
    }

    Vector2 PatrolPerch(int id) const
    {
        const Vector2 home = Center(id);
        // Choose a nearby reachable floor perch using a local breadth-first walk.
        Maze::Queue<int, CellCount> queue;
        std::array<bool, CellCount> visited{};
        queue.Push(id);
        visited[id] = true;
        int perch = id;
        while (!queue.Empty())
        {
            const int current = queue.Pop();
            if (DistanceSquared(Center(current), home) > DistanceSquared(Center(perch), home)) perch = current;
            for (int d = 0; d < 4; ++d)
            {
                const auto c = Maze::Neighbor(CellAt(current), d);
                if (!level.IsWalkable(c.column, c.row)) continue;
                const int next = CellId(c);
                if (visited[next] || barrier.BlocksEdge(current, d) || DistanceSquared(Center(next), home) > PatrolRadius * PatrolRadius) continue;
                visited[next] = true;
                queue.Push(next);
            }
        }
        return Center(perch);
    }

    void Evacuate(Enemy& e)
    {
        const int start = CellAt(e.position);
        Maze::Queue<int, CellCount> queue;
        std::array<int, CellCount> parent;
        parent.fill(NoCell);
        queue.Push(start);
        parent[start] = start;
        BeginSearch(e.route);
        e.lastGoal = start;
        while (!queue.Empty())
        {
            int id = queue.Pop();
            ++e.route.nodesExplored;
            if (barrier.IsClear(Center(id)))
            {
                parent[start] = NoCell;
                Reconstruct(e.route, parent, id);
                return;
            }
            for (int d = 0; d < 4; ++d)
            {
                auto c = Maze::Neighbor(CellAt(id), d);
                if (!Maze::Contains(c.column,c.row) || (e.kind == Kind::Angel && !level.IsWalkable(c.column,c.row))) continue;
                int next = CellId(c);
                if (parent[next] != NoCell || barrier.Room(Center(next), Maze::CellSize + AngelRadius)) continue;
                parent[next] = id;
                queue.Push(next);
            }
        }
    }

    void Tick(Enemy& e, float dt, Vector2 player, bool moving, const Lighting::DungeonLight& light,bool hidden)
    {
        ++e.updates;
        const bool isAngel = e.kind == Kind::Angel;
        const bool sight = Maze::HasLineOfSight(level, e.position, player);
        const float distance = DistanceSquared(e.position, player);
        const bool withinLeash = DistanceSquared(player, e.home) <= LeashRadius * LeashRadius;
        const bool senses = isAngel ? (sight && !hidden) : (moving || sight || distance < CloseSense * CloseSense);
        const bool safe = Safe(player);
        e.nearby = !safe && withinLeash && distance <= DetectionRadius * DetectionRadius;
        const bool detected = !safe && withinLeash && distance <= DetectionRadius * DetectionRadius && senses;
        if (!barrier.IsClear(e.position))
        {
            // A newly lit torch may surround an existing actor. Find a short
            // escape route, then resume normal navigation as soon as the body clears it.
            if (e.state != State::Repelled || e.lastGoal == NoCell)
                Evacuate(e);
            e.state = State::Repelled;
            e.memory = 0;
            // Stop the evacuation at the first legal position. Normal movement
            // validates every subsequent segment against the updated barrier.
            float budget = std::min(dt, 0.25f) * e.speed;
            while (budget > 0 && e.route.Remaining() && !barrier.IsClear(e.position))
            {
                const float step = std::min(1.0f, budget);
                FollowWaypoint(e, step / e.speed);
                budget -= step;
            }
            if (barrier.IsClear(e.position)) e.lastGoal = NoCell;
            return;
        }
        if (isAngel && light.IsWorldPositionObserved(e.position))
        {
            e.state = State::Frozen;
            return;
        }
        if (detected)
        {
            e.state = State::Chasing;
            e.memory = DisengageSeconds;
            e.target = player;
        }
        else if (!safe && e.memory > 0 && withinLeash)
        {
            e.state = State::Investigating;
            e.memory = std::max(0.0f, e.memory - dt);
        }
        else if (e.state == State::Patrolling || e.state == State::Dormant ||
                 DistanceSquared(e.position, e.home) < 1)
        {
            e.memory = 0;
            e.state = State::Patrolling;
            if (DistanceSquared(e.position, e.patrolGoal) < 1)
                e.patrolGoal = DistanceSquared(e.patrolGoal, e.home) < 1 ? e.perch : e.home;
            e.target = e.patrolGoal;
        }
        else
        {
            e.memory = 0;
            e.state = State::Returning;
            e.target = e.home;
        }
        const int goal = CellAt(e.target);
        if (goal != e.lastGoal)
        {
            int start = CellAt(e.position);
            if (e.route.Remaining() && barrier.SegmentClear(e.position, Center(e.route.cells[e.route.next]))) start = e.route.cells[e.route.next];
            if (!barrier.SegmentClear(e.position, Center(start)))
            {
                // A torch can cut off the forward half of the current edge.
                // Rejoin the route from its last completed waypoint if reachable.
                start = e.route.next > 0 ? e.route.cells[e.route.next - 1] : NoCell;
                if (start != NoCell && !barrier.SegmentClear(e.position, Center(start))) start = NoCell;
            }
            if (isAngel) FindAngelRoute(level, start, goal, e.route, barrier, e.home);
            else FindGhostRoute(level, barrier, start, goal, e.route, e.home);
            e.lastGoal = goal;
        }
        const float reach = e.radius + Movement::CollisionHalfSize;
        if (e.state == State::Chasing && sight && distance <= reach * reach) return;
        const float travel = dt * (e.state == State::Chasing ? 1.0f : PatrolSpeedFactor);
        if (e.route.Remaining() && !barrier.SegmentClear(e.position, Center(e.route.cells[e.route.next]))) return;
        if (!FollowWaypoint(e, travel) && CellAt(e.position) == goal && barrier.SegmentClear(e.position, e.target))
            MoveToward(e, e.target, travel);
    }

    const Maze::Layout& level;
    const Torches::State& torches;
    Enemy angel{Kind::Angel, AngelSpeed, AngelRadius};
    Enemy ghost{Kind::Ghost, GhostSpeed, GhostRadius};
    TorchBarrier barrier;
    float grace = GraceSeconds;
};
}
#endif
