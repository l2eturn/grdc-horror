// Load a .glb / .gltf and orbit around it with the mouse.
//   ./build/examples/03_model_viewer path/to/model.glb
// Drag = orbit · scroll = zoom · Esc quits
#include <ter/ter.h>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 2 || argv[1][0] == '-') {
        std::fprintf(stderr, "usage: 03_model_viewer model.glb [--shot out.png]\n");
        return 2;
    }
    ter::App app({.title = "Model Viewer"}, argc, argv);
    if (!app.ok()) return 1;

    auto scene = ter::makeGroup();
    auto model = ter::loadGltf(argv[1]);
    if (!model) {
        std::fprintf(stderr, "cannot load %s\n", argv[1]);
        return 1;
    }
    scene->add(model->root);
    model->root->traverse([](ter::Object3D& o) { o.castShadow = o.receiveShadow = true; });
    // play every animation in the file
    ter::AnimationMixer mixer(model->animations);

    // frame the model: find its world bounds from the geometry
    scene->updateMatrixWorld();
    glm::vec3 lo(1e9f), hi(-1e9f);
    model->root->traverse([&](ter::Object3D& o) {
        if (!o.geometry) return;
        o.geometry->computeBounds();
        for (int i = 0; i < 8; ++i) {
            glm::vec3 c((i & 1) ? o.geometry->bbMax.x : o.geometry->bbMin.x, (i & 2) ? o.geometry->bbMax.y : o.geometry->bbMin.y,
                        (i & 4) ? o.geometry->bbMax.z : o.geometry->bbMin.z);
            glm::vec3 w = o.localToWorld(c);
            lo = glm::min(lo, w);
            hi = glm::max(hi, w);
        }
    });
    if (lo.x > hi.x) lo = hi = glm::vec3(0);
    glm::vec3 center = (lo + hi) * 0.5f;
    float radius = std::max(0.5f, glm::length(hi - lo) * 0.5f);

    ter::Environment env;
    env.hemiSky = ter::hexc(0xdde8ff);
    env.hemiGround = ter::hexc(0x3a3430);
    env.hemiIntensity = 0.8f;
    env.sunIntensity = 1.0f;
    env.sunPosition = center + glm::vec3(1, 2, 1.5f) * radius * 2.0f;
    env.sunTarget = center;
    env.shadowHalf = radius * 1.5f;
    env.shadowFar = radius * 8;
    env.fogNear = 1e4f;
    env.fogFar = 2e4f;
    app.renderer().background = ter::hexc(0x2a2d34);

    ter::Camera cam;
    cam.fov = 40;
    cam.near = radius * 0.01f;
    cam.far = radius * 50;
    app.setScene(*scene, cam, env);

    float yaw = 0.6f, pitch = 0.35f, dist = radius * 3;
    return app.run([&](ter::Frame& f) {
        ter::Input& in = app.input();
        if (in.pressed(ter::Key::Escape)) app.quit();
        if (in.down(ter::Mouse::Left)) {
            yaw -= in.mouseDelta().x * 0.008f;
            pitch = std::clamp(pitch + in.mouseDelta().y * 0.008f, -1.4f, 1.4f);
        }
        dist = std::clamp(dist * std::pow(0.9f, in.scroll()), radius * 0.5f, radius * 20);
        mixer.update(f.dt);
        cam.position = center + glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)) * dist;
        cam.lookAt(center);
    });
}
