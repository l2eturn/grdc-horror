// Props placed from map.json: plain boxes, the bus shelter, street lamps, the bus-stop sign, a bin and the
// placeholder companion. Built from primitives + kit textures (PS2 budget, realistic shapes), except the shelter,
// which is a Blender model (assets/models/sala.glb).
// Each prefab is a group in its own local frame: +Z = "front" for signs/lamps, the shelter opens toward -Z.
// Rotate prefabs that have colliders by multiples of 90° (colliders are axis-aligned boxes).
#include <cstdio>
#include <functional>
#include <optional>
#include <glm/gtc/matrix_transform.hpp>
#include "MeshKit.h"
#include "Prefabs.h"
#include "Textures.h"

namespace {

// Parts of one prefab, placed in the prefab's local frame; colliders are converted to world space
struct Kit {
    Builder& b;
    ter::ObjPtr group;
    glm::vec3 origin;
    float yaw;

    ter::ObjPtr put(const ter::ObjPtr& m, glm::vec3 local, glm::vec3 rot = glm::vec3(0)) {
        m->position = local;
        m->rotation = rot;
        group->add(m);
        return m;
    }
    // collider for a local box (center, size)
    void block(glm::vec3 center, glm::vec3 size) {
        glm::vec3 c = origin + glm::vec3(glm::rotate(glm::mat4(1), yaw, glm::vec3(0, 1, 0)) * glm::vec4(center, 1));
        b.world.colliders.push_back(boundsOf(c, size, {0, yaw, 0}));
    }
};

Kit startPrefab(Builder& b, const json& j, const char* name) {
    glm::vec3 pos = vec3Of(j, "pos");
    float yaw = radians(j.value("rotY", 0.0f));
    auto g = ter::makeGroup(name);
    g->position = pos;
    g->rotation.y = yaw;
    b.add(g);
    return {b, g, pos, yaw};
}

// A texture painted with Canvas2D (signs, posters): draw(canvas) then upload
std::shared_ptr<ter::Texture> paintTexture(int w, int h, const std::function<void(ter::Canvas2D&)>& draw) {
    ter::Canvas2D c(w, h);
    draw(c);
    auto t = std::make_shared<ter::Texture>();
    c.upload(*t);
    return t;
}

// Ageing on a painted canvas: yellowed edges, dirt specks
void weather(ter::Canvas2D& c, float amount) {
    int w = c.width(), h = c.height();
    ter::Rng rng(w * 31 + h);
    c.beginPath();
    c.rect(0, 0, (float)w, (float)h);
    c.fillRadialGradient(w / 2.0f, h / 2.0f, std::min(w, h) * 0.3f, w / 2.0f, h / 2.0f, std::max(w, h) * 0.75f,
                         {{0, ter::RGBA{0.4f, 0.32f, 0.15f, 0}}, {1, ter::RGBA{0.35f, 0.27f, 0.12f, 0.55f * amount}}});
    for (int i = 0; i < (int)(w * h / 900.0f * amount); ++i) {
        c.setFill(ter::RGBA{0.15f, 0.12f, 0.08f, 0.15f + rng.f() * 0.3f});
        c.beginPath();
        c.arc(rng.f() * w, rng.f() * h, 0.5f + rng.f() * 2.5f, 0, ter::PI * 2);
        c.fill();
    }
}

// A flat textured panel (plane) facing +Z
ter::ObjPtr panel(float w, float h, const std::shared_ptr<ter::Texture>& tex, float glow = 0) {
    auto mat = ter::lambert(tex);
    if (glow > 0) {
        mat->emissiveMap = tex;
        mat->emissive = glm::vec3(glow);
    }
    return ter::makeMesh(ter::geo::plane(w, h), mat);
}

}  // namespace

// A plain box: walls, floors, roofs, benches. "unlit" boxes glow (lamps, distant windows).
// "texture" + "tile" (meters per repeat) give it a surface from the texture kit.
void Builder::box(const json& j) {
    glm::vec3 pos = vec3Of(j, "pos"), size = vec3Of(j, "size", glm::vec3(1));
    glm::vec3 rot = vec3Of(j, "rot") * (ter::PI / 180.0f);
    glm::vec3 color = colorOf(j, "color", ter::hexc(0x808080));
    ter::ObjPtr mesh;
    if (j.value("unlit", false)) mesh = ter::makeMesh(ter::geo::box(size.x, size.y, size.z), ter::basic(color));
    else if (j.contains("texture")) mesh = texturedBox(size, j["texture"].get<std::string>(), j.value("tile", 2.0f), color);
    else mesh = ter::makeMesh(ter::geo::box(size.x, size.y, size.z), ter::lambertColor(color));
    mesh->material()->fog = j.value("fog", true);
    mesh->position = pos;
    mesh->rotation = rot;
    add(mesh);
    if (j.value("collide", true)) world.colliders.push_back(boundsOf(pos, size, rot));
}

// Thai roadside bus shelter (ศาลาที่พักผู้โดยสาร) with a Thai gable roof: the model is assets/models/sala.glb
// (made in Blender, source in game/modelResource/sala_source.blend: ~1,200 triangles, 128-256 px tiling textures,
// ambient occlusion baked into vertex colours). The game adds what changes per map: the name board under the
// upper roof, posters on the notice board at the back, the tube light, and the colliders. Opens toward local -Z.
// Keys: title, subtitle, posters, light {intensity, range, flicker, color}
void Builder::busShelter(const json& j) {
    Kit k = startPrefab(*this, j, "bus_shelter");

    std::optional<ter::GltfScene> model = ter::loadGltf(ter::assetPath("models/sala.glb"));
    if (!model) {
        std::fprintf(stderr, "bus_shelter: could not load models/sala.glb\n");
        return;
    }
    k.group->add(model->root);

    // colliders: six wooden posts, the long bench at the back and the short one on the left side
    for (float x : {-2.4f, 0.0f, 2.4f})
        for (float z : {-1.25f, 1.25f}) k.block({x, 1.2f, z}, {0.16f, 2.4f, 0.16f});
    k.block({0, 0.5f, 0.88f}, {4.3f, 1.0f, 0.5f});
    k.block({-2.2f, 0.4f, -0.35f}, {0.45f, 0.8f, 1.5f});

    // name board on the strip of wall between the lower and the upper roof, facing the road
    std::string title = j.value("title", std::string("ศาลาที่พักผู้โดยสาร")), subtitle = j.value("subtitle", std::string(""));
    auto boardTex = paintTexture(1024, 96, [&](ter::Canvas2D& c) {
        c.setFill("#d9d0b6");
        c.fillRect(0, 0, 1024, 96);
        c.setFill("#8a1f17");
        c.setFont("Mali", 54, 600);
        c.setTextAlign(ter::TextAlign::Center);
        c.fillText(title, subtitle.empty() ? 512 : 400, 66);
        if (!subtitle.empty()) {
            c.setFont("Mali", 26, 500);
            c.setFill("#5a4a3a");
            c.fillText(subtitle, 860, 62);
        }
        weather(c, 1.0f);
    });
    const float boardW = 3.4f;
    k.put(panel(boardW, boardW * 96 / 1024, boardTex, 0.12f), {0, 2.9f, -1.09f}, {0, ter::PI, 0});

    // fluorescent tube (part of the model, under the middle tie beam) and its light
    const json light = j.value("light", json::object());
    auto tube = ter::makePointLight(colorOf(light, "color", ter::hexc(0xcfe4ff)), light.value("intensity", 1.3f), light.value("range", 6.5f));
    k.put(tube, {0, 2.25f, 0});
    if (float f = light.value("flicker", 0.06f); f > 0) world.flickers.push_back({tube, tube->intensity, f});
    world.lamps["shelter"] = {tube, nullptr, nullptr, glm::vec3(1)};   // rule 8: the shelter light goes out

    // notices on the board between the back-left and back-middle posts (placeholders for the misleading-sign system)
    json posters = j.value("posters", json::array({
        {{"kind", "timetable"}, {"title", "ตารางเดินรถ"}, {"lines", {"เที่ยวแรก 06:00", "เที่ยวสุดท้าย 18:30", "งดเดินรถหลังพลบค่ำ"}}},
        {{"kind", "missing"}, {"title", "ประกาศคนหาย"}, {"lines", {"หายตัวที่ป้ายนี้", "พบเห็นโปรดแจ้ง"}}},
    }));
    float px = -0.68f;   // the board spans x -1.95..-0.35; two posters fit
    for (const json& p : posters) {
        bool missing = p.value("kind", std::string()) == "missing";
        auto tex = paintTexture(256, 360, [&](ter::Canvas2D& c) {
            c.setFill(missing ? "#e9e4d6" : "#f1ecdf");
            c.fillRect(0, 0, 256, 360);
            c.setTextAlign(ter::TextAlign::Center);
            c.setFill(missing ? "#9a1a14" : "#1e3a6e");
            c.setFont("Mali", 34, 600);
            c.fillText(p.value("title", std::string()), 128, 52);
            float y = 100;
            if (missing) {
                // a dark, blurred "photo": the silhouette of a head and shoulders, cropped to the frame
                c.setFill("#77756f");
                c.fillRect(58, 72, 140, 150);
                c.save();
                c.beginPath();
                c.rect(58, 72, 140, 150);
                c.clip();
                const std::vector<ter::GradientStop> blur = {{0, ter::cssColor("#1d1c1a")}, {0.75f, ter::cssColor("#1d1c1a")}, {1, ter::cssColor("rgba(29,28,26,0)")}};
                c.beginPath();
                c.ellipse(128, 236, 72, 52);
                c.fillRadialGradient(128, 236, 0, 128, 236, 76, blur);
                c.beginPath();
                c.ellipse(128, 138, 30, 38);
                c.fillRadialGradient(128, 138, 0, 128, 138, 40, blur);
                c.restore();
                y = 262;
            }
            c.setFill("#2a2622");
            c.setFont("Mali", 24, 500);
            for (const json& line : p.value("lines", json::array())) {
                c.fillText(line.get<std::string>(), 128, y);
                y += 34;
            }
            weather(c, 1.4f);
        });
        k.put(panel(0.42f, 0.59f, tex, 0.05f), {px, 1.57f, 1.235f}, {0, ter::PI, (px > -1.0f ? 0.03f : -0.05f)});
        px -= 0.94f;
    }

    // jam poster in the right bay of the back: the bay is open, so it goes on a plank board nailed between
    // the rail and the tie beam. The logo (made for a dark background, mostly translucent white) is printed
    // on a night-blue sheet, aged like the other notices. "logo": "" = none
    std::string logo = j.value("logo", std::string("textures/jam_logo.png"));
    int lw, lh;
    std::vector<uint8_t> logoPx;
    if (!logo.empty() && ter::decodeImageFile(ter::assetPath(logo), lw, lh, logoPx)) {
        const int W = 512, H = W * lh / lw;
        auto tex = paintTexture(W, H, [&](ter::Canvas2D& c) {
            c.beginPath();
            c.rect(0, 0, (float)W, (float)H);
            c.fillLinearGradient(0, 0, 0, (float)H, {{0, ter::cssColor("#1b2140")}, {1, ter::cssColor("#0e1124")}});
            c.drawImage(logoPx, lw, lh, 0, 0, (float)W, (float)H);
            weather(c, 0.9f);
        });
        k.put(texturedBox({0.9f, 1.3f, 0.025f}, "wood", 1.0f, ter::hexc(0x8a7a66)), {1.15f, 1.6f, 1.26f});
        const float w = 0.62f;
        k.put(panel(w, w * H / W, tex, 0.05f), {1.15f, 1.62f, 1.235f}, {0, ter::PI, -0.02f});
    } else if (!logo.empty()) {
        std::fprintf(stderr, "bus_shelter: could not load %s\n", logo.c_str());
    }
}

// Street lamp: concrete footing, tapered octagonal steel pole, an arm that curves out over the road (local +Z)
// and a cobra-head lamp with a glowing glass, a point light and a soft halo. "on": false = dead lamp
// (dark glass, drooping head). Keys: height, reach, color, intensity, range, flicker, on, rotY
void Builder::lampPost(const json& j) {
    Kit k = startPrefab(*this, j, "lamp_post");
    const float H = j.value("height", 6.0f), reach = j.value("reach", 1.7f);
    const bool on = j.value("on", true);
    const glm::vec3 color = colorOf(j, "color", ter::hexc(0xffc27a));
    auto steel = kitMat("steel", ter::hexc(0x8e9296), {0.5f, 3});

    k.put(texturedBox({0.42f, 0.35f, 0.42f}, "concrete", 1.0f, ter::hexc(0xb0aca4)), {0, 0.175f, 0});
    k.put(ter::makeMesh(tubeGeo({0, 0.35f, 0}, {0, H, 0}, 0.1f, 0.06f, 8), steel), {});
    k.block({0, H / 2, 0}, {0.3f, H, 0.3f});

    // the arm: a quarter circle from vertical to horizontal, then straight out
    const float R = 0.45f;
    glm::vec3 prev(0, H, 0);
    for (int i = 1; i <= 5; ++i) {
        float a = (ter::PI / 2) * i / 5;
        glm::vec3 p(0, H + R * std::sin(a), R - R * std::cos(a));
        k.put(ter::makeMesh(tubeGeo(prev, p, 0.055f, 0.05f, 6), steel), {});
        prev = p;
    }
    glm::vec3 end(0, H + R + 0.08f, reach);
    k.put(ter::makeMesh(tubeGeo(prev, end, 0.05f, 0.045f, 6), steel), {});

    // cobra head: flattened half-ellipsoid housing, glass underneath; a dead lamp droops
    auto head = ter::makeGroup("lamp_head");
    head->position = end;
    head->rotation.x = on ? -0.06f : 0.45f;
    k.group->add(head);
    auto shell = ter::geo::sphere(1, 12, 6, 0, ter::PI * 2, 0, ter::PI / 2);
    shell->scale(0.17f, 0.1f, 0.4f);
    head->add(meshAt(shell, kitMat("steel", ter::hexc(0x6a6e72)), {0, 0, 0.3f}));
    auto glassGeo = ter::geo::circle(1, 16);
    glassGeo->rotateX(ter::PI / 2);
    glassGeo->scale(0.13f, 1, 0.33f);
    auto glassMat = ter::basic(on ? color : ter::hexc(0x1a1c1e));
    glassMat->side = ter::Side::Double;
    head->add(meshAt(glassGeo, glassMat, {0, -0.005f, 0.3f}));
    if (!on) return;

    auto light = ter::makePointLight(color, j.value("intensity", 2.2f), j.value("range", 11.0f));
    light->position = {0, -0.35f, 0.3f};
    head->add(light);
    if (float f = j.value("flicker", 0.0f); f > 0) world.flickers.push_back({light, light->intensity, f});
    // soft halo around the glass (bloom-like); unlit additive sprite
    auto haloMat = ter::basic(color * 0.6f, texture("glow"));
    haloMat->transparent = true;
    haloMat->blending = ter::Blending::Additive;
    haloMat->depthWrite = false;
    auto halo = ter::makeSprite(haloMat);
    halo->position = {0, -0.12f, 0.3f};
    halo->scale = {1.4f, 1.0f, 1};
    head->add(halo);
    if (j.contains("id")) world.lamps[j.value("id", std::string())] = {light, halo, glassMat, color};
}

// Bus-stop sign: galvanised pole on a small footing, blue panel with a bus symbol and Thai text.
// The panel faces local +Z (rotY 180 = toward the road at -Z). Keys: lines, background, glow, rotY
void Builder::sign(const json& j) {
    Kit k = startPrefab(*this, j, "sign");
    std::string background = j.value("background", std::string("#1f4e8c"));
    auto tex = paintTexture(400, 520, [&](ter::Canvas2D& c) {
        c.setFill(background);
        c.fillRect(0, 0, 400, 520);
        c.setStroke("#ececec");
        c.setLineWidth(10);
        c.strokeRect(12, 12, 376, 496);
        // bus symbol
        c.setFill("#f2f2f2");
        c.beginPath();
        c.roundRect(110, 44, 180, 120, 18);
        c.fill();
        c.setFill(background);
        c.fillRect(126, 60, 148, 52);
        c.fillRect(126, 122, 40, 22);
        c.fillRect(234, 122, 40, 22);
        c.setFill("#f2f2f2");
        for (float x : {148.0f, 252.0f}) {
            c.beginPath();
            c.arc(x, 172, 16, 0, ter::PI * 2);
            c.fill();
        }
        c.setTextAlign(ter::TextAlign::Center);
        float y = 250;
        bool first = true;
        for (const json& line : j.value("lines", json::array())) {
            c.setFont("Mali", first ? 52 : 36, 600);
            for (const std::string& part : c.wrapText(line.get<std::string>(), 350)) {
                c.fillText(part, 200, y);
                y += first ? 64 : 50;
            }
            first = false;
        }
        weather(c, 0.5f);
    });
    auto steel = kitMat("steel", glm::vec3(1), {0.3f, 2});
    k.put(texturedBox({0.25f, 0.12f, 0.25f}, "concrete", 1.0f, ter::hexc(0xa8a49c)), {0, 0.06f, 0});
    k.put(ter::makeMesh(tubeGeo({0, 0.1f, -0.06f}, {0, 2.75f, -0.06f}, 0.035f, 0.032f, 8), steel), {});
    k.put(ter::makeMesh(ter::geo::box(0.64f, 0.82f, 0.025f), steel), {0, 2.2f, -0.02f});
    k.put(panel(0.62f, 0.8f, tex, j.value("glow", 0.3f)), {0, 2.2f, 0.0f});
    k.block({0, 1.3f, -0.06f}, {0.2f, 2.6f, 0.2f});
}

// Roadside rubbish bin: painted steel drum with a lid left slightly open. Keys: color
void Builder::bin(const json& j) {
    Kit k = startPrefab(*this, j, "bin");
    auto paintMat = kitMat("metal", colorOf(j, "color", ter::hexc(0x4f7352)), {1.5f, 1});
    k.put(ter::makeMesh(ter::geo::cylinder(0.25f, 0.22f, 0.72f, 12, 1, true), paintMat), {0, 0.36f, 0});
    auto inside = ter::geo::circle(0.24f, 12);
    inside->rotateX(-ter::PI / 2);
    k.put(ter::makeMesh(inside, ter::lambertColor(ter::hexc(0x0c0c0c))), {0, 0.62f, 0});
    k.put(ter::makeMesh(ter::geo::cylinder(0.27f, 0.27f, 0.04f, 12), paintMat), {0.05f, 0.75f, 0.02f}, {0.12f, 0, 0.08f});
    k.block({0, 0.38f, 0}, {0.5f, 0.76f, 0.5f});
}

// A small backpack like the player's own (rule 11: it isn't): body, front pocket, top handle, two straps on the
// back, a zip line. ~150 triangles, faces local +Z (pocket side). Hidden until the rule's window (main.cpp).
// Keys: pos, rotY, color, visible
void Builder::bag(const json& j) {
    Kit k = startPrefab(*this, j, "bag");
    glm::vec3 tint = colorOf(j, "color", ter::hexc(0x3a4250));
    auto cloth = kitMat("cloth", tint, {1, 1});
    auto dark = kitMat("cloth", tint * 0.55f, {1, 1});
    auto zip = ter::lambertColor(ter::hexc(0x16181b));
    k.put(ter::makeMesh(beamGeo({0, 0, 0}, {0, 0.42f, 0}, 0.30f, 0.17f), cloth), {0, 0, 0});
    k.put(ter::makeMesh(beamGeo({0, 0.4f, 0}, {0, 0.46f, 0}, 0.26f, 0.13f), cloth), {0, 0, 0});           // rounded top
    k.put(ter::makeMesh(beamGeo({0, 0.04f, 0.09f}, {0, 0.22f, 0.09f}, 0.24f, 0.07f), dark), {0, 0, 0});  // front pocket
    k.put(ter::makeMesh(beamGeo({-0.12f, 0.22f, 0.13f}, {0.12f, 0.22f, 0.13f}, 0.012f, 0.012f), zip), {0, 0, 0});
    k.put(ter::makeMesh(beamGeo({-0.13f, 0.44f, 0.086f}, {0.13f, 0.44f, 0.086f}, 0.012f, 0.012f), zip), {0, 0, 0});
    k.put(ter::makeMesh(tubeGeo({-0.05f, 0.46f, 0}, {0.05f, 0.46f, 0}, 0.012f, 0.012f, 5), dark), {0, 0.03f, 0});  // handle
    for (float x : {-0.08f, 0.08f})
        k.put(ter::makeMesh(beamGeo({x, 0.40f, -0.1f}, {x * 1.4f, 0.05f, -0.1f}, 0.045f, 0.015f), dark), {0, 0, 0});  // straps
    k.group->visible = j.value("visible", false);
    world.bag = k.group;
}

// A person-shaped shadow (rule 4): the companion's model, every part flat black and unlit, a little taller and
// thinner, standing perfectly still. Only the fog makes it out. Keys: pos, rotY, visible
void Builder::shadowFigure(const json& j) {
    auto model = ter::loadGltf(ter::assetPath("models/npc.glb"));
    if (!model) {
        std::fprintf(stderr, "shadow_figure: could not load models/npc.glb\n");
        return;
    }
    Kit k = startPrefab(*this, j, "shadow_figure");
    auto black = ter::basic(ter::hexc(0x030304));
    model->root->traverse([&](ter::Object3D& o) {
        for (auto& m : o.materials) m = black;
    });
    model->root->scale = {0.92f, 1.1f, 0.92f};
    k.group->add(model->root);
    k.group->visible = j.value("visible", false);
    world.shadowFigure = k.group;
}

// Thai highway kilometre stone: white concrete post with a coloured top band, route number,
// place name and distance on both faces. Keys: route, place, km, band (colour), rotY (faces along ±Z)
void Builder::kmPost(const json& j) {
    Kit k = startPrefab(*this, j, "km_post");
    std::string route = j.value("route", std::string("1095")), place = j.value("place", std::string("ดอยหมอก"));
    std::string km = std::to_string(j.value("km", 12));
    std::string band = j.value("band", std::string("#1f4e8c"));
    auto tex = paintTexture(256, 384, [&](ter::Canvas2D& c) {
        c.setFill("#e9e6de");
        c.fillRect(0, 0, 256, 384);
        c.setFill(band);
        c.fillRect(0, 0, 256, 110);
        c.setTextAlign(ter::TextAlign::Center);
        c.setFill("#f4f4f4");
        c.setFont("Inter", 58, 700);
        c.fillText(route, 128, 80);
        c.setFill("#1a1a1a");
        c.setFont("Mali", 44, 600);
        c.fillText(place, 128, 180);
        c.setFont("Inter", 110, 700);
        c.fillText(km, 128, 320);
        weather(c, 1.2f);
    });
    k.put(texturedBox({0.32f, 0.9f, 0.16f}, "paint", 1.0f, ter::hexc(0xe0ddd4)), {0, 0.45f, 0});
    auto cap = ter::geo::cylinder(0.16f, 0.16f, 0.16f, 12, 1, false, 0, ter::PI);   // half-round top
    cap->rotateX(ter::PI / 2);
    cap->rotateY(ter::PI / 2);
    k.put(ter::makeMesh(cap, kitMat("paint", colorOf(j, "band", ter::hexc(0x1f4e8c)))), {0, 0.9f, 0});
    for (float side : {-1.0f, 1.0f}) k.put(panel(0.3f, 0.45f, tex, 0.12f), {0, 0.55f, side * 0.081f}, {0, side > 0 ? 0 : ter::PI, 0});
    k.block({0, 0.5f, 0}, {0.34f, 1.0f, 0.18f});
}

// Road sign on a steel pole: "shape" = "diamond" (yellow warning), "circle" (red ring) or "rect" (blue);
// "symbol" = "curve_left", "curve_right", "animal", "number" or "none"; "text" under or inside it.
// The face looks along local +Z. Keys: shape, symbol, text, height, size, glow, rotY
void Builder::roadSign(const json& j) {
    Kit k = startPrefab(*this, j, "road_sign");
    std::string shape = j.value("shape", std::string("diamond")), symbol = j.value("symbol", std::string("none"));
    std::string text = j.value("text", std::string());
    auto tex = paintTexture(256, 256, [&](ter::Canvas2D& c) {
        c.clear();
        c.beginPath();
        if (shape == "diamond") {
            c.moveTo(128, 6); c.lineTo(250, 128); c.lineTo(128, 250); c.lineTo(6, 128);
            c.closePath();
            c.setFill("#151515");
            c.fill();
            c.beginPath();
            c.moveTo(128, 20); c.lineTo(236, 128); c.lineTo(128, 236); c.lineTo(20, 128);
            c.closePath();
            c.setFill("#e8b81c");
            c.fill();
        } else if (shape == "circle") {
            c.arc(128, 128, 122, 0, ter::PI * 2);
            c.setFill("#c41e1a");
            c.fill();
            c.beginPath();
            c.arc(128, 128, 96, 0, ter::PI * 2);
            c.setFill("#f2f2f2");
            c.fill();
        } else {
            c.roundRect(6, 6, 244, 244, 18);
            c.setFill("#1f4e8c");
            c.fill();
        }
        c.setStroke("#151515");
        c.setFill("#151515");
        c.setLineWidth(18);
        c.setLineCap(true);
        if (symbol == "curve_left" || symbol == "curve_right") {
            float s = symbol == "curve_left" ? -1.0f : 1.0f;
            c.beginPath();
            c.moveTo(128 - s * 10, 196);
            c.lineTo(128 - s * 10, 140);
            c.quadTo(128 - s * 10, 96, 128 + s * 30, 86);
            c.stroke();
            c.beginPath();   // arrow head
            c.moveTo(128 + s * 62, 80); c.lineTo(128 + s * 22, 56); c.lineTo(128 + s * 26, 112);
            c.closePath();
            c.fill();
        } else if (symbol == "animal") {
            // a simple deer: body, neck, head, legs, antlers
            c.beginPath();
            c.ellipse(118, 140, 46, 22);
            c.fill();
            c.setLineWidth(10);
            for (float x : {88.0f, 100.0f, 136.0f, 150.0f}) {
                c.beginPath(); c.moveTo(x, 150); c.lineTo(x + (x < 120 ? -4 : 4), 196); c.stroke();
            }
            c.beginPath(); c.moveTo(156, 130); c.lineTo(176, 92); c.stroke();
            c.beginPath(); c.ellipse(184, 86, 16, 10, -0.3f); c.fill();
            c.setLineWidth(6);
            c.beginPath(); c.moveTo(176, 78); c.lineTo(168, 52); c.moveTo(170, 62); c.lineTo(156, 56); c.stroke();
            c.beginPath(); c.moveTo(188, 76); c.lineTo(196, 50); c.moveTo(193, 60); c.lineTo(208, 54); c.stroke();
        }
        if (!text.empty()) {
            c.setTextAlign(ter::TextAlign::Center);
            c.setFill(shape == "rect" ? "#f2f2f2" : "#151515");
            if (symbol == "number" || symbol == "none") {
                c.setFont("Inter", text.size() > 3 ? 56 : 96, 700);
                c.fillText(text, 128, 160);
            } else {
                c.setFont("Mali", 26, 600);
                c.fillText(text, 128, 226);
            }
        }
        weather(c, 0.4f);
    });
    float size = j.value("size", 0.75f), height = j.value("height", 2.2f);
    auto face = ter::lambert(tex);
    face->alphaTest = 0.5f;
    face->emissiveMap = tex;
    face->emissive = glm::vec3(j.value("glow", 0.25f));   // retro-reflective sheeting
    auto back = ter::lambert(tex, ter::hexc(0x55595d));
    back->alphaTest = 0.5f;
    k.put(ter::makeMesh(tubeGeo({0, 0, -0.04f}, {0, height + size / 2, -0.04f}, 0.035f, 0.032f, 8), kitMat("steel", glm::vec3(1), {0.3f, 2})), {});
    k.put(ter::makeMesh(ter::geo::plane(size, size), face), {0, height, 0});
    k.put(ter::makeMesh(ter::geo::plane(size, size), back), {0, height, -0.005f}, {0, ter::PI, 0});
    k.block({0, 1.0f, -0.04f}, {0.15f, 2.0f, 0.15f});
}

// Placeholder person built from primitives. Faces +Z; World turns it toward the player.
void Builder::companion(const json& j) {
    glm::vec3 pos = vec3Of(j, "pos");
    auto g = ter::makeGroup("companion");
    g->position = pos;
    // Blender model (assets/models/npc.glb, source modelResource/npc_source.blend + scripts/npc_build.py):
    // ~2,800 triangles, faces +Z, feet at y = 0. Ten rigid parts with pivots at the joints (the engine has no skinning),
    // animated by the clips in the file (scripts/npc_anims.py) through world.companionAnim.
    if (auto model = ter::loadGltf(ter::assetPath("models/npc.glb"))) {
        g->add(model->root);
        world.companionAnim = std::make_shared<Animator>(model->animations);
        world.companionAnim->play(j.value("anim", std::string("idle")));
    } else {
        std::fprintf(stderr, "companion: could not load models/npc.glb\n");
    }
    world.companionFacesPlayer = j.value("facePlayer", true);
    g->rotation.y = radians(j.value("rotY", 0.0f));
    add(g);
    world.companion = g;
    if (world.companionFacesPlayer) collide(pos + glm::vec3(0, 0.9f, 0), {0.5f, 1.8f, 0.5f});
}

// The old woman: assets/models/grandma.glb (source modelResource/grandma_source.blend + scripts/grandma_*.py),
// ~3,000 triangles, 1.5 m, stooped, faces +Z. Same jointed rig as the companion; clips from grandma_anims.py.
// The head carries two faces, grandma_face_normal and grandma_face_scary (+ grandma_eyes_glow); "face" picks one.
void Builder::grandma(const json& j) {
    auto model = ter::loadGltf(ter::assetPath("models/grandma.glb"));
    if (!model) {
        std::fprintf(stderr, "grandma: could not load models/grandma.glb\n");
        return;
    }
    Grandma g;
    g.id = j.value("id", std::string("grandma"));
    g.root = ter::makeGroup(g.id);
    g.root->position = vec3Of(j, "pos");
    g.root->rotation.y = radians(j.value("rotY", 0.0f));
    g.root->visible = j.value("visible", true);
    g.root->add(model->root);
    model->root->traverse([&](ter::Object3D& o) {
        if (o.name == "grandma_face_normal") g.faceNormal = &o;
        if (o.name == "grandma_face_scary") g.faceScary = &o;
    });
    g.anim = std::make_shared<Animator>(model->animations);
    g.anim->play(j.value("anim", std::string("idle")));
    g.facesPlayer = j.value("facePlayer", true);
    g.setScary(j.value("face", std::string("normal")) == "scary");
    add(g.root);
    world.grandmas.push_back(std::move(g));
}

// Any glTF model as a plain, decorative object (e.g. a showcase of the game's models).
// Keys: file (under assets/), pos, rotY, scale, anim (clip to loop), hide (node names to hide), collide [w, h, d]
void Builder::model(const json& j) {
    std::string file = j.value("file", std::string());
    auto model = ter::loadGltf(ter::assetPath(file));
    if (!model) {
        std::fprintf(stderr, "model: could not load %s\n", file.c_str());
        return;
    }
    Kit k = startPrefab(*this, j, "model");
    model->root->scale = glm::vec3(j.value("scale", 1.0f));
    k.group->add(model->root);
    std::vector<std::string> hide = j.value("hide", std::vector<std::string>());
    model->root->traverse([&](ter::Object3D& o) {
        if (std::find(hide.begin(), hide.end(), o.name) != hide.end()) o.visible = false;
    });
    if (std::string clip = j.value("anim", std::string()); !clip.empty() && !model->animations.empty()) {
        auto anim = std::make_shared<Animator>(model->animations);
        anim->play(clip);
        world.animators.push_back(anim);
    }
    if (j.contains("collide")) {
        glm::vec3 size = vec3Of(j, "collide");
        k.block({0, size.y / 2, 0}, size);
    }
}

// The spirit house under the old banyan (assets/models/shrine.glb, source modelResource/grandma_source.blend +
// scripts/shrine_build.py): ~2,900 triangles, the house faces +Z. "pos" is the foot of its post. Remembers where its
// door is (world.shrineDoor) for the old woman's head; colliders on the post and the tree trunk.
void Builder::spiritShrine(const json& j) {
    auto model = ter::loadGltf(ter::assetPath("models/shrine.glb"));
    if (!model) {
        std::fprintf(stderr, "spirit_shrine: could not load models/shrine.glb\n");
        return;
    }
    glm::vec3 pos = vec3Of(j, "pos");
    float yaw = radians(j.value("rotY", 0.0f));
    auto g = ter::makeGroup("spirit_shrine");
    g->position = pos;
    g->rotation.y = yaw;
    g->add(model->root);
    add(g);
    auto toWorld = [&](glm::vec3 local) {
        return pos + glm::vec3(local.x * std::cos(yaw) + local.z * std::sin(yaw), local.y, -local.x * std::sin(yaw) + local.z * std::cos(yaw));
    };
    glm::vec3 door{0, 1.42f, 0.12f};
    model->root->traverse([&](ter::Object3D& o) {
        if (o.name == "shrine_door") door = o.position;
    });
    world.shrineDoor = toWorld(door);
    world.hasShrine = true;
    collide(toWorld({0, 0.6f, 0}), {0.4f, 1.2f, 0.4f});
    collide(toWorld({0.95f, 1.5f, -0.85f}), {1.1f, 3.0f, 1.1f});
}
