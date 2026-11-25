#ifndef SHADOW_HUNT_PHYSICAL_ENEMIES_H
#define SHADOW_HUNT_PHYSICAL_ENEMIES_H

#include "enemy_manager.h"
#include "exploration_map.h"
#include "gadgets.h"
#include <cstdint>
#include <utility>

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
constexpr float PhysicalAttackHalfArc = 0.90f;
constexpr float PhysicalAttackTolerance = 4.0f;
constexpr int PhysicalAttackImpactFrame=4; // Inspected Wizard/Demon attack sheets.
inline float AttackFollowThrough(Kind kind)
{
    const int frames=kind==Kind::Sentinel?8:7;
    return Stats(kind).windup*(frames-PhysicalAttackImpactFrame)/PhysicalAttackImpactFrame;
}

inline float CombatSpacing(const Enemy& enemy)
{
    return Movement::CollisionHalfSize + enemy.radius;
}

struct PhysicalEnemy
{
    struct RejectedLure { int start = NoCell, goal = NoCell; };
    Enemy body{Kind::Sentinel, SentinelStats.speed, AngelRadius};
    bool active = false;
    int health = 0;
    float timer = 0;
    float burst = 0;
    float hurt = 0;
    bool swung = false;
    bool struck = false;
    bool startedWindup = false;
    bool hitThisSwing = false;
    int lureGoal = NoCell;
    std::array<RejectedLure, Gadgets::LurePickupCount> rejectedLures{};
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
                const Exploration::Map& exploration, Health& health, bool hidden = false,
                const MeleeFrame& melee = {}, const Gadgets::Run* gadgets = nullptr)
    {
        const bool sheltered = InEntrance(level, CellAt(CellAt(player)));
        const bool obscured = gadgets && gadgets->Obscures(level, player);
        int pursuers = 0;
        for (const auto& actor : actors) pursuers += Pursuing(actor);
        for (auto& actor : actors)
        {
            actor.swung = actor.struck = actor.startedWindup = false;
            if (!actor.active || health.Dead()) continue;
            actor.body.traveled = 0;
            const bool wasPursuing = Pursuing(actor);
            if (melee.started) actor.hitThisSwing = false;
            const bool canHit = melee.active && !sheltered && !hidden && actor.health > 0 && !actor.hitThisSwing;
            const Vector2 before = actor.body.position;
            if (canHit && (MeleeTouches(level, melee.from, melee.direction, melee.firstAge, before, actor.body.radius)
                || (actor.body.state == State::WindingUp
                    && SweptMeleeTouches(level, melee, before, before, actor.body.radius))))
                Strike(actor);
            Tick(actor, player, sheltered, wasPursuing || pursuers < ActivePursuitCap,
                 dt, health, hidden, obscured, gadgets);
            if (canHit && !actor.hitThisSwing && !health.Dead()
                && SweptMeleeTouches(level, melee, before, actor.body.position, actor.body.radius))
                Strike(actor);
            actor.animationTime=std::fmod(actor.animationTime+dt,60.0f);
            if(std::fabs(actor.body.facing.x)>0.01f) actor.mirrored=actor.body.facing.x<0;
            pursuers += static_cast<int>(Pursuing(actor)) - wasPursuing;
        }
        if (health.Dead()) return;
        if (!populated)
        {
            Populate(player, viewport, light, exploration);
            populated = true;
            ResetSpawnClock();
            return;
        }
        spawnClock -= dt;
        if (spawnClock > 0) return;
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
    void InvalidateRoutes()
    {
        for (auto& actor : actors)
        {
            actor.body.lastGoal = NoCell;
            actor.rejectedLures = {};
        }
    }

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

    static void Strike(PhysicalEnemy& actor)
    {
        actor.struck = true;
        actor.hitThisSwing = true;
        actor.startedWindup = false;
        actor.attackVisual=0;
        --actor.health;
        actor.hurt = HitReaction;
        actor.body.state = actor.health == 0 ? State::Dying : State::Recovering;
        actor.timer = actor.health == 0 ? DeathDuration : HitReaction;
    }

    static int RouteStart(const Enemy& body)
    {
        // Finish the current cardinal edge before turning toward a new goal.
        return body.route.Remaining() ? body.route.cells[body.route.next] : CellAt(body.position);
    }

    void PlanRoute(Enemy& body, int start, int goal, Route& route) const
    {
        if (body.kind == Kind::Sentinel) FindSentinelRoute(level, start, goal, route, body.home, WizardLeash);
        else FindFloorRoute(level, start, goal, route, body.home,
            [&](int, int, Maze::Cell cell) { return InEntrance(level, cell); }, DemonLeash);
    }

    // Limit forward travel at the first point the enemy's footprint would
    // overlap the player, including on a long frame.
    static float ApproachTime(const Enemy& body, Vector2 waypoint, Vector2 player, float dt)
    {
        const Vector2 heading{waypoint.x - body.position.x, waypoint.y - body.position.y};
        const float length = std::hypot(heading.x, heading.y);
        if (length <= 0) return dt;
        const Vector2 toward{(player.x - body.position.x), (player.y - body.position.y)};
        const float projection = (toward.x * heading.x + toward.y * heading.y) / length;
        const float radius = CombatSpacing(body);
        const float tangentSquared = std::max(0.0f, DistanceSquared(player, body.position) - projection * projection);
        if (projection <= 0 || tangentSquared >= radius * radius) return dt;
        const float contact = projection - std::sqrt(radius * radius - tangentSquared);
        return std::min(dt, std::max(0.0f, contact) / body.speed);
    }

    void Navigate(Enemy& body, int goal, float dt, const Vector2* player = nullptr) const
    {
        if (goal != body.lastGoal)
        {
            PlanRoute(body, RouteStart(body), goal, body.route);
            body.lastGoal = goal;
        }
        if (player && body.route.Remaining())
        {
            int index = body.route.next;
            if (DistanceSquared(body.position, Center(body.route.cells[index])) == 0) ++index;
            if (index < body.route.length)
                dt = ApproachTime(body, Center(body.route.cells[index]), *player, dt);
        }
        FollowWaypoint(body, dt);
    }

    bool MakeSpace(Enemy& body, Vector2 player, float leash, float dt) const
    {
        const float distance = DistanceSquared(body.position, player);
        const float separation = CombatSpacing(body);
        if (distance + 0.01f >= separation * separation || dt <= 0) return false;
        const Vector2 origin = body.position;
        const float step = std::min(body.speed * 0.65f * dt, separation - std::sqrt(distance));
        Vector2 best = origin;
        // Use the existing wall and furniture sweeps for this short recovery
        // step. A side step is possible when the direct retreat is blocked.
        for (const auto direction : Maze::Directions)
        {
            Vector2 point = origin;
            if (direction.column)
                point.x = Movement::SweepAxis(level, origin.x, origin.y, direction.column * step, true);
            else
                point.y = Movement::SweepAxis(level, origin.y, origin.x, direction.row * step, false);
            if (InEntrance(level, CellAt(CellAt(point)))
                || DistanceSquared(point, body.home) > leash * leash
                || DistanceSquared(point, player) <= DistanceSquared(best, player)) continue;
            best = point;
        }
        if (DistanceSquared(origin, best) == 0) return false;
        MoveToward(body, best, dt);
        // A side step can leave the old cardinal edge. Replan from the
        // current cell before following any prior waypoint.
        body.route.next = body.route.length = 0;
        body.lastGoal = NoCell;
        return true;
    }

    bool FollowLure(PhysicalEnemy& actor, const Gadgets::Run* gadgets, float leash, float dt) const
    {
        auto& body = actor.body;
        if (gadgets)
        {
            const auto& lures = gadgets->Lures();
            const int start = RouteStart(body);
            while (true)
            {
                int best = -1;
                float bestDistance = std::numeric_limits<float>::max();
                for (int i = 0; i < static_cast<int>(lures.size()); ++i)
                {
                    const auto& lure = lures[i];
                    if (lure.remaining <= 0) continue;
                    const int cell = CellAt(lure.position);
                    const auto& rejected = actor.rejectedLures[i];
                    const float distance = DistanceSquared(body.position, lure.position);
                    if (distance > bestDistance || cell == NoCell || InEntrance(level, CellAt(cell))
                        || (cell != actor.lureGoal && distance > Gadgets::LureRadius * Gadgets::LureRadius)
                        || DistanceSquared(body.home, Center(cell)) > leash * leash
                        || (start == rejected.start && cell == rejected.goal)) continue;
                    best = i;
                    bestDistance = distance;
                }
                if (best < 0) break;
                const int goal = CellAt(lures[best].position);
                if (goal != actor.lureGoal || goal != body.lastGoal)
                {
                    Route route;
                    route.searches = body.route.searches;
                    PlanRoute(body, start, goal, route);
                    if (route.length == 0)
                    {
                        actor.rejectedLures[best] = {start, goal};
                        body.route.searches = route.searches;
                        continue;
                    }
                    body.route = std::move(route);
                    body.lastGoal = goal;
                }
                actor.lureGoal = goal;
                body.state = State::Investigating;
                body.memory = std::max(0.0f, body.memory - dt);
                FollowWaypoint(body, dt);
                return true;
            }
        }
        if (actor.lureGoal != NoCell)
        {
            actor.lureGoal = NoCell;
            body.lastGoal = NoCell;
        }
        return false;
    }

    void Tick(PhysicalEnemy& actor, Vector2 player, bool sheltered, bool mayPursue, float dt,
              Health& health, bool hidden, bool obscured, const Gadgets::Run* gadgets) const
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
            if (sight && InAttackArc(body.position, body.facing, player,
                                     stats.reach + PhysicalAttackTolerance, PhysicalAttackHalfArc))
                health.Hit(body.kind);
            body.state = State::Recovering;
            actor.timer = stats.recovery;
            return;
        }
        if ((body.state == State::Dormant || body.state == State::Recovering) && actor.timer > 0) return;

        if (FollowLure(actor, gadgets, leash, dt)) return;

        const bool tracking = sight && !obscured && distance <= stats.detection * stats.detection;
        const bool inReach = sight && distance <= stats.reach * stats.reach;
        body.memory = tracking ? DisengageSeconds : std::max(0.0f, body.memory - dt);
        if (mayPursue && canTrack && (tracking || inReach || ((body.state == State::Chasing || body.state==State::Investigating) && body.memory > 0)) && !(stalker && body.state == State::Returning && actor.burst <= 0 && actor.timer > 0))
        {
            if (body.state != State::Chasing) actor.burst = StalkerBurst;
            body.state = tracking?State::Chasing:State::Investigating;
            if (tracking) body.target = player;
            if (inReach)
            {
                if (MakeSpace(body, player, leash, dt)) return;
                if (distance > 0)
                {
                    const float length = std::sqrt(distance);
                    body.facing = {(player.x-body.position.x)/length, (player.y-body.position.y)/length};
                }
                body.state = State::WindingUp;
                actor.timer = stats.windup;
                actor.startedWindup = true;
                return;
            }
            actor.burst -= dt;
            if (!stalker || actor.burst > 0)
            {
                Navigate(body, CellAt(body.target), dt, sight ? &player : nullptr);
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
