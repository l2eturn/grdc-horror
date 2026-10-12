#define MINIAUDIO_IMPLEMENTATION
#include "ter/audio/audio.h"
#include <miniaudio.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include "ter/core/math.h"

namespace ter {

namespace {

constexpr ma_uint32 kSynthRate = 48000;

// The engine lives until the last Sound using it is gone (a Sound may outlive Audio)
struct EngineBox {
    ma_engine engine{};
    bool ok = false;
    // generated sounds: the resource manager reads these buffers directly, so they live as long as the engine
    std::map<std::string, std::vector<float>> synths;
    ~EngineBox() {
        if (!ok) return;
        for (auto& [name, data] : synths) ma_resource_manager_unregister_data(ma_engine_get_resource_manager(&engine), name.c_str());
        ma_engine_uninit(&engine);
    }
};

// Add one Synth into `out` (mono floats) starting at sample `offset`
void renderSynth(const Synth& s, size_t offset, std::vector<float>& out) {
    size_t n = (size_t)(s.duration * kSynthRate);
    if (out.size() < offset + n) out.resize(offset + n, 0.0f);
    // deterministic noise so the same Synth always sounds the same
    uint32_t seed = 0x9e3779b9u;
    double phase = 0;
    float attack = std::max(s.attack, 0.001f);
    for (size_t i = 0; i < n; ++i) {
        float t = (float)i / kSynthRate;
        float k = t / std::max(s.duration, 1e-3f);
        float f = s.freqEnd > 0 ? s.freq * std::pow(s.freqEnd / s.freq, k) : s.freq;
        if (s.vibrato > 0) f *= std::pow(2.0f, s.vibrato / 12 * std::sin(2 * PI * s.vibratoRate * t));
        phase += f / kSynthRate;
        float ph = (float)(phase - std::floor(phase));
        float w;
        switch (s.wave) {
            case Wave::Sine: w = std::sin(ph * 2 * PI); break;
            case Wave::Triangle: w = 1 - 4 * std::abs(ph - 0.5f); break;
            case Wave::Square: w = ph < 0.5f ? 1.0f : -1.0f; break;
            case Wave::Sawtooth: w = 2 * ph - 1; break;
            default:
                seed = seed * 1664525u + 1013904223u;
                w = (seed >> 8) / 8388608.0f - 1;
                break;
        }
        // linear attack, then exponential decay to -60 dB at the end
        float env = t < attack ? t / attack : std::pow(0.001f, (t - attack) / std::max(s.duration - attack, 1e-3f));
        out[offset + i] += w * env * s.volume;
    }
}

}  // namespace

struct Sound::Impl {
    std::shared_ptr<EngineBox> box;
    ma_sound snd{};
    bool ok = false;
    ~Impl() {
        if (ok) ma_sound_uninit(&snd);
    }
};

Sound::~Sound() = default;

void Sound::play() {
    if (d->ok) ma_sound_start(&d->snd);
}
void Sound::stop() {
    if (d->ok) ma_sound_stop(&d->snd);
}
bool Sound::playing() const { return d->ok && ma_sound_is_playing(&d->snd); }
bool Sound::finished() const { return !d->ok || ma_sound_at_end(&d->snd); }
void Sound::setVolume(float v) {
    if (d->ok) ma_sound_set_volume(&d->snd, v);
}
void Sound::setPitch(float p) {
    if (d->ok) ma_sound_set_pitch(&d->snd, p);
}
void Sound::setPan(float p) {
    if (d->ok) ma_sound_set_pan(&d->snd, p);
}
void Sound::setLooping(bool loop) {
    if (d->ok) ma_sound_set_looping(&d->snd, loop);
}
void Sound::setPosition(glm::vec3 p) {
    if (d->ok) ma_sound_set_position(&d->snd, p.x, p.y, p.z);
}
void Sound::fadeTo(float volume, float seconds, bool stopAtEnd) {
    if (!d->ok) return;
    ma_uint64 ms = (ma_uint64)(seconds * 1000);
    // -1 = start the fade from the current volume
    ma_sound_set_fade_in_milliseconds(&d->snd, -1, volume, ms);
    if (stopAtEnd) ma_sound_set_stop_time_in_milliseconds(&d->snd, ma_engine_get_time_in_milliseconds(&d->box->engine) + ms);
}

struct Audio::Impl {
    std::shared_ptr<EngineBox> box = std::make_shared<EngineBox>();
    // one-shots nobody else holds on to
    std::vector<SoundPtr> live;
    float master = 1;
    bool muted = false;

    void applyVolume() {
        if (box->ok) ma_engine_set_volume(&box->engine, muted ? 0 : master);
    }
};

Audio::Audio() : d(std::make_unique<Impl>()) {}

Audio::~Audio() {
    stopAll();
    d->live.clear();
}

bool Audio::init() {
    if (d->box->ok) return true;
    d->box->ok = ma_engine_init(nullptr, &d->box->engine) == MA_SUCCESS;
    if (!d->box->ok) std::fprintf(stderr, "audio: no output device\n");
    d->applyVolume();
    return d->box->ok;
}

bool Audio::ok() const { return d->box->ok; }

SoundPtr Audio::start(const std::string& nameOrPath, const PlayOptions& o, bool streamed) {
    if (!d->box->ok) return nullptr;
    SoundPtr s(new Sound());
    s->d = std::make_unique<Sound::Impl>();
    s->d->box = d->box;
    ma_uint32 flags = streamed ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
    if (!o.spatial) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (ma_sound_init_from_file(&d->box->engine, nameOrPath.c_str(), flags, nullptr, nullptr, &s->d->snd) != MA_SUCCESS) {
        std::fprintf(stderr, "audio: cannot load %s\n", nameOrPath.c_str());
        return nullptr;
    }
    s->d->ok = true;
    ma_sound* m = &s->d->snd;
    ma_sound_set_pitch(m, o.pitch);
    ma_sound_set_looping(m, o.loop);
    if (o.spatial) {
        ma_sound_set_position(m, o.position.x, o.position.y, o.position.z);
        ma_sound_set_attenuation_model(m, ma_attenuation_model_inverse);
        ma_sound_set_min_distance(m, o.minDistance);
        ma_sound_set_max_distance(m, o.maxDistance);
    } else {
        ma_sound_set_pan(m, o.pan);
    }
    if (o.fadeIn > 0) {
        ma_sound_set_volume(m, 1);
        ma_sound_set_fade_in_milliseconds(m, 0, o.volume, (ma_uint64)(o.fadeIn * 1000));
    } else {
        ma_sound_set_volume(m, o.volume);
    }
    ma_sound_start(m);
    d->live.push_back(s);
    return s;
}

SoundPtr Audio::play(const std::string& nameOrPath, const PlayOptions& o) { return start(nameOrPath, o, false); }
SoundPtr Audio::stream(const std::string& path, const PlayOptions& o) { return start(path, o, true); }

void Audio::synth(const std::string& name, const Synth& s) { synth(name, std::vector<std::pair<float, Synth>>{{0.0f, s}}); }

void Audio::synth(const std::string& name, const std::vector<std::pair<float, Synth>>& parts) {
    if (!d->box->ok) return;
    ma_resource_manager* rm = ma_engine_get_resource_manager(&d->box->engine);
    if (d->box->synths.count(name)) ma_resource_manager_unregister_data(rm, name.c_str());
    std::vector<float>& buf = d->box->synths[name];
    buf.clear();
    for (auto& [at, s] : parts) renderSynth(s, (size_t)(std::max(at, 0.0f) * kSynthRate), buf);
    for (float& v : buf) v = std::clamp(v, -1.0f, 1.0f);
    ma_resource_manager_register_decoded_data(rm, name.c_str(), buf.data(), buf.size(), ma_format_f32, 1, kSynthRate);
}

void Audio::setListener(glm::vec3 p, glm::vec3 forward, glm::vec3 up) {
    if (!d->box->ok) return;
    ma_engine_listener_set_position(&d->box->engine, 0, p.x, p.y, p.z);
    ma_engine_listener_set_direction(&d->box->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&d->box->engine, 0, up.x, up.y, up.z);
}

void Audio::setMasterVolume(float v) {
    d->master = v;
    d->applyVolume();
}

void Audio::setMuted(bool m) {
    d->muted = m;
    d->applyVolume();
}

bool Audio::muted() const { return d->muted; }

void Audio::stopAll() {
    for (auto& s : d->live) s->stop();
}

void Audio::update() {
    // a sound only Audio still references is done once it stops (finished or stopped)
    d->live.erase(std::remove_if(d->live.begin(), d->live.end(), [](const SoundPtr& s) { return s.use_count() == 1 && !s->playing(); }),
                  d->live.end());
}

}  // namespace ter
