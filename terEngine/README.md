# terEngine

A small 3D game library for **macOS**, written in **C++20** on **OpenGL 4.1**.

- **Feels like three.js** — `Object3D`, `Mesh`, `Material`, `Camera`, scene graph, the same geometry generators.
- **Works like raylib / MLX42** — link the library, write `main()`, write your own loop. No editor, no project format.
- **Batteries for small games** — first-person camera, wall collision, 3D positional audio, synthesized placeholder sounds, Thai/emoji text UI, glTF models, a moody post-processing chain (fog, vignette, film grain, pixel-art scale).

```cpp
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "Hello"}, argc, argv);

    auto scene = ter::makeGroup();
    auto cube = ter::makeMesh(ter::geo::box(), ter::lambertColor(ter::hexc(0xff8a3d)));
    scene->add(cube);

    ter::Environment env;
    env.hemiIntensity = 1;
    ter::Camera cam;
    cam.position = {2, 2, 3};
    cam.lookAt({0, 0, 0});
    app.setScene(*scene, cam, env);

    return app.run([&](ter::Frame& f) { cube->rotation.y += f.dt; });
}
```

---

## Contents

- [Requirements](#requirements)
- [Quick start: build and run the examples](#quick-start-build-and-run-the-examples)
- [Use terEngine in your own game](#use-terengine-in-your-own-game)
- [How a terEngine program works](#how-a-terengine-program-works)
- [Documentation](#documentation)
- [Limitations](#limitations)
- [Third-party libraries](#third-party-libraries)

---

## Requirements

| What | How to get it |
|---|---|
| macOS (Apple Silicon or Intel) | — |
| Xcode Command Line Tools (clang, C++20) | `xcode-select --install` |
| CMake 3.20+ | `brew install cmake` |
Every dependency (GLFW, glm, cgltf, meshoptimizer, nlohmann/json, miniaudio) is downloaded by CMake on the first build and linked statically. You don't install anything else, and neither do your players.

---

## Quick start: build and run the examples

```sh
git clone https://github.com/l2eturn/terEngine.git
cd terEngine
cmake -S . -B build          # configure (first run downloads dependencies, ~1 min)
cmake --build build -j       # compile
./build/examples/01_hello_cube
```

You should see an orange cube spinning above a green floor, with a line of Thai text in the corner. Press **Esc** to quit.

| Example | What it shows | Try |
|---|---|---|
| `01_hello_cube` | The smallest complete program: mesh, shadows, fog, UI text | `./build/examples/01_hello_cube` |
| `02_dark_room` | A first-person horror room: flashlight, flickering light, film grain, wall collision, a sound you follow by ear, look-and-press-E, fade from black | `./build/examples/02_dark_room` — click to grab the mouse, WASD to walk, F for the flashlight |
| `03_model_viewer` | Load a `.glb`, play its animations, orbit with the mouse | `./build/examples/03_model_viewer path/to/model.glb` |

Reading the source of these three files (`examples/*.cpp`, 50–150 lines each) is the fastest way to learn the API.

---

## Use terEngine in your own game

**1. Lay out your project** with terEngine inside it (a git submodule or a plain copy both work):

```
mygame/
├── CMakeLists.txt
├── src/
│   └── main.cpp
├── assets/            sounds, textures, models (optional)
└── terEngine/         this repository
```

```sh
mkdir mygame && cd mygame
git init
git submodule add https://github.com/l2eturn/terEngine.git terEngine
```

**2. Write `CMakeLists.txt`:**

```cmake
cmake_minimum_required(VERSION 3.20)
# run on both Apple Silicon and Intel Macs, macOS 11 and newer (must come before project())
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
project(mygame LANGUAGES C CXX OBJCXX)   # OBJCXX is required: terEngine has .mm files
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)   # optimized by default
endif()
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(terEngine)

ter_add_game(mygame
  SOURCES src/main.cpp
  ASSETS  assets              # copied into the app; omit if you have no assets yet
  NAME    "My Game")
```

`ter_add_game` builds a real macOS app (`build/My Game.app`) with your assets and the engine's fonts inside, so the same build you test is the one you can give to other people. In code, load files with `ter::assetPath("sfx/door.wav")`.

**3. Write `src/main.cpp`** — start with the snippet at the top of this page.

**4. Build and run:**

```sh
cmake -S . -B build
cmake --build build -j
open "build/My Game.app"                      # or run the binary to see its terminal output:
"./build/My Game.app/Contents/MacOS/My Game"
```

**5. Share it:** `cmake --build build --target mygame_zip` creates `build/My Game-macOS.zip`, ready to upload. See [Shipping your game](docs/shipping.md).

**Next:** follow the **[tutorial](docs/tutorial.md)**. In about 120 lines it turns that empty window into a small finished game: a dark room you walk through with a flashlight, a humming object you find by ear, and a win condition.

---

## How a terEngine program works

Every program has the same five parts:

```cpp
#include <ter/ter.h>                                   // 1. one include gives you everything

int main(int argc, char** argv) {
    ter::App app({.title = "My Game"}, argc, argv);    // 2. open a window
    if (!app.ok()) return 1;

    auto scene = ter::makeGroup();                     // 3. build a scene: meshes, lights
    ter::Environment env;                              //    + global lighting and fog
    ter::Camera cam;                                   //    + a camera
    app.setScene(*scene, cam, env);

    app.onUi = [&](ter::Canvas2D& g, const ter::Frame& f) { /* ... */ };   // 4. (optional) 2D UI

    return app.run([&](ter::Frame& f) {               // 5. your game loop, once per frame
        // read app.input(), move things, play app.audio() ...
    });
}
```

Each frame, the engine runs: **poll input → your update → audio → draw the 3D scene → post-processing → UI → overlay → show.**

Conventions to remember:

- **Coordinates:** Y is up, **−Z is forward**, 1 unit = 1 meter (same as three.js).
- **Colors:** `ter::hexc(0xRRGGBB)`. Treat them like three.js colors; the renderer handles sRGB at the end.
- **Angles** are in radians (`ter::PI` is provided).
- **Ownership:** scene objects are `std::shared_ptr`s (`ter::ObjPtr`). The scene, camera and environment you pass to `setScene` must stay alive while `run()` is running. Declaring them in `main()` is enough.
- **Namespace:** everything is in `ter::`. Include `<ter/ter.h>` for all of it, or individual headers such as `<ter/scene/object.h>`.

---

## Documentation

| Guide | You'll learn |
|---|---|
| **[Tutorial: your first game](docs/tutorial.md)** | Step by step from an empty window to a finished mini-game. **Start here.** |
| [App and the game loop](docs/app.md) | `AppConfig`, `Frame`, `run`, window and cursor, screenshots |
| [Scene, meshes and materials](docs/scene.md) | `Object3D` tree, geometry, materials, instancing, sprites |
| [Lighting and post-processing](docs/lighting.md) | Ambient / sun / point / spot lights, shadows, fog, vignette, grain |
| [Input](docs/input.md) | Keyboard, mouse, gamepad, text input, simulated keys |
| [First-person camera and collision](docs/first-person.md) | Walking, mouse look, walls, "what am I looking at" raycasts |
| [UI and overlay](docs/ui.md) | Canvas2D (Thai text, shapes, gradients), fades and flashes |
| [Audio](docs/audio.md) | Sound files, music streaming, 3D sound, synthesized placeholders |
| [Models and textures](docs/models-and-textures.md) | glTF / glb, animations, PNG / JPEG / WebP, textures drawn in code |
| [Custom shaders](docs/shaders.md) | Writing GLSL materials, overriding built-in shaders, hot reload |
| [Testing with screenshots](docs/testing.md) | `--shot`, simulated input, checking scenes without playing |
| [Shipping your game](docs/shipping.md) | Packaging a `.app`, the upload zip, what players need to do, release checklist |

---

## Limitations

Know these before you start a project:

- **macOS only.** The engine uses ImageIO (images), CoreText (text) and OpenGL 4.1. Apple has deprecated OpenGL but it still works; the warnings are silenced.
  A Windows/Linux port would need `gfx/image.mm` → stb_image, `ui/canvas2d.mm` → FreeType + HarfBuzz, and `gfx/gl.h` → glad.
- **Not notarized.** Apps are ad-hoc signed, so players have to approve the game once in macOS's security settings. Removing that step requires a paid Apple Developer ID (see [Shipping](docs/shipping.md)).
- **No physics.** Collision is circles against axis-aligned boxes (enough for walking around rooms), plus ray-vs-box picking.
- **Only the sun casts shadows.** Point and spot lights light the scene but don't cast shadows yet.
- **Up to 16 point/spot lights** at once (the ones closest to the camera are used).
- **No editor or asset pipeline.** Scenes are built in code. For anything complex, model it in Blender and load a `.glb`.
- **Version 0.1.** The API may still change.

---

## Third-party libraries

Downloaded automatically by CMake:

| Library | Used for | License |
|---|---|---|
| [GLFW](https://www.glfw.org/) | Window, input, gamepad | zlib |
| [glm](https://github.com/g-truc/glm) | Vector and matrix math | MIT |
| [cgltf](https://github.com/jkuhlmann/cgltf) | glTF parsing | MIT |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | Compressed glTF meshes | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON | MIT |
| [miniaudio](https://miniaud.io/) | Audio playback and decoding | Public domain / MIT-0 |

Bundled fonts (in `fonts/`, SIL Open Font License, see `fonts/OFL-*.txt`): Mali, Inter, Cormorant Garamond.
