#ifndef SHADOW_HUNT_DUNGEON_EXPLORATION_H
#define SHADOW_HUNT_DUNGEON_EXPLORATION_H
#include "maze_generation.h"
#include "enemy_contact.h"
#include "enemy_pathfinding.h"
#include <memory>

namespace Dungeon
{
constexpr int BarrelCount=18,ChestCount=12,WebCount=32,KeyCount=3;
constexpr unsigned EscapeKey=1u<<2;
constexpr float InteractionRange=35;
// Barrel 4.a's inspected alpha bounds become 24x32 at our native display scale.
constexpr float BarrelWidth=24,BarrelHeight=32;
enum class Kind { None, Door, Chest, Barrel, Exit };
struct Object { Maze::Cell cell{}; bool used=false; Vector2 offset{}; };
enum class ChestState { CLOSED, OPEN_WITH_GOLD, EMPTY };
constexpr float ChestPickupDelay=0.4f;
struct Chest
{
    Maze::Cell cell{};
    ChestState state=ChestState::CLOSED;
    unsigned reward=0;
    float readyAt=0;
};
struct Door { Rooms::CellArea area{}; bool open=false; };
struct Interaction { Kind kind=Kind::None; int index=-1; float distance=1e9f; };

class Exploration
{
public:
    explicit Exploration(Maze::Layout& layout):level(layout) {}
    Object barrels[BarrelCount]{},webs[WebCount]{},keys[KeyCount]{};
    Chest chests[ChestCount]{};
    Door doors[2]{};
    unsigned keyMask=0;
    unsigned long long gold=0;
    bool hidden=false,exitOpen=false;
    int hidingBarrel=-1;
    float animation=0,ghostWarning=0,warningCooldown=0;
    float pickupFlash=0;int lastKey=-1;
    bool Initialize(unsigned seed)
    {
        random=seed^0xAF24BC31u;
        // The generator gives each chamber one complete doorway. Close both
        // Library/Storage gates, not just a single tile of a wide entrance.
        for(int i=0;i<2;++i) { doors[i].area=level.rooms[i+1].doorway; Gate(i,true); }
        const auto distances=Maze::MeasureDistances(level);
        bool occupied[Maze::Rows][Maze::Columns]{};
        occupied[level.spawn.row][level.spawn.column]=true;
        occupied[level.futureExit.row][level.futureExit.column]=true;
        for(int i=0;i<KeyCount;++i)
        {
            int best=-1,score=-1;
            for(int r=1;r<Maze::Rows-1;++r) for(int c=1;c<Maze::Columns-1;++c)
            {
                if(occupied[r][c] || distances.value[r][c]<12) continue;
                int separation=distances.value[r][c];
                for(int k=0;k<i;++k) separation=std::min(separation,std::abs(c-keys[k].cell.column)+std::abs(r-keys[k].cell.row));
                if(separation>score) { score=separation;best=r*Maze::Columns+c; }
            }
            if(best<0) return false;
            keys[i].cell={best%Maze::Columns,best/Maze::Columns};occupied[best/Maze::Columns][best%Maze::Columns]=true;
        }
        // Deterministic topology-based candidate walk: no solids are added to
        // corridors. Props are interaction footprints, so narrow routes stay open.
        int nb=0,nc=0,nw=0;
        for(int room=1;room<=2;++room)
        {
            const auto a=level.rooms[room].bounds;
            const Maze::Cell reward{(a.left+a.right)/2,(a.top+a.bottom)/2};
            chests[nc++].cell=reward;occupied[reward.row][reward.column]=true;
        }
        const int offset=Next()%Enemies::CellCount;
        // Prefer permanent wall corners first, then other wall-adjacent tiles.
        // Use base stone, not temporary locked doors that can later disappear.
        for(int pass=0;pass<2 && nb<BarrelCount;++pass)
        for(int j=0;j<Enemies::CellCount && nb<BarrelCount;++j)
        {
            const int id=(offset+j*43)%Enemies::CellCount;const auto cell=Enemies::CellAt(id);
            if(!level.IsWalkable(cell.column,cell.row)||occupied[cell.row][cell.column]||Enemies::InEntrance(level,cell))continue;
            bool wall[4]{};
            for(int d=0;d<4;++d){auto n=Maze::Neighbor(cell,d);wall[d]=!Maze::Contains(n.column,n.row)||!level.floor[n.row][n.column];}
            const bool horizontal=wall[0]||wall[2],vertical=wall[1]||wall[3];
            if(!(horizontal||vertical) || (pass==0 && !(horizontal&&vertical)))continue;
            bool allowed=true;
            for(const auto& room:level.rooms)if(In(room.doorway,cell))allowed=false;
            for(int b=0;b<nb;++b)if(std::abs(cell.column-barrels[b].cell.column)+std::abs(cell.row-barrels[b].cell.row)<6)allowed=false;
            if(!allowed)continue;
            barrels[nb].cell=cell;
            barrels[nb].offset={wall[2]?-(Maze::CellSize-BarrelWidth)/2:(wall[0]?(Maze::CellSize-BarrelWidth)/2:0),
                                wall[3]?-(Maze::CellSize-BarrelHeight)/2:(wall[1]?(Maze::CellSize-BarrelHeight)/2:0)};
            ++nb;occupied[cell.row][cell.column]=true;
        }
        for(int j=0;j<Enemies::CellCount;++j)
        {
            const int id=(offset+j*43)%Enemies::CellCount;const Maze::Cell cell=Enemies::CellAt(id);
            if(!level.IsWalkable(cell.column,cell.row) || occupied[cell.row][cell.column] || Enemies::InEntrance(level,cell)) continue;
            int neighbors=0;
            for(int d=0;d<4;++d){auto n=Maze::Neighbor(cell,d);neighbors+=level.IsWalkable(n.column,n.row);}
            bool gate=false;for(const auto& room:level.rooms) if(In(room.doorway,cell)) gate=true;
            if(gate) continue;
            bool spaced=true;
            for(int c=0;c<nc;++c) if(std::abs(cell.column-chests[c].cell.column)+std::abs(cell.row-chests[c].cell.row)<9) spaced=false;
            if(nc<ChestCount && spaced && (neighbors<=2 || In(level.rooms[1].bounds,cell) || In(level.rooms[2].bounds,cell)))
            { chests[nc++].cell=cell;occupied[cell.row][cell.column]=true;continue; }
            if(nw<WebCount && neighbors<4) webs[nw++].cell=cell;
        }
        barrelCount=nb;chestCount=nc;webCount=nw;
        if(!ValidateProgression())return false;
        level.locked[level.futureExit.row][level.futureExit.column]=true;
        return true;
    }
    static bool In(Rooms::CellArea area,Maze::Cell c) {return c.column>=area.left&&c.column<=area.right&&c.row>=area.top&&c.row<=area.bottom;}
    static Vector2 Center(Maze::Cell c) {return {(c.column+0.5f)*Maze::CellSize,(c.row+0.5f)*Maze::CellSize};}
    Vector2 BarrelPosition(int i)const{auto p=Center(barrels[i].cell);return {p.x+barrels[i].offset.x,p.y+barrels[i].offset.y};}
    Vector2 ChestPickupPosition(int i)const{auto p=Center(chests[i].cell);return {p.x+10,p.y+8};}
    int BarrelSize()const{return barrelCount;} int ChestSize()const{return chestCount;} int WebSize()const{return webCount;}
    unsigned long long Score(unsigned long long souls)const{return souls+gold;}
    bool HasKey(int id)const{return (keyMask&(1u<<id))!=0;}
    bool ValidateProgression()const
    {
        // State-space BFS: at most V*8 states. FIFO is the existing custom
        // queue; visited is indexed by cell and the three-bit key mask.
        constexpr int States=Enemies::CellCount*8;
        auto visited=std::make_unique<bool[]>(States);
        auto queue=std::make_unique<Maze::Queue<int,States>>();
        const int start=Enemies::CellId(level.spawn);queue->Push(start);visited[start]=true;
        while(!queue->Empty())
        {
            const int state=queue->Pop(),id=state%Enemies::CellCount;unsigned mask=state/Enemies::CellCount;
            const auto cell=Enemies::CellAt(id);
            for(int k=0;k<KeyCount;++k) if(keys[k].cell.column==cell.column&&keys[k].cell.row==cell.row) mask|=1u<<k;
            if(cell.column==level.futureExit.column&&cell.row==level.futureExit.row&&mask==7) return true;
            for(int d=0;d<4;++d)
            {
                auto next=Maze::Neighbor(cell,d);
                if(!Maze::Contains(next.column,next.row)||!level.floor[next.row][next.column]) continue;
                if(next.column==level.futureExit.column&&next.row==level.futureExit.row&&!(mask&EscapeKey))continue;
                bool blocked=false;for(int lock=0;lock<2;++lock) if(In(doors[lock].area,next)&&!(mask&(1u<<lock))) blocked=true;
                if(blocked) continue;
                const int code=static_cast<int>(mask)*Enemies::CellCount+Enemies::CellId(next);
                if(!visited[code]){visited[code]=true;queue->Push(code);}
            }
        }
        return false;
    }
    void Update(float dt) {animation+=dt;ghostWarning=std::max(0.0f,ghostWarning-dt);warningCooldown=std::max(0.0f,warningCooldown-dt);pickupFlash=std::max(0.0f,pickupFlash-dt);}
    bool Collect(Vector2 player)
    {
        bool collected=false;
        for(int k=0;k<KeyCount;++k) if(!keys[k].used&&Near(player,Center(keys[k].cell),19))
        {keys[k].used=true;keyMask|=1u<<k;collected=true;lastKey=k;pickupFlash=1.2f;}
        const auto cell=Enemies::CellAt(Enemies::CellAt(player));
        for(int r=cell.row-1;r<=cell.row+1;++r)for(int c=cell.column-1;c<=cell.column+1;++c)
        {
            if(!Maze::Contains(c,r))continue;
            const int id=r*Maze::Columns+c;
            if(coins[id]&&animation>=ready[id]&&Near(player,Center({c,r}),19)) {gold+=coins[id];coins[id]=0;collected=true;}
        }
        // Chest ownership is explicit, separate from the tile-indexed enemy
        // pickups. A nearby enemy coin cannot empty or re-loot a chest.
        for(int i=0;i<chestCount;++i)
        {
            auto& chest=chests[i];
            if(chest.state!=ChestState::OPEN_WITH_GOLD || animation<chest.readyAt
                || !Near(player,ChestPickupPosition(i),19))continue;
            gold+=chest.reward;
            chest.reward=0;
            chest.state=ChestState::EMPTY;
            collected=true;
        }
        return collected;
    }
    void Drop(Enemies::Kind kind,Vector2 position)
    {
        const int id=Enemies::CellAt(position);if(id<0)return;
        coins[id]+=kind==Enemies::Kind::Sentinel?3+Next()%4:1+Next()%3;ready[id]=animation+0.25f;
    }
    unsigned At(int id)const{return coins[id];}
    Interaction Nearest(Vector2 player)const
    {
        Interaction best;
        const auto consider=[&](Kind kind,int index,Vector2 point)
        {
            const float d=Enemies::DistanceSquared(player,point);
            if(d<best.distance && Near(player,point,InteractionRange)) best={kind,index,d};
        };
        for(int i=0;i<2;++i)if(!doors[i].open)
        {
            const auto a=doors[i].area;
            Vector2 nearest{std::clamp(player.x,a.left*32.0f,(a.right+1)*32.0f),std::clamp(player.y,a.top*32.0f,(a.bottom+1)*32.0f)};
            // Probe just outside the collision plane so a locked door is interactable.
            Vector2 point{nearest.x+(player.x-nearest.x)*0.02f,nearest.y+(player.y-nearest.y)*0.02f};
            consider(Kind::Door,i,point);
        }
        for(int i=0;i<chestCount;++i)if(chests[i].state==ChestState::CLOSED)consider(Kind::Chest,i,Center(chests[i].cell));
        for(int i=0;i<barrelCount;++i)if(Enemies::DistanceSquared(player,BarrelPosition(i))<=14*14)consider(Kind::Barrel,i,BarrelPosition(i));
        if(!exitOpen)
        {
            const auto e=level.futureExit;
            Vector2 point{std::clamp(player.x,e.column*32.0f,(e.column+1)*32.0f),std::clamp(player.y,e.row*32.0f,(e.row+1)*32.0f)};
            point.x+=(player.x-point.x)*0.02f;point.y+=(player.y-point.y)*0.02f;
            consider(Kind::Exit,0,point);
        }
        return best;
    }
    const char* Prompt(Interaction action)const
    {
        switch(action.kind){
        case Kind::Barrel:return "Press E to Hide";
        case Kind::Chest:return "Press E to Open Chest";
        case Kind::Door:return HasKey(action.index)?"Press E to Unlock":"This door is locked. Find the matching key.";
        case Kind::Exit:return HasKey(2)?"Press E to Unlock the Exit":"The exit is sealed. Find the Escape Key.";
        default:return "";}
    }
    bool Activate(Interaction action)
    {
        if(action.kind==Kind::Door&&HasKey(action.index)&&!doors[action.index].open){doors[action.index].open=true;Gate(action.index,false);return true;}
        if(action.kind==Kind::Exit&&HasKey(2)){exitOpen=true;level.locked[level.futureExit.row][level.futureExit.column]=false;return true;}
        if(action.kind==Kind::Chest && action.index>=0 && action.index<chestCount)
        {
            auto& chest=chests[action.index];
            if(chest.state!=ChestState::CLOSED)return false;
            chest.state=ChestState::OPEN_WITH_GOLD;
            chest.reward=10+Next()%16;
            chest.readyAt=animation+ChestPickupDelay;
            return true;
        }
        if(action.kind==Kind::Barrel){hidden=true;hidingBarrel=action.index;}
        return false;
    }
    void WarnGhost(){if(warningCooldown<=0){ghostWarning=2;warningCooldown=4;}}
private:
    bool Near(Vector2 a,Vector2 b,float range)const{return Enemies::DistanceSquared(a,b)<=range*range&&Maze::HasLineOfSight(level,a,b);}
    void Gate(int id,bool closed){auto a=doors[id].area;for(int r=a.top;r<=a.bottom;++r)for(int c=a.left;c<=a.right;++c)if(Maze::Contains(c,r))level.locked[r][c]=closed;}
    unsigned Next(){return random=random*1664525u+1013904223u;}
    Maze::Layout& level;unsigned random=0;
    int barrelCount=0,chestCount=0,webCount=0;
    unsigned coins[Enemies::CellCount]{};float ready[Enemies::CellCount]{};
};
}
#endif
