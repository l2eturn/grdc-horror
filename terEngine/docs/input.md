# Input

[← Back to README](../README.md) · Header: `ter/app/input.h`

Get the input for the current frame with `app.input()`. It is updated once per frame, before your update function runs.

```cpp
ter::Input& in = app.input();
```

---

## Keyboard

```cpp
in.down(ter::Key::W)          // held right now
in.pressed(ter::Key::E)       // went down this frame (true once per press)
in.released(ter::Key::Space)  // went up this frame
```

Use `pressed` for actions (open a door, jump, toggle a light) and `down` for continuous things (walking, aiming).

Keys: `A`–`Z`, `Num0`–`Num9`, `F1`–`F12`, `Space`, `Enter`, `Escape`, `Tab`, `Backspace`, arrows (`Up`, `Down`, `Left`, `Right`), `LeftShift`, `LeftControl`, `LeftAlt`, `LeftSuper` (and the `Right*` versions), punctuation, etc.
Any GLFW key code also works: `ter::Key(GLFW_KEY_KP_ADD)`.

## Mouse

```cpp
in.down(ter::Mouse::Left)        // also Right, Middle; pressed() / released() work too
in.mouse()                       // cursor position in points, y down (same units as Frame::width/height)
in.mouseDelta()                  // movement since the last frame; keeps working while the cursor is locked
in.scroll()                      // wheel / trackpad this frame (+ = up)
```

For first-person games, capture the cursor so it can't leave the window:

```cpp
if (in.pressed(ter::Mouse::Left)) app.lockCursor(true);
if (in.pressed(ter::Key::Escape)) app.lockCursor(false);
```

## Movement axis

```cpp
glm::vec2 m = in.moveAxis();   // WASD, arrow keys and left stick combined; x = right, y = forward; length <= 1
```

## Gamepad

Xbox button layout; PlayStation and Switch pads work through GLFW's mappings.

```cpp
if (in.gamepad()) {                       // is one connected?
    in.down(ter::Pad::A);                 // A, B, X, Y, LeftBumper, RightBumper, Back, Start, Guide,
    in.pressed(ter::Pad::Start);          // LeftThumb, RightThumb, DpadUp/Right/Down/Left
    glm::vec2 l = in.leftStick();         // -1..1, y down = +1, small dead zone applied
    glm::vec2 r = in.rightStick();
    float t = in.rightTrigger();          // 0..1, also leftTrigger()
}
```

`FirstPersonCamera` already handles both sticks (see [First-person camera](first-person.md)).

## Text input

`in.text()` returns the characters typed this frame as UTF-8, including Thai and other input methods. Use it for name entry or typed-answer puzzles:

```cpp
std::string answer;
answer += in.text();
if (in.pressed(ter::Key::Backspace) && !answer.empty()) answer.pop_back();   // careful: removes one byte, not one Thai character
if (!in.text().empty()) app.redrawUi();
```

## Simulated input (testing)

```cpp
in.simulate(ter::Key::W, true);    // from the next frame W is held (and "pressed" once)
in.simulate(ter::Key::W, false);   // release
```

Combine with `--shot` to check gameplay automatically (see [Testing](testing.md)).
