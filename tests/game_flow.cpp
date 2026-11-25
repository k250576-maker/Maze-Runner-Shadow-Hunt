#include "../include/escape_objective.h"
#include "../include/bats.h"
#include "../include/physical_sprite_art.h"
#include <cassert>
#include <cstdio>
#include <memory>

void ExitSeedsAndThresholds()
{
    for(unsigned index=0;index<130;++index)
    {
        const unsigned seed=index==128?42u:(index==129?0xffffffffu:index*2654435761u);
        auto generated=std::make_unique<Maze::GeneratedLevel>();
        assert(Maze::Generate(seed,*generated));
        Escape::Objective exit(generated->layout);
        assert(exit.Initialize());
        const auto distances=Maze::MeasureDistances(generated->layout);
        assert(exit.DistanceFromStart()==generated->exitDistance);
        assert(distances.value[generated->layout.futureExit.row][generated->layout.futureExit.column]>0);
        Vector2 before{};
        const Vector2 center=exit.Center();
        bool approach=false;
        for(int direction=0;direction<4;++direction)
        {
            const Maze::Cell cell=Maze::Neighbor(generated->layout.futureExit,direction);
            if(!generated->layout.IsWalkable(cell.column,cell.row)) continue;
            before={(cell.column+0.5f)*Maze::CellSize,(cell.row+0.5f)*Maze::CellSize};
            approach=true;break;
        }
        assert(approach);
        const Escape::Result stats{seed,93.5,35,4,3};
        assert(!exit.TryComplete(before,before,true,stats));
        const float length=std::sqrt(Enemies::DistanceSquared(before,center));
        const Vector2 nearby{center.x+(before.x-center.x)*15/length,center.y+(before.y-center.y)*15/length};
        assert(!exit.TryComplete(before,nearby,true,stats));
        assert(!exit.TryComplete(before,center,false,stats));
        auto dead=stats;dead.health=0;
        assert(!exit.TryComplete(before,center,true,dead));
        exit.SetUnlocked(false);
        assert(!exit.TryComplete(before,center,true,stats));
        exit.SetUnlocked(true);
        assert(exit.TryComplete(before,center,true,stats));
        assert(exit.Completed() && exit.FinalResult().seed==seed && exit.FinalResult().elapsed==93.5);
        auto changed=stats;changed.souls=999;
        assert(!exit.TryComplete(before,center,true,changed));
        assert(exit.FinalResult().souls==35 && exit.FinalResult().defeated==3);
        // Fresh objective has no completed result (same-seed restarts reset).
        Escape::Objective fresh(generated->layout);assert(fresh.Initialize());assert(!fresh.Completed());
    }
    // A wall separating the threshold from the previous position must block
    // completion even when a long segment geometrically crosses the rectangle.
    Maze::Layout blocked;
    blocked.spawn={1,1};blocked.futureExit={4,1};
    for(int c=1;c<=4;++c) blocked.floor[1][c]=true;
    blocked.floor[1][2]=false;
    Escape::Objective inaccessible(blocked);assert(!inaccessible.Initialize());
    assert(!inaccessible.TryComplete({48,48},{144,48},true,{1,1,0,5,0}));
    for(int c=1;c<=4;++c) blocked.floor[2][c]=true;
    Escape::Objective aroundWall(blocked);assert(aroundWall.Initialize());
    assert(!aroundWall.TryComplete({48,48},{144,48},true,{1,1,0,5,0}));
}

void BatFrameRates()
{
    int occupiedRooms = 0, nearTorches = 0, nearBarrels = 0;
    for (unsigned seed : {1u, 42u, 43u, 87u})
    {
        Maze::GeneratedLevel generated;
        assert(Maze::Generate(seed, generated));
        auto& level = generated.layout;
        Dungeon::Exploration dungeon(level);
        assert(dungeon.Initialize(seed));
        std::array<Vector2, Bats::Capacity> baseline{};
        for (int fps : {30, 60, 144})
        {
            Bats::Flock flock(level);
            flock.Place(seed);
            for (int tick = 0; tick < fps * 90; ++tick)
            {
                const auto before = flock.Actors();
                const float dt = 1.0f / fps;
                flock.Update(dt);
                for (std::size_t i = 0; i < before.size(); ++i)
                {
                    const auto& bat = flock.Actors()[i];
                    assert(bat.cell != Enemies::NoCell);
                    const auto position = bat.Ground();
                    assert(Maze::IsWalkablePosition(level, position));
                    assert(Maze::HasLineOfSight(level, before[i].Ground(), position));
                    assert(Enemies::DistanceSquared(Enemies::Center(bat.home), position) <=
                           (Bats::WanderRadius + 0.01f) * (Bats::WanderRadius + 0.01f));
                    const float movement = Enemies::DistanceSquared(before[i].Ground(), position);
                    assert(movement <= (46 * dt + 0.001f) * (46 * dt + 0.001f));
                    assert(bat.stalled < 0.25f && bat.wing != before[i].wing);
                    if (fps != 30 || tick % 7 != 0) continue;
                    const auto cell = Enemies::CellAt(Enemies::CellAt(position));
                    for (const auto& room : level.rooms) occupiedRooms += Dungeon::Exploration::In(room.bounds, cell);
                    for (int t = 0; t < level.torchCount; ++t)
                        nearTorches += Enemies::DistanceSquared(Dungeon::Exploration::Center(level.torchSites[t]), position) < 32 * 32;
                    for (int b = 0; b < dungeon.BarrelSize(); ++b)
                        nearBarrels += Enemies::DistanceSquared(dungeon.BarrelPosition(b), position) < 32 * 32;
                }
                if (tick == fps * 10)
                {
                    const auto paused = flock.Actors();
                    flock.Update(0);
                    assert(flock.Actors()[0].wing == paused[0].wing);
                    assert(Enemies::DistanceSquared(flock.Actors()[0].Ground(), paused[0].Ground()) == 0);
                }
            }
            for (std::size_t i = 0; i < baseline.size(); ++i)
            {
                const auto position = flock.Actors()[i].Ground();
                if (fps == 30) baseline[i] = position;
                else assert(Enemies::DistanceSquared(position, baseline[i]) < 0.04f);
            }
        }
    }
    assert(occupiedRooms > 0 && nearTorches > 0 && nearBarrels > 0);
}

void BatClosedDoor()
{
    Maze::Layout level;
    for (int c = 5; c <= 11; ++c) level.floor[5][c] = true;
    for (int r = 5; r <= 10; ++r) level.floor[r][11] = true;
    Bats::Flock flock(level);
    flock.Place(4);
    const auto& bat = flock.Actors()[0];
    assert(bat.cell != Enemies::NoCell && bat.destination != bat.cell);
    const auto blocked = Enemies::CellAt(bat.destination);
    flock.Update(0.20f);
    level.locked[blocked.row][blocked.column] = true;
    const auto before = bat.Ground();
    flock.Update(0.05f);
    assert(Enemies::DistanceSquared(before, bat.Ground()) > 0);
    for (int i = 0; i < 600; ++i)
    {
        flock.Update(1.0f / 60);
        assert(Maze::IsWalkablePosition(level, bat.Ground()) && bat.stalled < 0.3f);
    }

    // A one-tile pocket still has room to move, and will leave the pocket
    // once an adjacent gate opens.
    Maze::Layout pocket;
    pocket.floor[5][5] = pocket.floor[5][6] = true;
    pocket.locked[5][6] = true;
    Bats::Flock trapped(pocket);
    trapped.Place(42);
    const auto& isolated = trapped.Actors()[0];
    const auto start = isolated.Ground();
    trapped.Update(0.1f);
    assert(Enemies::DistanceSquared(start, isolated.Ground()) > 0);
    pocket.locked[5][6] = false;
    bool crossed = false;
    for (int i = 0; i < 180; ++i)
    {
        trapped.Update(1.0f / 60);
        crossed |= Enemies::CellAt(isolated.Ground()) == Enemies::CellId({6, 5});
    }
    assert(crossed);

    // Simulate a flight that has lost its velocity. The watchdog should
    // visibly resume local movement without retrying the failed leg.
    Bats::Flock slow(level);
    slow.Place(4);
    auto& stalled = const_cast<Bats::Bat&>(slow.Actors()[0]);
    const auto oldTarget = stalled.target;
    stalled.speed = 0.0001f;
    slow.Update(Bats::StallSeconds + 0.02f);
    assert(stalled.speed > 20 && Enemies::DistanceSquared(stalled.target, oldTarget) > 0);
    const auto recovered = stalled.Ground();
    slow.Update(0.2f);
    assert(Enemies::DistanceSquared(recovered, stalled.Ground()) > 4);
}

int main()
{
    ExitSeedsAndThresholds();BatFrameRates();BatClosedDoor();
    assert(Enemies::SentinelStats.health==3 && Enemies::StalkerStats.health==1);
    assert(Enemies::SentinelStats.reach==36 && Enemies::StalkerStats.reach==29);
    std::puts("130 escape seeds and 90-second bat flights at 30/60/144 FPS, including locked doors and pause/resume, passed.");
}
