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
constexpr float FadeDuration = 0.60f;

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
    enum class Phase { Menu, FadeToGame, Playing, FadeToMenu };

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
        const std::string dir = assetRoot + "audio/music/";
        // Menu music: always Deep Dungeon when available, looped from the start.
        LoadTrack(menuMusic, dir + "deep_dungeon.ogg") || LoadTrack(menuMusic, dir + "deep_dungeon.mp3");
        // Gameplay music: controlled by --music flag; preloaded but not yet playing.
        LoadGameSoundtrack(dir, selection);
        // Begin playing menu music immediately.
        if (IsMusicValid(menuMusic)) PlayMusicStream(menuMusic);
        phase = Phase::Menu;
        fadeTimer = 0.0f;

        // Footstep sound (unchanged from original).
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
            const int first = static_cast<int>(wave.sampleRate * StepClipStartSeconds);
            const int last  = static_cast<int>(wave.sampleRate * StepClipEndSeconds);
            if (first < last && static_cast<unsigned>(last) <= wave.frameCount)
                WaveCrop(&wave, first, last);
        }
        footstep = LoadSoundFromWave(wave);
        UnloadWave(wave);
        if (IsSoundValid(footstep)) SetSoundVolume(footstep, SfxVolume * FootstepMixGain);
    }

    // Called when Start Game is chosen: fades menu music out, then starts gameplay music.
    void TransitionToGame()
    {
        if (!deviceReady || phase == Phase::Playing || phase == Phase::FadeToGame) return;
        phase = Phase::FadeToGame;
        fadeTimer = 0.0f;
    }

    // Called when returning to the menu: fades gameplay music out, then resumes menu music.
    void TransitionToMenu()
    {
        if (!deviceReady || phase == Phase::Menu || phase == Phase::FadeToMenu) return;
        phase = Phase::FadeToMenu;
        fadeTimer = 0.0f;
    }

    bool IsInMenuPhase() const { return phase == Phase::Menu || phase == Phase::FadeToMenu; }

    void Update(float pixelsMoved)
    {
        if (!deviceReady) return;
        const float dt = GetFrameTime();
        UpdateFade(dt);
        if (IsMusicValid(menuMusic)) UpdateMusicStream(menuMusic);
        if (IsMusicValid(gameMusic)) UpdateMusicStream(gameMusic);

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
        if (IsSoundValid(footstep))   UnloadSound(footstep);
        if (IsMusicValid(menuMusic))  UnloadMusicStream(menuMusic);
        if (IsMusicValid(gameMusic))  UnloadMusicStream(gameMusic);
        CloseAudioDevice();
        footstep  = {};
        menuMusic = {};
        gameMusic = {};
        deviceReady = false;
    }

private:
    // Loads a track and sets looping + volume, but does NOT start playback.
    bool LoadTrack(Music& target, const std::string& path)
    {
        if (!FileExists(path.c_str())) return false;
        target = LoadMusicStream(path.c_str());
        if (!IsMusicValid(target)) { target = {}; return false; }
        target.looping = true;
        SetMusicVolume(target, MusicVolume);
        return true;
    }

    void LoadGameSoundtrack(const std::string& directory, Soundtrack selection)
    {
        if (selection == Soundtrack::Off) return;
        if (selection == Soundtrack::DeepDungeon
            && (LoadTrack(gameMusic, directory + "deep_dungeon.ogg")
                || LoadTrack(gameMusic, directory + "deep_dungeon.mp3"))) return;
        const bool cavernFirst = (selection == Soundtrack::DarkCavern);
        const char* first  = cavernFirst ? "dark_cavern_ambient_001.ogg" : "dungeon002.ogg";
        const char* second = cavernFirst ? "dungeon002.ogg" : "dark_cavern_ambient_001.ogg";
        if (!LoadTrack(gameMusic, directory + first) && !LoadTrack(gameMusic, directory + second))
            TraceLog(LOG_WARNING, "No usable dungeon soundtrack was found.");
    }

    void UpdateFade(float dt)
    {
        if (phase == Phase::Menu || phase == Phase::Playing) return;
        fadeTimer += dt;
        const float t = std::min(fadeTimer / FadeDuration, 1.0f);

        if (phase == Phase::FadeToGame)
        {
            if (IsMusicValid(menuMusic)) SetMusicVolume(menuMusic, MusicVolume * (1.0f - t));
            if (t >= 1.0f)
            {
                if (IsMusicValid(menuMusic))
                {
                    StopMusicStream(menuMusic);
                    SetMusicVolume(menuMusic, MusicVolume);
                }
                if (IsMusicValid(gameMusic)) PlayMusicStream(gameMusic);
                phase = Phase::Playing;
            }
        }
        else // FadeToMenu
        {
            if (IsMusicValid(gameMusic)) SetMusicVolume(gameMusic, MusicVolume * (1.0f - t));
            if (t >= 1.0f)
            {
                if (IsMusicValid(gameMusic))
                {
                    StopMusicStream(gameMusic);
                    SetMusicVolume(gameMusic, MusicVolume);
                }
                if (IsMusicValid(menuMusic))
                {
                    SeekMusicStream(menuMusic, 0);
                    PlayMusicStream(menuMusic);
                }
                phase = Phase::Menu;
            }
        }
    }

    bool  deviceReady = false;
    Music menuMusic{};
    Music gameMusic{};
    Sound footstep{};
    Phase phase     = Phase::Menu;
    float fadeTimer = 0.0f;
    float stepProgress = StepDistance - FirstStepDistance;
};
}
#endif
