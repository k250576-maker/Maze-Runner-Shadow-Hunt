#include "../include/audio_preferences.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main()
{
    using namespace AudioPreferences;
    const std::string path="build/audio-preferences-test.cfg";
    Settings defaults=Load("build/nonexistent-settings-test.cfg");
    assert(defaults.master==0.8f&&defaults.music==0.25f&&defaults.sfx==0.3f);
    Settings value{0.4f,0.7f,0.2f,true,false};assert(Save(path,value));
    const auto restored=Load(path);
    assert(restored.master==0.4f&&restored.music==0.7f&&restored.sfx==0.2f&&restored.muteMusic&&!restored.muteSfx);
    {std::ofstream bad(path);bad<<"master=8\nmusic=-4\nsfx=nan\nmute_music=bad\nmute_sfx=1\nunknown=2\n";}
    const auto clamped=Load(path);
    assert(clamped.master==1&&clamped.music==0&&clamped.sfx==0.3f&&!clamped.muteMusic&&clamped.muteSfx);
    {std::ofstream bad(path);bad<<"master=oops\nmusic=0.5garbage\nsfx=inf\n";}
    const auto invalid=Load(path);assert(invalid.master==0.8f&&invalid.music==0.25f&&invalid.sfx==0.3f);
    value.master=0;value.music=0;value.sfx=0;assert(Save(path,value));
    const auto silent=Load(path);assert(silent.master==0&&silent.MusicGain()==0&&silent.SfxGain()==0);
    assert(!Save("build/no-settings-directory/preferences.cfg",value));
    std::puts("Audio preference defaults, round-trip, corruption, clamp, mute, zero and write-failure tests passed.");
}
