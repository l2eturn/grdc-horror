#pragma once
// Keyboard / mouse / gamepad state for one frame. Values of Key, Mouse and Pad are the GLFW codes,
// so any GLFW_KEY_* constant also works (cast to Key).
#include <array>
#include <string>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace ter {

enum class Key : int {
    Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Semicolon = 59, Equal = 61,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    LeftBracket = 91, Backslash = 92, RightBracket = 93, GraveAccent = 96,
    Escape = 256, Enter, Tab, Backspace, Insert, Delete, Right, Left, Down, Up, PageUp, PageDown, Home, End,
    CapsLock = 280, ScrollLock, NumLock, PrintScreen, Pause,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift = 340, LeftControl, LeftAlt, LeftSuper, RightShift, RightControl, RightAlt, RightSuper,
};

enum class Mouse : int { Left = 0, Right = 1, Middle = 2 };

// Xbox layout (GLFW gamepad mapping): A bottom, B right, X left, Y top
enum class Pad : int {
    A = 0, B, X, Y, LeftBumper, RightBumper, Back, Start, Guide, LeftThumb, RightThumb, DpadUp, DpadRight, DpadDown, DpadLeft,
};

class Input {
public:
    // held right now
    bool down(Key k) const { return state(keys_, (int)k) & kDown; }
    // went down since the last frame (true once per press, even for very short taps)
    bool pressed(Key k) const { return state(keys_, (int)k) & kPressed; }
    // went up since the last frame
    bool released(Key k) const { return state(keys_, (int)k) & kReleased; }

    bool down(Mouse b) const { return state(mouse_, (int)b) & kDown; }
    bool pressed(Mouse b) const { return state(mouse_, (int)b) & kPressed; }
    bool released(Mouse b) const { return state(mouse_, (int)b) & kReleased; }
    // cursor in window points (y down, same units as AppConfig width / height)
    glm::vec2 mouse() const { return mousePos_; }
    // cursor movement since the last frame (keeps working while the cursor is locked)
    glm::vec2 mouseDelta() const { return mouseDelta_; }
    // wheel / trackpad scroll this frame (+ = up)
    float scroll() const { return scroll_; }
    // characters typed this frame, UTF-8 (for text boxes; handles Thai input)
    const std::string& text() const { return text_; }

    // a gamepad is connected
    bool gamepad() const { return padConnected_; }
    bool down(Pad b) const { return state(pad_, (int)b) & kDown; }
    bool pressed(Pad b) const { return state(pad_, (int)b) & kPressed; }
    // sticks in -1..1 (y down = +1, like GLFW) with a small dead zone; triggers 0..1
    glm::vec2 leftStick() const { return leftStick_; }
    glm::vec2 rightStick() const { return rightStick_; }
    float leftTrigger() const { return leftTrigger_; }
    float rightTrigger() const { return rightTrigger_; }

    // WASD / arrows / left stick as one vector: x = right, y = forward, length <= 1
    glm::vec2 moveAxis() const;

    // Fake a key for scripted tests: from the next frame it is down (and pressed once) until simulate(k, false)
    void simulate(Key k, bool isDown);

private:
    friend class App;
    static constexpr uint8_t kDown = 1, kPressed = 2, kReleased = 4;
    template <size_t N>
    static uint8_t state(const std::array<uint8_t, N>& a, int i) {
        return i >= 0 && i < (int)N ? a[i] : 0;
    }
    // GLFW callbacks land here
    void onKey(int key, int action);
    void onMouseButton(int button, int action);
    // before glfwPollEvents: forget last frame's edges, scroll and text
    void beginFrame();
    // after glfwPollEvents: read the cursor and the gamepad
    void endPoll(GLFWwindow* win);

    std::array<uint8_t, 512> keys_{};
    std::array<uint8_t, 8> mouse_{};
    std::array<uint8_t, 16> pad_{};
    glm::vec2 mousePos_{0}, mouseDelta_{0}, leftStick_{0}, rightStick_{0};
    float scroll_ = 0, leftTrigger_ = 0, rightTrigger_ = 0;
    std::string text_;
    // simulated key changes, applied at the start of the next frame so their edges are seen
    std::vector<std::pair<int, bool>> simulated_;
    bool padConnected_ = false, firstMouse_ = true;
};

}  // namespace ter
