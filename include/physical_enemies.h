#ifndef SHADOW_HUNT_PHYSICAL_ENEMIES_H
#define SHADOW_HUNT_PHYSICAL_ENEMIES_H

#include "enemy_manager.h"
#include "exploration_map.h"
#include <cstdint>

namespace Enemies
{
struct PhysicalStats
{
    float speed, detection, reach, windup, recovery;
    int health;
};

inline constexpr PhysicalStats SentinelStats{40, 6 * Maze::CellSize, 36, 0.93f, 0.98f, 3};
inline constexpr PhysicalStats StalkerStats{85, 5 * Maze::CellSize, 29, 0.42f, 0.86f, 1};
inline const PhysicalStats& Stats(Kind kind) { return kind == Kind::Sentinel ? SentinelStats : StalkerStats; }

constexpr int SentinelCap = 5;
constexpr int StalkerCap = 6;
constexpr float WizardLeash=9*Maze::CellSize, DemonLeash=8*Maze::CellSize;
constexpr float InitialPhysicalGrace = GraceSeconds;
constexpr float MinSpawnInterval = 45;
constexpr float MaxSpawnInterval = 75;
constexpr float SpawnRest = 2.5f;
constexpr float StalkerBurst = 1.75f;
constexpr float SpawnSpacing = 7 * Maze::CellSize;
constexpr int ActivePursuitCap = 2;
constexpr int MinSpawnSteps = 21;
constexpr float MinSpawnDistance = 9 * Maze::CellSize;
constexpr float DeathDuration = 0.52f;
constexpr float HitReaction = 0.23f;
constexpr int PhysicalAttackImpactFrame=4; // Inspected Wizard/Demon attack sheets.
inline float AttackFollowThrough(Kind kind)
{
    const int frames=kind==Kind::Sentinel?8:7;
    return Stats(kind).windup*(frames-PhysicalAttackImpactFrame)/PhysicalAttackImpactFrame;
}

struct PhysicalEnemy
{
    Enemy body{Kind::Sentinel, SentinelStats.speed, AngelRadius};
    bool active = false;
    int health = 0;
    float timer = 0;
    float burst = 0;
    float hurt = 0;
    bool swung = false;
    bool struck = false;
    float animationTime=0;
    float attackVisual=0;
    bool mirrored=false;

    void Spawn(Kind kind, Vector2 origin, float rest)
    {
        *this = {};
        body.kind = kind;
        body.speed = Stats(kind).speed;
        body.radius = kind == Kind::Sentinel ? AngelRadius : GhostRadius;
        body.position = body.home = body.target = origin;
        active = true;
        health = Stats(kind).health;
        timer = rest;
    }
};

class PhysicalEnemies
{
public:
    explicit PhysicalEnemies(const Maze::Layout& layout) : level(layout) {}

    void SetSeed(std::uint32_t seed) { random = seed; }
    const auto& Actors() const { return actors; }

    void Update(float dt, Vector2 player, const Rectangle& viewport, const Lighting::DungeonLight& light,
                const Exploration::Map& exploration, bool swing, Vector2 facing, Health& health, bool hidden=false)
    {
        const bool sheltered = InEntrance(level, CellAt(CellAt(player)));
        int pursuers = 0;
        for (const auto& actor : actors) pursuers += Pursuing(actor);
        for (auto& actor : actors)
        {
            actor.swung = actor.struck = false;
            if (!actor.active) continue;
            actor.body.traveled = 0;
            const bool wasPursuing = Pursuing(actor);
            if (swing && !sheltered) Strike(actor, player, facing);
            Tick(actor, player, sheltered, wasPursuing || pursuers < ActivePursuitCap, dt, health,hidden);
            actor.animationTime=std::fmod(actor.animationTime+dt,60.0f);
            if(std::fabs(actor.body.facing.x)>0.01f) actor.mirrored=actor.body.facing.x<0;
            pursuers += static_cast<int>(Pursuing(actor)) - wasPursuing;
        }
        if (!populated)
        {
            Populate(player, viewport, light, exploration);
            populated = true;
            ResetSpawnClock();
            return;
        }
        spawnClock -= dt;
        if (spawnClock > 0 || health.Dead()) return;
        Replace(player, viewport, light, exploration);
        ResetSpawnClock();
    }

    bool Dangerous() const
    {
        for (const auto& actor : actors)
            if (actor.active && (actor.body.state == State::Chasing || actor.body.state == State::WindingUp))
                return true;
        return false;
    }
    void InvalidateRoutes() { for(auto& actor:actors) actor.body.lastGoal=NoCell; }

private:
    static bool Pursuing(const PhysicalEnemy& actor)
    {
        return actor.active && (actor.body.state == State::Chasing || actor.body.state == State::WindingUp);
    }

    void ResetSpawnClock()
    {
        const float fraction = (NextRandom() >> 8u) / 16777216.0f;
        spawnClock = MinSpawnInterval + (MaxSpawnInterval - MinSpawnInterval) * fraction;
    }

    std::uint32_t NextRandom()
    {
        random = 1664525u * random + 1013904223u;
        return random;
    }

    void Populate(Vector2 player, Rectangle viewport, const Lighting::DungeonLight& light,
                  const Exploration::Map& exploration)
    {
        const auto distances = Maze::MeasureDistances(level, CellAt(CellAt(player)));
        int farthest=MinSpawnSteps;
        for(const auto& row:distances.value) for(int value:row) farthest=std::max(farthest,value);
        // Interleave types across increasing distance bands from the entrance.
        int knights = 0, stalkers = 0;
        for (int i = 0; i < static_cast<int>(actors.size()); ++i)
        {
            const bool stalker = stalkers < StalkerCap && (i % 2 == 0 || knights == SentinelCap);
            const int index = stalker ? SentinelCap + stalkers++ : knights++;
            TrySpawn(index, MinSpawnSteps+(farthest-MinSpawnSteps)*(i+1)/(static_cast<int>(actors.size())+1), InitialPhysicalGrace, distances, player, viewport, light, exploration);
        }
    }

    void Replace(Vector2 player, Rectangle viewport, const Lighting::DungeonLight& light,
                 const Exploration::Map& exploration)
    {
        const int count = static_cast<int>(actors.size());
        for (int i = 0; i < count; ++i)
        {
            const int slot = (i + nextSlot) % count;
            if (actors[slot].active) continue;
            const auto distances = Maze::MeasureDistances(level, CellAt(CellAt(player)));
            int farthest=MinSpawnSteps;
            for(const auto& row:distances.value) for(int value:row) farthest=std::max(farthest,value);
            const int desired=MinSpawnSteps+static_cast<int>(NextRandom()%static_cast<unsigned>(std::max(1,farthest-MinSpawnSteps)));
            if (TrySpawn(slot, desired, SpawnRest, distances, player, viewport, light, exploration))
                nextSlot = (slot + 1) % count;
            return;
        }
    }

    bool TrySpawn(int slot, int desiredSteps, float rest, const Maze::Distances& distances,
                  Vector2 player, Rectangle viewport, const Lighting::DungeonLight& light,
                  const Exploration::Map& exploration)
    {
        // Margin includes the full upright sprite and allows for camera interpolation.
        constexpr float margin = 64;
        viewport = {viewport.x - margin, viewport.y - margin, viewport.width + 2 * margin, viewport.height + 2 * margin};
        const int offset = static_cast<int>(NextRandom() % CellCount);
        int best = NoCell;
        int bestScore = std::numeric_limits<int>::max();
        for (int i = 0; i < CellCount; ++i)
        {
            const int id = (i + offset) % CellCount;
            const auto cell = CellAt(id);
            const int distance = distances.value[cell.row][cell.column];
            if (distance < MinSpawnSteps || InEntrance(level, cell)) continue;
            const auto point = Center(id);
            if (DistanceSquared(point, player) < MinSpawnDistance * MinSpawnDistance ||
                CheckCollisionPointRec(point, viewport) || light.IsWorldPositionObserved(point)) continue;
            bool occupied = false;
            for (const auto& actor : actors)
                if (actor.active && (DistanceSquared(actor.body.position, point) < SpawnSpacing * SpawnSpacing ||
                     DistanceSquared(actor.body.home, point) < SpawnSpacing * SpawnSpacing))
                    occupied = true;
            if (occupied) continue;
            const int score = (exploration.At(cell.column, cell.row) == Exploration::Tile::Unknown ? 0 : CellCount)
                + std::abs(distance - desiredSteps);
            if (score >= bestScore) continue;
            best = id;
            bestScore = score;
        }
        if (best == NoCell) return false;
        actors[slot].Spawn(slot < SentinelCap ? Kind::Sentinel : Kind::Stalker, Center(best), rest);
        return true;
    }

    void Strike(PhysicalEnemy& actor, Vector2 player, Vector2 facing) const
    {
        if (actor.health <= 0 || !InAttackArc(player, facing, actor.body.position, MeleeReach, MeleeHalfArc)
            || !Maze::HasLineOfSight(level, player, actor.body.position)) return;
        actor.struck = true;
        actor.attackVisual=0;
        --actor.health;
        actor.hurt = HitReaction;
        actor.body.state = actor.health == 0 ? State::Dying : State::Recovering;
        actor.timer = actor.health == 0 ? DeathDuration : HitReaction;
    }

    void Navigate(Enemy& body, int goal, float dt) const
    {
        if (goal != body.lastGoal)
        {
            // Finish the current cardinal edge before turning toward a new goal.
            const int start = body.route.Remaining() ? body.route.cells[body.route.next] : CellAt(body.position);
            if (body.kind == Kind::Sentinel) FindSentinelRoute(level, start, goal, body.route, body.home,WizardLeash);
            else FindFloorRoute(level, start, goal, body.route, body.home,
                [&](int, int, Maze::Cell cell) { return InEntrance(level, cell); },DemonLeash);
            body.lastGoal = goal;
        }
        FollowWaypoint(body, dt);
    }

    void Tick(PhysicalEnemy& actor, Vector2 player, bool sheltered, bool mayPursue, float dt, Health& health,bool hidden) const
    {
        Enemy& body = actor.body;
        ++body.updates;
        actor.attackVisual=std::max(0.0f,actor.attackVisual-dt);
        if (!actor.struck) actor.timer = std::max(0.0f, actor.timer - dt);
        actor.hurt = std::max(0.0f, actor.hurt - dt);
        if (body.state == State::Dying)
        {
            if (actor.timer == 0) actor.active = false;
            return;
        }
        if (health.Dead()) return;
        const auto& stats = Stats(body.kind);
        const bool stalker = body.kind == Kind::Stalker;
        const float distance = DistanceSquared(body.position, player);
        const float leash=stalker?DemonLeash:WizardLeash;
        const bool canTrack = !sheltered && DistanceSquared(body.home, player) <= leash*leash;
        const bool sight = !hidden && canTrack && Maze::HasLineOfSight(level, body.position, player);
        if (body.state == State::WindingUp)
        {
            if (actor.timer > 0) return;
            actor.swung = true;
            actor.attackVisual=AttackFollowThrough(body.kind);
            if (sight && InAttackArc(body.position, body.facing, player, stats.reach + 4, 0.9f))
                health.Hit(body.kind);
            body.state = State::Recovering;
            actor.timer = stats.recovery;
            return;
        }
        if ((body.state == State::Dormant || body.state == State::Recovering) && actor.timer > 0) return;

        const bool tracking = sight && distance <= stats.detection * stats.detection;
        body.memory = tracking ? DisengageSeconds : std::max(0.0f, body.memory - dt);
        if (mayPursue && canTrack && (tracking || ((body.state == State::Chasing || body.state==State::Investigating) && body.memory > 0)) && !(stalker && body.state == State::Returning && actor.burst <= 0 && actor.timer > 0))
        {
            if (body.state != State::Chasing) actor.burst = StalkerBurst;
            body.state = tracking?State::Chasing:State::Investigating;
            if (tracking) body.target = player;
            if (tracking && distance <= stats.reach * stats.reach)
            {
                if (distance > 0)
                {
                    const float length = std::sqrt(distance);
                    body.facing = {(player.x-body.position.x)/length, (player.y-body.position.y)/length};
                }
                body.state = State::WindingUp;
                actor.timer = stats.windup;
                return;
            }
            actor.burst -= dt;
            if (!stalker || actor.burst > 0)
            {
                Navigate(body, CellAt(body.target), dt);
                return;
            }
            actor.timer = stats.recovery;
        }

        if(body.state==State::Patrolling && DistanceSquared(body.position,body.target)>1)
        {Navigate(body,CellAt(body.target),dt*PatrolSpeedFactor);return;}
        body.state = State::Returning;
        if (DistanceSquared(body.position, body.home) < 1)
        {
            body.state = actor.timer > 0 ? State::Recovering : State::Patrolling;
            if(actor.timer<=0)
                for(int d=0;d<4;++d)
                {
                    auto next=Maze::Neighbor(CellAt(CellAt(body.home)),(d+CellAt(body.home))%4);
                    if(!level.IsWalkable(next.column,next.row)||InEntrance(level,next))continue;
                    body.target=Center(CellId(next));Navigate(body,CellId(next),dt*PatrolSpeedFactor);break;
                }
            return;
        }
        Navigate(body, CellAt(body.home), dt * (stalker ? 0.85f : PatrolSpeedFactor));
    }

    const Maze::Layout& level;
    std::array<PhysicalEnemy, SentinelCap + StalkerCap> actors{};
    int nextSlot = 0;
    std::uint32_t random = 0;
    float spawnClock = 0;
    bool populated = false;
};
}
#endif
