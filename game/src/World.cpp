#include "World.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include "Prefabs.h"
#include "Sounds.h"
#include "Textures.h"
#include "Trees.h"

// ---------------------------------------------------------------- JSON helpers

float radians(float degrees) { return degrees * ter::PI / 180.0f; }

glm::vec3 vec3Of(const json& j, const char* key, glm::vec3 fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() < 3) return fallback;
    return {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()};
}

glm::vec2 vec2Of(const json& j, const char* key, glm::vec2 fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() < 2) return fallback;
    return {(*it)[0].get<float>(), (*it)[1].get<float>()};
}

glm::vec3 colorOf(const json& j, const char* key, glm::vec3 fallback) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return fallback;
    std::string s = it->get<std::string>();
    if (s.size() != 7 || s[0] != '#') return fallback;
    try {
        return ter::hexc((uint32_t)std::stoul(s.substr(1), nullptr, 16));
    } catch (...) {
        return fallback;
    }
}

ter::Box boundsOf(glm::vec3 center, glm::vec3 size, glm::vec3 rotation) {
    glm::mat3 r = glm::mat3_cast(ter::eulerToQuat(rotation));
    glm::vec3 half = size * 0.5f, lo(1e9f), hi(-1e9f);
    for (int i = 0; i < 8; ++i) {
        glm::vec3 corner((i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z);
        glm::vec3 p = center + r * corner;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    return {lo, hi};
}

// ---------------------------------------------------------------- scatter and trees
// (props such as the shelter, lamps and signs are in Props.cpp; landscape pieces in Environment.cpp)

void Builder::scatter(const json& j, int defaultCount, const std::function<void(glm::vec2, ter::Rng&)>& place) {
    glm::vec2 lo = vec2Of(j, "min", {-30, -30}), hi = vec2Of(j, "max", {30, 30});
    std::vector<std::pair<glm::vec2, glm::vec2>> clear;
    for (const json& c : j.value("clear", json::array())) clear.push_back({vec2Of(c, "min"), vec2Of(c, "max")});
    ter::Rng rng(j.value("seed", 7));
    int count = j.value("count", defaultCount), placed = 0;
    for (int tries = 0; placed < count && tries < count * 20; ++tries) {
        glm::vec2 p(lo.x + rng.f() * (hi.x - lo.x), lo.y + rng.f() * (hi.y - lo.y));
        bool blocked = false;
        for (auto& [cmin, cmax] : clear)
            if (p.x > cmin.x && p.x < cmax.x && p.y > cmin.y && p.y < cmax.y) blocked = true;
        if (blocked) continue;
        place(p, rng);
        ++placed;
    }
}

void Builder::tree(glm::vec3 base, float scale, float yaw, int variant, glm::vec2 tilt) {
    glm::quat q = ter::eulerToQuat({tilt.x, yaw, tilt.y});
    trees.push_back({variant, ter::composeMatrix(base, q, glm::vec3(scale)), base});
    collide(base + glm::vec3(0, 1, 0), {0.5f * scale, 2, 0.5f * scale});
}

// Scatter trees in a rectangle, keeping "clear" rectangles empty (road, shelter, paths).
// "dead" = share of dead trees, "saplings" = share of small young pines.
void Builder::forest(const json& j) {
    float dead = j.value("dead", 0.08f), saplings = j.value("saplings", 0.15f);
    scatter(j, 100, [&](glm::vec2 p, ter::Rng& rng) {
        float kind = rng.f(), scale = 0.8f + rng.f() * 0.45f;
        int variant = (int)(rng.f() * 3) % 3;
        if (kind < dead) variant = 3;
        else if (kind < dead + saplings) scale = 0.2f + rng.f() * 0.25f;
        glm::vec2 tilt((rng.f() - 0.5f) * 0.08f, (rng.f() - 0.5f) * 0.08f);
        tree({p.x, 0, p.y}, scale, rng.f() * ter::PI * 2, variant, tilt);
    });
}

// Build the tree variants once and draw every collected tree with instancing: per variant one draw for
// the wood and one for the needles. Then darken the ground under the trees (fallen needles, shade).
void Builder::finishTrees() {
    if (trees.empty()) return;
    const TreeModel models[4] = {makePine(1), makePine(2), makePine(3), makeDeadTree(4)};
    auto bark = ter::lambert(texture("bark"));
    auto deadBark = ter::lambert(texture("bark"), ter::hexc(0x8f8b85));
    auto leaves = ter::lambert(texture("branch"));
    leaves->alphaTest = 0.45f;
    leaves->side = ter::Side::Double;
    for (int v = 0; v < 4; ++v) {
        std::vector<glm::mat4> matrices;
        for (const TreeInstance& t : trees)
            if (t.variant == v) matrices.push_back(t.matrix);
        if (matrices.empty()) continue;
        auto wood = ter::makeInstanced(models[v].trunk, {models[v].dead ? deadBark : bark}, matrices.size());
        for (size_t i = 0; i < matrices.size(); ++i) wood->instances[i] = matrices[i];
        add(wood);
        if (!models[v].foliage) continue;
        auto crown = ter::makeInstanced(models[v].foliage, {leaves}, matrices.size());
        for (size_t i = 0; i < matrices.size(); ++i) crown->instances[i] = matrices[i];
        add(crown);
    }

    if (!groundGeo || groundGeo->color.size() != groundGeo->position.size()) return;
    for (size_t i = 0; i < groundGeo->position.size(); ++i) {
        glm::vec2 p(groundGeo->position[i].x, groundGeo->position[i].z);
        float shade = 1;
        for (const TreeInstance& t : trees) {
            float d = glm::length(p - glm::vec2(t.base.x, t.base.z));
            if (d < 3.5f) shade *= glm::mix(0.6f, 1.0f, d / 3.5f);
        }
        shade = std::max(shade, 0.35f);
        // darker and browner where needles cover the grass
        groundGeo->color[i] *= glm::mix(glm::vec3(0.75f, 0.6f, 0.45f), glm::vec3(1), shade) * shade;
    }
}

namespace {

// Map "type" -> prefab builder. Add a new kind of object by adding one line here.
using PrefabFn = std::function<void(Builder&, const json&)>;
const std::map<std::string, PrefabFn>& prefabs() {
    static const std::map<std::string, PrefabFn> table = {
        {"box", [](Builder& b, const json& j) { b.box(j); }},
        {"tree", [](Builder& b, const json& j) { b.tree(vec3Of(j, "pos"), j.value("scale", 1.0f), radians(j.value("rotY", 0.0f)), j.value("variant", 0) % 4); }},
        {"forest", [](Builder& b, const json& j) { b.forest(j); }},
        {"lamp_post", [](Builder& b, const json& j) { b.lampPost(j); }},
        {"sign", [](Builder& b, const json& j) { b.sign(j); }},
        {"companion", [](Builder& b, const json& j) { b.companion(j); }},
        {"grandma", [](Builder& b, const json& j) { b.grandma(j); }},
        {"spirit_shrine", [](Builder& b, const json& j) { b.spiritShrine(j); }},
        {"model", [](Builder& b, const json& j) { b.model(j); }},
        {"bus_shelter", [](Builder& b, const json& j) { b.busShelter(j); }},
        {"bin", [](Builder& b, const json& j) { b.bin(j); }},
        {"bag", [](Builder& b, const json& j) { b.bag(j); }},
        {"shadow_figure", [](Builder& b, const json& j) { b.shadowFigure(j); }},
        {"km_post", [](Builder& b, const json& j) { b.kmPost(j); }},
        {"road_sign", [](Builder& b, const json& j) { b.roadSign(j); }},
        {"decals", [](Builder& b, const json& j) { b.decals(j); }},
        {"puddles", [](Builder& b, const json& j) { b.puddles(j); }},
        {"gutter", [](Builder& b, const json& j) { b.gutter(j); }},
        {"strip", [](Builder& b, const json& j) { b.strip(j); }},
        {"cliff", [](Builder& b, const json& j) { b.cliff(j); }},
        {"guardrail", [](Builder& b, const json& j) { b.guardrail(j); }},
        {"delineators", [](Builder& b, const json& j) { b.delineators(j); }},
        {"power_line", [](Builder& b, const json& j) { b.powerLine(j); }},
        {"rocks", [](Builder& b, const json& j) { b.rocks(j); }},
        {"grass", [](Builder& b, const json& j) { b.grass(j); }},
        {"mist", [](Builder& b, const json& j) { b.mist(j); }},
        {"smoke", [](Builder& b, const json& j) { b.smoke(j); }},
    };
    return table;
}

}  // namespace

bool loadWorld(const std::string& path, World& world) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "map: cannot open %s\n", path.c_str());
        return false;
    }
    json map;
    try {
        // comments are allowed in the map file (// and /* */)
        map = json::parse(file, nullptr, true, true);
    } catch (const json::parse_error& e) {
        std::fprintf(stderr, "map: %s: %s\n", path.c_str(), e.what());
        return false;
    }

    world = World{};
    world.root = ter::makeGroup("world");
    Builder builder{world};

    // ---- atmosphere: ambient light, fog and post-processing
    const json atmo = map.value("atmosphere", json::object());
    world.env.hemiSky = colorOf(atmo, "hemiSky", ter::hexc(0x20242c));
    world.env.hemiGround = colorOf(atmo, "hemiGround", ter::hexc(0x050505));
    world.env.hemiIntensity = atmo.value("hemiIntensity", 0.25f);
    world.env.sunIntensity = 0;
    world.env.fogColor = colorOf(atmo, "fogColor", ter::hexc(0x0b0d10));
    // read by assets/shaders/mesh.frag (volumetric fog): fogNear = fog height scale, fogFar = visibility
    world.env.fogNear = atmo.value("fogHeight", 4.0f);
    world.env.fogFar = atmo.value("visibility", 25.0f);
    world.look.background = world.env.fogColor;
    world.look.vignette = atmo.value("vignette", 1.1f);
    world.look.warm = atmo.value("warm", -0.05f);
    world.look.grain = atmo.value("grain", 0.06f);

    // ---- ground and sky
    builder.ground(map.value("ground", json::object()));
    if (map.contains("sky")) builder.sky(map["sky"]);

    // ---- invisible walls at the edge of the playable area
    if (map.contains("bounds")) {
        glm::vec2 lo = vec2Of(map["bounds"], "min"), hi = vec2Of(map["bounds"], "max");
        glm::vec2 mid = (lo + hi) * 0.5f, span = hi - lo;
        world.colliders.push_back(ter::Box::centered({mid.x, 1.5f, lo.y - 0.5f}, {span.x + 2, 3, 1}));
        world.colliders.push_back(ter::Box::centered({mid.x, 1.5f, hi.y + 0.5f}, {span.x + 2, 3, 1}));
        world.colliders.push_back(ter::Box::centered({lo.x - 0.5f, 1.5f, mid.y}, {1, 3, span.y + 2}));
        world.colliders.push_back(ter::Box::centered({hi.x + 0.5f, 1.5f, mid.y}, {1, 3, span.y + 2}));
    }

    // ---- objects
    for (const json& obj : map.value("objects", json::array())) {
        std::string type = obj.value("type", std::string());
        auto it = prefabs().find(type);
        if (it == prefabs().end()) {
            std::fprintf(stderr, "map: unknown object type \"%s\" — skipped\n", type.c_str());
            continue;
        }
        it->second(builder, obj);
    }
    builder.finishTrees();

    // ---- extra lights (not part of a prefab)
    for (const json& l : map.value("lights", json::array())) {
        auto light = ter::makePointLight(colorOf(l, "color", glm::vec3(1)), l.value("intensity", 1.0f), l.value("range", 8.0f));
        light->position = vec3Of(l, "pos");
        world.root->add(light);
        if (float f = l.value("flicker", 0.0f); f > 0) world.flickers.push_back({light, light->intensity, f});
    }

    // ---- zones (for rules and events later)
    for (const json& z : map.value("zones", json::array()))
        world.zones.push_back({z.value("id", std::string()), ter::Box{vec3Of(z, "min"), vec3Of(z, "max")}});

    // ---- sounds
    for (const json& s : map.value("sounds", json::array())) {
        LoopSound loop;
        loop.sound = s.value("sound", std::string());
        loop.spatial = s.contains("pos");
        loop.position = vec3Of(s, "pos");
        loop.volume = s.value("volume", 1.0f);
        loop.maxDistance = s.value("maxDistance", 20.0f);
        world.loops.push_back(loop);
    }
    for (const json& s : map.value("ambient", json::array())) {
        AmbientSound a;
        a.sound = s.value("sound", std::string());
        glm::vec2 every = vec2Of(s, "every", {2, 6}), radius = vec2Of(s, "radius", {5, 15});
        a.everyMin = every.x;
        a.everyMax = every.y;
        a.radiusMin = radius.x;
        a.radiusMax = radius.y;
        a.volume = s.value("volume", 1.0f);
        a.timer = a.everyMin;
        world.ambients.push_back(a);
    }

    // ---- where the player starts
    const json spawn = map.value("spawn", json::object());
    world.spawn = vec3Of(spawn, "pos") + glm::vec3(0, 1.6f, 0);   // pos is on the ground; add eye height
    world.spawnYaw = radians(spawn.value("yawDeg", 0.0f));
    return true;
}

void startWorldSounds(World& world, ter::Audio& audio) {
    for (const LoopSound& l : world.loops) {
        ter::PlayOptions o{.volume = l.volume, .loop = true, .spatial = l.spatial, .position = l.position, .maxDistance = l.maxDistance, .fadeIn = 2};
        if (auto s = audio.play(soundSource(l.sound), o)) world.playing.push_back(s);
        else std::fprintf(stderr, "map: cannot play sound \"%s\"\n", l.sound.c_str());
    }
}

void updateWorld(World& world, float dt, glm::vec3 playerPos, ter::Audio& audio) {
    world.time += dt;

    // flicker: most frames normal, sometimes a quick dip
    for (FlickerLight& f : world.flickers) f.light->intensity = ter::frand() < f.chance ? f.intensity * 0.15f : f.intensity;

    // the sky (dome, mountains, moon) moves with the player, so it never gets closer
    if (world.sky) world.sky->position = playerPos;

    // ground fog drifts
    for (MistLayer& m : world.mists) m.material->mapOffset = m.speed * world.time;

    // smoke puffs float with the wind, wrap around inside their area and bob gently
    for (SmokePuff& s : world.smoke) {
        glm::vec2 span = s.areaMax - s.areaMin;
        glm::vec2 p = glm::vec2(s.start.x, s.start.z) - s.areaMin + s.wind * world.time;
        p = s.areaMin + glm::mod(p, span);
        s.sprite->position = {p.x, s.start.y + std::sin(world.time * 0.3f + s.phase) * 0.25f, p.y};
    }

    // ambient one-shots at random spots around the player
    for (AmbientSound& a : world.ambients) {
        a.timer -= dt;
        if (a.timer > 0) continue;
        a.timer = a.everyMin + ter::frand() * (a.everyMax - a.everyMin);
        float angle = ter::frand() * ter::PI * 2, dist = a.radiusMin + ter::frand() * (a.radiusMax - a.radiusMin);
        glm::vec3 at = playerPos + glm::vec3(std::cos(angle) * dist, 0, std::sin(angle) * dist);
        at.y = 0.3f;
        audio.play(soundSource(a.sound), {.volume = a.volume, .pitch = 0.9f + ter::frand() * 0.2f, .spatial = true, .position = at, .maxDistance = a.radiusMax + 5});
    }

    if (world.companionAnim) world.companionAnim->update(dt);
    for (auto& a : world.animators) a->update(dt);
    // the companion turns to face the player (smoothly), unless it is placed in a fixed pose (sitting on a bench)
    if (world.companion && world.companionFacesPlayer) {
        glm::vec3 d = playerPos - world.companion->position;
        float target = std::atan2(d.x, d.z);
        float diff = ter::wrapAngle(target - world.companion->rotation.y);
        world.companion->rotation.y += diff * std::min(1.0f, dt * 3.0f);
    }
    for (Grandma& g : world.grandmas) {
        if (g.anim) g.anim->update(dt);
        if (!g.facesPlayer || !g.root->visible) continue;
        glm::vec3 d = playerPos - g.root->position;
        float diff = ter::wrapAngle(std::atan2(d.x, d.z) - g.root->rotation.y);
        g.root->rotation.y += diff * std::min(1.0f, dt * 1.5f);   // slower than the companion: she is old
    }
}

std::string zoneAt(const World& world, glm::vec3 p) {
    for (const Zone& z : world.zones)
        if (z.box.contains(p)) return z.id;
    return "";
}
