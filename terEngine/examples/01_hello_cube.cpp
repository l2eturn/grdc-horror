// The smallest terEngine program: a lit, shadowed cube on a floor, plus a line of Thai UI text.
//   ./build/examples/01_hello_cube            (Esc quits)
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "Hello terEngine", .width = 960, .height = 600}, argc, argv);
    if (!app.ok()) return 1;

    // scene graph: a group holding the meshes
    auto scene = ter::makeGroup();
    auto floor = ter::makeMesh(ter::geo::plane(20, 20), ter::lambertColor(ter::hexc(0x6a8c5a)));
    floor->rotation.x = -ter::PI / 2;
    floor->receiveShadow = true;
    auto cube = ter::makeMesh(ter::geo::box(1, 1, 1), ter::standard(ter::hexc(0xff8a3d), 0.5f, 0.1f));
    cube->position = {0, 1, 0};
    cube->castShadow = true;
    scene->add(floor, cube);

    // lighting: sky / ground ambient + a sun that casts shadows + fog
    ter::Environment env;
    env.hemiSky = ter::hexc(0xbfd9ff);
    env.hemiGround = ter::hexc(0x4a3c2a);
    env.hemiIntensity = 0.6f;
    env.sunColor = ter::hexc(0xfff1d6);
    env.sunIntensity = 1.2f;
    env.sunPosition = {4, 8, 3};
    env.fogColor = ter::hexc(0x9cc2e8);
    env.fogNear = 10;
    env.fogFar = 30;
    app.renderer().background = env.fogColor;

    ter::Camera cam;
    cam.fov = 45;
    cam.position = {3, 2.5f, 4};
    cam.lookAt({0, 0.8f, 0});
    app.setScene(*scene, cam, env);

    // UI is redrawn only when asked (redrawUi), so static text is free
    app.onUi = [](ter::Canvas2D& g, const ter::Frame&) {
        g.setFont("Mali", 22, 600);
        g.setFill("#ffffff");
        g.setShadow(6, ter::cssColor("rgba(0,0,0,0.5)"), 0, 2);
        g.fillText("สวัสดี terEngine 👋", 24, 44);
    };

    return app.run([&](ter::Frame& f) {
        cube->rotation.y += f.dt;
        cube->rotation.x = std::sin(f.time) * 0.3f;
        if (app.input().pressed(ter::Key::Escape)) app.quit();
    });
}
