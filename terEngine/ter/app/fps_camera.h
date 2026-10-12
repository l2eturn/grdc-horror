#pragma once
// First-person controller: mouse / right stick to look, WASD / left stick to walk, Shift to run.
// Optional collide() keeps the player out of walls (see ter/core/collide.h).
#include <algorithm>
#include <cmath>
#include <functional>
#include <glm/glm.hpp>
#include "ter/app/input.h"
#include "ter/scene/renderer.h"

namespace ter {

struct FirstPersonCamera {
    // eye position
    glm::vec3 position{0, 1.6f, 0};
    // radians; yaw 0 looks towards -Z, positive yaw turns left; pitch + looks up
    float yaw = 0, pitch = 0;
    float walkSpeed = 2.5f, runSpeed = 5;
    // radians per mouse point / per second at full stick
    float mouseSensitivity = 0.0025f, stickSensitivity = 2.5f;
    // fly along the view direction (editor / noclip) instead of walking on the ground plane
    bool fly = false;
    // given the current and the wanted position, return where the player may actually go
    std::function<glm::vec3(glm::vec3 from, glm::vec3 to)> collide;

    // unit vector the eye looks along
    glm::vec3 forward() const {
        return {-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
    }
    glm::vec3 right() const { return {std::cos(yaw), 0, -std::sin(yaw)}; }

    // mouseLook: use the mouse delta (normally only while the cursor is locked)
    // returns how far the player moved this frame (for footstep sounds / head bob)
    float update(const Input& in, float dt, bool mouseLook = true) {
        if (mouseLook) {
            yaw -= in.mouseDelta().x * mouseSensitivity;
            pitch -= in.mouseDelta().y * mouseSensitivity;
        }
        yaw -= in.rightStick().x * stickSensitivity * dt;
        pitch -= in.rightStick().y * stickSensitivity * dt;
        pitch = std::clamp(pitch, -1.5f, 1.5f);

        glm::vec2 m = in.moveAxis();
        bool running = in.down(Key::LeftShift) || in.down(Pad::LeftThumb);
        float speed = running ? runSpeed : walkSpeed;
        glm::vec3 fwd = fly ? forward() : glm::vec3(-std::sin(yaw), 0, -std::cos(yaw));
        glm::vec3 wanted = position + (fwd * m.y + right() * m.x) * speed * dt;
        glm::vec3 next = collide ? collide(position, wanted) : wanted;
        float moved = glm::length(next - position);
        position = next;
        return moved;
    }

    // write the view into a Camera (projection stays as the camera's fov)
    void apply(Camera& cam) const {
        cam.position = position;
        cam.lookAt(position + forward());
    }
};

}  // namespace ter
