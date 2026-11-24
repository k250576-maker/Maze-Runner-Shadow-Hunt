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
    auto generated=std::make_unique<Maze::GeneratedLevel>();assert(Maze::Generate(42,*generated));
    Bats::Flock initial(generated->layout);initial.Place(42);
    const auto& bat=initial.Actors()[0];
    const Vector2 player=Enemies::Center(bat.perch);
    // Interpolation eases continuously to the exact perch and does not depend
    // on torch state. Lighting is not an input to the bat simulation.
    Bats::Bat flying=bat;
    for(int direction=0;direction<4;++direction)
    {
        const auto next=Maze::Neighbor(Enemies::CellAt(bat.perch),direction);
        if(generated->layout.IsWalkable(next.column,next.row)) { flying.destination=Enemies::CellId(next);break; }
    }
    assert(flying.destination!=Enemies::NoCell);
    flying.flight=0;const Vector2 start=flying.Ground();
    flying.flight=Bats::FlightSeconds*0.5f;const Vector2 half=flying.Ground();
    flying.flight=Bats::FlightSeconds;const Vector2 end=flying.Ground();
    assert(std::fabs(half.x-(start.x+end.x)/2)<0.001f);
    assert(std::fabs(half.y-(start.y+end.y)/2)<0.001f);
    for(int fps:{30,60,144})
    {
        Bats::Flock flock(generated->layout);flock.Place(42);
        for(int frame=0;frame<fps*3;++frame) flock.Update(1.0f/fps,player);
        assert(std::fabs(flock.Actors()[0].wing-(bat.wing+3))<0.002f);
        for(const auto& actor:flock.Actors())
        {
            if(actor.perch==Enemies::NoCell) continue;
            assert(generated->layout.IsWalkable(Enemies::CellAt(actor.perch).column,Enemies::CellAt(actor.perch).row));
        }
    }
}

int main()
{
    ExitSeedsAndThresholds();BatFrameRates();
    assert(Enemies::SentinelStats.health==3 && Enemies::StalkerStats.health==1);
    assert(Enemies::SentinelStats.reach==36 && Enemies::StalkerStats.reach==29);
    std::puts("130 exit seeds: reachability, thresholds, death precedence, result latching, reset, and 30/60/144 FPS bat checks passed.");
}
