#include "raylib.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string_view>
struct Gain {void* id=nullptr;float value=0;};
std::array<Gain,64> sounds{},music{};int soundCount=0,musicCount=0,loads=0;float master=-1;
Sound wizard{},defeat{};
Music victory{},exploration{},menu{};
int wizardPlays=0,defeatPlays=0,victoryPlays=0,effectPlays=0;
void Record(std::array<Gain,64>& rows,int& count,void* id,float value)
{
    assert(value>=0&&value<=1);
    int i=0;while(i<count&&rows[i].id!=id)++i;
    if(i==count)++count;
    assert(count<=64);rows[i]={id,value};
}
void SoundGain(Sound s,float value){Record(sounds,soundCount,s.stream.buffer,value);SetSoundVolume(s,value);}
void MusicGain(Music m,float value){Record(music,musicCount,m.stream.buffer,value);SetMusicVolume(m,value);}
void MasterGain(float value){master=value;SetMasterVolume(value);}
Sound SoundFile(const char* path)
{
    ++loads;
    Sound sound=LoadSound(path);
    const std::string_view name(path);
    if(name.find("wizard-2.mp3")!=name.npos) wizard=sound;
    if(name.find("Game_Over_3.wav")!=name.npos) defeat=sound;
    return sound;
}
Sound SoundWave(Wave wave){++loads;return LoadSoundFromWave(wave);}
Music MusicFile(const char* path)
{
    ++loads;
    Music track=LoadMusicStream(path);
    const std::string_view name(path);
    if(name.find("victory.wav")!=name.npos) victory=track;
    if(name.find("deep_dungeon")!=name.npos || (!IsMusicValid(menu) && name.find("dungeon002")!=name.npos)) menu=track;
    else if(name.find("dungeon002")!=name.npos) exploration=track;
    return track;
}
void PlayEffect(Sound sound)
{
    ++effectPlays;
    if(sound.stream.buffer==wizard.stream.buffer) ++wizardPlays;
    if(sound.stream.buffer==defeat.stream.buffer) ++defeatPlays;
    PlaySound(sound);
}
void PlayTrack(Music track)
{
    if(track.stream.buffer==victory.stream.buffer)
    {
        assert(!track.looping);
        ++victoryPlays;
    }
    PlayMusicStream(track);
}
float FrameTime(){return 1.0f/60;}
#define SetSoundVolume SoundGain
#define SetMusicVolume MusicGain
#define SetMasterVolume MasterGain
#define LoadSound SoundFile
#define LoadSoundFromWave SoundWave
#define LoadMusicStream MusicFile
#define PlaySound PlayEffect
#define PlayMusicStream PlayTrack
#define GetFrameTime FrameTime
#include "../include/game_audio.h"
#undef SetSoundVolume
#undef SetMusicVolume
#undef SetMasterVolume
#undef LoadSound
#undef LoadSoundFromWave
#undef LoadMusicStream
#undef PlaySound
#undef PlayMusicStream
#undef GetFrameTime
bool Has(const std::array<Gain,64>& values,int count,float expected)
{for(int i=0;i<count;++i)if(std::fabs(values[i].value-expected)<0.00001f)return true;return false;}
bool AllZero(const std::array<Gain,64>& values,int count)
{for(int i=0;i<count;++i)if(values[i].value!=0)return false;return true;}
bool HasGain(const std::array<Gain,64>& values,int count,void* id,float expected)
{for(int i=0;i<count;++i)if(values[i].id==id)return std::fabs(values[i].value-expected)<0.00001f;return false;}
int main()
{
    GameAudio::System audio;AudioPreferences::Settings settings;
    audio.ApplySettings(settings);audio.Initialize("assets/",GameAudio::DefaultSoundtrack);
    assert(IsAudioDeviceReady());audio.CollectGold();audio.OpenChest();audio.Damage();
    assert(IsMusicValid(victory));
    const int initialLoads=loads;
    settings.master=0.4f;audio.ApplySettings(settings);
    assert(master==0.4f && std::fabs(GetMasterVolume()-0.4f)<0.00001f);
    assert(Has(sounds,soundCount,GameAudio::FootstepVolume*0.3f));
    assert(Has(sounds,soundCount,GameAudio::GoldPickupVolume*0.3f));
    assert(Has(sounds,soundCount,GameAudio::ChestOpeningVolume*0.3f));
    assert(Has(sounds,soundCount,GameAudio::PlayerDamageVolume*0.3f));
    assert(HasGain(music,musicCount,victory.stream.buffer,GameAudio::VictoryVolume*0.25f));
    if(IsSoundValid(defeat))
        assert(HasGain(sounds,soundCount,defeat.stream.buffer,GameAudio::DefeatVolume*0.3f));
    assert(Has(music,musicCount,GameAudio::MenuMusicVolume*0.25f));
    settings.muteSfx=true;audio.ApplySettings(settings);
    assert(AllZero(sounds,soundCount)&&!AllZero(music,musicCount));
    settings.muteSfx=false;settings.muteMusic=true;audio.ApplySettings(settings);
    assert(!AllZero(sounds,soundCount)&&AllZero(music,musicCount));
    settings.master=0;settings.muteMusic=false;audio.ApplySettings(settings);assert(GetMasterVolume()==0);
    audio.TransitionToGame(42);
    for(int i=0;i<180;++i)audio.Update(0,true);
    assert(master==0 && Has(music,musicCount,GameAudio::GameplayMusicVolume*0.25f));
    audio.PlayGadget(GameAudio::Cue::GadgetPickup);
    assert(Has(sounds,soundCount,0.21f*0.3f));
    GameAudio::EnemyAudio cast;cast.wizardDistance=55;
    audio.Update(0,false,cast);audio.Update(0,false,cast);
    if(IsSoundValid(wizard))
    {
        assert(wizardPlays==1 && IsSoundPlaying(wizard));
        assert(HasGain(sounds,soundCount,wizard.stream.buffer,GameAudio::WizardCastVolume*0.75f*0.75f*0.3f));
    }
    audio.Damage();
    const int duringDamage=effectPlays;
    GameAudio::EnemyAudio hit;hit.Action(GameAudio::Cue::EnemyHit,20);
    audio.Update(0,false,hit);
    assert(effectPlays==duringDamage+1);
    audio.SetPaused(true);audio.ApplySettings(settings);
    assert(Has(music,musicCount,GameAudio::GameplayMusicVolume*0.25f*0.3f));
    if(IsSoundValid(wizard)) assert(!IsSoundPlaying(wizard));
    audio.SetPaused(false);audio.PlayDefeat();audio.PlayDefeat();audio.Update(0);
    if(IsSoundValid(defeat)) assert(defeatPlays==1 && IsSoundPlaying(defeat));
    if(IsMusicValid(exploration)) assert(!IsMusicStreamPlaying(exploration));
    audio.SetPaused(true);
    if(IsSoundValid(defeat)) assert(IsSoundPlaying(defeat));
    audio.SetPaused(false);audio.TransitionToMenu();for(int i=0;i<180;++i)audio.Update(0);
    if(IsSoundValid(defeat)) assert(!IsSoundPlaying(defeat));
    assert(Has(music,musicCount,GameAudio::MenuMusicVolume*0.25f));
    audio.PlayVictory();audio.PlayVictory();audio.Update(0);
    assert(victoryPlays==1 && IsMusicStreamPlaying(victory));
    if(IsMusicValid(menu)) assert(!IsMusicStreamPlaying(menu));
    settings.muteMusic=true;audio.ApplySettings(settings);
    assert(HasGain(music,musicCount,victory.stream.buffer,0));
    audio.TransitionToGame(43);
    assert(!IsMusicStreamPlaying(victory));
    settings.muteMusic=false;
    settings.music=settings.sfx=0;settings.master=0.8f;audio.ApplySettings(settings);
    audio.CollectSoul();audio.Damage();audio.CollectGold();audio.OpenChest();audio.PlayVictory();
    GameAudio::EnemyAudio cues;cues.Action(GameAudio::Cue::Sword,0);audio.Update(80,false,cues);
    assert(AllZero(sounds,soundCount)&&AllZero(music,musicCount));
    assert(loads==initialLoads);audio.Shutdown();
    GameAudio::System missing;
    missing.Initialize("build/missing-polish-assets/",GameAudio::Soundtrack::Off);
    missing.TransitionToGame(42);missing.Update(0,false,cast);
    missing.PlayDefeat();missing.TransitionToGame(42);missing.PlayVictory();
    missing.Update(0);missing.TransitionToMenu();missing.Update(0);missing.Shutdown();
    std::puts("Mixer: category gains, mutes, Wizard cast attenuation, non-looping one-shot endings, pause, transitions and no reloads passed.");
}
