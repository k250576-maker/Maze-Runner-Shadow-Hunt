#ifndef SHADOW_HUNT_GAME_AUDIO_H
#define SHADOW_HUNT_GAME_AUDIO_H

#include "raylib.h"
#include <cmath>
#include <string>
#include <string_view>

namespace GameAudio
{
enum class Soundtrack { Dungeon002, DarkCavern, DeepDungeon, Off };
constexpr Soundtrack DefaultSoundtrack = Soundtrack::Dungeon002;
constexpr float MasterVolume = 0.80f;
constexpr float MusicVolume = 0.25f;
constexpr float SfxVolume = 0.30f;
constexpr float FootstepMixGain = 0.50f;
constexpr float StepDistance = 38.0f;
constexpr float FirstStepDistance = 6.0f;
constexpr float StepClipStartSeconds = 0.50f;
constexpr float StepClipEndSeconds = 0.74f;

inline bool ParseSoundtrack(std::string_view name, Soundtrack& track)
{
    if (name == "dungeon002") track = Soundtrack::Dungeon002;
    else if (name == "dark-cavern") track = Soundtrack::DarkCavern;
    else if (name == "deep-dungeon") track = Soundtrack::DeepDungeon;
    else if (name == "off") track = Soundtrack::Off;
    else return false;
    return true;
}

class System
{
public:
    void Initialize(const std::string& assetRoot, Soundtrack selection)
    {
        InitAudioDevice();
        deviceReady = IsAudioDeviceReady();
        if (!deviceReady)
        {
            TraceLog(LOG_WARNING, "Audio device unavailable. Continuing with game audio muted.");
            return;
        }
        SetMasterVolume(MasterVolume);
        LoadSoundtrack(assetRoot + "audio/music/", selection);

        const std::string stepPath = assetRoot + "audio/sfx/footsteps-use-it-again.mp3";
        Wave wave = FileExists(stepPath.c_str()) ? LoadWave(stepPath.c_str()) : Wave{};
        const bool suppliedRecording = IsWaveValid(wave);
        if (!suppliedRecording)
        {
            const std::string bundledStep = assetRoot + "audio/sfx/stone_step.wav";
            if (!FileExists(bundledStep.c_str())) return;
            wave = LoadWave(bundledStep.c_str());
        }
        if (!IsWaveValid(wave)) return;
        if (suppliedRecording)
        {
            // The local six-second recording has repeated impacts. Keep only the first one.
            const int first = static_cast<int>(wave.sampleRate * StepClipStartSeconds);
            const int last = static_cast<int>(wave.sampleRate * StepClipEndSeconds);
            if (first < last && static_cast<unsigned>(last) <= wave.frameCount)
                WaveCrop(&wave, first, last);
        }
        footstep = LoadSoundFromWave(wave);
        UnloadWave(wave);
        if (IsSoundValid(footstep)) SetSoundVolume(footstep, SfxVolume * FootstepMixGain);
    }

    void Update(float pixelsMoved)
    {
        if (!deviceReady) return;
        if (IsMusicValid(music)) UpdateMusicStream(music);
        if (!IsSoundValid(footstep)) return;

        if (pixelsMoved <= 0.0f)
        {
            if (IsSoundPlaying(footstep)) StopSound(footstep);
            stepProgress = StepDistance - FirstStepDistance;
            return;
        }
        stepProgress += pixelsMoved;
        if (stepProgress >= StepDistance && !IsSoundPlaying(footstep))
        {
            PlaySound(footstep);
            stepProgress = std::fmod(stepProgress, StepDistance);
        }
    }

    void Shutdown()
    {
        if (!deviceReady) return;
        if (IsSoundValid(footstep)) UnloadSound(footstep);
        if (IsMusicValid(music)) UnloadMusicStream(music);
        CloseAudioDevice();
        footstep = {};
        music = {};
        deviceReady = false;
    }

private:
    bool StartMusic(const std::string& path)
    {
        if (!FileExists(path.c_str())) return false;
        music = LoadMusicStream(path.c_str());
        if (!IsMusicValid(music)) return false;
        music.looping = true;
        SetMusicVolume(music, MusicVolume);
        PlayMusicStream(music);
        return true;
    }

    void LoadSoundtrack(const std::string& directory, Soundtrack selection)
    {
        if (selection == Soundtrack::Off) return;
        if (selection == Soundtrack::DeepDungeon
            && (StartMusic(directory + "deep_dungeon.ogg") || StartMusic(directory + "deep_dungeon.mp3"))) return;
        const bool cavernFirst = selection == Soundtrack::DarkCavern;
        const char* first = cavernFirst ? "dark_cavern_ambient_001.ogg" : "dungeon002.ogg";
        const char* second = cavernFirst ? "dungeon002.ogg" : "dark_cavern_ambient_001.ogg";
        if (!StartMusic(directory + first) && !StartMusic(directory + second))
            TraceLog(LOG_WARNING, "No usable dungeon soundtrack was found.");
    }

    bool deviceReady = false;
    Music music{};
    Sound footstep{};
    float stepProgress = StepDistance - FirstStepDistance;
};
}
#endif
