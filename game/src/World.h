#pragma once
// World: builds the level from assets/data/map.json — meshes, colliders, lights, sounds, spawn point.
// Change the map by editing the JSON; no C++ changes needed. The format is documented in game/MAP_FORMAT.md.
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <ter/ter.h>
#include "Animator.h"

// A named area, used later by rules and events ("player entered the shelter")
struct Zone {
    std::string id;
    ter::Box box;
};

// A light that randomly dips for a frame (broken street lamp, failing tube)
struct FlickerLight {
    ter::ObjPtr light;
    float intensity;   // normal brightness
    float chance;      // probability per frame of a dip
};

// A light that events switch off and on (a lamp_post or the shelter's tube with an "id" in the map)
struct Lamp {
    ter::ObjPtr light, halo;
    ter::MatPtr glass;      // the glowing glass, darkened while off (nullptr = none)
    glm::vec3 color{1};
    void set(bool on) {
        if (light) light->visible = on;
        if (halo) halo->visible = on;
        if (glass) glass->color = on ? color : glm::vec3(0.01f);
    }
};

// A looping sound at a fixed place (or everywhere when not spatial)
struct LoopSound {
    std::string sound;
    glm::vec3 position{0};
    bool spatial = true;
    float volume = 1, maxDistance = 20;
};

// A sound played now and then at a random spot around the player (crickets, twigs)
struct AmbientSound {
    std::string sound;
    float everyMin = 2, everyMax = 6;      // seconds between plays
    float radiusMin = 5, radiusMax = 15;   // meters from the player
    float volume = 1;
    float timer = 0;
};

// A scrolling ground-fog layer
struct MistLayer {
    ter::MatPtr material;
    glm::vec2 speed;   // texture repeats per second
};

// A drifting puff of smoke (sprite). It moves with the wind and wraps around inside its area.
struct SmokePuff {
    ter::ObjPtr sprite;
    glm::vec3 start;
    float phase;
    glm::vec2 areaMin, areaMax, wind;
};

// Post-processing / atmosphere settings that are not part of ter::Environment
struct Look {
    glm::vec3 background{0};
    float vignette = 1.0f, warm = 0, grain = 0.05f;
};

// The old woman (assets/models/grandma.glb, map type "grandma"). Two faces on one head: the rule tells the player to
// help her when her face is scary and to pretend not to see her when it is ordinary, so only the face differs.
struct Grandma {
    std::string id;
    ter::ObjPtr root;
    ter::Object3D* faceNormal = nullptr;
    ter::Object3D* faceScary = nullptr;   // includes the glowing eyes
    std::shared_ptr<Animator> anim;       // clips: idle, walk, beckon, fallen, fallen_call, lunge
    bool scary = false;
    bool facesPlayer = true;
    void setScary(bool s) {
        scary = s;
        if (faceNormal) faceNormal->visible = !s;
        if (faceScary) faceScary->visible = s;
    }
};

struct World {
    ter::ObjPtr root;                    // every mesh and light of the level
    std::vector<ter::Box> colliders;     // what the player can't walk through
    std::vector<Zone> zones;
    std::vector<FlickerLight> flickers;
    std::map<std::string, Lamp> lamps;   // by map "id" (rules' signals turn them off)
    std::vector<LoopSound> loops;
    std::vector<AmbientSound> ambients;
    std::vector<ter::SoundPtr> playing;  // keeps the looping sounds alive
    std::vector<MistLayer> mists;
    std::vector<SmokePuff> smoke;
    ter::ObjPtr sky;                     // sky dome + mountains + moon; follows the player so it looks infinitely far
    ter::ObjPtr bag;                     // rule 11: a bag just like the player's (map "bag"), hidden until its time
    ter::ObjPtr shadowFigure;            // rule 4: a black figure standing by the road (map "shadow_figure"), hidden
    ter::ObjPtr companion;               // the companion NPC, may be null
    std::shared_ptr<Animator> companionAnim;   // its clips: idle, walk, talk, sit, sit_talk, crouch, prone, board_bus
    bool companionFacesPlayer = true;
    std::vector<Grandma> grandmas;       // "grandma" entries in the map, in file order
    glm::vec3 shrineDoor{0};             // world point just inside the spirit house's door (map "spirit_shrine")
    bool hasShrine = false;
    std::vector<std::shared_ptr<Animator>> animators;   // "model" entries that play a clip
    float time = 0;
    ter::Environment env;
    Look look;
    glm::vec3 spawn{0, 1.6f, 0};
    float spawnYaw = 0;                  // radians, 0 = facing north (-Z)
};

// Read the map file and build everything. Returns false (and prints why) if the file is missing or broken.
bool loadWorld(const std::string& path, World& world);

// Start the looping sounds. Call once after loadWorld, when audio is ready.
void startWorldSounds(World& world, ter::Audio& audio);

// Per-frame world behaviour: light flicker, ambient sounds, companion turning to the player.
void updateWorld(World& world, float dt, glm::vec3 playerPos, ter::Audio& audio);

// Id of the zone containing the point, or "" when outside every zone
std::string zoneAt(const World& world, glm::vec3 p);
