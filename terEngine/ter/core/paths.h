#pragma once
// Where the game's files are at runtime, independent of the folder it was built in or started from.
// In an app bundle (MyGame.app/Contents/MacOS/MyGame) files live in MyGame.app/Contents/Resources;
// for a plain executable they live next to it. ter_add_game() in CMake copies them there.
#include <string>

namespace ter {

// Folder holding the running executable (no trailing slash)
std::string executableDir();

// Contents/Resources inside an app bundle, otherwise executableDir()
std::string resourceDir();

// Full path of a file in the game's assets folder: assetPath("sfx/door.wav").
// Looks in setAssetRoot() if set, else resourceDir() + "/assets".
std::string assetPath(const std::string& relative);

// Override the assets folder (e.g. to load straight from the source tree while developing)
void setAssetRoot(const std::string& dir);

}  // namespace ter
