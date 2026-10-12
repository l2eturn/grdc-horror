// A first-person walk through a dark room: flashlight (spot light), fog, film grain, wall collision,
// a positional sound you can follow, looking at things to interact, and a fade overlay.
//   ./build/examples/02_dark_room
// Click = lock the mouse · WASD walk · Shift run · F flashlight · E interact · Esc unlock / quit
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "Dark Room", .width = 1280, .height = 720, .pixelScale = 2}, argc, argv);
    if (!app.ok()) return 1;
    ter::Renderer& r = app.renderer();
    r.vignette = 1.2f;
    r.warm = -0.06f;
    r.grain = 0.06f;
    r.background = ter::hexc(0x020203);

    auto scene = ter::makeGroup();
    std::vector<ter::Box> walls;
    auto wallMat = ter::lambertColor(ter::hexc(0x6b6f78));
    // add a box mesh and its collider
    auto block = [&](glm::vec3 center, glm::vec3 size, const ter::MatPtr& m) {
        auto mesh = ter::makeMesh(ter::geo::box(size.x, size.y, size.z), m);
        mesh->position = center;
        mesh->castShadow = mesh->receiveShadow = true;
        scene->add(mesh);
        walls.push_back(ter::Box::centered(center, size));
        return mesh;
    };
    auto floor = ter::makeMesh(ter::geo::plane(16, 16), ter::lambertColor(ter::hexc(0x3a3430)));
    floor->rotation.x = -ter::PI / 2;
    floor->receiveShadow = true;
    scene->add(floor);
    // four walls and a few pillars
    block({0, 1.5f, -8}, {16, 3, 0.3f}, wallMat);
    block({0, 1.5f, 8}, {16, 3, 0.3f}, wallMat);
    block({-8, 1.5f, 0}, {0.3f, 3, 16}, wallMat);
    block({8, 1.5f, 0}, {0.3f, 3, 16}, wallMat);
    for (int i = 0; i < 4; ++i) block({-4.5f + i * 3, 1.5f, -2.5f + (i % 2) * 4}, {0.6f, 3, 0.6f}, wallMat);
    // the thing to find: a glowing box in the far corner
    auto noteMat = ter::lambertColor(ter::hexc(0xd8c9a0));
    noteMat->emissive = ter::hexc(0x3a2a10);
    auto note = block({6.5f, 0.4f, -6.5f}, {0.5f, 0.8f, 0.5f}, noteMat);
    size_t noteIndex = walls.size() - 1;
    // a dim red lamp near the note
    auto lamp = ter::makePointLight(ter::hexc(0xff3020), 1.5f, 5);
    lamp->position = {6, 2.2f, -6};
    scene->add(lamp);
    // the flashlight: a spot light we move with the camera each frame
    auto flashlight = ter::makeSpotLight(ter::hexc(0xfff2d8), 3.0f, 14, 0.42f, 0.5f);
    scene->add(flashlight);

    ter::Environment env;
    env.hemiSky = ter::hexc(0x20242c);
    env.hemiGround = ter::hexc(0x050505);
    env.hemiIntensity = 0.25f;
    env.sunIntensity = 0;
    env.fogColor = ter::hexc(0x020203);
    env.fogNear = 2;
    env.fogFar = 14;

    ter::Camera cam;
    cam.fov = 70;
    cam.near = 0.05f;
    ter::FirstPersonCamera player;
    player.position = {-6, 1.6f, 6};
    // look across the room towards the far corner (+X, -Z)
    player.yaw = -ter::PI / 4;
    player.collide = [&](glm::vec3, glm::vec3 to) { return ter::resolveCircle(to, 0.3f, walls); };
    player.apply(cam);
    app.setScene(*scene, cam, env);

    // placeholder sounds made from synths (replace with real files later: audio.play("assets/x.wav"))
    ter::Audio& audio = app.audio();
    audio.synth("step", {.wave = ter::Wave::Noise, .duration = 0.09f, .volume = 0.12f});
    audio.synth("hum", {.wave = ter::Wave::Sine, .freq = 110, .duration = 2.0f, .attack = 0.6f, .volume = 0.25f, .vibrato = 0.3f, .vibratoRate = 3});
    audio.synth("click", {.wave = ter::Wave::Square, .freq = 1800, .freqEnd = 900, .duration = 0.04f, .volume = 0.1f});
    audio.synth("found", std::vector<std::pair<float, ter::Synth>>{
                             {0.0f, {.wave = ter::Wave::Triangle, .freq = 220, .duration = 1.2f, .volume = 0.2f}},
                             {0.15f, {.wave = ter::Wave::Triangle, .freq = 233, .duration = 1.2f, .volume = 0.2f}}});
    // a looping hum at the note: walk towards it and it gets louder, turn and it pans
    auto hum = audio.play("hum", {.volume = 0.8f, .loop = true, .spatial = true, .position = note->position, .maxDistance = 16});

    const std::string startHint = "คลิกเพื่อเริ่ม · หาเสียงที่ดังอยู่";
    std::string message = startHint;
    app.onUi = [&](ter::Canvas2D& g, const ter::Frame& f) {
        g.setFont("Mali", 20, 500);
        g.setTextAlign(ter::TextAlign::Center);
        g.setFill("rgba(230,220,200,0.9)");
        g.fillText(message, f.width / 2.0f, f.height - 40.0f);
        // crosshair dot
        g.beginPath();
        g.arc(f.width / 2.0f, f.height / 2.0f, 2, 0, ter::PI * 2);
        g.fill();
    };

    bool lightOn = true, found = false;
    float stepDistance = 0, fade = 1;
    return app.run([&](ter::Frame& f) {
        ter::Input& in = app.input();
        if (in.pressed(ter::Mouse::Left) && !app.cursorLocked()) app.lockCursor(true);
        if (in.pressed(ter::Key::Escape)) {
            if (app.cursorLocked()) app.lockCursor(false);
            else app.quit();
        }

        stepDistance += player.update(in, f.dt, app.cursorLocked());
        player.apply(cam);
        // a footstep every 0.7 m, slightly different pitch each time
        if (stepDistance > 0.7f) {
            stepDistance = 0;
            audio.play("step", {.volume = 0.7f, .pitch = 0.8f + ter::frand() * 0.4f});
        }

        // flashlight sits at the eye, a little to the right, and points where we look
        flashlight->position = player.position + player.right() * 0.15f - glm::vec3(0, 0.15f, 0);
        flashlight->lookAt(player.position + player.forward() * 5.0f);
        if (in.pressed(ter::Key::F)) {
            lightOn = !lightOn;
            flashlight->intensity = lightOn ? 3.0f : 0.0f;
            audio.play("click");
        }
        // flicker
        if (lightOn) flashlight->intensity = ter::frand() < 0.02f ? 0.6f : 3.0f;

        // what are we looking at?
        float dist = 0;
        int hit = ter::raycast(player.position, player.forward(), walls, 2.0f, &dist);
        std::string want = found ? "เจอแล้ว… กด Esc เพื่อออก" : hit == (int)noteIndex ? "กด E เพื่อหยิบ" : app.cursorLocked() ? "" : startHint;
        if (hit == (int)noteIndex && in.pressed(ter::Key::E) && !found) {
            found = true;
            hum->fadeTo(0, 1.5f, true);
            audio.play("found");
        }
        if (want != message) {
            message = want;
            app.redrawUi();
        }

        // fade in from black at the start
        fade = std::max(0.0f, fade - f.dt * 0.5f);
        if (fade > 0) app.overlay().fill({0, 0, 0, fade});
    });
}
