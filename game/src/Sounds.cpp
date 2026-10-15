#include "Sounds.h"

void registerPlaceholderSounds(ter::Audio& audio) {
    using ter::Wave;
    // footstep on gravel: a short, low burst of noise
    audio.synth("step", {.wave = Wave::Noise, .freq = 200, .duration = 0.08f, .volume = 0.10f});
    // flashlight switch
    audio.synth("click", {.wave = Wave::Square, .freq = 1800, .freqEnd = 900, .duration = 0.04f, .volume = 0.08f});
    // message tone: two short bright notes
    audio.synth("notify", std::vector<std::pair<float, ter::Synth>>{
                              {0.0f, {.wave = Wave::Sine, .freq = 1320, .duration = 0.14f, .volume = 0.09f}},
                              {0.15f, {.wave = Wave::Sine, .freq = 1760, .duration = 0.22f, .volume = 0.08f}}});
    // phone vibrating (in the hand or the pocket): two low buzzes
    audio.synth("vibrate", std::vector<std::pair<float, ter::Synth>>{
                               {0.0f, {.wave = Wave::Square, .freq = 52, .duration = 0.32f, .attack = 0.02f, .volume = 0.07f}},
                               {0.5f, {.wave = Wave::Square, .freq = 52, .duration = 0.32f, .attack = 0.02f, .volume = 0.07f}}});
    // wind: long noise with a slow attack, looped -> rising and falling gusts
    audio.synth("wind", {.wave = Wave::Noise, .duration = 6.0f, .attack = 2.5f, .volume = 0.05f});
    // cricket: a fast high trill (vibrato does the chirping)
    audio.synth("cricket", {.wave = Wave::Square, .freq = 4200, .duration = 0.35f, .volume = 0.025f, .vibrato = 2, .vibratoRate = 32});
    // electric hum of a street lamp / fluorescent tube
    audio.synth("buzz", {.wave = Wave::Sawtooth, .freq = 120, .duration = 2.0f, .attack = 0.8f, .volume = 0.035f, .vibrato = 0.1f, .vibratoRate = 2});
    // electric bus: low motor whine (looped) and the air hiss of its doors
    audio.synth("bus_hum", {.wave = Wave::Sawtooth, .freq = 58, .duration = 3.0f, .attack = 0.05f, .volume = 0.05f, .vibrato = 0.15f, .vibratoRate = 3});
    // the old woman: a wet, creaking stretch (looped while the neck grows), a whisper, the scream, the cart coming free
    audio.synth("neck_creak", {.wave = Wave::Sawtooth, .freq = 46, .freqEnd = 38, .duration = 1.6f, .attack = 0.2f, .volume = 0.05f, .vibrato = 3, .vibratoRate = 9});
    audio.synth("whisper", {.wave = Wave::Noise, .freq = 900, .duration = 2.6f, .attack = 0.9f, .volume = 0.05f});
    audio.synth("scream", std::vector<std::pair<float, ter::Synth>>{
                              {0.0f, {.wave = Wave::Sawtooth, .freq = 900, .freqEnd = 1400, .duration = 0.9f, .attack = 0.02f, .volume = 0.12f, .vibrato = 1.5f, .vibratoRate = 14}},
                              {0.0f, {.wave = Wave::Noise, .duration = 0.9f, .attack = 0.02f, .volume = 0.12f}}});
    audio.synth("cart_creak", {.wave = Wave::Square, .freq = 210, .freqEnd = 160, .duration = 0.7f, .attack = 0.05f, .volume = 0.04f, .vibrato = 2, .vibratoRate = 11});
    // rule 5: a cassette tape rewinding, coming closer in the dark (played three times, nearer each time)
    audio.synth("tape_rewind", {.wave = Wave::Sawtooth, .freq = 320, .freqEnd = 980, .duration = 2.4f, .attack = 0.1f, .volume = 0.06f, .vibrato = 40, .vibratoRate = 7});
    // rule 9: something heavy dragged across the ground (looped while it lasts)
    audio.synth("drag", {.wave = Wave::Noise, .freq = 160, .duration = 2.2f, .attack = 0.6f, .volume = 0.09f, .vibrato = 0.4f, .vibratoRate = 1.3f});
    // AirDrop received: a soft rising two-note chime
    audio.synth("airdrop", std::vector<std::pair<float, ter::Synth>>{
                               {0.0f, {.wave = Wave::Sine, .freq = 880, .freqEnd = 990, .duration = 0.12f, .volume = 0.08f}},
                               {0.11f, {.wave = Wave::Sine, .freq = 1320, .freqEnd = 1480, .duration = 0.25f, .volume = 0.08f}}});
    // holding your breath: heartbeat (lub-dub), a quiet breath out, a loud one, a sharp gasp
    audio.synth("heart", std::vector<std::pair<float, ter::Synth>>{
                             {0.0f, {.wave = Wave::Sine, .freq = 58, .freqEnd = 40, .duration = 0.14f, .attack = 0.005f, .volume = 0.22f}},
                             {0.2f, {.wave = Wave::Sine, .freq = 52, .freqEnd = 38, .duration = 0.12f, .attack = 0.005f, .volume = 0.15f}}});
    audio.synth("breath_soft", {.wave = Wave::Noise, .freq = 500, .duration = 1.1f, .attack = 0.4f, .volume = 0.025f});
    audio.synth("breath_loud", {.wave = Wave::Noise, .freq = 900, .duration = 0.9f, .attack = 0.08f, .volume = 0.12f});
    audio.synth("breath_gasp", {.wave = Wave::Noise, .freq = 1400, .duration = 0.35f, .attack = 0.01f, .volume = 0.16f});
    // the ending: a dissonant stab when the companion shows his face, a low boom on the cut to black
    audio.synth("sting", std::vector<std::pair<float, ter::Synth>>{
                             {0.0f, {.wave = Wave::Sawtooth, .freq = 220, .freqEnd = 207, .duration = 1.8f, .attack = 0.01f, .volume = 0.10f, .vibrato = 0.6f, .vibratoRate = 7}},
                             {0.0f, {.wave = Wave::Sawtooth, .freq = 233, .freqEnd = 246, .duration = 1.8f, .attack = 0.01f, .volume = 0.10f}},
                             {0.0f, {.wave = Wave::Noise, .duration = 0.5f, .attack = 0.005f, .volume = 0.10f}}});
    audio.synth("boom", {.wave = Wave::Sine, .freq = 70, .freqEnd = 30, .duration = 2.5f, .attack = 0.005f, .volume = 0.25f});
    // the ghost dancer's music: a ranat (wooden xylophone) line on a five-note scale over a slow drum, a little out
    // of tune; 4.8 s, looped
    {
        std::vector<std::pair<float, ter::Synth>> notes;
        const float scale[] = {523, 587, 659, 784, 880, 1047};
        const int line[] = {4, 3, 2, 3, 4, 5, 4, 2, 1, 2, 3, 2, 1, 0, 1, 2};
        for (int i = 0; i < 16; ++i)
            notes.push_back({i * 0.3f, {.wave = Wave::Triangle, .freq = scale[line[i]] * 0.985f, .duration = 0.28f, .attack = 0.004f, .volume = 0.05f, .vibrato = 0.25f, .vibratoRate = 5}});
        for (int i = 0; i < 4; ++i)
            notes.push_back({i * 1.2f, {.wave = Wave::Sine, .freq = 95, .freqEnd = 55, .duration = 0.5f, .attack = 0.005f, .volume = 0.12f}});
        notes.push_back({4.79f, {.wave = Wave::Sine, .freq = 20, .duration = 0.01f, .volume = 0.0f}});
        audio.synth("dance_music", notes);
    }
    audio.synth("bus_hiss", {.wave = Wave::Noise, .duration = 0.9f, .attack = 0.03f, .volume = 0.12f});
}

std::string soundSource(const std::string& name) {
    // a dot means a file name ("sfx/wind.ogg"); otherwise it is a synth registered above
    return name.find('.') != std::string::npos ? ter::assetPath(name) : name;
}
