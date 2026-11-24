#ifndef SHADOW_HUNT_PHYSICAL_SPRITE_ART_H
#define SHADOW_HUNT_PHYSICAL_SPRITE_ART_H

#include "physical_enemies.h"
#include <string>

namespace Enemies
{
enum class Clip { Idle, Move, Attack, Hurt, Death, Count };
struct AnimationSheet { Texture2D texture{}; int frameSize=0,frames=0; Vector2 anchor{}; float scale=1; };

class PhysicalSprites
{
public:
    void Load(const std::string& root)
    {
        const std::string wizard=root+"characters/new enemies/EVil Wizard 2/Sprites/";
        const std::string demon=root+"characters/new enemies/Tiny RPG Character Asset Pack 02 -Free Demon_A&Blood Monster_A/Characters(100x100 split)/Demon_A/Demon_A/";
        const char* wizardNames[]={"Idle.png","Run.png","Attack1.png","Take hit.png","Death.png"};
        const char* demonNames[]={"Demon_A_Idle.png","Demon_A_Walk.png","Demon_A_Attack01.png","Demon_A_Hurt.png","Demon_A_Death.png"};
        const int wizardCounts[]={8,8,8,3,7},demonCounts[]={6,8,7,4,4};
        for(int i=0;i<5;++i)
        {
            LoadSheet(wizardClips[i],wizard+wizardNames[i],250,wizardCounts[i],{125,167},0.5f);
            LoadSheet(demonClips[i],demon+demonNames[i],100,demonCounts[i],{50,60},2.0f);
        }
    }
    void Unload()
    {
        for(auto* clips:{wizardClips,demonClips})
            for(int i=0;i<5;++i) { if(IsTextureValid(clips[i].texture)) UnloadTexture(clips[i].texture); clips[i]={}; }
    }
    bool Draw(const PhysicalEnemy& actor) const
    {
        Clip clip=Clip::Idle;
        const bool dying=actor.body.state==State::Dying;
        if(dying) clip=Clip::Death;
        else if(actor.hurt>0) clip=Clip::Hurt;
        else if(actor.body.state==State::WindingUp || actor.attackVisual>0) clip=Clip::Attack;
        else if(actor.body.traveled>0) clip=Clip::Move;
        const AnimationSheet& sheet=Sheets(actor.body.kind)[static_cast<int>(clip)];
        if(!IsTextureValid(sheet.texture)) return false;
        int frame=0;
        if(clip==Clip::Death) frame=static_cast<int>((1-actor.timer/DeathDuration)*sheet.frames);
        else if(clip==Clip::Hurt) frame=static_cast<int>((1-actor.hurt/HitReaction)*sheet.frames);
        else if(clip==Clip::Attack)
        {
            // Damage remains at the end of the original telegraph. The same
            // update displays impact frame 4, followed by recovery frames.
            if(actor.body.state==State::WindingUp)
                frame=std::min(PhysicalAttackImpactFrame-1,static_cast<int>((1-actor.timer/Stats(actor.body.kind).windup)*PhysicalAttackImpactFrame));
            else frame=PhysicalAttackImpactFrame+static_cast<int>((1-actor.attackVisual/AttackFollowThrough(actor.body.kind))*(sheet.frames-PhysicalAttackImpactFrame));
        }
        else frame=static_cast<int>(actor.animationTime*(clip==Clip::Move?10:8))%sheet.frames;
        Color tint=actor.hurt>0?Color{255,174,165,255}:WHITE;
        if(dying) tint.a=static_cast<unsigned char>(255*std::clamp(actor.timer/(DeathDuration*0.25f),0.0f,1.0f));
        DrawSheet(sheet,std::clamp(frame,0,sheet.frames-1),actor.body.position,actor.mirrored,sheet.scale,tint);
        return true;
    }
    bool Portrait(Kind kind,Vector2 feet,float height,float time,bool mirrored,Color tint) const
    {
        const AnimationSheet& sheet=Sheets(kind)[0];
        if(!IsTextureValid(sheet.texture)) return false;
        DrawSheet(sheet,static_cast<int>(time*8)%sheet.frames,feet,mirrored,height/(kind==Kind::Sentinel?104.0f:22.0f),tint);
        return true;
    }
private:
    const AnimationSheet* Sheets(Kind kind) const { return kind==Kind::Sentinel?wizardClips:demonClips; }
    static void LoadSheet(AnimationSheet& sheet,const std::string& path,int size,int count,Vector2 anchor,float scale)
    {
        if(!FileExists(path.c_str())) { TraceLog(LOG_WARNING,"Optional enemy animation missing: %s",path.c_str()); return; }
        Texture2D texture=LoadTexture(path.c_str());
        if(!IsTextureValid(texture)) return;
        if(texture.width!=size*count || texture.height!=size)
        { TraceLog(LOG_WARNING,"Unexpected enemy sheet dimensions: %s",path.c_str()); UnloadTexture(texture); return; }
        SetTextureFilter(texture,TEXTURE_FILTER_POINT);
        sheet={texture,size,count,anchor,scale};
    }
    static void DrawSheet(const AnimationSheet& sheet,int frame,Vector2 feet,bool mirrored,float scale,Color tint)
    {
        const float size=static_cast<float>(sheet.frameSize);
        DrawTexturePro(sheet.texture,{frame*size,0,mirrored?-size:size,size},
                       {feet.x,feet.y,size*scale,size*scale},
                       {(mirrored?size-sheet.anchor.x:sheet.anchor.x)*scale,sheet.anchor.y*scale},0,tint);
    }
    AnimationSheet wizardClips[5]{},demonClips[5]{};
};
}
#endif
