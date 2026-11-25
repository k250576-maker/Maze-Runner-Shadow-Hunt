#include "../include/dungeon_exploration.h"
#include "../include/escape_objective.h"
#include "../include/physical_enemies.h"
#include <cassert>
#include <cstdio>
void HiddenSensing()
{
    auto level=std::make_unique<Maze::Layout>();
    for(auto& row:level->floor)for(bool& floor:row)floor=true;
    level->spawn={4,4};level->rooms[0].bounds={1,1,8,8};
    Torches::State torches(*level);Enemies::Manager enemies(*level,torches);assert(enemies.Spawn());
    Lighting::DungeonLight light(*level);light.SetObserverEnabled(false);Enemies::Health health;
    auto ghost=enemies.Ghost().position;Vector2 player{ghost.x+24,ghost.y};light.Update(player,{1,0});
    enemies.Update(13,player,player,light,health,true);
    for(int i=0;i<80&&health.points==5;++i)enemies.Update(0.02f,player,player,light,health,true);
    assert(health.points<5&&health.lastHit==Enemies::Kind::Ghost);
    auto angel=enemies.Angel().position;player={angel.x+24,angel.y};light.Update(player,{1,0});
    for(int i=0;i<30;++i)enemies.Update(0.02f,player,player,light,health,true);
    assert(enemies.Angel().state!=Enemies::State::Chasing&&enemies.Angel().windup==0);
    Enemies::PhysicalEnemies physical(*level);Exploration::Map map(*level);
    player={144,144};physical.Update(0.01f,player,{120,120,100,100},light,map,health,true);
    for(int i=0;i<800;++i)physical.Update(0.02f,player,{120,120,100,100},light,map,health,true);
    for(const auto& actor:physical.Actors())assert(actor.body.state!=Enemies::State::Chasing&&actor.body.state!=Enemies::State::WindingUp);
}
int main()
{
    HiddenSensing();
    for(unsigned seed=0;seed<128;++seed)
    {
        auto generated=std::make_unique<Maze::GeneratedLevel>();assert(Maze::Generate(seed,*generated));
        Escape::Objective exit(generated->layout);assert(exit.Initialize());exit.SetUnlocked(false);
        auto state=std::make_unique<Dungeon::Exploration>(generated->layout);
        assert(state->Initialize(seed));assert(state->ValidateProgression());
        assert(state->BarrelSize()>0 && state->ChestSize()>0 && state->WebSize()>0);
        for(int i=0;i<state->BarrelSize();++i)
        {
            const auto cell=state->barrels[i].cell;const auto p=state->BarrelPosition(i);
            const auto stone=[&](int c,int r){return !Maze::Contains(c,r)||!generated->layout.floor[r][c];};
            const bool touching=(stone(cell.column-1,cell.row)&&std::fabs(p.x-Dungeon::BarrelWidth/2-cell.column*32)<0.01f)
                ||(stone(cell.column+1,cell.row)&&std::fabs(p.x+Dungeon::BarrelWidth/2-(cell.column+1)*32)<0.01f)
                ||(stone(cell.column,cell.row-1)&&std::fabs(p.y-Dungeon::BarrelHeight/2-cell.row*32)<0.01f)
                ||(stone(cell.column,cell.row+1)&&std::fabs(p.y+Dungeon::BarrelHeight/2-(cell.row+1)*32)<0.01f);
            assert(touching);
        }
        if(seed<24)
        {
            Lighting::DungeonLight light(generated->layout);Exploration::Map chart(generated->layout);Enemies::Health hp;
            Enemies::PhysicalEnemies pool(generated->layout);pool.SetSeed(seed);
            const auto start=Dungeon::Exploration::Center(generated->layout.spawn);light.Update(start,{1,0});
            pool.Update(0.02f,start,{start.x-64,start.y-64,128,128},light,chart,hp);
            int wizard=0,demon=0,near=100000,far=0;
            const auto distances=Maze::MeasureDistances(generated->layout);
            for(const auto& actor:pool.Actors())if(actor.active)
            {
                if(actor.body.kind==Enemies::Kind::Sentinel)++wizard;else ++demon;
                const auto cell=Enemies::CellAt(Enemies::CellAt(actor.body.position));
                near=std::min(near,distances.value[cell.row][cell.column]);far=std::max(far,distances.value[cell.row][cell.column]);
            }
            assert(wizard<=Enemies::SentinelCap&&demon<=Enemies::StalkerCap&&wizard+demon>=8);
            assert(far>near+20);
        }
        assert(!state->exitOpen && !generated->layout.IsWalkable(generated->layout.futureExit.column,generated->layout.futureExit.row));
        for(int k=0;k<3;++k)
        {
            auto distances=Maze::MeasureDistances(generated->layout);
            auto cell=state->keys[k].cell;assert(distances.value[cell.row][cell.column]>=0);
            assert(state->Collect(Dungeon::Exploration::Center(cell)));assert(state->HasKey(k));
            assert(!state->Collect(Dungeon::Exploration::Center(cell)));
        }
        for(int d=0;d<2;++d)
        {
            assert(state->Activate({Dungeon::Kind::Door,d,0}));
            auto a=state->doors[d].area;
            for(int r=a.top;r<=a.bottom;++r)for(int c=a.left;c<=a.right;++c)assert(generated->layout.IsWalkable(c,r));
        }
        assert(state->Activate({Dungeon::Kind::Exit,0,0}));exit.SetUnlocked(state->exitOpen);
        assert(generated->layout.IsWalkable(generated->layout.futureExit.column,generated->layout.futureExit.row));
        const auto chest=Dungeon::Interaction{Dungeon::Kind::Chest,0,0};
        const auto beforeChest=state->gold;
        assert(state->Activate(chest));const auto reward=state->chests[0].reward;assert(reward>=10&&reward<=25);
        assert(state->gold==beforeChest&&state->chests[0].state==Dungeon::ChestState::OPEN_WITH_GOLD);
        assert(!state->Collect(state->ChestPickupPosition(0))); // opening delay
        const auto gold=state->gold;
        assert(!state->Activate(chest)&&state->gold==gold);
        state->Update(Dungeon::ChestPickupDelay+0.01f);
        // An enemy drop on the same tile is a separate pickup. Approaching
        // from the left collects it without reaching the chest's owned gold.
        const auto center=Dungeon::Exploration::Center(state->chests[0].cell);
        state->Drop(Enemies::Kind::Stalker,center);state->Update(0.3f);
        assert(state->Collect({center.x-16,center.y}));
        const auto enemyGold=state->gold;assert(enemyGold>=gold+1&&enemyGold<=gold+3);
        assert(state->chests[0].state==Dungeon::ChestState::OPEN_WITH_GOLD);
        assert(state->Collect(state->ChestPickupPosition(0)));
        assert(state->gold==enemyGold+reward&&state->chests[0].state==Dungeon::ChestState::EMPTY&&state->chests[0].reward==0);
        assert(!state->Collect(state->ChestPickupPosition(0))&&!state->Activate(chest));
        const auto looted=state->gold;
        auto p=Dungeon::Exploration::Center(generated->layout.spawn);
        state->Drop(Enemies::Kind::Sentinel,p);state->Update(0.3f);
        assert(state->Collect(p));assert(state->gold>=looted+3&&state->gold<=looted+6);
        auto after=state->gold;assert(!state->Collect(p)&&state->gold==after);
        assert(state->Score(15)==after+15);
        if(seed<4)
        {
            auto freshLevel=std::make_unique<Maze::GeneratedLevel>();assert(Maze::Generate(seed,*freshLevel));
            auto fresh=std::make_unique<Dungeon::Exploration>(freshLevel->layout);assert(fresh->Initialize(seed));
            assert(fresh->gold==0);
            for(int i=0;i<fresh->ChestSize();++i)assert(fresh->chests[i].state==Dungeon::ChestState::CLOSED&&fresh->chests[i].reward==0);
        }
        assert(state->Activate({Dungeon::Kind::Barrel,0,0})==false&&state->hidden);
        state->WarnGhost();assert(state->ghostWarning==2);state->Update(1);state->WarnGhost();assert(state->ghostWarning==1);
    }
    std::puts("128 seeds: all keys reachable before locks, state-space BFS escape, full-width door collision, persistent chests, exact-once loot, hiding state and warning cooldown passed.");
}
