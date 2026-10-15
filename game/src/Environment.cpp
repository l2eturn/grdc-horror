// Landscape builders: ground, road strips, cliff, guardrail, roadside posts, power lines, rocks, grass,
// ground mist and the sky (dome, distant mountains, moon). Everything is low-poly with small textures
// from the texture kit (PS2 style), and placed from map.json.
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include "MeshKit.h"
#include "Noise.h"
#include "Prefabs.h"
#include "Textures.h"

namespace {

// Angle for rotation.y that turns local +X toward direction d (x, z)
float yawAlong(glm::vec2 d) { return std::atan2(-d.y, d.x); }

// Instanced mesh from a list of matrices
ter::ObjPtr instanced(const ter::GeoPtr& geo, const ter::MatPtr& mat, const std::vector<glm::mat4>& matrices) {
    auto obj = ter::makeInstanced(geo, {mat}, matrices.size());
    for (size_t i = 0; i < matrices.size(); ++i) obj->instances[i] = matrices[i];
    return obj;
}

// A lumpy low-poly rock: a dodecahedron with every corner pushed in or out, squashed, flat-shaded
ter::GeoPtr rockGeometry(int seed) {
    auto g = ter::geo::dodecahedron(1, 1);
    for (glm::vec3& p : g->position) {
        // same corner -> same push, so the faces stay joined
        int qx = (int)std::lround(p.x * 1000), qy = (int)std::lround(p.y * 1000), qz = (int)std::lround(p.z * 1000);
        float k = 0.72f + 0.5f * hash(qx * 7 + qz, qy, seed);
        p = glm::normalize(p) * k;
        p.y *= 0.62f;
    }
    return ter::geo::worldUV(g, 0.9f);
}

}  // namespace

// Ground: a big plane with a tiled texture, darkened and lightened in large patches by vertex colours
void Builder::ground(const json& j) {
    glm::vec2 lo = vec2Of(j, "min", {-45, -45}), hi = vec2Of(j, "max", {45, 45});
    glm::vec2 size = hi - lo, mid = (lo + hi) * 0.5f;
    auto g = ter::geo::plane(size.x, size.y, std::max(1, (int)(size.x / 1.5f)), std::max(1, (int)(size.y / 1.5f)));
    g->rotateX(-ter::PI / 2);
    g->translate(mid.x, 0, mid.y);
    int seed = j.value("seed", 3);
    g->color.resize(g->position.size());
    for (size_t i = 0; i < g->position.size(); ++i) {
        glm::vec3 p = g->position[i];
        float big = worldNoise(p.x, p.z, 9, 3, seed), small = worldNoise(p.x, p.z, 2.5f, 2, seed + 7);
        g->color[i] = glm::vec3((0.5f + 0.7f * big) * (0.85f + 0.3f * small));
    }
    auto mat = ter::lambert(texture(j.value("texture", std::string("grass"))), colorOf(j, "color", glm::vec3(1)));
    mat->vertexColors = true;
    mat->mapRepeat = size / j.value("tile", 4.0f);
    add(ter::makeMesh(g, mat));
    groundGeo = g;
}

// A flat textured band from "from" to "to" ([x, z]): roads, shoulders, paths.
// "tile" = meters along per texture repeat; "tileWidth" = meters across (default: the whole width = 1 repeat);
// "roughness" (0..1) = glossy surface, e.g. 0.4 for damp asphalt.
void Builder::strip(const json& j) {
    glm::vec2 a = vec2Of(j, "from"), b = vec2Of(j, "to"), d = b - a;
    float len = glm::length(d), width = j.value("width", 3.0f);
    auto g = ter::geo::plane(len, width, std::max(1, (int)(len / 4)), 1);
    g->rotateX(-ter::PI / 2);
    // "roughness" makes it glossy (wet asphalt reflecting the lamps); without it the surface is matte
    ter::MatPtr mat;
    if (j.contains("roughness")) {
        mat = ter::standard(colorOf(j, "color", glm::vec3(1)), j["roughness"].get<float>(), 0);
        mat->map = texture(j.value("texture", std::string("dirt")));
    } else {
        mat = ter::lambert(texture(j.value("texture", std::string("dirt"))), colorOf(j, "color", glm::vec3(1)));
    }
    mat->mapRepeat = {len / j.value("tile", 4.0f), width / j.value("tileWidth", width)};
    // drawn on top of the ground without z-fighting; higher "layer" wins
    mat->polygonOffset = true;
    mat->polygonOffsetFactor = -1;
    mat->polygonOffsetUnits = -2.0f * j.value("layer", 1);
    auto mesh = ter::makeMesh(g, mat);
    glm::vec2 mid = (a + b) * 0.5f;
    mesh->position = {mid.x, j.value("y", 0.005f), mid.y};
    mesh->rotation.y = yawAlong(d);
    add(mesh);
}

// A rock face dropping from the edge "from"-"to" (at ground level) into the valley, leaning toward "out"
void Builder::cliff(const json& j) {
    glm::vec2 a = vec2Of(j, "from"), b = vec2Of(j, "to"), out = glm::normalize(vec2Of(j, "out", {0, -1}));
    float depth = j.value("depth", 30.0f), lean = j.value("lean", 8.0f), tile = j.value("tile", 6.0f);
    float len = glm::length(b - a);
    int cols = std::max(2, (int)(len / 2.5f)), rows = 8, seed = j.value("seed", 5);
    auto g = std::make_shared<ter::Geometry>();
    for (int r = 0; r <= rows; ++r)
        for (int c = 0; c <= cols; ++c) {
            float t = (float)c / cols, s = (float)r / rows;
            glm::vec2 edge = glm::mix(a, b, t);
            // jagged: push in and out with noise, more the further down (the top row meets the ground exactly)
            float jag = (worldNoise(t * len, s * depth, 4, 3, seed) - 0.5f) * 5.0f * std::sqrt(s);
            glm::vec2 xz = edge + out * (lean * s + jag);
            g->position.push_back({xz.x, -depth * s, xz.y});
            g->uv.push_back({t * len / tile, s * depth / tile});
        }
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            uint32_t i = r * (cols + 1) + c, k = i + cols + 1;
            g->index.insert(g->index.end(), {i, k, i + 1, i + 1, k, k + 1});
        }
    g->computeVertexNormals();
    auto mat = ter::lambert(texture("cliff"));
    mat->side = ter::Side::Double;
    add(ter::makeMesh(g, mat));
}

// W-beam guardrail: the real two-bump steel profile, mounted on blocks in front of posts every "spacing" m,
// with small amber reflectors. The bumps face +Z of the line's local frame (for from→to along +X: toward +Z).
// Keep it axis-aligned: the collider is a box around it.
void Builder::guardrail(const json& j) {
    glm::vec2 a = vec2Of(j, "from"), b = vec2Of(j, "to"), d = b - a, mid = (a + b) * 0.5f;
    float len = glm::length(d), h = j.value("height", 0.75f), spacing = j.value("spacing", 2.0f), yaw = yawAlong(d);
    glm::quat q = ter::eulerToQuat({0, yaw, 0});

    // the rail: a strip extruded along local X from the W cross-section (y, z)
    const glm::vec2 profile[] = {{-0.155f, 0}, {-0.12f, 0.03f}, {-0.085f, 0.04f}, {-0.05f, 0.03f}, {-0.02f, 0.005f},
                                 {0.02f, 0.005f}, {0.05f, 0.03f}, {0.085f, 0.04f}, {0.12f, 0.03f}, {0.155f, 0}};
    const int np = sizeof(profile) / sizeof(profile[0]);
    auto rail = std::make_shared<ter::Geometry>();
    for (int e = 0; e < 2; ++e)
        for (int i = 0; i < np; ++i) {
            float x = (e ? 0.5f : -0.5f) * len;
            rail->position.push_back({x, h - 0.16f + profile[i].x, 0.08f + profile[i].y});
            rail->uv.push_back({x / 4, (float)i / (np - 1)});
        }
    for (int i = 0; i + 1 < np; ++i) {
        uint32_t k = i, n = i + np;
        rail->index.insert(rail->index.end(), {k, n, k + 1, k + 1, n, n + 1});
    }
    rail->computeVertexNormals();
    auto railMat = ter::lambert(texture("metal"), colorOf(j, "color", glm::vec3(1)));
    railMat->side = ter::Side::Double;
    add(meshAt(rail, railMat, {mid.x, 0, mid.y}, {0, yaw, 0}));

    // posts (behind the rail) and the spacer blocks between post and rail, instanced
    auto postGeo = ter::geo::box(0.1f, h + 0.05f, 0.1f);
    postGeo->translate(0, (h + 0.05f) / 2, -0.05f);
    auto blockGeo = ter::geo::box(0.08f, 0.2f, 0.12f);
    blockGeo->translate(0, h - 0.16f, 0.02f);
    auto reflectorGeo = ter::geo::box(0.07f, 0.05f, 0.012f);
    reflectorGeo->translate(0, h - 0.16f, 0.13f);
    std::vector<glm::mat4> posts, reflectors;
    int count = 0;
    for (float t = 0; t <= len; t += spacing, ++count) {
        glm::vec2 p = a + d * (t / len);
        posts.push_back(ter::composeMatrix({p.x, 0, p.y}, q, glm::vec3(1)));
        if (count % 2 == 0) reflectors.push_back(posts.back());
    }
    add(instanced(postGeo, kitMat("steel", ter::hexc(0x9a9ea2), {0.2f, 0.6f}), posts));
    add(instanced(blockGeo, kitMat("steel", ter::hexc(0x7a7e82), {0.2f, 0.3f}), posts));
    add(instanced(reflectorGeo, ter::basic(ter::hexc(0x9a5a12)), reflectors));
    world.colliders.push_back(boundsOf({mid.x, h / 2, mid.y}, {len, h, 0.3f}, {0, yaw, 0}));
}

// Roadside guide posts: white posts with an orange reflector, every "spacing" meters
void Builder::delineators(const json& j) {
    glm::vec2 a = vec2Of(j, "from"), b = vec2Of(j, "to"), d = b - a;
    float len = glm::length(d), spacing = j.value("spacing", 12.0f);
    glm::quat q = ter::eulerToQuat({0, yawAlong(d), 0});
    std::vector<glm::mat4> at;
    for (float t = 0; t <= len; t += spacing) {
        glm::vec2 p = a + d * (t / len);
        at.push_back(ter::composeMatrix({p.x, 0, p.y}, q, glm::vec3(1)));
        collide({p.x, 0.5f, p.y}, {0.12f, 1, 0.12f});
    }
    auto postGeo = ter::geo::box(0.1f, 1.0f, 0.1f);
    postGeo->translate(0, 0.5f, 0);
    auto postMat = ter::lambert(texture("concrete"), ter::hexc(0xe6e4de));
    postMat->mapRepeat = {0.2f, 0.6f};
    auto bandGeo = ter::geo::box(0.104f, 0.12f, 0.104f);
    bandGeo->translate(0, 0.72f, 0);
    // the reflector pokes out on both sides; unlit so it glows faintly like retro-reflective tape
    auto reflectorGeo = ter::geo::box(0.05f, 0.1f, 0.112f);
    reflectorGeo->translate(0, 0.86f, 0);
    add(instanced(postGeo, postMat, at));
    add(instanced(bandGeo, ter::lambertColor(ter::hexc(0x151515)), at));
    add(instanced(reflectorGeo, ter::basic(colorOf(j, "reflector", ter::hexc(0x8a4a10))), at));
}

// Concrete utility poles with a cross-arm and three sagging wires between them
void Builder::powerLine(const json& j) {
    glm::vec2 a = vec2Of(j, "from"), b = vec2Of(j, "to"), d = b - a;
    float len = glm::length(d), spacing = j.value("spacing", 24.0f), h = j.value("height", 8.5f), sag = j.value("sag", 0.7f);
    glm::vec2 dir = d / len;
    glm::vec3 across = glm::vec3(-dir.y, 0, dir.x);   // horizontal, perpendicular to the line
    glm::quat q = ter::eulerToQuat({0, yawAlong(d), 0});

    std::vector<glm::vec3> bases;
    for (float t = 0; t <= len + 0.01f; t += spacing) bases.push_back(glm::vec3(a.x, 0, a.y) + glm::vec3(dir.x, 0, dir.y) * t);

    auto poleGeo = ter::geo::cylinder(0.1f, 0.15f, h, 8);
    poleGeo->translate(0, h / 2, 0);
    auto armGeo = ter::geo::box(0.1f, 0.1f, 1.8f);   // local z = across the line
    armGeo->translate(0, h - 0.4f, 0);
    std::vector<glm::mat4> poles;
    for (glm::vec3 p : bases) {
        poles.push_back(ter::composeMatrix(p, q, glm::vec3(1)));
        collide(p + glm::vec3(0, h / 2, 0), {0.3f, h, 0.3f});
    }
    auto concrete = ter::lambert(texture("concrete"));
    concrete->mapRepeat = {0.5f, 3};
    add(instanced(poleGeo, concrete, poles));
    add(instanced(armGeo, ter::lambertColor(ter::hexc(0x3a3532)), poles));

    // wires: parabola between neighbouring poles, 10 straight pieces each
    std::vector<ter::GeoPtr> pieces;
    const glm::vec3 attach[3] = {across * -0.8f + glm::vec3(0, h - 0.33f, 0), across * 0.8f + glm::vec3(0, h - 0.33f, 0), glm::vec3(0, h + 0.05f, 0)};
    for (size_t k = 0; k + 1 < bases.size(); ++k)
        for (const glm::vec3& off : attach) {
            glm::vec3 p0 = bases[k] + off, p1 = bases[k + 1] + off, prev = p0;
            for (int s = 1; s <= 10; ++s) {
                float t = s / 10.0f;
                glm::vec3 p = glm::mix(p0, p1, t) - glm::vec3(0, sag * 4 * t * (1 - t), 0);
                pieces.push_back(beamGeo(prev, p, 0.035f, 0.035f));
                prev = p;
            }
        }
    if (!pieces.empty()) add(ter::makeMesh(ter::geo::merge(pieces), ter::lambertColor(ter::hexc(0x0e0e0e))));
}

// Scattered rocks in three shapes; big ones block the player. "scale": [min, max]
void Builder::rocks(const json& j) {
    glm::vec2 scale = vec2Of(j, "scale", {0.25f, 1.1f});
    std::vector<glm::mat4> variants[3];
    scatter(j, 30, [&](glm::vec2 p, ter::Rng& rng) {
        float s = scale.x + rng.f() * (scale.y - scale.x);
        glm::vec3 sv(s * (0.8f + rng.f() * 0.5f), s * (0.7f + rng.f() * 0.6f), s * (0.8f + rng.f() * 0.5f));
        glm::quat q = ter::eulerToQuat({0, rng.f() * ter::PI * 2, 0});
        variants[(int)(rng.f() * 3) % 3].push_back(ter::composeMatrix({p.x, s * 0.1f, p.y}, q, sv));
        if (s > 0.55f) collide({p.x, s * 0.4f, p.y}, {sv.x * 1.6f, s * 0.8f, sv.z * 1.6f});
    });
    auto mat = ter::lambert(texture("rock"), colorOf(j, "color", glm::vec3(1)));
    for (int v = 0; v < 3; ++v)
        if (!variants[v].empty()) add(instanced(rockGeometry(v * 17 + 3), mat, variants[v]));
}

// Grass tufts: two crossed alpha-tested cards per tuft, all in one instanced draw
void Builder::grass(const json& j) {
    auto card = ter::geo::plane(0.7f, 0.55f);
    card->translate(0, 0.275f, 0);
    auto card2 = ter::geo::plane(0.7f, 0.55f);
    card2->translate(0, 0.275f, 0);
    card2->rotateY(ter::PI / 2);
    auto geo = ter::geo::merge({card, card2});
    std::vector<glm::mat4> tufts;
    scatter(j, 600, [&](glm::vec2 p, ter::Rng& rng) {
        float s = 0.6f + rng.f() * 0.8f;
        glm::quat q = ter::eulerToQuat({0, rng.f() * ter::PI, 0});
        tufts.push_back(ter::composeMatrix({p.x, 0, p.y}, q, glm::vec3(s, s * (0.7f + rng.f() * 0.6f), s)));
    });
    auto mat = ter::lambert(texture("grasscard"), colorOf(j, "color", glm::vec3(1)));
    mat->alphaTest = 0.5f;
    mat->side = ter::Side::Double;
    add(instanced(geo, mat, tufts));
}

// A flat sheet of drifting fog just above the ground. Unlit and transparent; "speed" = repeats per second.
void Builder::mist(const json& j) {
    glm::vec3 pos = vec3Of(j, "pos");
    glm::vec2 size = vec2Of(j, "size", {80, 80});
    auto g = ter::geo::plane(size.x, size.y);
    g->rotateX(-ter::PI / 2);
    auto mat = ter::basic(colorOf(j, "color", ter::hexc(0x59616b)), texture("mist"));
    mat->transparent = true;
    mat->depthWrite = false;
    mat->opacity = j.value("opacity", 0.8f);
    mat->side = ter::Side::Double;
    mat->mapRepeat = size / j.value("tile", 14.0f);
    auto mesh = ter::makeMesh(g, mat);
    mesh->position = pos;
    add(mesh);
    world.mists.push_back({mat, vec2Of(j, "speed", {0.005f, 0.002f})});
}

// Drifting smoke: soft lit sprites scattered in an area, carried by "wind" (m/s) and wrapping around.
// "size" [min, max] meters, "height" [min, max] of the puff centres, "opacity", "color".
void Builder::smoke(const json& j) {
    glm::vec2 size = vec2Of(j, "size", {4, 8}), height = vec2Of(j, "height", {0.8f, 2.5f});
    glm::vec2 lo = vec2Of(j, "min", {-30, -30}), hi = vec2Of(j, "max", {30, 30}), wind = vec2Of(j, "wind", {0.45f, 0.18f});
    glm::vec3 color = colorOf(j, "color", ter::hexc(0x9aa4b0));
    float opacity = j.value("opacity", 0.35f);
    scatter(j, 40, [&](glm::vec2 p, ter::Rng& rng) {
        auto mat = ter::spriteMaterial(texture(rng.f() < 0.5f ? "smoke" : "smoke2"), color, opacity * (0.6f + 0.4f * rng.f()));
        mat->transparent = true;
        mat->depthWrite = false;
        if (rng.f() < 0.5f) {   // mirror half of them for variety
            mat->mapRepeat = {-1, 1};
            mat->mapOffset = {1, 0};
        }
        auto sprite = ter::makeSprite(mat);
        float s = size.x + rng.f() * (size.y - size.x);
        sprite->scale = {s, s * 0.55f, 1};
        glm::vec3 start(p.x, height.x + rng.f() * (height.y - height.x), p.y);
        sprite->position = start;
        add(sprite);
        world.smoke.push_back({sprite, start, rng.f() * 6.28f, lo, hi, wind});
    });
}

// Flat decals lying on the ground: fallen needles, oil stains, skid marks...
// Scatter keys + "texture" (a kit RGBA texture), "size" [min, max] (m), "aspect" (length / width),
// "color" (tint), "opacity", "y" (height above the ground), "align" (0 = random rotation, 1 = along X).
void Builder::decals(const json& j) {
    auto tex = texture(j.value("texture", std::string("litter")));
    glm::vec2 size = vec2Of(j, "size", {0.6f, 1.4f});
    float aspect = j.value("aspect", 1.0f), y = j.value("y", 0.015f);
    bool align = j.value("align", 0) != 0;
    auto mat = ter::lambert(tex, colorOf(j, "color", glm::vec3(1)));
    mat->transparent = true;
    mat->opacity = j.value("opacity", 1.0f);
    mat->depthWrite = false;
    mat->polygonOffset = true;
    mat->polygonOffsetFactor = -1;
    mat->polygonOffsetUnits = -6;
    auto quad = ter::geo::plane(1, 1);
    quad->rotateX(-ter::PI / 2);
    std::vector<glm::mat4> at;
    scatter(j, 20, [&](glm::vec2 p, ter::Rng& rng) {
        float s = size.x + rng.f() * (size.y - size.x);
        float yaw = align ? (rng.f() - 0.5f) * 0.15f : rng.f() * ter::PI * 2;
        at.push_back(ter::composeMatrix({p.x, y, p.y}, ter::eulerToQuat({0, yaw, 0}), {s * aspect, 1, s}));
    });
    add(instanced(quad, mat, at));
}

// Rain puddles on the road: dark, very glossy, so the street lamps and the flashlight glint in them.
// Scatter keys + "size" [min, max].
void Builder::puddles(const json& j) {
    auto mat = ter::standard(colorOf(j, "color", ter::hexc(0x1a1d20)), j.value("roughness", 0.08f), 0);
    mat->map = texture("puddle");
    mat->transparent = true;
    mat->depthWrite = false;
    mat->polygonOffset = true;
    mat->polygonOffsetFactor = -1;
    mat->polygonOffsetUnits = -8;
    glm::vec2 size = vec2Of(j, "size", {0.6f, 2.0f});
    auto quad = ter::geo::plane(1, 1);
    quad->rotateX(-ter::PI / 2);
    std::vector<glm::mat4> at;
    scatter(j, 10, [&](glm::vec2 p, ter::Rng& rng) {
        float s = size.x + rng.f() * (size.y - size.x);
        at.push_back(ter::composeMatrix({p.x, 0.016f, p.y}, ter::eulerToQuat({0, rng.f() * ter::PI * 2, 0}), {s * (1 + rng.f()), 1, s}));
    });
    add(instanced(quad, mat, at));
}

// Concrete drainage channel along the road edge: two low curbs and a damp, dark bottom. No colliders
// (you can step over it). "from", "to", "width".
void Builder::gutter(const json& j) {
    glm::vec2 a = vec2Of(j, "from"), b = vec2Of(j, "to"), d = b - a, mid = (a + b) * 0.5f;
    float len = glm::length(d), width = j.value("width", 0.5f), yaw = yawAlong(d);
    auto curb = kitMat("concrete", ter::hexc(0xa09c94), {len / 1.5f, 1});
    glm::vec3 across(std::sin(yaw), 0, std::cos(yaw));   // local +Z of the line
    for (float side : {-1.0f, 1.0f})
        add(meshAt(ter::geo::box(len, 0.09f, 0.08f), curb, glm::vec3(mid.x, 0.045f, mid.y) + across * (side * (width / 2 - 0.04f)), {0, yaw, 0}));
    auto bottom = ter::standard(ter::hexc(0x6a6a66), 0.25f, 0);
    bottom->map = texture("concrete");
    bottom->mapRepeat = {len / 1.5f, 0.4f};
    auto g = ter::geo::plane(len, width - 0.16f);
    g->rotateX(-ter::PI / 2);
    add(meshAt(g, bottom, {mid.x, 0.01f, mid.y}, {0, yaw, 0}));
}

// Sky: a gradient dome, rings of distant mountain silhouettes and a hazy moon.
// The whole group follows the player (see updateWorld), so it behaves as if infinitely far away.
void Builder::sky(const json& j) {
    auto group = ter::makeGroup("sky");
    world.sky = group;
    add(group);
    glm::vec3 horizon = colorOf(j, "horizon", world.env.fogColor);
    glm::vec3 zenith = colorOf(j, "zenith", horizon * 0.5f);
    glm::vec3 below = colorOf(j, "below", horizon * 0.6f);

    const float domeRadius = 90;
    auto dome = ter::geo::sphere(domeRadius, 24, 16);
    dome->color.resize(dome->position.size());
    for (size_t i = 0; i < dome->position.size(); ++i) {
        float y = dome->position[i].y / domeRadius;
        dome->color[i] = y >= 0 ? glm::mix(horizon, zenith, ter::smoothstep(0.0f, 0.55f, y)) : glm::mix(horizon, below, ter::smoothstep(0.0f, 0.12f, -y));
    }
    auto domeMat = ter::basic(glm::vec3(1));
    domeMat->vertexColors = true;
    domeMat->side = ter::Side::Back;
    // the fog shader only gathers fog up to FOG_MAX_DIST, so the sky shows through where the fog is thin (looking up)
    domeMat->depthWrite = false;
    group->add(ter::makeMesh(dome, domeMat));

    // mountains: a closed ring of peaks; heights from tileable noise around the circle
    for (const json& m : j.value("mountains", json::array())) {
        float radius = m.value("radius", 70.0f);
        glm::vec2 height = vec2Of(m, "height", {6, 16});
        int seed = m.value("seed", 1), n = 160;
        auto g = std::make_shared<ter::Geometry>();
        for (int i = 0; i <= n; ++i) {
            float u = (float)i / n, a = u * ter::PI * 2;
            float peak = std::pow(fbm(u, 0.5f, 9, 1, 4, seed), 1.6f) * 1.6f;
            float top = height.x + (height.y - height.x) * glm::clamp(peak, 0.0f, 1.0f);
            glm::vec3 dir(std::cos(a), 0, std::sin(a));
            g->position.push_back(dir * radius + glm::vec3(0, -30, 0));
            g->position.push_back(dir * radius + glm::vec3(0, top, 0));
        }
        for (int i = 0; i < n; ++i) {
            uint32_t k = i * 2;
            g->index.insert(g->index.end(), {k, k + 1, k + 2, k + 2, k + 1, k + 3});
        }
        g->computeVertexNormals();
        auto mat = ter::basic(colorOf(m, "color", horizon * 0.7f));
        mat->fog = m.value("fog", true);
        mat->side = ter::Side::Double;
        group->add(ter::makeMesh(g, mat));
    }

    // moon: a soft disc and a wide faint halo, seen through the haze
    if (j.contains("moon")) {
        const json& mo = j["moon"];
        glm::vec3 dir = glm::normalize(vec3Of(mo, "dir", {0.4f, 0.35f, -1}));
        glm::vec3 color = colorOf(mo, "color", ter::hexc(0xb8c2cc));
        float size = mo.value("size", 5.0f);
        glm::quat facing = glm::quatLookAt(dir, glm::vec3(0, 1, 0));   // +Z of the disc points back at the player
        struct Layer {
            float radius, opacity;
            const char* tex;
        };
        for (const Layer& l : {Layer{size * 4.0f, mo.value("halo", 0.12f), "glow"}, Layer{size, mo.value("brightness", 0.55f), "disc"}}) {
            auto [radius, opacity, tex] = l;
            auto mat = ter::basic(color, texture(tex));
            mat->transparent = true;
            mat->opacity = opacity;
            mat->blending = ter::Blending::Additive;
            mat->fog = false;
            mat->depthWrite = false;
            mat->side = ter::Side::Double;
            auto disc = ter::makeMesh(ter::geo::circle(radius, 32), mat);
            disc->position = dir * (domeRadius - 6);
            disc->setQuaternion(facing);
            group->add(disc);
        }
    }
}
