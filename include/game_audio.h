#ifndef SHADOW_HUNT_GAME_AUDIO_H
#define SHADOW_HUNT_GAME_AUDIO_H

#include "raylib.h"
#include "enemy.h"
#include "audio_preferences.h"
#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace GameAudio
{
enum class Soundtrack { Dungeon002, DarkCavern, Off };
constexpr Soundtrack DefaultSoundtrack = Soundtrack::Dungeon002;
constexpr float MasterVolume = 0.80f;
constexpr float MenuMusicVolume = 0.28f;
constexpr float GameplayMusicVolume = 0.09f;
constexpr float MelodyVolume = 0.065f;
constexpr float MelodyFade = 2.0f;
constexpr float MelodyResumeQuiet = 2.0f;
constexpr float MelodyDangerFade = 0.65f;
constexpr float MelodySilenceMin = 120.0f;
constexpr float MelodySilenceMax = 240.0f;
constexpr float EnemyMotionVolume = 0.17f;
constexpr float EnemyAttackVolume = 0.26f;
constexpr float EnemyHitVolume = 0.28f;
constexpr float PlayerDamageVolume = 0.34f;
constexpr float EnemySoundRange = 220.0f;
constexpr float FootstepVolume = 0.075f;
constexpr float GoldPickupVolume = 0.22f;
constexpr float ChestOpeningVolume = 0.20f;
constexpr float StepDistance = 78.0f;
constexpr float MinimumWalkDistance = 28.0f;
constexpr float StepInterval = 0.52f;
constexpr float StepResetAfter = 0.24f;
constexpr float StepAttackSeconds = 0.012f;
constexpr float StepReleaseSeconds = 0.035f;
constexpr float FadeDuration = 0.60f;

inline void ApplyFootstepEnvelope(Wave& wave)
{
    WaveFormat(&wave, static_cast<int>(wave.sampleRate), 32, static_cast<int>(wave.channels));
    if (!IsWaveValid(wave) || wave.sampleSize != 32) return;
    const float attack = std::max(1.0f, wave.sampleRate * StepAttackSeconds);
    const float release = std::max(1.0f, wave.sampleRate * StepReleaseSeconds);
    auto* samples = static_cast<float*>(wave.data);
    for (unsigned frame = 0; frame < wave.frameCount; ++frame)
    {
        const float gain = std::min({1.0f, frame / attack, (wave.frameCount - 1 - frame) / release});
        for (unsigned channel = 0; channel < wave.channels; ++channel)
            samples[frame * wave.channels + channel] *= gain;
    }
}

inline bool ParseSoundtrack(std::string_view name, Soundtrack& track)
{
    if (name == "dungeon002") track = Soundtrack::Dungeon002;
    else if (name == "dark-cavern") track = Soundtrack::DarkCavern;
    else if (name == "deep-dungeon") track = Soundtrack::Dungeon002;
    else if (name == "off") track = Soundtrack::Off;
    else return false;
    return true;
}

enum class Cue { Stone, Whisper, Armor, Rasp, Sword, Lunge, EnemyHit, EnemyDeath, Soul, GhostStrike, Count };
struct EnemyAudio
{
    Cue motion = Cue::Count;
    Cue action = Cue::Count;
    float motionDistance = EnemySoundRange;
    float actionDistance = EnemySoundRange;
    bool urgent = false;

    void Movement(Enemies::Kind kind, float distance)
    {
        if (distance >= motionDistance) return;
        switch (kind)
        {
            case Enemies::Kind::Angel: motion = Cue::Stone; break;
            case Enemies::Kind::Ghost: motion = Cue::Whisper; break;
            case Enemies::Kind::Sentinel: motion = Cue::Armor; break;
            case Enemies::Kind::Stalker: motion = Cue::Rasp; break;
        }
        motionDistance = distance;
    }

    void Action(Cue cue, float distance)
    {
        const bool important = cue == Cue::EnemyDeath || cue == Cue::EnemyHit;
        if (distance >= EnemySoundRange || (urgent && !important)
            || (important == urgent && distance >= actionDistance)) return;
        action = cue;
        actionDistance = distance;
        urgent = important;
    }
};

inline Sound Synthesize(Cue cue)
{
    struct Profile { float seconds, from, to, grit, ring; };
    constexpr Profile profiles[] = {
        {0.24f, 83, 44, 0.73f, 1.32f}, {0.31f, 420, 170, 0.62f, 1.21f},
        {0.23f, 115, 65, 0.41f, 5.31f}, {0.19f, 173, 75, 0.69f, 1.4f},
        {0.22f, 710, 210, 0.61f, 2.42f}, {0.16f, 310, 92, 0.66f, 1.7f},
        {0.105f, 210, 93, 0.48f, 3.34f}, {0.31f, 151, 41, 0.48f, 2.11f},
        {0.28f, 475, 835, 0.03f, 1.5f}, {0.29f, 530, 75, 0.58f, 1.87f}
    };
    const Profile p = profiles[static_cast<unsigned>(cue)];
    constexpr unsigned rate = 22050;
    std::array<short, rate / 2> samples{};
    const unsigned count = static_cast<unsigned>(rate * p.seconds);
    std::uint32_t random = 27;
    float phase = 0, noise = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        const float t = float(i) / count;
        random = random * 1664525u + 1013904223u;
        const float sample = (random >> 16) / 32768.0f - 1;
        noise += (sample - noise) * (cue == Cue::Whisper ? 0.34f : 0.17f);
        phase += (p.from + (p.to - p.from) * t) * 6.2831853f / rate;
        const float envelope = std::min(1.0f, t * (cue == Cue::Whisper ? 5 : 28)) * (1-t) * (1-t);
        const float voice = 0.54f * std::sin(phase) + 0.36f * std::sin(phase * p.ring);
        samples[i] = static_cast<short>(24500 * envelope * ((1-p.grit) * voice + p.grit * noise));
    }
    return LoadSoundFromWave({count, rate, 16, 1, samples.data()});
}

class System
{
    enum class Scene { Menu, Gameplay };

public:
    void ApplySettings(AudioPreferences::Settings settings)
    {
        settings.Validate();preferences=settings;
        if(!deviceReady)return;
        // Master is applied ONLY by the device; category/individual gains below
        // never multiply it again.
        SetMasterVolume(preferences.master);
        for(int i=0;i<voiceCount;++i)
            SetSoundVolume(voices[i].sound,preferences.SfxGain()*voices[i].individual);
        if(IsMusicValid(menuMusic))SetMusicGain(menuMusic,activeScene==Scene::Menu?MenuMusicVolume*musicGain*(paused?0.3f:1):0);
        if(IsMusicValid(gameMusic))SetMusicGain(gameMusic,activeScene==Scene::Gameplay?GameplayMusicVolume*musicGain*(paused?0.3f:1):0);
        for(int i=0;i<static_cast<int>(melodies.size());++i)
            if(IsMusicValid(melodies[i]))SetMusicGain(melodies[i],i==activeMelody?MelodyVolume*melodyGain*(paused?0.3f:1):0);
    }
    void Initialize(const std::string& assetRoot, Soundtrack selection)
    {
        InitAudioDevice();
        deviceReady = IsAudioDeviceReady();
        if (!deviceReady)
        {
            TraceLog(LOG_WARNING, "Audio device unavailable. Continuing with game audio muted.");
            return;
        }
        SetMasterVolume(preferences.master);
        LoadLootSound(chest,assetRoot+"audio/sfx/minecraft-chest-open-and-close.mp3",ChestOpeningVolume);
        LoadLootSound(goldPickup,assetRoot+"audio/sfx/diablo-2-gold.mp3",GoldPickupVolume);
        const std::string dir = assetRoot + "audio/music/";
        // Menu music: always Deep Dungeon when available, looped from the start.
        if (!LoadLocalTrack(menuMusic, dir, "deep_dungeon"))
        {
            TraceLog(LOG_WARNING, "Optional track missing or unreadable: Deep Dungeon. Using menu ambience.");
            LoadGameSoundtrack(menuMusic, dir, Soundtrack::Dungeon002);
        }
        // Gameplay music: controlled by --music flag; preloaded but not yet playing.
        LoadGameSoundtrack(gameMusic, dir, selection);
        if (selection != Soundtrack::Off)
        {
            constexpr const char* filenames[] = {"rich_house", "elmshore"};
            constexpr const char* titles[] = {"Rich House", "Elmshore"};
            for (std::size_t i = 0; i < melodies.size(); ++i)
            {
                if (LoadLocalTrack(melodies[i], dir, filenames[i])) melodies[i].looping = false;
                else TraceLog(LOG_WARNING, "Optional exploration track missing or unreadable: %s.", titles[i]);
            }
        }
        for (std::size_t i = 0; i < enemies.size(); ++i)
        {
            enemies[i] = Synthesize(static_cast<Cue>(i));
            SetSfxGain(enemies[i],1.0f);
        }
        activeScene = requestedScene = Scene::Menu;
        musicGain = 1.0f;
        if (IsMusicValid(menuMusic))
        {
            SetMusicGain(menuMusic, MenuMusicVolume);
            PlayMusicStream(menuMusic);
        }

        // Original short descending pulse with a decaying, deterministic noise transient.
        std::array<short, 3528> samples{};
        unsigned noise = 7;
        float phase = 0;
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const float t = static_cast<float>(i) / samples.size();
            noise = 1664525u * noise + 1013904223u;
            phase += (245 - 162 * t) / 22050.0f;
            const float pulse = std::fmod(phase, 1.0f) < 0.34f ? 0.56f : -0.29f;
            const float grit = (static_cast<float>(noise >> 16) / 32768 - 1) * 0.45f * (1-t);
            const float envelope = std::min(1.0f, t * 60) * (1-t) * (1-t) * (1-t);
            samples[i] = static_cast<short>(22000 * envelope * (pulse + grit));
        }
        Wave impact{static_cast<unsigned>(samples.size()), 22050, 16, 1, samples.data()};
        damage = LoadSoundFromWave(impact);
        if (IsSoundValid(damage)) SetSfxGain(damage, PlayerDamageVolume);

        const std::string local = assetRoot + "audio/sfx/footsteps-use-it-again.mp3";
        footstepPath=local;
        Wave wave = FileExists(local.c_str()) ? LoadWave(local.c_str()) : Wave{};
        if (IsWaveValid(wave))
        {
            const unsigned first = static_cast<unsigned>(wave.sampleRate * 0.50f);
            const unsigned end = static_cast<unsigned>(wave.sampleRate * 0.74f);
            if (end <= wave.frameCount) WaveCrop(&wave, first, end);
            else { UnloadWave(wave); wave = {}; }
        }
        if (!IsWaveValid(wave))
        {
            TraceLog(LOG_WARNING,"Recorded footsteps unavailable: %s. Using bundled stone_step.wav.",local.c_str());
            const std::string fallback = assetRoot + "audio/sfx/stone_step.wav";
            footstepPath=fallback;
            if (FileExists(fallback.c_str())) wave = LoadWave(fallback.c_str());
        }
        if (!IsWaveValid(wave))
        { TraceLog(LOG_WARNING,"No usable footsteps at %s.",footstepPath.c_str()); footstepPath.clear(); return; }
        ApplyFootstepEnvelope(wave);
        footstep = LoadSoundFromWave(wave);
        UnloadWave(wave);
        if (IsSoundValid(footstep))
        {
            SetSfxGain(footstep, FootstepVolume);
            TraceLog(LOG_INFO,"Footstep source: %s (volume %.3f, stride %.0f px)",footstepPath.c_str(),FootstepVolume,StepDistance);
        }
    }

    const std::string& FootstepSource() const { return footstepPath; }

    void CollectSoul()
    {
        if (!deviceReady || DamagePlaying()) return;
        Sound& sound = Clip(Cue::Soul);
        if (!IsSoundValid(sound) || IsSoundPlaying(sound)) return;
        SetSfxGain(sound, 0.22f);
        PlaySound(sound);
    }
    void OpenChest()
    {
        if(!deviceReady || paused || !IsSoundValid(chest) || IsSoundPlaying(chest))return;
        PlaySound(chest);
    }
    void CollectGold()
    {
        if(!deviceReady || paused || !IsSoundValid(goldPickup) || IsSoundPlaying(goldPickup))return;
        PlaySound(goldPickup);
    }

    void Damage(Enemies::Kind source = Enemies::Kind::Angel)
    {
        if (!deviceReady) return;
        Sound impact = source == Enemies::Kind::Ghost ? Clip(Cue::GhostStrike) : damage;
        if (!IsSoundValid(impact)) return;
        StopCue(motion);
        StopCue(action);
        if (IsSoundValid(Clip(Cue::Soul))) StopSound(Clip(Cue::Soul));
        if (IsSoundValid(damage)) StopSound(damage);
        if (IsSoundValid(Clip(Cue::GhostStrike))) StopSound(Clip(Cue::GhostStrike));
        SetSfxGain(impact, PlayerDamageVolume);
        PlaySound(impact);
    }

    void TransitionToGame(std::uint32_t seed = 0)
    {
        if(IsSoundValid(victory)) StopSound(victory);
        if(IsSoundValid(footstep)) StopSound(footstep);
        stepProgress=StepDistance-MinimumWalkDistance;
        timeSinceStep=StepInterval;
        idleTime=0;
        requestedScene = Scene::Gameplay;
        StopMelody();
        nextMelody = static_cast<int>(seed % melodies.size());
        melodyRandom = seed ^ 0x8F15AB33u;
        ScheduleMelody();
    }

    void TransitionToMenu()
    {
        if(IsSoundValid(victory)) StopSound(victory);
        requestedScene = Scene::Menu;
    }

    void SetPaused(bool value)
    {
        if (paused==value || !deviceReady) return;
        paused=value;
        if (paused)
        {
            StopCue(motion); StopCue(action);
            if(IsSoundValid(footstep)) StopSound(footstep);
            if(IsSoundValid(damage)) StopSound(damage);
            if(IsSoundValid(chest)) StopSound(chest);
            if(IsSoundValid(goldPickup)) StopSound(goldPickup);
            for(auto& sound:enemies) if(IsSoundValid(sound)) StopSound(sound);
            stepProgress=StepDistance-MinimumWalkDistance;
            idleTime=0;
        }
        if(IsMusicValid(ActiveMusic())) SetMusicGain(ActiveMusic(),ActiveVolume()*musicGain);
        if(activeMelody>=0) SetMusicGain(melodies[activeMelody],MelodyVolume*melodyGain*(paused?0.3f:1.0f));
    }

    void PlayVictory()
    {
        if(!deviceReady) return;
        if(!IsSoundValid(victory))
        {
            // Original ascending three-note chime; no external recording.
            std::array<short,22050> notes{};
            constexpr float pitches[]={523.25f,659.25f,783.99f};
            for(std::size_t i=0;i<notes.size();++i)
            {
                const float t=float(i)/22050;
                float value=0;
                for(int note=0;note<3;++note)
                {
                    const float age=t-note*0.18f;
                    if(age>=0) value+=std::sin(2*PI*pitches[note]*age)*std::exp(-age*5)*std::min(1.0f,age*90);
                }
                notes[i]=static_cast<short>(3200*value);
            }
            Wave wave{static_cast<unsigned>(notes.size()),22050,16,1,notes.data()};
            victory=LoadSoundFromWave(wave);
            if(IsSoundValid(victory)) SetSfxGain(victory,0.3f);
        }
        if(IsSoundValid(victory)) PlaySound(victory);
    }

    void Update(float pixelsMoved, bool quiet = false, const EnemyAudio& cues = {})
    {
        if (!deviceReady) return;
        const float dt = GetFrameTime();
        if(!paused) UpdateMelody(dt, quiet);
        else if(activeMelody>=0 && !melodyPaused) UpdateMusicStream(melodies[activeMelody]);
        UpdateMusic(dt);
        if(paused) return;
        UpdateCues(dt, cues);

        if (!IsSoundValid(footstep)) return;
        timeSinceStep = std::min(StepInterval, timeSinceStep + dt);
        if (pixelsMoved <= 0.0f)
        {
            idleTime = std::min(StepResetAfter, idleTime + dt);
            if (idleTime >= StepResetAfter) stepProgress = StepDistance - MinimumWalkDistance;
            return;
        }
        idleTime = 0;
        stepProgress = std::min(StepDistance, stepProgress + pixelsMoved);
        if (stepProgress >= StepDistance && timeSinceStep >= StepInterval && !IsSoundPlaying(footstep))
        {
            PlaySound(footstep);
            stepProgress = 0;
            timeSinceStep = 0;
        }
    }

    void Shutdown()
    {
        if (!deviceReady) return;
        if (IsSoundValid(damage)) UnloadSound(damage);
        if (IsSoundValid(victory)) UnloadSound(victory);
        if (IsSoundValid(chest)) UnloadSound(chest);
        if (IsSoundValid(goldPickup)) UnloadSound(goldPickup);
        for (auto& sound : enemies) if (IsSoundValid(sound)) UnloadSound(sound);
        for (Music& track : melodies) if (IsMusicValid(track)) UnloadMusicStream(track);
        if (IsSoundValid(footstep))   UnloadSound(footstep);
        if (IsMusicValid(menuMusic))  UnloadMusicStream(menuMusic);
        if (IsMusicValid(gameMusic))  UnloadMusicStream(gameMusic);
        CloseAudioDevice();
        footstep  = {};
        menuMusic = {};
        gameMusic = {};
        deviceReady = false;
        voiceCount=0;
    }

private:
    void SetMusicGain(Music music,float individual)
    {
        if(IsMusicValid(music))SetMusicVolume(music,preferences.MusicGain()*std::clamp(individual,0.0f,1.0f));
    }
    void SetSfxGain(Sound sound,float individual)
    {
        if(!IsSoundValid(sound))return;
        individual=std::clamp(individual,0.0f,1.0f);
        int index=0;
        while(index<voiceCount && voices[index].sound.stream.buffer!=sound.stream.buffer)++index;
        if(index==voiceCount && voiceCount<static_cast<int>(voices.size()))++voiceCount;
        if(index<static_cast<int>(voices.size()))voices[index]={sound,individual};
        SetSoundVolume(sound,preferences.SfxGain()*individual);
    }
    void LoadLootSound(Sound& target,const std::string& path,float volume)
    {
        if(!FileExists(path.c_str())){TraceLog(LOG_WARNING,"Loot sound missing; continuing muted: %s",path.c_str());return;}
        target=LoadSound(path.c_str());
        if(!IsSoundValid(target)){TraceLog(LOG_WARNING,"Loot sound unreadable: %s",path.c_str());return;}
        SetSfxGain(target,volume); // Raylib master gain applies normally.
        TraceLog(LOG_INFO,"Loot sound loaded: %s (volume %.2f)",path.c_str(),volume);
    }
    Sound& Clip(Cue cue) { return enemies[static_cast<unsigned>(cue)]; }
    void StopCue(Cue& cue)
    {
        if (cue == Cue::Count) return;
        if (IsSoundValid(Clip(cue))) StopSound(Clip(cue));
        cue = Cue::Count;
    }

    void PlayCue(Cue& current, Cue cue, float distance, float volume)
    {
        if (cue == Cue::Count || !IsSoundValid(Clip(cue))) return;
        const float gain = std::clamp(1.0f - distance / EnemySoundRange, 0.0f, 1.0f);
        current = cue;
        SetSfxGain(Clip(cue), volume * gain * gain);
        PlaySound(Clip(cue));
    }

    bool DamagePlaying()
    {
        return (IsSoundValid(damage) && IsSoundPlaying(damage))
            || (IsSoundValid(Clip(Cue::GhostStrike)) && IsSoundPlaying(Clip(Cue::GhostStrike)));
    }

    void UpdateCues(float dt, const EnemyAudio& cues)
    {
        motionDelay = std::max(0.0f, motionDelay - dt);
        if (motion != cues.motion) StopCue(motion);
        if (action != Cue::Count && !IsSoundPlaying(Clip(action))) action = Cue::Count;
        if (DamagePlaying()) return;
        if (cues.action != Cue::Count && (action == Cue::Count || cues.urgent))
        {
            StopCue(motion);
            StopCue(action);
            PlayCue(action, cues.action, cues.actionDistance, cues.urgent ? EnemyHitVolume : EnemyAttackVolume);
        }
        if (action == Cue::Count && cues.motion != Cue::Count && motionDelay == 0
            && (motion == Cue::Count || !IsSoundPlaying(Clip(motion))))
        {
            PlayCue(motion, cues.motion, cues.motionDistance, EnemyMotionVolume);
            motionDelay = 0.43f;
        }
    }

    void ScheduleMelody()
    {
        melodyRandom = melodyRandom * 1664525u + 1013904223u;
        melodyWait = MelodySilenceMin + (MelodySilenceMax - MelodySilenceMin) * ((melodyRandom >> 8) / 16777216.0f);
    }

    void StopMelody()
    {
        if (activeMelody >= 0) StopMusicStream(melodies[activeMelody]);
        activeMelody = -1;
        melodyGain = 0;
        melodyPaused = false;
        resumeQuiet = 0;
    }

    void UpdateMelody(float dt, bool quiet)
    {
        if (!IsMusicValid(melodies[0]) && !IsMusicValid(melodies[1])) return;
        quiet = quiet && activeScene == Scene::Gameplay && requestedScene == Scene::Gameplay;
        if (activeMelody < 0)
        {
            if (!quiet) return;
            melodyWait -= dt;
            if (melodyWait > 0) return;
            if (!IsMusicValid(melodies[nextMelody])) nextMelody = 1 - nextMelody;
            activeMelody = nextMelody;
            nextMelody = 1 - nextMelody;
            SetMusicGain(melodies[activeMelody], 0);
            PlayMusicStream(melodies[activeMelody]);
        }

        Music& melody = melodies[activeMelody];
        if (melodyPaused)
        {
            if (requestedScene != Scene::Gameplay) { StopMelody(); return; }
            resumeQuiet = quiet ? resumeQuiet + dt : 0.0f;
            if (resumeQuiet < MelodyResumeQuiet) return;
            ResumeMusicStream(melody);
            melodyPaused = false;
        }
        UpdateMusicStream(melody);
        const float remaining = GetMusicTimeLength(melody) - GetMusicTimePlayed(melody);
        const float target = quiet ? std::clamp(remaining / MelodyFade, 0.0f, 1.0f) : 0.0f;
        const float step = dt / (quiet ? MelodyFade : MelodyDangerFade);
        melodyGain += std::clamp(target - melodyGain, -step, step);
        SetMusicGain(melody, MelodyVolume * melodyGain);

        if (!IsMusicStreamPlaying(melody) || remaining <= 0.03f)
        {
            StopMelody();
            ScheduleMelody();
        }
        else if (!quiet && melodyGain == 0)
        {
            if (requestedScene != Scene::Gameplay) StopMelody();
            else
            {
                PauseMusicStream(melody);
                melodyPaused = true;
                resumeQuiet = 0;
            }
        }
    }

    bool LoadLocalTrack(Music& track, const std::string& directory, const char* basename)
    {
        for (const char* extension : {".ogg", ".mp3", ".flac", ".wav"})
            if (LoadTrack(track, directory + basename + extension)) return true;
        return false;
    }

    bool LoadTrack(Music& target, const std::string& path)
    {
        if (!FileExists(path.c_str())) return false;
        target = LoadMusicStream(path.c_str());
        if (!IsMusicValid(target)) { target = {}; return false; }
        target.looping = true;
        return true;
    }

    void LoadGameSoundtrack(Music& stream, const std::string& directory, Soundtrack selection)
    {
        if (selection == Soundtrack::Off) return;
        const bool cavernFirst = (selection == Soundtrack::DarkCavern);
        const char* first  = cavernFirst ? "dark_cavern_ambient_001.ogg" : "dungeon002.ogg";
        const char* second = cavernFirst ? "dungeon002.ogg" : "dark_cavern_ambient_001.ogg";
        if (!LoadTrack(stream, directory + first) && !LoadTrack(stream, directory + second))
            TraceLog(LOG_WARNING, "No usable dungeon soundtrack was found.");
    }

    Music& ActiveMusic()
    {
        return activeScene == Scene::Menu ? menuMusic : gameMusic;
    }

    float ActiveVolume() const
    {
        return (activeScene == Scene::Menu ? MenuMusicVolume : GameplayMusicVolume)*(paused?0.3f:1.0f);
    }

    void UpdateMusic(float dt)
    {
        float fadeStep = dt / FadeDuration;
        if (activeScene != requestedScene)
        {
            Music& outgoing = ActiveMusic();
            if (IsMusicValid(outgoing))
            {
                const float faded = std::min(musicGain, fadeStep);
                musicGain -= faded;
                fadeStep -= faded;
                SetMusicGain(outgoing, ActiveVolume() * musicGain);
                if (musicGain > 0.0f)
                {
                    UpdateMusicStream(outgoing);
                    return;
                }
                StopMusicStream(outgoing);
            }

            // Keep one stream playing at a time. A reversed transition raises
            // the current stream from its actual gain if it is still active.
            activeScene = requestedScene;
            musicGain = 0.0f;
            Music& incoming = ActiveMusic();
            if (IsMusicValid(incoming))
            {
                SetMusicGain(incoming, 0.0f);
                PlayMusicStream(incoming);
            }
        }

        Music& current = ActiveMusic();
        if (!IsMusicValid(current)) return;
        if (musicGain < 1.0f)
        {
            musicGain = std::min(1.0f, musicGain + fadeStep);
            SetMusicGain(current, ActiveVolume() * musicGain);
        }
        UpdateMusicStream(current);
    }

    bool  deviceReady = false;
    AudioPreferences::Settings preferences;
    struct Voice {Sound sound{};float individual=1;};
    std::array<Voice,32> voices{};
    int voiceCount=0;
    bool paused = false;
    Music menuMusic{};
    Music gameMusic{};
    Sound footstep{}, damage{}, victory{}, chest{}, goldPickup{};
    std::array<Sound, static_cast<unsigned>(Cue::Count)> enemies{};
    Cue motion = Cue::Count, action = Cue::Count;
    float motionDelay = 0;
    std::array<Music, 2> melodies{};
    int activeMelody = -1, nextMelody = 0;
    std::uint32_t melodyRandom = 0;
    float melodyWait = MelodySilenceMin, melodyGain = 0;
    float resumeQuiet = 0;
    bool melodyPaused = false;
    Scene activeScene = Scene::Menu;
    Scene requestedScene = Scene::Menu;
    float musicGain = 1.0f;
    float stepProgress = StepDistance - MinimumWalkDistance;
    float timeSinceStep = StepInterval;
    float idleTime = 0;
    std::string footstepPath;

};
}
#endif
