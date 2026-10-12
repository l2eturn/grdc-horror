# Testing with screenshots

[← Back to README](../README.md)

Every program that creates its `App` with `argc, argv` can take a screenshot and quit by itself. That lets you check a scene, from a script or a CI job, without sitting down to play.

Games built with `ter_add_game` are app bundles. Pass flags to the executable inside the bundle (here `./build/mygame` stands for `"./build/My Game.app/Contents/MacOS/My Game"`):

```sh
./build/mygame --shot out.png                   # run ~0.5 s, save out.png, quit
./build/mygame --shot out.png --shot-delay 3    # wait 3 seconds first (let fades/animations finish)
./build/mygame --pixel 3 --shot chunky.png      # combine with other flags
```

The screenshot contains everything on screen: the 3D scene, post-processing, UI and overlay. Keep in mind:

- **Fades and intros:** if the game fades in from black, a short delay gives a black image. `f.dt` is capped at 0.1 s and the first frames are slower while things load, so a fade can take longer than its wall-clock time. Use a longer `--shot-delay`.
- **The window still opens.** This is not headless rendering; it needs a logged-in macOS session.

## Screenshots from code

```cpp
if (in.pressed(ter::Key::F12)) app.screenshot("shot.png");   // saves the next frame
```

## Simulating input

To check movement or interaction, fake key presses and take the shot later:

```cpp
// walk forward for the first second, then stop
if (f.time < 1.0f) app.input().simulate(ter::Key::W, true);
else app.input().simulate(ter::Key::W, false);
```

```sh
./build/mygame --shot after-walk.png --shot-delay 1.5
```

A simulated key is held from the next frame until you release it, and `pressed()` fires once, just like a real press.

## A quick check loop

```sh
cmake --build build -j && ./build/mygame --shot /tmp/check.png --shot-delay 2 && open /tmp/check.png
```
