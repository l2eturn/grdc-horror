# Tutorial: your first game

In this tutorial you build **"Find the Box"**: you wake up in a dark room with a flashlight. Somewhere a box is humming. Follow the sound, find the box, press **E** to take it.

By the end you will have used every main part of terEngine: window and game loop, meshes, lights, the first-person camera, collision, 3D sound, UI text and screen fades. The finished program is about 120 lines.

Each step builds on the previous one and compiles on its own. Build and run after every step.

**Before you start:** you need a project set up as described in [Use terEngine in your own game](../README.md#use-terengine-in-your-own-game): a `CMakeLists.txt` that links `ter::engine`, and an empty `src/main.cpp`.

---

## Step 1 — Open a window

```cpp
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "My Game"}, argc, argv);
    if (!app.ok()) return 1;

    app.renderer().background = ter::hexc(0x203040);

    return app.run([&](ter::Frame& f) {
        if (app.input().pressed(ter::Key::Escape)) app.quit();
    });
}
```

```sh
cmake --build build -j && "./build/My Game.app/Contents/MacOS/My Game"
```

(`ter_add_game` builds an app bundle; the executable is inside it. `open "build/My Game.app"` also works, but then you won't see error messages in the terminal.)

You should see a dark blue window. **Esc** closes it.

What's going on:

- `ter::App` opens the window and owns everything else: renderer, input, audio, UI. The `{.title = ...}` is an `AppConfig` (window size, fullscreen, pixel scale, ...). See [App and the game loop](app.md).
- Passing `argc, argv` gives you free command-line flags such as `--shot out.png` (see [Testing](testing.md)).
- `app.run(...)` calls your lambda **once per frame** until the window closes or you call `app.quit()`. `f.dt` is the time since the last frame in seconds. Multiply speeds by it so movement does not depend on frame rate.
- `pressed()` is true only on the frame the key goes down. `down()` is true for as long as it's held.

---

## Step 2 — Put something in the world

A scene is a tree of objects, just like three.js. A **mesh** = **geometry** (shape) + **material** (surface). Replace `main` with:

```cpp
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "My Game"}, argc, argv);
    if (!app.ok()) return 1;

    auto scene = ter::makeGroup();

    // a 20 x 20 m floor: planes are created standing up, so lay it flat
    auto floor = ter::makeMesh(ter::geo::plane(20, 20), ter::lambertColor(ter::hexc(0x556b4f)));
    floor->rotation.x = -ter::PI / 2;
    floor->receiveShadow = true;
    scene->add(floor);

    // a 1 m cube standing on the floor
    auto cube = ter::makeMesh(ter::geo::box(1, 1, 1), ter::standard(ter::hexc(0xff8a3d), 0.5f, 0.0f));
    cube->position = {0, 0.5f, 0};
    cube->castShadow = true;
    scene->add(cube);

    // light: soft sky/ground ambient + a sun that casts shadows
    ter::Environment env;
    env.hemiSky = ter::hexc(0xbfd9ff);
    env.hemiGround = ter::hexc(0x4a3c2a);
    env.hemiIntensity = 0.6f;
    env.sunIntensity = 1.2f;
    env.sunPosition = {4, 8, 3};

    ter::Camera cam;
    cam.fov = 50;
    cam.position = {3, 2.5f, 4};
    cam.lookAt({0, 0.5f, 0});

    app.setScene(*scene, cam, env);

    return app.run([&](ter::Frame& f) {
        cube->rotation.y += f.dt;   // one radian per second
        if (app.input().pressed(ter::Key::Escape)) app.quit();
    });
}
```

You should see an orange cube turning on a green floor, casting a shadow.

What's going on:

- `ter::makeMesh(geometry, material)` returns a `ter::ObjPtr` (a `shared_ptr`). Set `position`, `rotation` (radians) and `scale`, then `add` it to a parent.
- `lambertColor` is a cheap matte material. `standard(color, roughness, metalness)` is PBR. See [Scene, meshes and materials](scene.md).
- Without light everything is black. `Environment` holds the **ambient** light (`hemi*`), one **sun** (the only light that casts shadows) and **fog**. See [Lighting](lighting.md).
- `app.setScene(*scene, cam, env)` tells the app what to draw. These three variables must outlive `run()`. Since they live in `main()`, they do.

---

## Step 3 — Walk around

Now replace the orbiting view with a first-person player, and turn the cube into walls you can't walk through.

```cpp
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "My Game"}, argc, argv);
    if (!app.ok()) return 1;

    auto scene = ter::makeGroup();
    std::vector<ter::Box> walls;   // collision boxes

    auto floor = ter::makeMesh(ter::geo::plane(20, 20), ter::lambertColor(ter::hexc(0x556b4f)));
    floor->rotation.x = -ter::PI / 2;
    floor->receiveShadow = true;
    scene->add(floor);

    // helper: a visible box AND a matching collider
    auto wallMat = ter::lambertColor(ter::hexc(0x8a8f99));
    auto addWall = [&](glm::vec3 center, glm::vec3 size) {
        auto mesh = ter::makeMesh(ter::geo::box(size.x, size.y, size.z), wallMat);
        mesh->position = center;
        mesh->castShadow = mesh->receiveShadow = true;
        scene->add(mesh);
        walls.push_back(ter::Box::centered(center, size));
    };
    addWall({0, 1.5f, -6}, {12, 3, 0.3f});   // north
    addWall({0, 1.5f, 6}, {12, 3, 0.3f});    // south
    addWall({-6, 1.5f, 0}, {0.3f, 3, 12});   // west
    addWall({6, 1.5f, 0}, {0.3f, 3, 12});    // east
    addWall({0, 0.5f, 0}, {1, 1, 1});        // a crate in the middle

    ter::Environment env;
    env.hemiSky = ter::hexc(0xbfd9ff);
    env.hemiGround = ter::hexc(0x4a3c2a);
    env.hemiIntensity = 0.6f;
    env.sunIntensity = 1.2f;
    env.sunPosition = {4, 8, 3};

    ter::Camera cam;
    cam.fov = 70;
    ter::FirstPersonCamera player;
    player.position = {0, 1.6f, 4};   // eye height 1.6 m, looking toward -Z
    player.collide = [&](glm::vec3, glm::vec3 to) { return ter::resolveCircle(to, 0.3f, walls); };
    player.apply(cam);

    app.setScene(*scene, cam, env);

    return app.run([&](ter::Frame& f) {
        ter::Input& in = app.input();
        // click to capture the mouse; Esc releases it, a second Esc quits
        if (in.pressed(ter::Mouse::Left)) app.lockCursor(true);
        if (in.pressed(ter::Key::Escape)) {
            if (app.cursorLocked()) app.lockCursor(false);
            else app.quit();
        }
        player.update(in, f.dt, app.cursorLocked());
        player.apply(cam);
    });
}
```

**Click** in the window, then use **WASD** to walk, the **mouse** to look and **Shift** to run. You can't walk through the walls or the crate.

What's going on:

- `FirstPersonCamera` reads WASD / mouse / gamepad and moves an eye position. `apply(cam)` copies the result into the `Camera` that is rendered.
- `player.collide` is called with where the player wants to go and returns where they may go. `resolveCircle` treats the player as a 0.3 m circle and pushes it out of every `Box`.
- The mesh and the collider are separate things. The `addWall` helper keeps them in sync. This "mesh + box" pattern is how you build levels in terEngine.

More in [First-person camera and collision](first-person.md).

---

## Step 4 — Turn off the lights

Time for atmosphere. We remove the sun, make the ambient light very dim, add black fog, and give the player a flashlight: a **spot light** that follows the camera.

Starting from step 3, make these changes:

**a)** Right after `if (!app.ok()) return 1;`, set up a darker image and add `pixelScale` to the config:

```cpp
ter::App app({.title = "Find the Box", .pixelScale = 2}, argc, argv);
if (!app.ok()) return 1;

ter::Renderer& r = app.renderer();
r.background = ter::hexc(0x020203);
r.vignette = 1.2f;   // dark corners
r.grain = 0.06f;     // film noise
```

**b)** Replace the `Environment` block with a dark one:

```cpp
ter::Environment env;
env.hemiSky = ter::hexc(0x20242c);
env.hemiGround = ter::hexc(0x050505);
env.hemiIntensity = 0.25f;
env.fogColor = r.background;   // fog fades into the background color
env.fogNear = 2;
env.fogFar = 12;
```

**c)** After the walls, create the flashlight:

```cpp
// color, intensity, range (m), cone half-angle (rad), edge softness
auto flashlight = ter::makeSpotLight(ter::hexc(0xfff2d8), 3.0f, 14, 0.42f, 0.5f);
scene->add(flashlight);
```

**d)** In the loop, after `player.apply(cam);`, move it with the player:

```cpp
// hold it slightly right of and below the eyes, pointing where we look
flashlight->position = player.position + player.right() * 0.15f - glm::vec3(0, 0.15f, 0);
flashlight->lookAt(player.position + player.forward() * 5.0f);
```

Run it. The room is now pitch black except for the flashlight cone, and walls disappear into the fog.

- `pixelScale = 2` renders at half resolution and scales up with sharp pixels, which gives a lo-fi look and is faster. Try `--pixel 3` on the command line.
- Spot lights shine along their local +Z axis. `lookAt(target)` aims them.
- Point and spot lights don't cast shadows; only the sun does. In a dark scene nobody notices.

See [Lighting and post-processing](lighting.md) for every setting.

---

## Step 5 — Something is humming

Sound is how the player finds the goal. We place a glowing box in a corner and attach a looping **3D sound** to it. It gets louder as you approach and moves left/right in your headphones as you turn.

**a)** After the walls, add the goal. It also gets a collider, because we will raycast against it in the next step:

```cpp
auto boxMat = ter::lambertColor(ter::hexc(0xd8c9a0));
boxMat->emissive = ter::hexc(0x3a2a10);   // faint glow, visible without light
auto goal = ter::makeMesh(ter::geo::box(0.5f, 0.5f, 0.5f), boxMat);
goal->position = {4.5f, 0.25f, -4.5f};
scene->add(goal);
walls.push_back(ter::Box::centered(goal->position, {0.5f, 0.5f, 0.5f}));
int goalIndex = (int)walls.size() - 1;
```

**b)** After `app.setScene(...)`, create the sounds. We don't have audio files yet, so we **synthesize placeholders**. Later you can swap them for real recordings without changing anything else:

```cpp
ter::Audio& audio = app.audio();
audio.synth("step", {.wave = ter::Wave::Noise, .duration = 0.09f, .volume = 0.12f});
audio.synth("hum", {.wave = ter::Wave::Sine, .freq = 110, .duration = 2, .attack = 0.6f, .volume = 0.25f, .vibrato = 0.3f});
audio.synth("win", {.wave = ter::Wave::Triangle, .freq = 440, .freqEnd = 880, .duration = 0.8f});

// a looping hum placed at the goal
auto hum = audio.play("hum", {.loop = true, .spatial = true, .position = goal->position, .maxDistance = 14});
```

**c)** Footsteps: `player.update` returns how far the player moved. Play a step sound every 0.7 m. Declare `float walked = 0;` before `app.run`, then change the update line in the loop:

```cpp
walked += player.update(in, f.dt, app.cursorLocked());
player.apply(cam);
if (walked > 0.7f) {
    walked = 0;
    audio.play("step", {.pitch = 0.8f + ter::frand() * 0.4f});   // vary the pitch so steps don't sound robotic
}
```

Put on headphones and run it. Turn around slowly and the hum moves from ear to ear. Walk toward it and it gets louder.

- `audio.play(name or path, options)` returns a `ter::SoundPtr`. Keep it if you want to control the sound later (we will fade `hum` out). Ignore it for fire-and-forget sounds like footsteps.
- `.spatial = true` + `.position` makes a 3D sound. The listener ("ears") follows the camera automatically.
- To use a real file: `audio.play(ter::assetPath("sfx/hum.wav"), {...})`.

See [Audio](audio.md).

---

## Step 6 — Win condition, text and a fade-in

Last step: show hints on screen, detect when the player looks at the box and presses **E**, and fade in from black at the start.

**a)** Before `app.run`, set up the UI. `onUi` draws with an HTML-canvas-style API. It runs **only when you call `app.redrawUi()`**, so text that doesn't change costs nothing:

```cpp
std::string message = "Click to start. Follow the hum.";
app.onUi = [&](ter::Canvas2D& g, const ter::Frame& f) {
    g.setFont("Inter", 20, 500);
    g.setTextAlign(ter::TextAlign::Center);
    g.setFill("rgba(230,220,200,0.9)");
    g.fillText(message, f.width / 2.0f, f.height - 40.0f);
};

bool won = false;
float fade = 1;
```

**b)** At the end of the loop body, add:

```cpp
// what is in front of us, within 2 m?
int hit = ter::raycast(player.position, player.forward(), walls, 2.0f);

std::string want = won ? "You found it. Press Esc to quit."
                 : hit == goalIndex ? "Press E to take the box"
                 : app.cursorLocked() ? "" : "Click to start. Follow the hum.";

if (!won && hit == goalIndex && in.pressed(ter::Key::E)) {
    won = true;
    goal->visible = false;
    hum->fadeTo(0, 1.5f, true);   // fade the hum out over 1.5 s, then stop it
    audio.play("win");
}
// only redraw the UI when the text actually changes
if (want != message) {
    message = want;
    app.redrawUi();
}

// fade in from black over the first 2 seconds
fade = std::max(0.0f, fade - f.dt * 0.5f);
if (fade > 0) app.overlay().fill({0, 0, 0, fade});
```

Run it, find the box and take it. **That's a complete game.**

- `ter::raycast` returns the index of the nearest `Box` the ray hits (or −1). Because the goal's collider is in `walls`, comparing with `goalIndex` tells us the player is looking at it.
- `app.overlay()` draws rectangles/images **on top of everything, including the UI**, and must be called every frame. It is the tool for fades, damage flashes and health bars. The UI canvas is for text and anything that changes rarely.
- `"Inter"` is one of the bundled fonts. `"Mali"` supports Thai: try `message = "หาเสียงให้เจอ";`.

See [UI and overlay](ui.md).

---

## The complete program

```cpp
// "Find the Box": the complete tiny game from the terEngine tutorial
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "Find the Box", .pixelScale = 2}, argc, argv);
    if (!app.ok()) return 1;

    // a dark, grainy look
    ter::Renderer& r = app.renderer();
    r.background = ter::hexc(0x020203);
    r.vignette = 1.2f;
    r.grain = 0.06f;

    // ---------- scene ----------
    auto scene = ter::makeGroup();
    std::vector<ter::Box> walls;

    auto floor = ter::makeMesh(ter::geo::plane(20, 20), ter::lambertColor(ter::hexc(0x3a3430)));
    floor->rotation.x = -ter::PI / 2;
    scene->add(floor);

    auto wallMat = ter::lambertColor(ter::hexc(0x6b6f78));
    auto addWall = [&](glm::vec3 center, glm::vec3 size) {
        auto mesh = ter::makeMesh(ter::geo::box(size.x, size.y, size.z), wallMat);
        mesh->position = center;
        scene->add(mesh);
        walls.push_back(ter::Box::centered(center, size));
    };
    addWall({0, 1.5f, -6}, {12, 3, 0.3f});
    addWall({0, 1.5f, 6}, {12, 3, 0.3f});
    addWall({-6, 1.5f, 0}, {0.3f, 3, 12});
    addWall({6, 1.5f, 0}, {0.3f, 3, 12});
    addWall({-2, 1.5f, 0}, {0.6f, 3, 0.6f});
    addWall({2, 1.5f, -2}, {0.6f, 3, 0.6f});

    // the goal: a faintly glowing box in a corner
    auto boxMat = ter::lambertColor(ter::hexc(0xd8c9a0));
    boxMat->emissive = ter::hexc(0x3a2a10);
    auto goal = ter::makeMesh(ter::geo::box(0.5f, 0.5f, 0.5f), boxMat);
    goal->position = {4.5f, 0.25f, -4.5f};
    scene->add(goal);
    walls.push_back(ter::Box::centered(goal->position, {0.5f, 0.5f, 0.5f}));
    int goalIndex = (int)walls.size() - 1;

    // the flashlight follows the player every frame
    auto flashlight = ter::makeSpotLight(ter::hexc(0xfff2d8), 3.0f, 14, 0.42f, 0.5f);
    scene->add(flashlight);

    ter::Environment env;
    env.hemiSky = ter::hexc(0x20242c);
    env.hemiGround = ter::hexc(0x050505);
    env.hemiIntensity = 0.25f;
    env.fogColor = r.background;
    env.fogNear = 2;
    env.fogFar = 12;

    // ---------- player ----------
    ter::Camera cam;
    cam.fov = 70;
    cam.near = 0.05f;
    ter::FirstPersonCamera player;
    player.position = {-4.5f, 1.6f, 4.5f};
    player.collide = [&](glm::vec3, glm::vec3 to) { return ter::resolveCircle(to, 0.3f, walls); };
    player.apply(cam);
    app.setScene(*scene, cam, env);

    // ---------- sound ----------
    ter::Audio& audio = app.audio();
    audio.synth("step", {.wave = ter::Wave::Noise, .duration = 0.09f, .volume = 0.12f});
    audio.synth("hum", {.wave = ter::Wave::Sine, .freq = 110, .duration = 2, .attack = 0.6f, .volume = 0.25f, .vibrato = 0.3f});
    audio.synth("win", {.wave = ter::Wave::Triangle, .freq = 440, .freqEnd = 880, .duration = 0.8f});
    auto hum = audio.play("hum", {.loop = true, .spatial = true, .position = goal->position, .maxDistance = 14});

    // ---------- UI ----------
    std::string message = "Click to start. Follow the hum.";
    app.onUi = [&](ter::Canvas2D& g, const ter::Frame& f) {
        g.setFont("Inter", 20, 500);
        g.setTextAlign(ter::TextAlign::Center);
        g.setFill("rgba(230,220,200,0.9)");
        g.fillText(message, f.width / 2.0f, f.height - 40.0f);
    };

    // ---------- game loop ----------
    bool won = false;
    float walked = 0, fade = 1;
    return app.run([&](ter::Frame& f) {
        ter::Input& in = app.input();
        if (in.pressed(ter::Mouse::Left)) app.lockCursor(true);
        if (in.pressed(ter::Key::Escape)) {
            if (app.cursorLocked()) app.lockCursor(false);
            else app.quit();
        }

        walked += player.update(in, f.dt, app.cursorLocked());
        player.apply(cam);
        if (walked > 0.7f) {
            walked = 0;
            audio.play("step", {.pitch = 0.8f + ter::frand() * 0.4f});
        }

        flashlight->position = player.position + player.right() * 0.15f - glm::vec3(0, 0.15f, 0);
        flashlight->lookAt(player.position + player.forward() * 5.0f);

        int hit = ter::raycast(player.position, player.forward(), walls, 2.0f);
        std::string want = won ? "You found it. Press Esc to quit."
                         : hit == goalIndex ? "Press E to take the box"
                         : app.cursorLocked() ? "" : "Click to start. Follow the hum.";
        if (!won && hit == goalIndex && in.pressed(ter::Key::E)) {
            won = true;
            goal->visible = false;
            hum->fadeTo(0, 1.5f, true);
            audio.play("win");
        }
        if (want != message) {
            message = want;
            app.redrawUi();
        }

        fade = std::max(0.0f, fade - f.dt * 0.5f);
        if (fade > 0) app.overlay().fill({0, 0, 0, fade});
    });
}
```

---

## Where to go next

Ideas to extend the game, each using one more part of the engine:

| Idea | Look at |
|---|---|
| A flickering flashlight and an **F** key to toggle it | `examples/02_dark_room.cpp` |
| Replace the synth sounds with real recordings | [Audio](audio.md) — `audio.play(ter::assetPath("sfx/hum.wav"), ...)` |
| A ghost model from Blender that drifts toward you | [Models and textures](models-and-textures.md), `loadGltf` + `AnimationMixer` |
| A red flash and a timer bar | [UI and overlay](ui.md) — `overlay().fill`, `overlay().rect` |
| A note on the wall with readable text | [UI and overlay](ui.md) — draw a `Canvas2D`, `upload` it into a texture, put it on a `plane` |
| A wobbly, transparent ghost shader | [Custom shaders](shaders.md) |
| Check your scene automatically after each change | [Testing with screenshots](testing.md) |
