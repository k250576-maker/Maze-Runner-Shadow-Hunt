#ifndef SHADOW_HUNT_AUDIO_PREFERENCES_H
#define SHADOW_HUNT_AUDIO_PREFERENCES_H
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <string>

namespace AudioPreferences
{
struct Settings
{
    float master=0.80f,music=0.25f,sfx=0.30f;
    bool muteMusic=false,muteSfx=false;
    void Validate()
    {
        const auto clamp=[](float value,float fallback){return std::isfinite(value)?std::clamp(value,0.0f,1.0f):fallback;};
        master=clamp(master,0.80f);music=clamp(music,0.25f);sfx=clamp(sfx,0.30f);
    }
    float MusicGain()const{return muteMusic?0:music;}
    float SfxGain()const{return muteSfx?0:sfx;}
};
inline Settings Load(const std::string& path)
{
    Settings result;std::ifstream file(path);std::string line;
    for(int count=0;count<32&&std::getline(file,line);++count)
    {
        if(line.size()>256)continue;
        const auto equals=line.find('=');if(equals==std::string::npos)continue;
        const std::string key=line.substr(0,equals),text=line.substr(equals+1);
        try
        {
            std::size_t used=0;float value=std::stof(text,&used);
            if(text.find_first_not_of(" \t\r",used)!=std::string::npos)continue;
            if(key=="master")result.master=value;
            else if(key=="music")result.music=value;
            else if(key=="sfx")result.sfx=value;
            else if(key=="mute_music" && (value==0||value==1))result.muteMusic=value==1;
            else if(key=="mute_sfx" && (value==0||value==1))result.muteSfx=value==1;
        }catch(...){/* Invalid entries keep the defaults; audio must still work. */}
    }
    result.Validate();return result;
}
inline bool Save(const std::string& path,Settings settings)
{
    settings.Validate();std::ofstream file(path,std::ios::trunc);
    if(!file)return false;
    file<<std::setprecision(6)<<"master="<<settings.master<<"\nmusic="<<settings.music<<"\nsfx="<<settings.sfx
        <<"\nmute_music="<<settings.muteMusic<<"\nmute_sfx="<<settings.muteSfx<<'\n';
    file.flush();return static_cast<bool>(file);
}
}
#endif
