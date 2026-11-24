#include "../include/physical_enemies.h"
#include "../include/run_progress.h"
#include "../include/bats.h"
#include <cstring>
#include <cassert>
#include <cstdio>

using namespace Enemies;

struct Field
{
    Maze::Layout level{};
    Torches::State torches{level};
    Lighting::DungeonLight light{level};
    Manager enemies{level, torches};
    Health health{};
    Field()
    {
        level.spawn = {4, 4};
        level.rooms[0].bounds = {1,1,8,8};
        for (auto& row : level.floor) for (bool& tile : row) tile = true;
        assert(enemies.Spawn());
    }
    void tick(Vector2 p, Vector2 facing, float dt = 0.02f)
    {
        light.Update(p, facing);
        enemies.Update(dt, p, p, light, health);
    }
};

void SpawnsAndPatrol()
{
    for (unsigned seed = 0; seed < 128; ++seed)
    {
        Maze::GeneratedLevel g;
        assert(Maze::Generate(seed*2654435761u, g));
        Torches::State t(g.layout);
        Manager m(g.layout,t);
        assert(m.Spawn());
        const auto distances = Maze::MeasureDistances(g.layout);
        for (const Enemy* e : {&m.Angel(), &m.Ghost()})
        {
            const auto c = CellAt(CellAt(e->position));
            assert(distances.value[c.row][c.column] >= 32 && !m.Safe(e->position));
            assert(!e->discovered && e->state == State::Dormant);
        }
        assert(DistanceSquared(m.Angel().position,m.Ghost().position) >= SpawnSeparation*SpawnSeparation);
        if (seed > 7) continue;
        Health hp;
        Lighting::DungeonLight light(g.layout);
        const Vector2 p = Center(CellId(g.layout.spawn));
        for (int frame = 0; frame < 1500; ++frame)
        {
            m.Update(0.02f,p,p,light,hp);
            for (const Enemy* e : {&m.Angel(),&m.Ghost()})
            {
                assert(!m.Safe(e->position));
                assert(DistanceSquared(e->position,e->home) <= PatrolRadius*PatrolRadius+1);
            }
            for (float x : {-AngelRadius,AngelRadius})
                for (float y : {-AngelRadius,AngelRadius})
                    assert(Maze::IsWalkablePosition(g.layout,{m.Angel().position.x+x,m.Angel().position.y+y}));
        }
        assert(hp.points == 5 && m.Angel().route.searches > 0 && m.Angel().route.searches < 20);
    }
}

void SensesAndHits(Kind kind)
{
    Field f;
    const Enemy& e = kind == Kind::Angel ? f.enemies.Angel() : f.enemies.Ghost();
    const float side = e.position.x > 300 ? -1.0f : 1.0f;
    const Vector2 p{e.position.x+side*9,e.position.y};
    f.tick(p,{side,0},GraceSeconds);
    assert(!e.discovered && f.health.points == StartingHealth);
    for (int i=0;i<65;++i) f.tick(p,{side,0});
    assert(f.health.points == StartingHealth && !e.discovered);
    f.tick(p,{-side,0});
    assert(e.discovered);
    if (kind == Kind::Angel) assert(e.state == State::Frozen);
    const int early = kind == Kind::Angel ? 25 : 18;
    for (int i=0;i<early;++i) f.tick(p,{side,0});
    assert(e.discovered && f.health.points == StartingHealth);
    const int untilHit = kind == Kind::Angel ? 35 : 8;
    for (int i=0;i<untilHit;++i) f.tick(p,{side,0});
    if (kind == Kind::Angel) assert(f.health.Dead() && f.health.lastHit == kind);
    else
    {
        assert(f.health.points == StartingHealth-1 && f.health.lastHit == kind && e.attackPulse > 0);
        for (int i=0;i<45;++i) f.tick(p,{side,0});
        assert(f.health.points == StartingHealth-1);
    }
}

void LossOfInterest()
{
    Field f;
    const Enemy& a = f.enemies.Angel();
    const float inward = a.home.x > 10*Maze::CellSize ? -1.0f : 1.0f;
    const Vector2 near{a.home.x+inward*90,a.home.y};
    f.tick(near,{inward,0},GraceSeconds);
    f.tick(near,{inward,0});
    assert(a.state == State::Chasing);
    Vector2 lost{a.home.x+inward*9.9f*Maze::CellSize,a.home.y};
    f.tick(lost,{inward,0});
    assert(a.state == State::Investigating && DistanceSquared(a.target,near)==0);
    for(int i=0;i<155;++i) f.tick(lost,{inward,0});
    assert(a.state == State::Returning);
    f.tick(Center(CellId(f.level.spawn)),{1,0});
    assert(a.state != State::Chasing);
}

void PathsAndWards()
{
    Field f;
    auto& l = f.level;
    const int start = CellId({18,18}), goal = CellId({20,18});
    TorchBarrier wards(l);
    wards.Refresh(f.torches);
    Route path;
    l.floor[18][19] = false;
    FindAngelRoute(l,start,goal,path,wards,Center(start));
    assert(path.length == 5);
    FindGhostRoute(l,wards,start,goal,path,Center(start));
    assert(path.length == 5);
    for (int y = 13; y <= 23; ++y) l.floor[y][19] = false;
    FindGhostRoute(l,wards,start,goal,path,Center(start));
    assert(path.length == 3 && path.cells[1] == CellId({19,18}));
    assert(!wards.SegmentClear(Center(CellId({0,4})),Center(CellId({13,4}))));

    l.torchSites[0] = {32,32};
    l.torchCount = 1;
    Vector2 source = f.torches.WorldPosition(0);
    for(int y=28;y<36;++y) l.floor[y][33]=false;
    assert(wards.IsClear(source));
    assert(f.torches.ActivateNearby(source));
    assert(wards.Refresh(f.torches) && !wards.Refresh(f.torches));
    Vector2 behind{source.x+60,source.y};
    assert(wards.IsClear(behind) && !f.enemies.Safe(behind));
    assert(!wards.IsClear(source) && f.enemies.Safe(source));
    assert(!wards.SegmentClear({source.x-30,source.y-82},{source.x-30,source.y+82}));
    for (bool ghost : {false,true})
    {
        const int from=CellId({29,29}), to=CellId({30,35});
        if (ghost) FindGhostRoute(l,wards,from,to,path,Center(from));
        else FindAngelRoute(l,from,to,path,wards,Center(from));
        assert(path.length>0);
        for(int i=1;i<path.length;++i) assert(wards.SegmentClear(Center(path.cells[i-1]),Center(path.cells[i])));
    }
}

void GhostCrossesWall()
{
    Field f;
    const Enemy& ghost = f.enemies.Ghost();
    const Maze::Cell home = CellAt(CellAt(ghost.home));
    const int direction = home.column > Maze::Columns / 2 ? -1 : 1;
    const int wall = home.column + direction;
    for (auto& row : f.level.floor) row[wall] = false;

    const Vector2 player = Center(CellId({home.column + 3 * direction, home.row}));
    f.tick(player, {static_cast<float>(direction), 0}, GraceSeconds);
    bool enteredWall = false;
    for (int frame = 0; frame < 200; ++frame)
    {
        f.enemies.Update(0.02f, {player.x + 1, player.y}, player, f.light, f.health);
        enteredWall |= !Maze::IsWalkablePosition(f.level, ghost.position);
    }
    assert(enteredWall && (CellAt(CellAt(ghost.position)).column - wall) * direction > 0);
    assert(ghost.state == State::Chasing && ghost.route.searches == 1);
    assert(f.health.points == StartingHealth);
}

void TestNewWard(Kind kind)
{
    Field f;
    const Enemy& e = kind == Kind::Angel ? f.enemies.Angel() : f.enemies.Ghost();
    const auto site = CellAt(CellAt(e.home));
    f.level.torchSites[0] = site;
    f.level.torchCount = 1;
    const Vector2 torch = f.torches.WorldPosition(0);
    f.tick(torch, {0,1}, GraceSeconds);
    assert(f.torches.ActivateNearby(torch));
    f.tick(torch,{0,1});
    assert(e.state == State::Repelled && f.enemies.Safe(torch));
    bool outside = false;
    for (int i=0; i<400; ++i)
    {
        f.tick(torch,{0,1});
        const bool inside = f.torches.IsPositionProtectedByTorch(e.position);
        assert(!outside || !inside);
        outside = !inside;
        assert(e.state != State::Chasing && f.health.points == StartingHealth);
    }
    assert(outside);
}

void TestDiscoveryOcclusion()
{
    Field f;
    const auto& a = f.enemies.Angel();
    const float step = a.home.x > 160 ? -1 : 1;
    Vector2 viewer{a.home.x + step * 3 * Maze::CellSize, a.home.y};
    const auto wall = CellAt(CellAt({a.home.x + step * Maze::CellSize,a.home.y}));
    f.level.floor[wall.row][wall.column] = false;
    f.tick(viewer,{-step,0});
    assert(!a.discovered);
    f.level.floor[wall.row][wall.column] = true;
    f.tick(viewer,{-step,0});
    assert(a.discovered);
    f.tick(viewer,{step,0});
    assert(a.discovered);
}

void PhysicalNavigation()
{
    Field f;
    const int start = CellId({18,18}), goal = CellId({22,18});
    for (int row = 16; row <= 20; ++row) f.level.floor[row][20] = false;
    Route astar, bfs;
    FindSentinelRoute(f.level, start, goal, astar, Center(start));
    FindFloorRoute(f.level, start, goal, bfs, Center(start),
                   [&](int, int, Maze::Cell c) { return InEntrance(f.level, c); });
    assert(astar.length == 11 && astar.length == bfs.length);
    assert(astar.nodesExplored < bfs.nodesExplored);
    for (const auto* route : {&astar, &bfs})
        for (int i = 1; i < route->length; ++i)
        {
            const auto cell = CellAt(route->cells[i]);
            assert(Manhattan(route->cells[i-1], route->cells[i]) == 1);
            assert(f.level.IsWalkable(cell.column, cell.row));
        }
    const int entrance = CellId({7,7}), outside = CellId({10,7});
    FindSentinelRoute(f.level, outside, entrance, astar, Center(outside));
    assert(astar.length == 0);
}

void PhysicalSpawns()
{
    for (unsigned seed = 0; seed < 24; ++seed)
    {
        Maze::GeneratedLevel g;
        assert(Maze::Generate(seed * 2654435761u, g));
        Exploration::Map map(g.layout);
        Lighting::DungeonLight light(g.layout);
        PhysicalEnemies a(g.layout), b(g.layout);
        Health first, second;
        const auto player = Center(CellId(g.layout.spawn));
        const Rectangle view{player.x - 192, player.y - 160, 384, 320};
        light.Update(player, {1,0});
        map.Visit(g.layout.spawn);
        a.SetSeed(seed);
        b.SetSeed(seed);
        auto update = [&](float dt) {
            a.Update(dt, player, view, light, map, false, {1,0}, first);
            b.Update(dt, player, view, light, map, false, {1,0}, second);
        };
        update(0.02f);
        for (const auto& actor : a.Actors())
            assert(actor.active && actor.body.state == State::Dormant && actor.timer == InitialPhysicalGrace);
        for (int i = 0; i < 6; ++i) update(MaxSpawnInterval);
        const auto distances = Maze::MeasureDistances(g.layout);
        for (std::size_t i = 0; i < a.Actors().size(); ++i)
        {
            const auto& actor = a.Actors()[i];
            assert(actor.active && actor.health == Stats(actor.body.kind).health);
            assert(i < SentinelCap ? actor.body.kind == Kind::Sentinel : actor.body.kind == Kind::Stalker);
            const auto c = CellAt(CellAt(actor.body.position));
            assert(distances.value[c.row][c.column] >= MinSpawnSteps && !InEntrance(g.layout, c));
            assert(!CheckCollisionPointRec(actor.body.position, view) && !light.IsWorldPositionObserved(actor.body.position));
            assert(DistanceSquared(actor.body.position, b.Actors()[i].body.position) == 0);
            for (std::size_t j = 0; j < i; ++j)
                assert(DistanceSquared(actor.body.home, a.Actors()[j].body.home) >= SpawnSpacing * SpawnSpacing);
        }
        assert(first.points == StartingHealth && second.points == StartingHealth);
    }
}

void PhysicalCombat()
{
    Field f;
    PhysicalEnemies physical(f.level);
    Exploration::Map map(f.level);
    Melee melee;
    Vector2 player = Center(CellId(f.level.spawn));
    auto update = [&](float dt, bool swing = false, Vector2 facing = {1,0}) {
        f.health.Update(dt);
        f.light.Update(player, facing);
        physical.Update(dt, player, {player.x-110, player.y-110, 220, 220}, f.light, map,
                        melee.Update(dt, swing, facing), facing, f.health);
    };

    update(0.02f);
    const auto& knight = physical.Actors()[0];
    player = {knight.body.position.x-24, knight.body.position.y};
    const auto site = CellAt(CellAt(knight.body.position));
    f.level.torchSites[0] = site;
    f.level.torchCount = 1;
    assert(f.torches.ActivateNearby(player) && f.enemies.Safe(player));
    update(0.1f);
    assert(knight.body.state == State::Dormant && f.health.points == StartingHealth);
    update(InitialPhysicalGrace);
    assert(knight.body.state == State::WindingUp && f.health.points == StartingHealth);
    update(SentinelStats.windup + 0.01f);
    assert(knight.swung && f.health.points == StartingHealth-1 && f.health.lastHit == Kind::Sentinel);
    update(0.1f);
    assert(f.health.points == StartingHealth-1 && !knight.swung);

    // A diagonal line through the corner of a neighboring wall cannot hit.
    player = {knight.body.position.x+17, knight.body.position.y+17};
    f.level.floor[site.row][site.column+1] = false;
    f.level.floor[site.row+1][site.column] = false;
    assert(!Maze::HasLineOfSight(f.level, player, knight.body.position));
    update(0.01f, true, {-0.7071f,-0.7071f});
    assert(knight.health == 3);
    f.level.floor[site.row][site.column+1] = true;
    f.level.floor[site.row+1][site.column] = true;
    player = {knight.body.position.x-24, knight.body.position.y};
    update(MeleeCooldown + 0.01f, true, {-1,0});
    assert(knight.health == 3);
    update(MeleeCooldown + 0.01f, true);
    assert(knight.health == 2 && knight.struck);
    update(0.01f, true);
    assert(knight.health == 2 && !knight.struck);
    update(MeleeCooldown + 0.01f, true);
    assert(knight.health == 1);
    update(MeleeCooldown + 0.01f, true);
    assert(knight.health == 0 && knight.body.state == State::Dying);
    update(DeathDuration + 0.01f);
    assert(!knight.active);

    player = Center(CellId(f.level.spawn));
    const auto& stalker = physical.Actors()[SentinelCap];
    assert(!knight.active);
    assert(stalker.active && stalker.body.kind == Kind::Stalker);
    const float flank = stalker.body.position.x < 100 ? 1.0f : -1.0f;
    player = {stalker.body.position.x+flank*65, stalker.body.position.y};
    update(0.02f);
    for (int frame = 0; frame < 260 && stalker.body.state == State::Dormant; ++frame)
        update(0.05f);
    assert(stalker.body.state == State::Chasing && stalker.body.route.searches == 1);
    for (int frame = 0; frame < 60 && stalker.body.state == State::Chasing; ++frame)
        update(0.02f);
    assert(stalker.body.state == State::WindingUp);
    player.y += 48;
    update(StalkerStats.windup + 0.01f);
    assert(stalker.swung && stalker.body.state == State::Recovering);
    assert(f.health.points == StartingHealth-1);
    player = {stalker.body.position.x+flank*24, stalker.body.position.y};
    update(MeleeCooldown + 0.01f, true, {-flank,0});
    assert(stalker.health == 0 && stalker.body.state == State::Dying);

    assert(f.health.points == StartingHealth-1);
    update(DeathDuration + 0.01f);
    assert(!knight.active && !stalker.active);
    player = Center(CellId(f.level.spawn));
    update(MaxSpawnInterval);
    assert(knight.active && knight.body.state == State::Dormant && knight.timer == SpawnRest && !stalker.active);
    update(MaxSpawnInterval);
    assert(stalker.active && stalker.body.state == State::Dormant);

    // The swing only visits the physical pool, even when the arc covers a spirit.
    player = {f.enemies.Angel().position.x-24, f.enemies.Angel().position.y};
    update(MeleeCooldown + 0.01f, true);
    assert(f.enemies.Angel().state == State::Dormant && f.enemies.Ghost().state == State::Dormant);
}

void RunProgressAndBats()
{
    Field f;
    Run::Clock clock{42};
    clock.Update(59, false);
    clock.Update(9, true);
    clock.Update(2.5f, false);
    char time[24];
    clock.Format(time);
    assert(std::strcmp(time, "01:01") == 0 && clock.seed == 42);

    Run::Souls souls;
    const auto cell = CellId({16,16});
    const Vector2 point = Center(cell);
    souls.Drop(Kind::Ghost, point, 0);
    assert(souls.At(cell) == 0);
    souls.Drop(Kind::Sentinel, {point.x + 5, point.y}, 0);
    souls.Drop(Kind::Stalker, point, 0);
    assert(souls.At(cell) == 20 && souls.Total() == 0);
    assert(souls.Collect(f.level,point, 0) == 0);
    assert(souls.Collect(f.level,{point.x+24,point.y}, 1) == 0);
    assert(souls.Collect(f.level,{point.x+4,point.y}, 1) == 20 && souls.Total() == 20);
    assert(souls.Collect(f.level,point, 1) == 0 && souls.At(cell) == 0 && souls.Total() == 20);

    Bats::Flock bats(f.level), repeat(f.level);
    bats.Place(42);
    repeat.Place(42);
    for (std::size_t i = 0; i < bats.Actors().size(); ++i)
        assert(bats.Actors()[i].perch == repeat.Actors()[i].perch);
    const auto first = &bats.Actors()[0];
    const Vector2 near = Center(first->perch);
    bool flown = false, rested = false;
    for (int i = 0; i < 1000; ++i)
    {
        bats.Update(0.02f,near);
        const auto tile = CellAt(first->perch);
        assert(f.level.IsWalkable(tile.column,tile.row) && !InEntrance(f.level,tile));
        if (first->destination != NoCell)
        {
            assert(Manhattan(first->perch,first->destination) == 1);
            flown = true;
        }
        else if (flown) rested = true;
    }
    assert(flown && rested && f.health.points == StartingHealth);
}

int main()
{
    SpawnsAndPatrol();
    SensesAndHits(Kind::Angel);
    SensesAndHits(Kind::Ghost);
    LossOfInterest();
    PathsAndWards();
    GhostCrossesWall();
    TestNewWard(Kind::Angel);
    TestNewWard(Kind::Ghost);
    TestDiscoveryOcclusion();
    PhysicalNavigation();
    PhysicalSpawns();
    PhysicalCombat();
    RunProgressAndBats();
    std::puts("128 supernatural and 24 physical spawn seeds, combat, respawns, timer, souls and bats passed.");
}
