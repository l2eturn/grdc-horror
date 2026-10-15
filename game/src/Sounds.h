#pragma once
// Placeholder sounds generated with ter::Audio::synth, until real recordings exist.
#include <string>
#include <ter/ter.h>

// Register every placeholder sound by name ("step", "wind", ...)
void registerPlaceholderSounds(ter::Audio& audio);

// Sound names in data files are either a placeholder name ("cricket") or a file in assets/ ("sfx/cricket.wav").
// Swapping a placeholder for a real file is then only a data change.
std::string soundSource(const std::string& name);
