#ifndef SHADOW_HUNT_ESCAPE_OBJECTIVE_H
#define SHADOW_HUNT_ESCAPE_OBJECTIVE_H

#include "maze_generation.h"
#include "maze_visibility.h"
#include "lighting.h"
#include "run_progress.h"

namespace Escape
{
struct Result
{
    std::uint32_t seed=0;
    double elapsed=0;
    Run::Score souls=0;
    int health=0;
    unsigned defeated=0;
    unsigned long long gold=0;
};

class Objective
{
public:
    explicit Objective(const Maze::Layout& layout):level(layout) {}
    bool Initialize()
    {
        const Maze::Cell cell=level.futureExit;
        if(!level.IsWalkable(cell.column,cell.row)) return false;
        // Reuse the custom queue/BFS. The generator already selected the
        // farthest reachable non-entrance chamber tile; do not carve a new exit.
        const auto distances=Maze::MeasureDistances(level);
        distance=distances.value[cell.row][cell.column];
        valid=distance>0;
        return valid;
    }
    Vector2 Center() const { return {(level.futureExit.column+0.5f)*Maze::CellSize,(level.futureExit.row+0.5f)*Maze::CellSize}; }
    Rectangle Threshold() const { const auto p=Center(); return {p.x-10,p.y-6,20,12}; }
    bool Completed() const { return complete; }
    int DistanceFromStart() const { return distance; }
    const Result& FinalResult() const { return result; }
    void SetUnlocked(bool value) { unlocked=value; }
    bool Unlocked()const{return unlocked;}

    bool TryComplete(Vector2 previous,Vector2 current,bool alive,const Result& statistics)
    {
        if(!valid || complete || !unlocked || !alive || statistics.health<=0) return false;
        if(previous.x==current.x && previous.y==current.y) return false;
        const Rectangle threshold=Threshold();
        if(CheckCollisionPointRec(previous,threshold) || !Maze::HasLineOfSight(level,previous,current)) return false;
        // Segment/rectangle slab intersection catches a complete crossing even
        // in a long frame. Nearby positions or a cell across a wall cannot win.
        float first=0,last=1;
        const auto clip=[&](float start,float delta,float low,float high)
        {
            if(std::fabs(delta)<0.00001f) return start>=low && start<=high;
            float a=(low-start)/delta,b=(high-start)/delta;
            if(a>b) std::swap(a,b);
            first=std::max(first,a); last=std::min(last,b);
            return first<=last;
        };
        if(!clip(previous.x,current.x-previous.x,threshold.x,threshold.x+threshold.width)
            || !clip(previous.y,current.y-previous.y,threshold.y,threshold.y+threshold.height)) return false;
        result=statistics;
        complete=true;
        return true;
    }
    Lighting::PointLight Light() const
    {
        if(!unlocked)
        {
            for(int d=0;d<4;++d){const auto n=Maze::Neighbor(level.futureExit,d);if(level.IsWalkable(n.column,n.row))return {{(n.column+0.5f)*32,(n.row+0.5f)*32},48,{255,180,80,255},0.5f,0.15f,0};}
        }
        return {Center(),2.5f*Maze::CellSize,{255,201,100,255},1.0f,0.34f,0.025f};
    }
    void Draw(Texture2D atlas,double elapsed) const
    {
        const Vector2 p=Center();
        // Inspected open stone arch at (304,16), 80x96, in mainlevbuild.png.
        DrawRectangleGradientV(static_cast<int>(p.x)-10,static_cast<int>(p.y)-34,20,34,
                               {12,10,13,255},{65,42,18,255});
        DrawTexturePro(atlas,{304,16,80,96},{p.x-20,p.y-42,40,48},{0,0},0,{255,224,158,255});
        DrawRectangleRec(Threshold(),{90,61,27,230});
        if(!unlocked)for(int i=-8;i<=8;i+=4)DrawRectangle(static_cast<int>(p.x)+i,static_cast<int>(p.y)-34,1,39,{48,36,23,255});
        const float pulse=0.6f+0.2f*std::sin(static_cast<float>(elapsed)*2);
        DrawRectangleLinesEx(Threshold(),1,Fade({255,223,140,255},pulse));
        // Glints remain inside the exit footprint; no illumination bypasses LOS.
        for(int i=0;i<4;++i)
        {
            const float t=static_cast<float>(elapsed)*0.7f+i*0.25f;
            DrawRectangle(static_cast<int>(p.x-7+14*std::sin(t*3+i)),
                          static_cast<int>(p.y-4-24*(t-std::floor(t))),1,2,{251,218,135,220});
        }
    }
    void DrawLabel(Vector2 player) const
    {
        const Vector2 p=Center();
        const float dx=player.x-p.x,dy=player.y-p.y;
        if(dx*dx+dy*dy<9*Maze::CellSize*Maze::CellSize && Maze::HasLineOfSight(level,player,p))
            DrawText("ESCAPE",static_cast<int>(p.x)-18,static_cast<int>(p.y)-53,10,{255,228,153,255});
    }
private:
    const Maze::Layout& level;
    bool valid=false,unlocked=true,complete=false;
    int distance=-1;
    Result result{};
};
}
#endif
