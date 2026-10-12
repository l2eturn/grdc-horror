#pragma once
// Sound playback on miniaudio's engine: music / effects from files (wav, mp3, flac, ogg-vorbis via miniaudio),
// 3D positional sounds with a listener, and small synthesized sounds for placeholder audio.
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace ter {

enum class Wave { Sine, Triangle, Square, Sawtooth, Noise };

// A generated sound: one oscillator (or noise) with a short attack and an exponential fade-out
struct Synth {
    Wave wave = Wave::Sine;
    // start frequency in Hz; slides exponentially to freqEnd (0 = no slide)
    float freq = 440, freqEnd = 0;
    float duration = 0.3f;
    float attack = 0.01f;
    float volume = 0.3f;
    // vibrato: depth in semitones and rate in Hz (0 = off)
    float vibrato = 0, vibratoRate = 6;
};

struct PlayOptions {
    float volume = 1;
    // playback speed: 2 = one octave up
    float pitch = 1;
    // -1 left .. 1 right (only for non-spatial sounds)
    float pan = 0;
    bool loop = false;
    // positional sound: volume and panning follow the listener (Audio::setListener)
    bool spatial = false;
    glm::vec3 position{0};
    // full volume up to minDistance, then fades with distance (inverse model) until maxDistance
    float minDistance = 1, maxDistance = 30;
    // fade in over this many seconds
    float fadeIn = 0;
};

class Audio;

// One playing (or paused) instance. Keep the shared_ptr to control it; drop it and a one-shot
// keeps playing until it ends (Audio holds it).
class Sound {
public:
    ~Sound();
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    void play();
    void stop();
    bool playing() const;
    bool finished() const;
    void setVolume(float v);
    void setPitch(float p);
    void setPan(float p);
    void setLooping(bool loop);
    void setPosition(glm::vec3 p);
    // fade the volume to `volume` over `seconds` (stop when it reaches 0 with stopAtEnd)
    void fadeTo(float volume, float seconds, bool stopAtEnd = false);

private:
    friend class Audio;
    Sound() = default;
    struct Impl;
    std::unique_ptr<Impl> d;
};
using SoundPtr = std::shared_ptr<Sound>;

class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // Start the output device. Every other call is a no-op (returns null) until this succeeds.
    bool init();
    bool ok() const;

    // Play a file path, or a name registered with synth(). Returns null if it cannot be loaded.
    // Files are decoded once and cached, so playing the same file many times is cheap.
    SoundPtr play(const std::string& nameOrPath, const PlayOptions& o = {});
    // Long music files: streamed from disk instead of decoded into memory
    SoundPtr stream(const std::string& path, const PlayOptions& o = {});

    // Render a Synth into memory under `name` (then play(name) like a file)
    void synth(const std::string& name, const Synth& s);
    // Several synths mixed into one sound; each starts at its own offset in seconds (chords, jingles)
    void synth(const std::string& name, const std::vector<std::pair<float, Synth>>& parts);

    // Where the ears are, for spatial sounds (usually the camera)
    void setListener(glm::vec3 position, glm::vec3 forward, glm::vec3 up = {0, 1, 0});
    void setMasterVolume(float v);
    void setMuted(bool muted);
    bool muted() const;
    // stop everything that is playing
    void stopAll();

    // Free finished one-shot sounds (App calls this every frame)
    void update();

private:
    SoundPtr start(const std::string& nameOrPath, const PlayOptions& o, bool streamed);
    struct Impl;
    std::unique_ptr<Impl> d;
};

}  // namespace ter
