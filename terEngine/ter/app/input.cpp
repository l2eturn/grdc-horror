#include "ter/app/input.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace ter {

namespace {
// ignore small stick drift
glm::vec2 deadZone(float x, float y) {
    glm::vec2 v(x, y);
    float len = glm::length(v);
    if (len < 0.18f) return glm::vec2(0);
    return v / len * std::min(1.0f, (len - 0.18f) / 0.82f);
}

void setButton(uint8_t& s, bool isDown, uint8_t kDown, uint8_t kPressed, uint8_t kReleased) {
    bool was = s & kDown;
    if (isDown && !was) s |= kDown | kPressed;
    if (!isDown && was) s = (uint8_t)((s & ~kDown) | kReleased);
}
}  // namespace

glm::vec2 Input::moveAxis() const {
    glm::vec2 v(0);
    if (down(Key::W) || down(Key::Up)) v.y += 1;
    if (down(Key::S) || down(Key::Down)) v.y -= 1;
    if (down(Key::D) || down(Key::Right)) v.x += 1;
    if (down(Key::A) || down(Key::Left)) v.x -= 1;
    if (padConnected_) v += glm::vec2(leftStick_.x, -leftStick_.y);
    float len = glm::length(v);
    return len > 1 ? v / len : v;
}

void Input::simulate(Key k, bool isDown) { simulated_.push_back({(int)k, isDown}); }

void Input::onKey(int key, int action) {
    if (key < 0 || key >= (int)keys_.size() || action == GLFW_REPEAT) return;
    setButton(keys_[key], action == GLFW_PRESS, kDown, kPressed, kReleased);
}

void Input::onMouseButton(int button, int action) {
    if (button < 0 || button >= (int)mouse_.size()) return;
    setButton(mouse_[button], action == GLFW_PRESS, kDown, kPressed, kReleased);
}

void Input::beginFrame() {
    auto clearEdges = [](auto& states) {
        for (auto& s : states) s &= kDown;
    };
    clearEdges(keys_);
    clearEdges(mouse_);
    clearEdges(pad_);
    for (auto [k, isDown] : simulated_)
        if (k >= 0 && k < (int)keys_.size()) setButton(keys_[k], isDown, kDown, kPressed, kReleased);
    simulated_.clear();
    scroll_ = 0;
    text_.clear();
}

void Input::endPoll(GLFWwindow* win) {
    double x, y;
    glfwGetCursorPos(win, &x, &y);
    glm::vec2 p((float)x, (float)y);
    mouseDelta_ = firstMouse_ ? glm::vec2(0) : p - mousePos_;
    mousePos_ = p;
    firstMouse_ = false;

    GLFWgamepadstate gp;
    padConnected_ = glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &gp);
    if (padConnected_) {
        for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST && i < (int)pad_.size(); ++i)
            setButton(pad_[i], gp.buttons[i] == GLFW_PRESS, kDown, kPressed, kReleased);
        leftStick_ = deadZone(gp.axes[GLFW_GAMEPAD_AXIS_LEFT_X], gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
        rightStick_ = deadZone(gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
        // triggers report -1 (released) .. 1 (fully pressed)
        leftTrigger_ = (gp.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1) * 0.5f;
        rightTrigger_ = (gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1) * 0.5f;
    } else {
        pad_.fill(0);
        leftStick_ = rightStick_ = glm::vec2(0);
        leftTrigger_ = rightTrigger_ = 0;
    }
}

}  // namespace ter
