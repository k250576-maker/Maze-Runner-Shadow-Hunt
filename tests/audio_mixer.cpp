#include "raylib.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
struct Gain {void* id=nullptr;float value=0;};
std::array<Gain,64> sounds{},music{};int soundCount=0,musicCount=0,loads=0;float master=-1;
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
Sound SoundFile(const char* path){++loads;return LoadSound(path);}
Sound SoundWave(Wave wave){++loads;return LoadSoundFromWave(wave);}
Music MusicFile(const char* path){++loads;return LoadMusicStream(path);}
float FrameTime(){return 1.0f/60;}
#define SetSoundVolume SoundGain
#define SetMusicVolume MusicGain
#define SetMasterVolume MasterGain
#define LoadSound SoundFile
#define LoadSoundFromWave SoundWave
#define LoadMusicStream MusicFile
#define GetFrameTime FrameTime
#include "../include/game_audio.h"
#undef SetSoundVolume
#undef SetMusicVolume
#undef SetMasterVolume
#undef LoadSound
#undef LoadSoundFromWave
#undef LoadMusicStream
#undef GetFrameTime
bool Has(const std::array<Gain,64>& values,int count,float expected)
{for(int i=0;i<count;++i)if(std::fabs(values[i].value-expected)<0.00001f)return true;return false;}
bool AllZero(const std::array<Gain,64>& values,int count)
{for(int i=0;i<count;++i)if(values[i].value!=0)return false;return true;}
int main()
{
    GameAudio::System audio;AudioPreferences::Settings settings;
    audio.ApplySettings(settings);audio.Initialize("assets/",GameAudio::DefaultSoundtrack);
    assert(IsAudioDeviceReady());audio.PlayVictory();audio.CollectGold();audio.OpenChest();audio.Damage();
    const int initialLoads=loads;
    settings.master=0.4f;audio.ApplySettings(settings);
    assert(master==0.4f && std::fabs(GetMasterVolume()-0.4f)<0.00001f);
    assert(Has(sounds,soundCount,GameAudio::FootstepVolume*0.3f));
    assert(Has(sounds,soundCount,GameAudio::GoldPickupVolume*0.3f));
    assert(Has(sounds,soundCount,GameAudio::ChestOpeningVolume*0.3f));
    assert(Has(sounds,soundCount,GameAudio::PlayerDamageVolume*0.3f));
    assert(Has(sounds,soundCount,0.3f*0.3f)); // procedural victory chime
    assert(Has(music,musicCount,GameAudio::MenuMusicVolume*0.25f));
    settings.muteSfx=true;audio.ApplySettings(settings);
    assert(AllZero(sounds,soundCount)&&!AllZero(music,musicCount));
    settings.muteSfx=false;settings.muteMusic=true;audio.ApplySettings(settings);
    assert(!AllZero(sounds,soundCount)&&AllZero(music,musicCount));
    settings.master=0;settings.muteMusic=false;audio.ApplySettings(settings);assert(GetMasterVolume()==0);
    audio.TransitionToGame(42);
    for(int i=0;i<180;++i)audio.Update(0,true);
    assert(master==0 && Has(music,musicCount,GameAudio::GameplayMusicVolume*0.25f));
    audio.SetPaused(true);audio.ApplySettings(settings);
    assert(Has(music,musicCount,GameAudio::GameplayMusicVolume*0.25f*0.3f));
    audio.SetPaused(false);audio.TransitionToMenu();for(int i=0;i<180;++i)audio.Update(0);
    assert(Has(music,musicCount,GameAudio::MenuMusicVolume*0.25f));
    settings.music=settings.sfx=0;settings.master=0.8f;audio.ApplySettings(settings);
    audio.CollectSoul();audio.Damage();audio.CollectGold();audio.OpenChest();audio.PlayVictory();
    GameAudio::EnemyAudio cues;cues.Action(GameAudio::Cue::Sword,0);audio.Update(80,false,cues);
    assert(AllZero(sounds,soundCount)&&AllZero(music,musicCount));
    assert(loads==initialLoads);audio.Shutdown();
    std::puts("Mixer: immediate music/SFX refresh, independent mutes, exact zero, single master gain, procedural/recorded sounds, transitions, pause attenuation and no reloads passed.");
}
