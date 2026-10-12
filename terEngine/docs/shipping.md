# Shipping your game

[← Back to README](../README.md) · CMake: `cmake/TerGame.cmake` · Header: `ter/core/paths.h`

This page turns your game into a zip that anyone with a Mac can download and play, e.g. for a game jam or itch.io.

---

## 1. Set up the project for shipping

Your `CMakeLists.txt` should look like this:

```cmake
cmake_minimum_required(VERSION 3.20)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")   # Apple Silicon + Intel
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")        # macOS 11 Big Sur and newer
project(mygame LANGUAGES C CXX OBJCXX)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)   # optimized by default
endif()
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(terEngine)

ter_add_game(mygame
  SOURCES src/main.cpp src/monster.cpp
  ASSETS  assets
  NAME    "Find the Box"
  ID      com.yourname.findthebox
  VERSION 1.0
  ICON    icon.icns)                # optional
```

| Argument | Meaning |
|---|---|
| `SOURCES` | Your `.cpp` files (required) |
| `ASSETS` | A folder copied into the app as `Contents/Resources/assets` |
| `NAME` | Name of the app, the zip and the menu bar (default: the target name) |
| `ID` | Unique reverse-domain identifier. macOS uses it to tell apps apart. |
| `VERSION` | Shown in Finder → Get Info |
| `ICON` | An `.icns` file (see [App icon](#app-icon)) |

The two `CMAKE_OSX_*` lines must come **before** `project()`. If you add them to an existing build folder, delete `build/` once so CMake picks them up.

## 2. Load files with `ter::assetPath`

Never use absolute paths or paths relative to the current directory: on the player's Mac the game is somewhere else, and double-clicking an app starts it with `/` as the working directory.

```cpp
audio.play(ter::assetPath("sfx/door.wav"));           // -> .../Find the Box.app/Contents/Resources/assets/sfx/door.wav
tex->load(ter::assetPath("textures/wall.png"));
auto model = ter::loadGltf(ter::assetPath("models/ghost.glb"));
```

| Function | Returns |
|---|---|
| `ter::assetPath("a/b.wav")` | Full path of a file in your assets folder |
| `ter::resourceDir()` | `Contents/Resources` of the app (or the executable's folder for a plain executable) |
| `ter::executableDir()` | The folder containing the running executable |
| `ter::setAssetRoot(dir)` | Load assets from another folder instead (e.g. for tools) |

The built-in fonts are copied into the app as well and found automatically.

Assets are copied into the app on every `cmake --build`. Only changed files are copied (with CMake 3.26 or newer). After editing a sound or texture, rebuild before running. Files you delete from `assets/` stay in the build until you delete `build/`.

## 3. Build the zip

```sh
cmake -S . -B build
cmake --build build -j
cmake --build build --target mygame_zip
```

This signs the app (ad-hoc, see below) and writes **`build/Find the Box-macOS.zip`**, which is the file you upload.

Always zip with this target, not Finder's "Compress" or `zip`. It uses `ditto`, which keeps the app's symlinks, permissions and signature intact.

## 4. Test it like a player would

Before uploading, check the zip on a clean copy:

```sh
mkdir -p /tmp/release-test && cd /tmp/release-test
ditto -x -k "path/to/build/Find the Box-macOS.zip" .
open "Find the Box.app"
```

Better still, try it on a friend's Mac, ideally one with a different processor (Intel vs. Apple Silicon). To check the Intel version on an Apple Silicon Mac with Rosetta installed:

```sh
arch -x86_64 "/tmp/release-test/Find the Box.app/Contents/MacOS/Find the Box"
```

What the build gives you:

| Check | Command | Expected |
|---|---|---|
| Runs on both Mac types | `lipo -info "Find the Box.app/Contents/MacOS/Find the Box"` | `x86_64 arm64` |
| No Homebrew/local libraries | `otool -L "Find the Box.app/Contents/MacOS/Find the Box"` | only `/usr/lib/...` and `/System/...` |
| Signed | `codesign -dv "Find the Box.app"` | `Signature=adhoc` |

---

## What players have to do: opening an unsigned app

The app is **ad-hoc signed**, which is enough for macOS to run it but doesn't identify you as a developer. When a player downloads it, macOS blocks the first launch. Copy these instructions onto your game page:

> **How to open on macOS**
> 1. Unzip and double-click **Find the Box.app**. macOS will say it can't verify the app; click **Done** / **Cancel**.
> 2. Open **System Settings → Privacy & Security**, scroll down and click **Open Anyway** next to "Find the Box".
> 3. Confirm with **Open**. From then on it opens normally.
>
> (macOS 14 and earlier: right-click the app → **Open** → **Open**.)
>
> Or in Terminal: `xattr -dr com.apple.quarantine "Find the Box.app"`

To remove this step entirely you need an Apple Developer ID ($99/year), then sign and notarize:

```sh
codesign --force --deep --options runtime --sign "Developer ID Application: Your Name (TEAMID)" "Find the Box.app"
ditto -c -k --keepParent "Find the Box.app" game.zip
xcrun notarytool submit game.zip --apple-id you@example.com --team-id TEAMID --wait
xcrun stapler staple "Find the Box.app"
ditto -c -k --keepParent "Find the Box.app" "Find the Box-macOS.zip"   # re-zip the stapled app
```

## App icon

Make a 1024×1024 PNG, then convert it to `.icns`:

```sh
mkdir icon.iconset
for s in 16 32 128 256 512; do
  sips -z $s $s icon.png --out icon.iconset/icon_${s}x${s}.png
  sips -z $((s*2)) $((s*2)) icon.png --out icon.iconset/icon_${s}x${s}@2x.png
done
iconutil -c icns icon.iconset   # -> icon.icns
```

Then add `ICON icon.icns` to `ter_add_game`.

---

## Release checklist

- [ ] Every file is loaded through `ter::assetPath(...)`; `grep -rn '"/Users' src` finds nothing
- [ ] `CMAKE_OSX_ARCHITECTURES` is `arm64;x86_64`, and `lipo -info` confirms it
- [ ] Built optimized: the template defaults to `Release` (check `CMAKE_BUILD_TYPE` in `build/CMakeCache.txt`)
- [ ] The zip opens and plays from a fresh folder (step 4), including sound and text
- [ ] Esc / quitting works, and the game doesn't need a terminal
- [ ] Your game page says **macOS only** and includes the "How to open" steps
- [ ] Font licenses: the bundled fonts are OFL; their license files are inside the app (`Contents/Resources/fonts/OFL-*.txt`), which satisfies the license
