#include "ter/app/app.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ter/gfx/texture.h"
#include "ter/ui/canvas2d.h"

namespace ter {

// ---------------------------------------------------------------------------
// Overlay
// ---------------------------------------------------------------------------
void Overlay::rect(float x, float y, float w, float h, glm::vec4 color, float radius) {
    quads_.push_back({{x, y, w, h}, color, {0, 0, 1, 1}, nullptr, radius, false});
}

void Overlay::fill(glm::vec4 color) {
    // a huge rectangle; it is clipped to the window
    rect(-1e5f, -1e5f, 2e5f, 2e5f, color);
}

void Overlay::image(const Texture& tex, float x, float y, float w, float h, glm::vec4 tint, float radius) {
    // textures are uploaded flipY (row 0 at v = 1), so v runs 1 -> 0 down the screen
    quads_.push_back({{x, y, w, h}, tint, {0, 1, 1, 0}, &tex, radius, false});
}

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------
App::App(const AppConfig& cfg) : cfg_(cfg) {
    if (!glfwInit()) {
        std::fprintf(stderr, "terEngine: glfwInit failed\n");
        return;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, cfg_.resizable ? GLFW_TRUE : GLFW_FALSE);
    GLFWmonitor* monitor = nullptr;
    int w = cfg_.width, h = cfg_.height;
    if (cfg_.fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        w = mode->width;
        h = mode->height;
    }
    win_ = glfwCreateWindow(w, h, cfg_.title.c_str(), monitor, nullptr);
    if (!win_) {
        std::fprintf(stderr, "terEngine: cannot create an OpenGL 4.1 window\n");
        return;
    }
    glfwMakeContextCurrent(win_);
    glfwSwapInterval(cfg_.vsync ? 1 : 0);
    glfwSetWindowUserPointer(win_, this);
    glfwSetKeyCallback(win_, [](GLFWwindow* w, int key, int, int action, int) {
        static_cast<App*>(glfwGetWindowUserPointer(w))->input_.onKey(key, action);
    });
    glfwSetMouseButtonCallback(win_, [](GLFWwindow* w, int button, int action, int) {
        static_cast<App*>(glfwGetWindowUserPointer(w))->input_.onMouseButton(button, action);
    });
    glfwSetScrollCallback(win_, [](GLFWwindow* w, double, double y) {
        static_cast<App*>(glfwGetWindowUserPointer(w))->input_.scroll_ += (float)y;
    });
    glfwSetCharCallback(win_, [](GLFWwindow* w, unsigned int cp) {
        std::string& t = static_cast<App*>(glfwGetWindowUserPointer(w))->input_.text_;
        // UTF-8 encode the code point
        if (cp < 0x80) {
            t += (char)cp;
        } else if (cp < 0x800) {
            t += (char)(0xc0 | (cp >> 6));
            t += (char)(0x80 | (cp & 0x3f));
        } else if (cp < 0x10000) {
            t += (char)(0xe0 | (cp >> 12));
            t += (char)(0x80 | ((cp >> 6) & 0x3f));
            t += (char)(0x80 | (cp & 0x3f));
        } else {
            t += (char)(0xf0 | (cp >> 18));
            t += (char)(0x80 | ((cp >> 12) & 0x3f));
            t += (char)(0x80 | ((cp >> 6) & 0x3f));
            t += (char)(0x80 | (cp & 0x3f));
        }
    });

    if (!renderer_.init()) {
        std::fprintf(stderr, "terEngine: renderer shaders failed to compile\n");
        return;
    }
    overlayShader_ = std::make_unique<Shader>();
    if (!overlayShader_->load("overlay")) return;
    glGenVertexArrays(1, &overlayVao_);
    registerBuiltinFonts();
    uiCanvas_ = std::make_unique<Canvas2D>(16, 16);
    uiTex_ = std::make_unique<Texture>();
    ok_ = true;
}

App::App(AppConfig cfg, int argc, char** argv) : App([&] {
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            bool hasValue = i + 1 < argc;
            if (a == "--shot" && hasValue) cfg.shotPath = argv[++i];
            else if (a == "--shot-delay" && hasValue) cfg.shotDelay = (float)std::atof(argv[++i]);
            else if (a == "--pixel" && hasValue) cfg.pixelScale = std::max(1, std::atoi(argv[++i]));
            else if (a == "--fullscreen") cfg.fullscreen = true;
        }
        return cfg;
    }()) {}

App::~App() {
    // GPU objects must go before the context
    audio_.reset();
    uiTex_.reset();
    overlayShader_.reset();
    if (overlayVao_) glDeleteVertexArrays(1, &overlayVao_);
    if (win_) glfwDestroyWindow(win_);
    glfwTerminate();
}

Audio& App::audio() {
    if (!audio_) {
        audio_ = std::make_unique<Audio>();
        audio_->init();
    }
    return *audio_;
}

void App::quit() { quit_ = true; }

void App::setScene(Object3D& scene, Camera& camera, Environment& env) {
    scene_ = &scene;
    camera_ = &camera;
    env_ = &env;
}

void App::clearScene() { scene_ = nullptr; }

void App::lockCursor(bool locked) {
    cursorLocked_ = locked;
    glfwSetInputMode(win_, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(win_, GLFW_RAW_MOUSE_MOTION, locked ? GLFW_TRUE : GLFW_FALSE);
    // the cursor jumps when the mode changes: skip that delta
    input_.firstMouse_ = true;
}

void App::setTitle(const std::string& title) { glfwSetWindowTitle(win_, title.c_str()); }

int App::run(const std::function<void(Frame&)>& update) {
    if (!ok_) return 1;
    double start = glfwGetTime(), last = start;
    while (!quit_ && !glfwWindowShouldClose(win_)) {
        input_.beginFrame();
        glfwPollEvents();
        input_.endPoll(win_);

        int fw, fh, ww, wh;
        glfwGetFramebufferSize(win_, &fw, &fh);
        glfwGetWindowSize(win_, &ww, &wh);
        // minimised: wait instead of spinning
        if (fw == 0 || fh == 0) {
            glfwWaitEventsTimeout(0.1);
            last = glfwGetTime();
            continue;
        }
        double now = glfwGetTime();
        frame_.dt = (float)std::min(now - last, 0.1);
        frame_.time = (float)(now - start);
        last = now;
        frame_.width = ww;
        frame_.height = wh;
        frame_.fbWidth = fw;
        frame_.fbHeight = fh;
        frame_.scale = ww > 0 ? (float)fw / ww : 1;
        if (fw != uiW_ || fh != uiH_) uiDirty_ = true;

        update(frame_);

        if (audio_) {
            if (listenerFollowsCamera && camera_) {
                // the camera looks down -Z of its view matrix
                glm::vec3 fwd = -glm::vec3(camera_->view[0][2], camera_->view[1][2], camera_->view[2][2]);
                audio_->setListener(camera_->position, fwd);
            }
            audio_->update();
        }

        renderer_.time = frame_.time;
        if (scene_) {
            renderer_.resize(fw, fh, cfg_.pixelScale);
            camera_->aspect = (float)fw / fh;
            camera_->updateProjection();
            renderer_.render(*scene_, *camera_, *env_);
            renderer_.present();
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fw, fh);
            // background is linear like every scene colour: encode it for the screen
            glm::vec3 c = glm::pow(glm::max(renderer_.background, glm::vec3(0)), glm::vec3(1 / 2.2f));
            glClearColor(c.r, c.g, c.b, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        drawUi();
        drawOverlay();

        bool scriptedShot = !cfg_.shotPath.empty() && frame_.time >= cfg_.shotDelay && frame_.index >= 3;
        if (scriptedShot) {
            if (saveFrame(cfg_.shotPath)) std::printf("terEngine: saved %s\n", cfg_.shotPath.c_str());
            quit_ = true;
        }
        if (!pendingShot_.empty()) {
            if (saveFrame(pendingShot_)) std::printf("terEngine: saved %s\n", pendingShot_.c_str());
            pendingShot_.clear();
        }
        glfwSwapBuffers(win_);
        frame_.index++;
    }
    return 0;
}

void App::drawUi() {
    if (!onUi) return;
    if (uiDirty_) {
        uiDirty_ = false;
        uiW_ = frame_.fbWidth;
        uiH_ = frame_.fbHeight;
        Canvas2D& c = *uiCanvas_;
        c.resize(uiW_, uiH_);
        c.clear();
        c.save();
        c.scale(frame_.scale, frame_.scale);
        onUi(c, frame_);
        c.restore();
        c.uploadPremultiplied(*uiTex_);
        uiHasContent_ = true;
    }
    if (uiHasContent_) {
        // the canvas texture is premultiplied with its top row first
        overlay_.quads_.insert(overlay_.quads_.begin(),
                               Overlay::Quad{{0, 0, (float)frame_.width, (float)frame_.height}, glm::vec4(1), {0, 0, 1, 1}, uiTex_.get(), 0, true});
    }
}

void App::drawOverlay() {
    if (overlay_.quads_.empty()) return;
    int fw = frame_.fbWidth, fh = frame_.fbHeight;
    float s = frame_.scale;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fw, fh);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBindVertexArray(overlayVao_);
    Shader& sh = *overlayShader_;
    sh.use();
    sh.set("uScreen", glm::vec2(fw, fh));
    sh.set("uTex", 0);
    for (auto& q : overlay_.quads_) {
        if (q.premultiplied) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        // clip the rectangle to the window so fill() does not lose precision
        glm::vec4 r = q.rect * s;
        if (!q.tex) {
            float x0 = std::max(r.x, -1.0f), y0 = std::max(r.y, -1.0f);
            float x1 = std::min(r.x + r.z, fw + 1.0f), y1 = std::min(r.y + r.w, fh + 1.0f);
            r = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
        }
        sh.set("uRect", r);
        sh.set("uUv", q.uv);
        sh.set("uColor", q.color);
        sh.set("uRadius", q.radius * s);
        sh.set("uHasTex", q.tex ? 1 : 0);
        if (q.tex) q.tex->bind(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    overlay_.quads_.clear();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

bool App::saveFrame(const std::string& path) {
    int w = frame_.fbWidth, h = frame_.fbHeight;
    std::vector<uint8_t> px((size_t)w * h * 4), flipped(px.size());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    // GL rows are bottom-up; PNG wants the top row first, fully opaque
    size_t row = (size_t)w * 4;
    for (int y = 0; y < h; ++y) std::memcpy(&flipped[(size_t)y * row], &px[(size_t)(h - 1 - y) * row], row);
    for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
    if (!savePNG(path, w, h, flipped)) {
        std::fprintf(stderr, "terEngine: cannot write %s\n", path.c_str());
        return false;
    }
    return true;
}

}  // namespace ter
