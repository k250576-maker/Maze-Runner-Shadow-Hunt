#ifndef SHADOW_HUNT_EXPLORATION_ART_H
#define SHADOW_HUNT_EXPLORATION_ART_H
#include "dungeon_exploration.h"
#include <string>
namespace Dungeon
{
class Art
{
public:
    void Load(const std::string& root)
    {
        barrel=LoadSmall(root+"Furniture/Storage - Barrel 4.a - Dark.png",32);
        web=LoadSmall(root+"Furniture/Clutter - Webs 1 - Thin.png",32);
        closed=LoadSmall(root+"enviroment/CHESTS/png/CLOSED/NO_BACKLIGHT/chests-01.png",32);
        opened=LoadSmall(root+"enviroment/CHESTS/png/OPEN_GOLD/NO_BACKLIGHT/chests-01.png",32);
        empty=LoadSmall(root+"enviroment/CHESTS/png/OPEN_EMPTY/NO_BACKLIGHT/chests-01.png",32);
        const char* colors[]={"BRONZE","SILVER","GOLD"};
        for(int i=0;i<3;++i)key[i]=LoadSmall(root+"enviroment/keys/Key 9/Key 9 - "+colors[i]+" -.png",24);
        for(int i=0;i<8;++i)coins[i]=LoadSmall(root+"enviroment/coin_"+std::to_string(i+1)+".png",16);
    }
    void Unload()
    {
        for(auto texture:{barrel,web,closed,opened,empty})if(IsTextureValid(texture))UnloadTexture(texture);
        for(auto texture:key)if(IsTextureValid(texture))UnloadTexture(texture);
        for(auto texture:coins)if(IsTextureValid(texture))UnloadTexture(texture);
    }
    void Draw(const Exploration& state,Rectangle visible)const
    {
        for(int i=0;i<state.WebSize();++i)Sprite(web,Exploration::Center(state.webs[i].cell),visible);
        for(int i=0;i<state.BarrelSize();++i)Sprite(barrel,state.BarrelPosition(i),visible);
        for(int i=0;i<state.ChestSize();++i)
        {
            const ChestState phase=state.chests[i].state;
            const Texture2D texture=phase==ChestState::CLOSED?closed:
                (phase==ChestState::OPEN_WITH_GOLD?opened:(IsTextureValid(empty)?empty:closed));
            Sprite(texture,Exploration::Center(state.chests[i].cell),visible);
            if(phase==ChestState::OPEN_WITH_GOLD)
                Sprite(coins[static_cast<int>(state.animation*8)%8],state.ChestPickupPosition(i),visible);
        }
        for(int i=0;i<3;++i)if(!state.keys[i].used)
        {
            auto p=Exploration::Center(state.keys[i].cell);p.y-=2*std::sin(state.animation*3+i);
            Sprite(key[i],p,visible);
        }
        for(int id=0;id<Enemies::CellCount;++id)if(state.At(id))Sprite(coins[static_cast<int>(state.animation*8)%8],Enemies::Center(id),visible);
        for(int i=0;i<2;++i)if(!state.doors[i].open)
        {
            const auto a=state.doors[i].area;
            Rectangle door{a.left*32.0f,a.top*32.0f,(a.right-a.left+1)*32.0f,(a.bottom-a.top+1)*32.0f};
            DrawRectangleRec(door,{78,49,25,255});DrawRectangleLinesEx(door,2,i==0?Color{190,127,60,255}:Color{183,199,209,255});
            for(float x=door.x+5;x<door.x+door.width;x+=8)DrawLineEx({x,door.y+2},{x,door.y+door.height-2},2,{29,22,20,255});
            DrawCircleV({door.x+door.width/2,door.y+door.height/2},4,{226,185,93,255});
        }
    }
private:
    static Texture2D LoadSmall(const std::string& path,int extent)
    {
        if(!FileExists(path.c_str())){TraceLog(LOG_WARNING,"Exploration asset missing: %s",path.c_str());return {};}
        Image image=LoadImage(path.c_str());if(!IsImageValid(image))return {};
        auto bounds=GetImageAlphaBorder(image,0.03f);if(bounds.width>0&&bounds.height>0)ImageCrop(&image,bounds);
        const float scale=static_cast<float>(extent)/std::max(image.width,image.height);
        ImageResizeNN(&image,std::max(1,static_cast<int>(image.width*scale)),std::max(1,static_cast<int>(image.height*scale)));
        Texture2D texture=LoadTextureFromImage(image);UnloadImage(image);SetTextureFilter(texture,TEXTURE_FILTER_POINT);return texture;
    }
    static void Sprite(Texture2D texture,Vector2 p,Rectangle visible)
    {
        if(!IsTextureValid(texture)||!CheckCollisionPointRec(p,visible))return;
        DrawTextureV(texture,{p.x-texture.width/2.0f,p.y-texture.height/2.0f},WHITE);
    }
    Texture2D barrel{},web{},closed{},opened{},empty{},key[3]{},coins[8]{};
};
}
#endif
