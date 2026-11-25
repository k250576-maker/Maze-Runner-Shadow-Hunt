#include "../include/physical_enemies.h"
#include <cassert>
#include <cstdio>

using namespace Enemies;

struct Arena
{
    Maze::Layout level;
    Lighting::DungeonLight light{level};
    Exploration::Map map{level};
    PhysicalEnemies enemies{level};
    Health health;
    Melee melee;
    Vector2 player = Center(CellId({10, 10}));

    Arena()
    {
        for (int r = 5; r <= 18; ++r)
            for (int c = 5; c <= 18; ++c) level.floor[r][c] = true;
        Tick(0);
    }

    MeleeFrame Tick(float dt, bool pressed = false, Vector2 facing = {1, 0})
    {
        return Move(dt, player, pressed, facing);
    }

    MeleeFrame Move(float dt, Vector2 to, bool pressed = false, Vector2 facing = {1, 0})
    {
        const auto frame = melee.Update(dt, pressed, facing, player, to);
        player = to;
        health.Update(dt);
        constexpr Rectangle wholeMap{0, 0, Maze::Columns * Maze::CellSize, Maze::Rows * Maze::CellSize};
        enemies.Update(dt, player, wholeMap, light, map, health, false, frame);
        return frame;
    }

    PhysicalEnemy& Spawn(Kind kind, Vector2 position, float rest = 0, int slot = 0)
    {
        auto& actor = const_cast<PhysicalEnemy&>(enemies.Actors()[slot]);
        actor.Spawn(kind, position, rest);
        return actor;
    }

    Vector2 Offset(float x, float y) const { return {player.x + x, player.y + y}; }
};

void InputAndCooldown()
{
    Melee melee;
    const Vector2 at{200, 200};
    assert(melee.Update(0, true, {1, 0}, at, at).started && melee.effect == MeleeEffect);
    assert(!melee.Update(0.20f, true, {0, 1}, at, at).started && !melee.buffered);
    assert(!melee.Update(0.17f, true, {0, 1}, at, at).started && melee.buffered);
    assert(melee.Update(0.081f, false, {-1, 0}, at, at).started);
    assert(melee.direction.x == -1 && !melee.buffered);
    const auto active = melee.Update(0.05f, true, {1, 0}, at, at);
    assert(active.active && !active.started && active.direction.x == -1);
    const auto tail = melee.Update(0.12f, false, {1, 0}, at, {210, 200});
    assert(tail.active && tail.lastAge == MeleeActiveTime && tail.to.x < 210);
    assert(!melee.Update(0.001f, false, {1, 0}, at, at).active);
    assert(!melee.Update(0.28f, true, {1, 0}, at, at, false).active && !melee.buffered);

    Melee rapid;
    float previous = -1;
    int swings = 0;
    for (int i = 0; i <= 200; ++i)
        if (rapid.Update(0.01f, true, {1, 0}, at, at).started)
        {
            const float at = i * 0.01f;
            assert(at - previous >= MeleeCooldown - 0.0001f);
            previous = at;
            ++swings;
        }
    assert(swings == 5);
}

void StationaryTargets()
{
    Arena arena;
    auto& wizard = arena.Spawn(Kind::Sentinel, arena.Offset(46.5f, 0), 1);
    auto& demon = arena.Spawn(Kind::Stalker, arena.Offset(29, 24), 1, 1);
    auto& behind = arena.Spawn(Kind::Stalker, arena.Offset(-22, 0), 1, 2);
    auto& beyond = arena.Spawn(Kind::Stalker, arena.Offset(46.5f, 0), 1, 3);
    assert(wizard.body.radius == AngelRadius && demon.body.radius == GhostRadius);
    assert(arena.Tick(0, true).started);
    assert(wizard.struck && wizard.health == 2);
    assert(!demon.struck && !behind.struck && !beyond.struck);
    arena.Tick(0.055f, false, {-1, 0});
    assert(demon.struck && demon.health == 0);
    assert(!wizard.struck && wizard.health == 2 && !behind.struck && !beyond.struck);
    arena.Tick(0.04f);
    assert(!demon.struck && wizard.health == 2);

    Arena turn;
    auto& left = turn.Spawn(Kind::Sentinel, turn.Offset(-28, 7), 1);
    assert(turn.Tick(0, true, {-1, 0}).started && left.struck);
    Arena up;
    auto& above = up.Spawn(Kind::Stalker, up.Offset(-5, -29), 1);
    assert(up.Tick(0, true, {0, -1}).started && above.struck);
    Arena down;
    auto& below = down.Spawn(Kind::Stalker, down.Offset(5, 29), 1);
    assert(down.Tick(0, true, {0, 1}).started && below.struck);
}

void MovingTargets()
{
    for (Kind kind : {Kind::Sentinel, Kind::Stalker})
    {
        Arena arena;
        auto& actor = arena.Spawn(kind, arena.Offset(MeleeReach + (kind == Kind::Sentinel ? AngelRadius : GhostRadius)
                                                     + 0.7f, 0));
        arena.Tick(0, true);
        assert(!actor.struck && actor.body.state == State::Chasing);
        arena.Tick(0.045f);
        assert(actor.struck && actor.body.traveled > 0 && actor.health == Stats(kind).health - 1);
    }

    Arena arena;
    // An unusually long frame can take a target through the slash, with
    // misses at both endpoints. The actual movement sweep sees the crossing.
    const auto before = arena.Offset(32, -35);
    const auto after = arena.Offset(32, 35);
    const MeleeFrame slice{false, true, arena.player, arena.player, {1, 0}, 0.01f, 0.065f, 1};
    assert(!MeleeTouches(arena.level, slice.from, slice.direction, slice.firstAge, before, GhostRadius));
    assert(!MeleeTouches(arena.level, slice.to, slice.direction, slice.lastAge, after, GhostRadius));
    assert(SweptMeleeTouches(arena.level, slice, before, after, GhostRadius));
    auto clipped = slice;
    clipped.motionFraction = 0.04f;
    assert(!SweptMeleeTouches(arena.level, clipped, before, after, GhostRadius));

    // Check relative motion from a moving player too. A static target is
    // reachable momentarily while the player moves past it.
    const auto target = arena.Offset(32, 0);
    const MeleeFrame playerMoves{false, true, arena.Offset(0, 35), arena.Offset(0, -35), {1, 0}, 0.01f, 0.065f, 1};
    assert(SweptMeleeTouches(arena.level, playerMoves, target, target, AngelRadius));
}

void ObstructionAndEnemyAttacks()
{
    Arena arena;
    arena.player = {319, 336};
    auto& target = arena.Spawn(Kind::Sentinel, {353, 336}, 2);
    arena.level.locked[10][10] = true;
    arena.Tick(0, true);
    arena.Tick(0.07f);
    assert(!target.struck && target.health == 3);
    arena.level.locked[10][10] = false;
    arena.level.floor[10][10] = false;
    arena.Tick(MeleeCooldown, true);
    arena.Tick(0.07f);
    assert(!target.struck && target.health == 3);
    arena.level.floor[10][10] = true;
    arena.Tick(MeleeCooldown, true);
    assert(target.struck && target.health == 2);

    for (Kind kind : {Kind::Stalker, Kind::Sentinel})
    {
        Arena fight;
        auto& actor = fight.Spawn(kind, fight.Offset(44, 0));
        for (int i = 0; i < 8 && actor.body.state != State::WindingUp; ++i)
        {
            fight.Tick(0.35f);
            assert(std::sqrt(DistanceSquared(actor.body.position, fight.player)) >= CombatSpacing(actor.body) - 0.01f);
        }
        assert(actor.startedWindup && actor.body.state == State::WindingUp);
        const auto feet = actor.body.position;
        const auto facing = actor.body.facing;
        fight.Move(Stats(kind).windup + 0.01f, {feet.x, feet.y + 31});
        assert(actor.swung && actor.body.state == State::Recovering && fight.health.points == StartingHealth);
        assert(DistanceSquared(feet, actor.body.position) == 0 && DistanceSquared(facing, actor.body.facing) == 0);
        fight.Move(0.05f, feet);
        assert(DistanceSquared(feet, actor.body.position) == 0);
        fight.Tick(Stats(kind).recovery + 0.01f);
        assert(std::sqrt(DistanceSquared(fight.player, actor.body.position)) >= CombatSpacing(actor.body) - 0.01f);
        fight.Tick(0.01f);
        assert(actor.body.state == State::WindingUp);
    }
}

int main()
{
    InputAndCooldown();
    StationaryTargets();
    MovingTargets();
    ObstructionAndEnemyAttacks();
    std::puts("Combat: cooldown, locked facing, body edges, multiple targets, motion sweeps, obstructions and enemy spacing passed.");
}
