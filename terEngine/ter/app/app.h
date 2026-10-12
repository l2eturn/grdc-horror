#pragma once
// App: opens the window (OpenGL 4.1 core), runs the frame loop and draws, in order:
//   the 3D scene (setScene) -> the post chain -> the UI canvas (onUi) -> overlay quads -> swap.
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "ter/app/input.h"
#include "ter/audio/audio.h"
#include "ter/scene/renderer.h"

struct GLFWwindow;

namespace ter {

class Canvas2D;
class Texture;

struct AppConfig {
    std::string title = "terEngine";
    // window size in points (on a Retina screen the framebuffer is twice as large)
    int width = 1280, height = 720;
    bool resizable = true;
    bool vsync = true;
    bool fullscreen = false;
    // size of one scene pixel in framebuffer pixels: 1 = sharp, 2-3 = chunky pixel-art look
    int pixelScale = 1;
    // --shot: save a PNG this many seconds after start, then quit (for automated checks)
    std::string shotPath;
    float shotDelay = 0.5f;
};

struct Frame {
    // seconds since the last frame (capped at 0.1 so a stall does not teleport things) and since start
    float dt = 0, time = 0;
    uint64_t index = 0;
    // window size in points (UI / mouse units) and framebuffer size in pixels
    int width = 0, height = 0, fbWidth = 0, fbHeight = 0;
    // framebuffer pixels per point (2 on Retina)
    float scale = 1;
};

// Screen-space quads drawn on top of everything, in window points (y down). Queue them during the
// frame; they are drawn once and cleared. Colours are straight alpha, 0..1.
class Overlay {
public:
    // solid rectangle (fades, flashes, health bars)
    void rect(float x, float y, float w, float h, glm::vec4 color, float radius = 0);
    // the whole window in one colour (fade to black: fill({0, 0, 0, k}))
    void fill(glm::vec4 color);
    // a texture (e.g. loaded with Texture::load, or a Canvas2D uploaded into it); tint multiplies it
    void image(const Texture& tex, float x, float y, float w, float h, glm::vec4 tint = glm::vec4(1), float radius = 0);

private:
    friend class App;
    struct Quad {
        glm::vec4 rect, color, uv;
        const Texture* tex;
        float radius;
        bool premultiplied;
    };
    std::vector<Quad> quads_;
};

class App {
public:
    explicit App(const AppConfig& cfg = {});
    // also reads command-line flags: --shot out.png  --shot-delay s  --pixel n  --fullscreen
    App(AppConfig cfg, int argc, char** argv);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // false if the window or the renderer could not be created (the reason is printed)
    bool ok() const { return ok_; }

    // Run until the window closes or quit() is called. `update` runs once per frame before drawing.
    // Returns 0 on a normal exit (1 if the app is not ok()).
    int run(const std::function<void(Frame&)>& update);
    void quit();

    // What to draw every frame. The objects must stay alive while the app runs.
    // Without a scene the window is cleared to renderer().background.
    void setScene(Object3D& scene, Camera& camera, Environment& env);
    void clearScene();

    // UI layer: a full-window Canvas2D (Thai text, emoji, shapes) in window points.
    // `onUi` is called only when redrawUi() was requested (and after a resize), with the canvas
    // already cleared — so static UI costs nothing per frame. Call redrawUi() whenever its content changes.
    std::function<void(Canvas2D&, const Frame&)> onUi;
    void redrawUi() { uiDirty_ = true; }

    Input& input() { return input_; }
    Renderer& renderer() { return renderer_; }
    Overlay& overlay() { return overlay_; }
    // Audio starts on first use
    Audio& audio();
    // move the audio listener to the camera every frame (turn off to place it yourself)
    bool listenerFollowsCamera = true;
    const Frame& frame() const { return frame_; }

    // hide the cursor and keep it in the window: mouseDelta() then drives a first-person camera
    void lockCursor(bool locked);
    bool cursorLocked() const { return cursorLocked_; }
    void setTitle(const std::string& title);
    // save the next frame (everything on screen) as a PNG
    void screenshot(const std::string& path) { pendingShot_ = path; }
    GLFWwindow* window() const { return win_; }

private:
    void drawUi();
    void drawOverlay();
    bool saveFrame(const std::string& path);

    AppConfig cfg_;
    GLFWwindow* win_ = nullptr;
    bool ok_ = false, quit_ = false, cursorLocked_ = false, uiDirty_ = true;
    Input input_;
    Renderer renderer_;
    Overlay overlay_;
    std::unique_ptr<Audio> audio_;
    Frame frame_;
    Object3D* scene_ = nullptr;
    Camera* camera_ = nullptr;
    Environment* env_ = nullptr;
    std::string pendingShot_;

    // UI canvas + its texture, and the overlay program
    std::unique_ptr<Canvas2D> uiCanvas_;
    std::unique_ptr<Texture> uiTex_;
    bool uiHasContent_ = false;
    std::unique_ptr<Shader> overlayShader_;
    unsigned overlayVao_ = 0;
    int uiW_ = 0, uiH_ = 0;
};

}  // namespace ter
