# Audio

[← Back to README](../README.md) · Header: `ter/audio/audio.h`

Audio runs on [miniaudio](https://miniaud.io/). Get it with `app.audio()`; it starts the first time you call it.

```cpp
ter::Audio& audio = app.audio();
```

---

## Playing sounds

```cpp
// play a file once (wav, mp3, flac). Decoded on first use, then cached, so repeats are cheap.
audio.play(ter::assetPath("sfx/door.wav"));

// with options
audio.play(ter::assetPath("sfx/step.wav"), {.volume = 0.6f, .pitch = 0.9f + ter::frand() * 0.2f});

// long music: stream from disk instead of loading it all into memory
auto music = audio.stream(ter::assetPath("music/ambient.mp3"), {.volume = 0.4f, .loop = true, .fadeIn = 3});
```

### `PlayOptions`

Write the fields in this order when using designated initializers:

| Field | Default | Meaning |
|---|---|---|
| `volume` | 1 | Loudness |
| `pitch` | 1 | Playback speed; 2 = one octave up, 0.5 = one down |
| `pan` | 0 | −1 left … 1 right (non-spatial sounds only) |
| `loop` | false | Repeat forever |
| `spatial` | false | 3D sound (see below) |
| `position` | 0,0,0 | World position of a 3D sound |
| `minDistance` | 1 | Full volume up to this distance |
| `maxDistance` | 30 | Fades out with distance until here |
| `fadeIn` | 0 | Seconds to fade in from silence |

### Controlling a sound

`play` and `stream` return a `ter::SoundPtr`. Keep it to control the sound later:

```cpp
ter::SoundPtr rain = audio.play(ter::assetPath("sfx/rain.wav"), {.loop = true});

rain->setVolume(0.3f);
rain->setPitch(0.8f);
rain->fadeTo(0, 2.0f, true);   // fade to silence over 2 s, then stop
rain->stop();
rain->play();                  // resume
rain->playing();  rain->finished();
```

If you don't keep the pointer, the sound plays to the end and is cleaned up for you. That is fine for one-shot effects. `play` returns `nullptr` if the file can't be loaded, so check it before using it if the file may be missing.

---

## 3D (positional) sound

A spatial sound gets quieter with distance and pans left/right depending on which way the player faces. The "ears" (listener) follow the camera automatically.

```cpp
auto whisper = audio.play(ter::assetPath("sfx/whisper.wav"),
                          {.loop = true, .spatial = true, .position = ghost->position, .minDistance = 1, .maxDistance = 20});

// every frame, keep it on the ghost
whisper->setPosition(ghost->worldPosition());
```

This makes sound usable as a gameplay mechanic: players can find things or avoid monsters by ear. Headphones make it much stronger.

To place the listener yourself (e.g. third-person games where the ears should be on the character, not the camera):

```cpp
app.listenerFollowsCamera = false;
audio.setListener(characterPos, characterForward);
```

---

## Placeholder sounds from a synthesizer

No sound files yet? Generate simple sounds in code and play them by name exactly like files. When real recordings are ready, change only the name in `play`.

```cpp
audio.synth("step",  {.wave = ter::Wave::Noise, .duration = 0.09f, .volume = 0.12f});
audio.synth("hum",   {.wave = ter::Wave::Sine, .freq = 110, .duration = 2, .attack = 0.6f, .volume = 0.25f, .vibrato = 0.3f});
audio.synth("alarm", {.wave = ter::Wave::Square, .freq = 880, .freqEnd = 440, .duration = 0.4f});

audio.play("step");
audio.play("hum", {.loop = true, .spatial = true, .position = {3, 1, -2}});
```

`ter::Synth` fields (in declaration order):

| Field | Default | Meaning |
|---|---|---|
| `wave` | `Sine` | `Sine`, `Triangle`, `Square`, `Sawtooth`, `Noise` |
| `freq` | 440 | Start frequency in Hz |
| `freqEnd` | 0 | Slide to this frequency (0 = no slide) |
| `duration` | 0.3 | Seconds |
| `attack` | 0.01 | Fade-in time; the sound then fades out exponentially |
| `volume` | 0.3 | Loudness |
| `vibrato`, `vibratoRate` | 0, 6 | Wobble depth in semitones and speed in Hz |

Layer several synths into one sound (chords, stingers, jingles); each part has its own start time:

```cpp
audio.synth("sting", std::vector<std::pair<float, ter::Synth>>{
    {0.00f, {.wave = ter::Wave::Triangle, .freq = 220, .duration = 1.2f}},
    {0.15f, {.wave = ter::Wave::Triangle, .freq = 233, .duration = 1.2f}},   // a dissonant half step
});
```

---

## Global controls

```cpp
audio.setMasterVolume(0.8f);
audio.setMuted(true);   audio.muted();
audio.stopAll();
```
