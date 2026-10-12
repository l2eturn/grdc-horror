# App and the game loop

[← Back to README](../README.md) · Header: `ter/app/app.h`

`ter::App` is the heart of every program. It opens the window, owns the renderer, input, audio and UI, and runs your update function once per frame.

---

## Creating the app

```cpp
ter::AppConfig cfg;
cfg.title = "My Game";
cfg.width = 1280;          // window size in points (on Retina the framebuffer is 2x larger)
cfg.height = 720;
cfg.resizable = true;
cfg.vsync = true;
cfg.fullscreen = false;
cfg.pixelScale = 1;        // 1 = sharp, 2-3 = chunky pixel-art look

ter::App app(cfg, argc, argv);
if (!app.ok()) return 1;   // window or shaders failed (the reason is printed to stderr)
```

Because `AppConfig` is a plain struct, you can also write it inline with designated initializers. Keep the fields in the order they are declared:

```cpp
ter::App app({.title = "My Game", .width = 960, .height = 600, .pixelScale = 2}, argc, argv);
```

Passing `argc, argv` lets the app read these command-line flags:

| Flag | Effect |
|---|---|
| `--shot out.png` | Save a screenshot after a short delay, then quit (see [Testing](testing.md)) |
| `--shot-delay 2` | Seconds to wait before `--shot` (default 0.5) |
| `--pixel 3` | Override `pixelScale` |
| `--fullscreen` | Start fullscreen |

Use `ter::App app(cfg)` if you don't want command-line flags.

---

## The loop

```cpp
app.setScene(*scene, camera, env);   // what to draw (must outlive run())

return app.run([&](ter::Frame& f) {
    // runs once per frame, BEFORE drawing
    if (app.input().pressed(ter::Key::Escape)) app.quit();
});
```

`run()` returns `0` when the window closes or you call `app.quit()`, so `return app.run(...)` is the usual last line of `main`.

### `ter::Frame`

| Field | Meaning |
|---|---|
| `f.dt` | Seconds since the previous frame. **Capped at 0.1** so a hitch doesn't teleport things. |
| `f.time` | Seconds since `run()` started |
| `f.index` | Frame counter |
| `f.width`, `f.height` | Window size in **points**. Use these for UI and mouse positions. |
| `f.fbWidth`, `f.fbHeight` | Framebuffer size in **pixels** |
| `f.scale` | Pixels per point (2 on Retina) |

Multiply every speed by `f.dt`: `obj->position.x += 2.0f * f.dt;` moves 2 m per second at any frame rate.

### What happens each frame

1. Poll input
2. **Your update function**
3. Audio update (the listener moves to the camera)
4. Draw the 3D scene
5. Post-processing (vignette, color, grain; see [Lighting](lighting.md))
6. UI canvas (`onUi`, see [UI](ui.md))
7. Overlay quads (`overlay()`, see [UI](ui.md))
8. Present

Because your update runs first, anything you change in it shows up in the same frame.

---

## Other `App` functions

| Function | Use |
|---|---|
| `app.setScene(scene, cam, env)` | Choose what to draw. Can be changed at any time, e.g. to switch levels. |
| `app.clearScene()` | Draw no 3D scene; the window is cleared to `renderer().background` (menus, title screens) |
| `app.renderer()` | Image settings: background, vignette, grain, shadows ... |
| `app.input()` | This frame's input (see [Input](input.md)) |
| `app.audio()` | The audio system. It starts on first call. (see [Audio](audio.md)) |
| `app.onUi`, `app.redrawUi()` | 2D UI layer (see [UI](ui.md)) |
| `app.overlay()` | Per-frame rectangles and images on top of everything |
| `app.lockCursor(true / false)`, `app.cursorLocked()` | Hide and capture the mouse for first-person games |
| `app.screenshot("a.png")` | Save the next frame as PNG |
| `app.setTitle("...")` | Change the window title |
| `app.frame()` | The current `Frame` (from outside the update function) |
| `app.listenerFollowsCamera` | `true` by default; set `false` to place the audio listener yourself |
| `app.window()` | The raw `GLFWwindow*` if you need GLFW directly |
| `app.quit()` | Stop after this frame |

---

## Pattern: game states

`run()` takes a single function, so use a variable for menus, play and game-over screens:

```cpp
enum class State { Title, Playing, GameOver };
State state = State::Title;

return app.run([&](ter::Frame& f) {
    switch (state) {
    case State::Title:
        if (app.input().pressed(ter::Key::Enter)) {
            state = State::Playing;
            app.setScene(*scene, cam, env);
            app.redrawUi();
        }
        break;
    case State::Playing:
        updateGame(f);
        if (playerDied) state = State::GameOver;
        break;
    case State::GameOver:
        // ...
        break;
    }
});
```
