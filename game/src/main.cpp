// grdc-horror — a bus stop on a foggy mountain road at night; survive until 06:00 by following the rules
// an unknown sender texts to your phone.
// The level comes from assets/data/map.json (see MAP_FORMAT.md), the clock / battery from assets/data/game.json,
// the messages and rules from assets/data/rules.json; this file is the player and the loop.
//
// Controls: click = grab the mouse · WASD walk · Shift run · Tab phone · E / click read messages (wheel scrolls)
//           · F phone flash · M mute · Esc release mouse / quit
// Debug:    ` rule overlay · J open the next rule's window now · B bus · G old woman · H breath mini-game
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ter/ter.h>
#include "Bus.h"
#include "Companion.h"
#include "GrandmaEvent.h"
#include "Dialogue.h"
#include "Breath.h"
#include "Ending.h"
#include "Hiding.h"
#include "Searcher.h"
#include "GameClock.h"
#include "Inbox.h"
#include "Phone.h"
#include "Playtest.h"
#include "RuleSystem.h"
#include "Sounds.h"
#include "World.h"

namespace {

// Player settings that survive restarts (for now only the mute switch), kept outside the app bundle
std::filesystem::path settingsFile() {
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / "Library/Application Support/grdc-horror/settings.txt";
}

bool loadMuted() {
    std::ifstream in(settingsFile());
    std::string line;
    while (std::getline(in, line))
        if (line == "muted=1") return true;
    return false;
}

void saveMuted(bool muted) {
    std::error_code ec;
    std::filesystem::create_directories(settingsFile().parent_path(), ec);
    std::ofstream(settingsFile()) << "muted=" << (muted ? 1 : 0) << "\n";
}

// assets/data/game.json: night length, battery, what the phone shows
bool loadGameConfig(const std::string& path, GameClock& clock, Phone& phone) {
    std::ifstream in(path);
    auto j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded()) {
        std::fprintf(stderr, "could not read %s\n", path.c_str());
        return false;
    }
    auto c = j.value("clock", nlohmann::json::object());
    clock.start = parseTime(c.value("start", "22:00"), clock.start);
    clock.end = parseTime(c.value("end", "05:30"), clock.end);
    clock.secondsPerHour = c.value("secondsPerHour", clock.secondsPerHour);
    auto b = j.value("battery", nlohmann::json::object());
    phone.battery.level = b.value("start", 100.0f);
    phone.battery.idlePerMinute = b.value("idlePerMinute", 0.1f);
    phone.battery.screenPerMinute = b.value("screenPerMinute", 0.8f);
    phone.battery.flashPerMinute = b.value("flashPerMinute", 2.0f);
    auto p = j.value("phone", nlohmann::json::object());
    phone.sender = p.value("sender", "?");
    phone.date = p.value("date", "");
    phone.carrier = p.value("carrier", "ไม่มีบริการ");
    return true;
}

// assets/data/game.json "breath": mini-game settings per difficulty ("easy", "hard", ...)
std::map<std::string, BreathSettings> loadBreathSettings(const std::string& path) {
    std::map<std::string, BreathSettings> out;
    std::ifstream in(path);
    auto j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded()) return out;
    const auto levels = j.value("breath", nlohmann::json::object());
    for (auto& [name, e] : levels.items()) {
        BreathSettings s;
        s.arrowsMin = e.value("arrowsMin", s.arrowsMin);
        s.arrowsMax = e.value("arrowsMax", s.arrowsMax);
        s.bpm = e.value("bpm", s.bpm);
        s.rounds = e.value("rounds", s.rounds);
        s.mark = e.value("mark", s.mark);
        s.perfect = e.value("perfect", s.perfect);
        s.good = e.value("good", s.good);
        s.missNoise = e.value("missNoise", s.missNoise);
        s.gaspNoise = e.value("gaspNoise", s.gaspNoise);
        s.drain = e.value("drain", s.drain);
        out[name] = s;
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    // our own shaders (assets/shaders/mesh.frag: volumetric fog) must be registered before the App
    // creates the renderer, which compiles its shaders right away
    ter::addShaderPath(ter::assetPath("shaders"));
    // --fps: print the average frame rate every 2 seconds (performance check)
    // --mute / --sound: start muted or with sound, overriding the saved setting
    // --time HH:MM: start the night at that time (messages due before it are already in the phone)
    // --phone held|read: start with the phone in hand / open on the messages (for --shot checks)
    // --bus: the bus arrives right away (B does the same while playing)
    bool showFps = false, muted = loadMuted();
    std::string startTime, startPhone;
    bool callBus = false, callVan = false;
    // --grandma normal|scary|help: the old woman with the stuck cart appears right away (G does the same while
    // playing); help = an ordinary face, already helped (the forest walk)
    std::string callGrandma;
    // --ending: skip straight to the ending (inside the last bus)
    bool startEnding = false;
    // --breath easy|hard: start the hold-your-breath mini-game right away (H does "easy" while playing)
    std::string startBreath;
    // --hide <id>: start crouched in that hiding place (map.json "hideSpots")
    std::string startHide;
    // --play script.txt [--play-out dir]: the game plays itself from a script (Playtest.h, game/playtests/)
    std::string playScript, playOut = "playtest_out";
    // --rule <id>: open that rule's window right away (rules.json "check"); ` shows the rule overlay
    std::string startRule;
    bool showRules = false;
    // --dev: the test keys (B bus, V van, G old woman, H breath game, J next rule, ` rule overlay); off in the shipped game
    bool devKeys = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        devKeys |= arg == "--dev";
        showFps |= arg == "--fps";
        if (arg == "--mute") muted = true;
        if (arg == "--sound") muted = false;
        if (arg == "--time" && i + 1 < argc) startTime = argv[++i];
        if (arg == "--phone" && i + 1 < argc) startPhone = argv[++i];
        if (arg == "--bus") callBus = true;
        if (arg == "--van") callVan = true;
        if (arg == "--play" && i + 1 < argc) playScript = argv[++i];
        if (arg == "--play-out" && i + 1 < argc) playOut = argv[++i];
        if (arg == "--grandma" && i + 1 < argc) callGrandma = argv[++i];
        startEnding |= arg == "--ending";
        if (arg == "--breath" && i + 1 < argc) startBreath = argv[++i];
        if (arg == "--hide" && i + 1 < argc) startHide = argv[++i];
        if (arg == "--rule" && i + 1 < argc) startRule = argv[++i], showRules = true;
    }

    ter::App app({.title = "grdc-horror", .width = 1280, .height = 720, .pixelScale = 2}, argc, argv);
    if (!app.ok()) return 1;

    // ---------- level ----------
    World world;
    if (!loadWorld(ter::assetPath("data/map.json"), world)) return 1;

    ter::Renderer& r = app.renderer();
    r.background = world.look.background;
    r.vignette = world.look.vignette;
    r.warm = world.look.warm;
    r.grain = world.look.grain;

    // the scene = the level + things that belong to the player (the phone in hand)
    auto scene = ter::makeGroup("scene");
    scene->add(world.root);
    Phone phone(*scene);

    // ---------- night: clock + messages ----------
    GameClock clock;
    if (!loadGameConfig(ter::assetPath("data/game.json"), clock, phone)) return 1;
    clock.reset();
    if (!startTime.empty()) clock.set(parseTime(startTime, clock.start));
    Inbox inbox;
    if (!inbox.load(ter::assetPath("data/rules.json"))) return 1;
    inbox.update(clock);   // anything due at the start time is already there, without a notification
    if (!startPhone.empty()) phone.toggleHeld();
    if (startPhone == "read" && phone.setReading(true)) inbox.open();

    // ---------- player ----------
    ter::Camera cam;
    cam.fov = 70;
    cam.near = 0.05f;
    ter::FirstPersonCamera player;
    player.position = world.spawn;
    player.yaw = world.spawnYaw;
    player.walkSpeed = 2.2f;
    player.runSpeed = 4.2f;
    player.collide = [&](glm::vec3, glm::vec3 to) { return ter::resolveCircle(to, 0.3f, world.colliders); };
    player.apply(cam);
    app.setScene(*scene, cam, world.env);

    // ---------- sound ----------
    ter::Audio& audio = app.audio();
    audio.setMuted(muted);
    registerPlaceholderSounds(audio);
    startWorldSounds(world, audio);

    // ---------- the bus + conversations ----------
    Dialogue dialogue;
    if (!dialogue.load(ter::assetPath("data/dialogue.json"))) return 1;
    BusEvent bus;
    if (!bus.load(ter::assetPath("data/bus.json"), world)) return 1;
    if (callBus) bus.callNow();
    // rule 6: the small van (the bus code with a shrunk bus; van.json). --van / V calls it now
    BusEvent van;
    if (!van.load(ter::assetPath("data/van.json"), world)) return 1;
    if (callVan) van.callNow();
    bool vanWasActive = false, busWasActive = false;
    // rule 10: the bus without lights (bus code, dark_bus.json); it comes with the rule's window
    BusEvent darkBus;
    if (!darkBus.load(ter::assetPath("data/dark_bus.json"), world)) return 1;
    GrandmaEvent grandma;
    if (!grandma.load(ter::assetPath("data/grandma.json"), world)) return 1;
    if (callGrandma == "help") {   // test the forest walk: an ordinary face, helped at once
        grandma.callNow("normal");
        grandma.help(dialogue, audio);
    } else if (!callGrandma.empty()) grandma.callNow(callGrandma);
    const std::string helpHint = "E ช่วยยาย", bagHint = "E หยิบกระเป๋า";
    // hiding places and holding your breath in them (Hiding.h, Breath.h)
    Hiding hiding;
    if (!hiding.load(ter::assetPath("data/map.json"))) return 1;
    Breath breath;
    Searcher searcher;   // the ghost dancer who comes looking while you hold your breath
    if (!searcher.load(world)) return 1;
    // rule 10: the dark bus's driver gets out and walks round the shelter looking for you (same code, his model)
    Searcher driverSearch;
    if (!driverSearch.load(world, {.model = "models/driver.glb", .searchClip = "walk", .nearClip = "walk", .foundClip = "idle",
                                   .music = "", .hover = 0, .bob = 0}))
        return 1;
    Searcher* hunter = &searcher;   // who is looking during this breath game / scare
    bool searchPending = false;
    auto breathSettings = loadBreathSettings(ter::assetPath("data/game.json"));
    auto startBreathGame = [&](const std::string& level) {
        auto it = breathSettings.find(level);
        breath.start(it != breathSettings.end() ? it->second : BreathSettings{});
        searchPending = true;   // she appears once the player's view is known this frame
        app.redrawUi();
    };
    if (!startBreath.empty()) startBreathGame(startBreath);
    if (const HideSpot* h = hiding.find(startHide)) hiding.enter(*h, player);
    const std::string hideHint = "E ซ่อนตัว", unhideHint = "E ออกจากที่ซ่อน", sitHint = "E นั่งบนม้านั่ง", standHint = "E ลุกขึ้น";
    // the ending: on the 05:30 bus with the companion (Ending.h)
    // Jimmy, the companion: sits, follows, chats, reacts to the rules (Companion.h, assets/data/companion.json)
    Companion jimmy;
    if (!jimmy.load(ter::assetPath("data/companion.json"), world)) return 1;
    const std::string talkHint = "E คุยกับ" + jimmy.name();
    Ending ending;
    if (!ending.load(world)) return 1;
    // the rules being checked (RuleSystem.h): breaking one = a scare + a strike, never death
    RuleSystem rules;
    if (!rules.load(ter::assetPath("data/rules.json"))) return 1;
    if (!startTime.empty()) {   // --time: what was due before it doesn't come late
        rules.skipBefore(clock.minutes());
        bus.skipBefore(clock.minutes());
        van.skipBefore(clock.minutes());
        grandma.skipBefore(clock.minutes());
    }
    if (!startRule.empty() && !rules.startNow(startRule)) std::fprintf(stderr, "--rule: no check for '%s'\n", startRule.c_str());
    auto inZone = [&](const std::string& id, glm::vec3 p) {
        for (const Zone& z : world.zones)
            if (z.id == id) return p.x >= z.box.min.x && p.x <= z.box.max.x && p.z >= z.box.min.z && p.z <= z.box.max.z;
        return false;
    };
    // props a rule's signal shows for its window (map "bag", "shadow_figure")
    auto signalProp = [&](const std::string& signal) -> ter::ObjPtr {
        if (signal == "bag") return world.bag;
        if (signal == "shadow_figure") return world.shadowFigure;
        return nullptr;
    };
    // sounds a signal plays later (the tape coming closer), and the loop it keeps running (dragging)
    struct LaterSound {
        float t;
        std::string sound;
        glm::vec3 pos;
        float volume;
    };
    std::vector<LaterSound> laterSounds;
    ter::SoundPtr dragLoop;
    // the ghost dancer always comes for rule 5: not hidden in time = she comes out of the fog at you (this timer)
    float dancerScare = 0;
    // rule 2: lost in the fog, the player comes out at the other end of the zone they left
    float wrapTime = 0;
    std::string wrapZone;
    float uiRefresh = 0;

    // ---------- UI ----------
    const std::string clickHint = "คลิกเพื่อเริ่ม";
    const std::string controlsHint = "WASD เดิน · Shift วิ่ง · Tab หยิบโทรศัพท์ · M เสียง · Esc ออก";
    const std::string phoneHint = "E อ่านข้อความ · F ไฟฉาย · Tab เก็บโทรศัพท์";
    const std::string readingHint = "ลูกกลิ้งเมาส์ เลื่อนข้อความ · E ปิด";
    std::string message = clickHint;
    std::string notice;       // short status line ("sound off"), shown for `noticeTime` seconds
    float noticeTime = 0;
    app.onUi = [&](ter::Canvas2D& g, const ter::Frame& f) {
        g.setFont("Mali", 20, 500);
        g.setShadow(6, ter::cssColor("rgba(0,0,0,0.8)"), 0, 2);
        g.setFill("rgba(225,220,205,0.9)");
        if (!message.empty()) {
            g.setTextAlign(ter::TextAlign::Center);
            g.fillText(message, f.width / 2.0f, f.height - 40.0f);
        }
        if (!notice.empty()) {
            g.setTextAlign(ter::TextAlign::Right);
            g.fillText(notice, f.width - 24.0f, 40.0f);
        }
        dialogue.draw(g, (float)f.width, (float)f.height);
        breath.draw(g, (float)f.width, (float)f.height);
        ending.drawTitle(g, (float)f.width, (float)f.height);
        if (showRules) {
            g.setFont("Mali", 13, 500);
            g.setTextAlign(ter::TextAlign::Left);
            g.setFill("rgba(160,255,170,0.9)");
            float y = 24;
            for (const std::string& line : rules.debugLines()) g.fillText(line, 16, y += 17);
        }
    };

    // ---------- loop ----------
    float fade = 1, playTime = 0, boardedTime = 0;
    int lastStep = 0;
    std::string lastZone;
    float fpsTime = 0;
    int fpsFrames = 0;
    int phoneHints = 3;       // show the phone's key hints until the thread has been opened this many times
    bool wasDead = false;
    auto beginEnding = [&] {
        bus.vanish();
        if (world.companion) world.companion->visible = false;
        phone.stow();
        dialogue.stop();
        ending.start(dialogue);
        dialogue.start("ending");
    };
    if (startEnding) beginEnding();
    Playtest playtest;
    if (!playScript.empty()) {
        setvbuf(stdout, nullptr, _IOLBF, 0);   // the log is read while it runs: one line at a time, not at exit
        if (!playtest.load(playScript)) return 1;
        std::filesystem::create_directories(playOut);
        dialogue.log = true;
    }
    std::string loggedNotice;
    std::set<std::string> touching;   // playtest collision audit: who is inside what right now
    return app.run([&](ter::Frame& f) {
        ter::Input& in = app.input();
        // scripted playtest: keys / walking are fed in here, the game-side commands run here too
        auto playCondition = [&](const std::string& cond) {
            auto after = [&](const char* prefix) { return cond.substr(std::string(prefix).size()); };
            if (cond.rfind("rule:", 0) == 0) return rules.isActive(after("rule:"));
            if (cond.rfind("say:", 0) == 0) return dialogue.active() && dialogue.text().find(after("say:")) != std::string::npos;
            if (cond.rfind("notice:", 0) == 0) return notice.find(after("notice:")) != std::string::npos;
            if (cond == "asking") return dialogue.asking();
            if (cond == "quiet") return !dialogue.active();   // nobody is talking (E would start something new)
            if (cond == "bus") return bus.active();
            if (cond == "van") return van.active();
            if (cond == "grandma") return grandma.active();
            if (cond == "ending") return ending.active();
            std::printf("[play] unknown waitfor '%s'\n", cond.c_str());
            return true;
        };
        for (const PlayCommand& c : playtest.update(f.dt, in, player, playCondition)) {
            std::string a0 = c.args.empty() ? "" : c.args[0];
            if (c.name == "clock") {
                clock.set(parseTime(a0, clock.minutes()) - 0.01f);
                rules.skipBefore(clock.minutes());   // only what is due from the new time on plays
                bus.skipBefore(clock.minutes());
                van.skipBefore(clock.minutes());
                grandma.skipBefore(clock.minutes());
            } else if (c.name == "shot") {
                app.screenshot(playOut + "/" + a0 + ".png");
                std::printf("[play %.1f] shot %s  (clock %s, pos %.1f %.1f, strikes %d)\n", playtest.time(), a0.c_str(),
                            clock.text().c_str(), player.position.x, player.position.z, rules.strikes());
            } else if (c.name == "rule") {
                rules.startNow(a0);
            } else if (c.name == "grandma") {
                grandma.callNow(a0);
            } else if (c.name == "bus") {
                bus.callNow();
            } else if (c.name == "van") {
                van.callNow();
            } else if (c.name == "breathbot") {
                breath.autoPlay = a0 == "on";
            } else if (c.name == "log") {
                std::string text;
                for (auto& w : c.args) text += w + " ";
                std::printf("[play %.1f] -- %s\n", playtest.time(), text.c_str());
            } else if (c.name == "quit") {
                app.quit();
            } else {
                std::printf("[play] line %d: unknown command '%s'\n", c.line, c.name.c_str());
            }
        }
        if (playtest.active() && notice != loggedNotice) {
            if (!notice.empty()) std::printf("[notice] %s\n", notice.c_str());
            loggedNotice = notice;
        }
        if (showFps && (fpsTime += f.dt, ++fpsFrames, fpsTime >= 2)) {
            std::printf("fps: %.1f\n", fpsFrames / fpsTime);
            fpsTime = 0;
            fpsFrames = 0;
        }
        // this click only grabs the mouse (never during a scripted playtest: the script steers the view, and a grabbed
        // mouse there would just look dead to whoever clicked the window)
        bool grabClick = in.pressed(ter::Mouse::Left) && !app.cursorLocked() && !playtest.active();
        if (grabClick) app.lockCursor(true);
        if (in.pressed(ter::Key::Escape)) {
            if (app.cursorLocked()) app.lockCursor(false);
            else app.quit();
        }

        // the ending owns the camera: the player can only look around
        if (ending.active()) {
            dialogue.update(f.dt, in);
            for (const std::string& action : dialogue.takeActions()) ending.onAction(action, audio);
            bool title = ending.titleShown();
            ending.update(f.dt, in, cam, audio, app.cursorLocked());
            if (dialogue.changed() || title != ending.titleShown()) app.redrawUi();
            updateWorld(world, f.dt, cam.position, audio);
            if (!message.empty()) {
                message.clear();
                app.redrawUi();
            }
            if (ending.black() > 0) app.overlay().fill({0, 0, 0, ending.black()});
            return;
        }

        // walking: the phone keeps the walk cycle; the camera bobs with it and each half cycle is a footstep
        // holding your breath: the arrow keys are the mini-game's, not walking
        const float walkSpeed = player.walkSpeed, runSpeed = player.runSpeed;
        if (breath.active()) player.walkSpeed = player.runSpeed = 0;
        float walked = player.update(in, f.dt, app.cursorLocked() && !playtest.active());
        player.walkSpeed = walkSpeed;
        player.runSpeed = runSpeed;
        hiding.update(f.dt, player);
        if (hiding.amount() > 0) walked = 0;
        // the old woman's forest walk owns where the player is (they can still look around) and turns their view
        // toward what they must see
        if (grandma.cutscene()) {
            player.position = grandma.playerSpot(player.position, f.dt);
            grandma.steer(player.yaw, f.dt);
            walked = 0;
        }

        // night clock; a new message vibrates the phone (heard from the pocket too) and plays the tone
        clock.update(f.dt);
        if (inbox.update(clock) > 0) {
            phone.buzz();
            audio.play("vibrate");
            audio.play("notify", {.volume = phone.held() ? 1.0f : 0.55f});
        }

        // phone: Tab out / away, E or click to read (wheel scrolls), F flash (only in hand)
        if (in.pressed(ter::Key::Tab)) phone.toggleHeld();
        // E next to the old woman helps her instead of reading
        bool helping = in.pressed(ter::Key::E) && grandma.canHelp(player.position);
        if (helping) phone.setReading(false);
        if (helping) grandma.help(dialogue, audio);
        // E with the phone away next to a hiding place: get in / out
        const HideSpot* hideSpot = hiding.near(player.position);
        // rule 11: E next to the bag that looks like yours picks it up (it isn't yours)
        bool nearBag = world.bag && world.bag->visible && !hiding.hidden() && !phone.held() &&
                       glm::length(glm::vec2(player.position.x - world.bag->position.x, player.position.z - world.bag->position.z)) < 1.3f;
        bool takeBag = in.pressed(ter::Key::E) && !helping && nearBag;
        if (takeBag) {
            world.bag->visible = false;
            dialogue.flags["took_bag"] = "yes";
            notice = "กระเป๋าเบากว่าที่คิด... ข้างในว่างเปล่า";
            noticeTime = 4;
            app.redrawUi();
        }
        helping |= takeBag;   // the same E isn't also a hide / read
        // E while looking at Jimmy (close by, phone away, nothing else being said): talk to him
        bool nearJimmy = false;
        if (world.companion && !hiding.hidden() && !phone.held() && !dialogue.active() && jimmy.canTalk(player.position, world)) {
            glm::vec3 to = world.companion->position + glm::vec3(0, 1.2f, 0) - player.position;
            nearJimmy = glm::dot(glm::normalize(to), player.forward()) > 0.85f;
        }
        bool talkKey = in.pressed(ter::Key::E) && !helping && nearJimmy;
        if (talkKey) jimmy.talk(clock.minutes(), dialogue);
        helping |= talkKey;
        bool hideKey = in.pressed(ter::Key::E) && !helping && !phone.held() && (hiding.hidden() || hideSpot);
        if (hideKey) {
            if (hiding.hidden()) hiding.leave();
            else hiding.enter(*hideSpot, player);
        }
        bool readKey = (in.pressed(ter::Key::E) && !helping && !hideKey) || (in.pressed(ter::Mouse::Left) && !grabClick);
        if (readKey && phone.held()) {
            if (phone.reading()) phone.setReading(false);
            else if (phone.setReading(true)) {
                inbox.open();
                phoneHints = std::max(0, phoneHints - 1);
            }
        }
        if (phone.reading() && in.scroll() != 0) phone.scroll(in.scroll() * 30);
        if (in.pressed(ter::Key::F) && phone.held()) {
            phone.toggleFlash();
            audio.play("click");
        }
        if (phone.dead() && !wasDead) {
            notice = "แบตเตอรี่หมด";
            noticeTime = 3;
            app.redrawUi();
        }
        wasDead = phone.dead();

        phone.update(f.dt, player, walked, world.colliders, clock, inbox);
        player.apply(cam);
        if (hiding.amount() <= 0) cam.position += phone.headBob();
        cam.lookAt(cam.position + player.forward());

        // the breath mini-game (H starts one to test)
        if (devKeys && in.pressed(ter::Key::H) && !breath.active()) startBreathGame("easy");
        bool breathing = breath.active();
        breath.update(f.dt, in, audio);
        if (breath.changed()) app.redrawUi();
        if (searchPending) {
            hunter = rules.isActive("dark_bus") ? &driverSearch : &searcher;
            hunter->start(player.position, hiding.spot() ? glm::vec3(-std::sin(hiding.spot()->yaw), 0, -std::cos(hiding.spot()->yaw)) : player.forward());
            searchPending = false;
        }
        searcher.update(f.dt, breath.noise(), player.position, audio);
        driverSearch.update(f.dt, breath.noise(), player.position, audio);
        if (breathing && !breath.active()) {
            hunter->finish(breath.caught(), player.position, player.forward(), audio);
            // the rule system (M3) will decide what being heard costs; for now just say what happened
            notice = breath.caught() ? "มีบางอย่างได้ยินเสียงหายใจของคุณ..." : "มันไปแล้ว...";
            noticeTime = 3;
            app.redrawUi();
        }
        breath.drawOverlay(app.overlay(), (float)f.width, (float)f.height);

        // ---- rules: open windows on the clock, play their signals, check, punish ----
        if (grandma.cutscene()) dialogue.flags["grandma_forest"] = "yes";   // rule 7: left the light with her
        if (devKeys && in.pressed(ter::Key::J) && !rules.startNext()) {
            notice = "ไม่มีกฎที่รออยู่";
            noticeTime = 2;
        }
        if (devKeys && in.pressed(ter::Key::GraveAccent)) {
            showRules = !showRules;
            app.redrawUi();
        }
        RuleInputs ri;
        ri.position = player.position;
        ri.forward = player.forward();
        ri.phoneHeld = phone.held();
        ri.phoneReading = phone.reading();
        ri.flashOn = phone.lightOn();
        ri.hideSpot = hiding.hidden() && hiding.amount() >= 1 ? hiding.spot()->id : "";
        ri.breathActive = breath.active();
        ri.breathEnded = breathing && !breath.active();
        ri.breathCaught = breath.caught();
        ri.flags = &dialogue.flags;
        ri.inZone = inZone;
        ri.busy = bus.active() || van.active() || grandma.active() || dialogue.asking();
        rules.update(f.dt, clock, ri, [&](const std::string& signal) {
            // the signals that exist so far; the rest come with the EventDirector (M6) and skip their rule until then
            bool done = false;
            if (signal == "phone_buzz") {   // rule 3: it buzzes, but no message arrives
                phone.buzz();
                audio.play("vibrate");
                done = true;
            }
            if (ter::ObjPtr prop = signalProp(signal)) {   // a prop that appears for the window
                prop->visible = true;
                done = true;
            }
            auto lamp = [&](const char* id, bool on) {
                if (auto it = world.lamps.find(id); it != world.lamps.end()) it->second.set(on);
            };
            if (signal == "street_lights_off") {   // rule 5: both lamps die, a tape rewinds closer and closer
                lamp("lamp_west", false);
                lamp("lamp_east", false);
                audio.play("boom", {.volume = 0.5f});
                const float t = world.time;
                laterSounds.push_back({t + 1.0f, "tape_rewind", {-22, 1, -3}, 0.5f});
                laterSounds.push_back({t + 7.0f, "tape_rewind", {-12, 1, -2}, 0.8f});
                laterSounds.push_back({t + 13.0f, "tape_rewind", {-5, 1, 0}, 1.0f});
                done = true;
            }
            if (signal == "shelter_light_off") {   // rule 8: the tube in the shelter goes out
                lamp("shelter", false);
                audio.play("click", {.volume = 0.8f});
                done = true;
            }
            if (signal == "dragging_sound") {   // rule 9: one lamp out, something dragged across the ground behind
                lamp("lamp_east", false);
                dragLoop = audio.play("drag", {.volume = 1.0f, .loop = true, .spatial = true, .position = {-4, 0.3f, 7}, .maxDistance = 25, .fadeIn = 2});
                done = true;
            }
            if (signal == "dark_bus") {   // rule 10: a bus with no lights; its seat is empty, the driver is out looking
                darkBus.callNow();
                darkBus.setDriverVisible(false);
                done = true;
            }
            if (signal == "scream") {   // rule 12: a scream out in the forest
                audio.play("scream", {.volume = 1.0f, .spatial = true, .position = {-6, 1.5f, 14}, .maxDistance = 40});
                done = true;
            }
            if (done) jimmy.onSignal(signal);   // ...and Jimmy has something to say about it
            return done;
        });
        for (const std::string& signal : rules.takeEndedSignals()) {
            if (ter::ObjPtr prop = signalProp(signal)) prop->visible = false;
            if (signal == "street_lights_off" || signal == "dragging_sound" || signal == "shelter_light_off")
                for (auto& [id, l] : world.lamps) l.set(true);   // the lights come back on by themselves
            if (signal == "dragging_sound" && dragLoop) dragLoop->stop(), dragLoop.reset();
            if (signal == "dark_bus") {   // he is back at the wheel and it drives off, still dark
                darkBus.setDriverVisible(true);
                darkBus.onAction("leave", world);
            }
        }
        for (auto it = laterSounds.begin(); it != laterSounds.end();) {
            if (world.time < it->t) {
                ++it;
                continue;
            }
            audio.play(it->sound, {.volume = it->volume, .spatial = true, .position = it->pos, .maxDistance = 40});
            it = laterSounds.erase(it);
        }
        for (const std::string& id : rules.takeOpened()) jimmy.onRuleOpened(id);
        for (const std::string& id : rules.takeClosed()) jimmy.onRuleClosed(id);
        for (const RuleViolation& v : rules.takeViolations()) {
            jimmy.onStrike();   // every broken rule, he comes a step closer
            std::string sound = v.onViolate.value("sound", "");
            if (!sound.empty()) audio.play(sound);
            // the shadow knows you saw it: gone the moment the rule breaks
            if (ter::ObjPtr prop = signalProp(v.onViolate.value("hide", ""))) prop->visible = false;
            // she comes for you: out of the fog in front, then right in your face (if she isn't already here)
            // rule 10: the driver finds you instead ("scare": "driver")
            Searcher* scarer = v.onViolate.value("scare", "") == "driver" ? &driverSearch
                               : v.onViolate.value("dancer", false)        ? &searcher
                                                                            : nullptr;
            if (scarer && !scarer->active() && !breath.active()) {
                hunter = scarer;
                hunter->start(player.position, player.forward());
                dancerScare = 2.2f;
            }
            if (v.onViolate.contains("wrap")) {
                wrapZone = v.onViolate.value("wrap", "");
                wrapTime = 2.4f;
            }
        }
        if (dancerScare > 0 && (dancerScare -= f.dt) <= 0) hunter->finish(true, player.position, player.forward(), audio);
        std::string wantBreath = rules.breathWanted();
        if (!wantBreath.empty() && !breath.active()) startBreathGame(wantBreath);
        if (wrapTime > 0) {
            float before = wrapTime;
            wrapTime = std::max(0.0f, wrapTime - f.dt);
            if (before > 1.2f && wrapTime <= 1.2f) {
                // at full black: come out on the opposite side, 1 m inside the edge
                for (const Zone& z : world.zones) {
                    if (z.id != wrapZone) continue;
                    glm::vec3& p = player.position;
                    if (p.x > z.box.max.x) p.x = z.box.min.x + 1;
                    else if (p.x < z.box.min.x) p.x = z.box.max.x - 1;
                    if (p.z > z.box.max.z) p.z = z.box.min.z + 1;
                    else if (p.z < z.box.min.z) p.z = z.box.max.z - 1;
                }
            }
        }
        if (showRules && (uiRefresh -= f.dt) <= 0) {
            uiRefresh = 0.2f;
            app.redrawUi();
        }
        int step = (int)std::floor(phone.walkPhase() / ter::PI);
        if (step != lastStep) {
            lastStep = step;
            audio.play("step", {.volume = 0.8f, .pitch = 0.75f + ter::frand() * 0.3f});
        }

        if (in.pressed(ter::Key::M)) {
            muted = !muted;
            audio.setMuted(muted);
            saveMuted(muted);
            notice = muted ? "เสียง: ปิด" : "เสียง: เปิด";
            noticeTime = 2;
            app.redrawUi();
        }
        if (noticeTime > 0 && (noticeTime -= f.dt) <= 0) {
            notice.clear();
            app.redrawUi();
        }

        updateWorld(world, f.dt, player.position, audio);
        jimmy.update(f.dt, {.player = player.position, .playerInShelter = inZone("shelter", player.position),
                            .clock = clock.minutes(), .paused = (bus.active() && bus.isFinal()) || grandma.cutscene()},
                     world, dialogue, audio);

        // the bus: arrives on the clock (or B), the driver's questions are answered with 1-9
        if (devKeys && in.pressed(ter::Key::B)) bus.callNow();
        if (devKeys && in.pressed(ter::Key::V)) van.callNow();
        // test: G calls the old woman (or swaps her face once she is there)
        if (devKeys && in.pressed(ter::Key::G)) {
            if (grandma.active()) grandma.toggleFace();
            else grandma.callNow();
            for (Grandma& g : world.grandmas) g.setScary(!g.scary);
        }
        dialogue.update(f.dt, in);
        // after 01:00 the companion "gets new rules" on his phone and offers to AirDrop them (they are his fakes)
        if (inbox.airdropDue(clock) && !dialogue.active() && !rules.windowOpen() && !bus.active() && !van.active() &&
            dialogue.start(inbox.airdropScript()))
            inbox.airdropOffered();
        for (const std::string& action : dialogue.takeActions()) {
            if (action == "airdrop_accept") {
                if (inbox.receiveAirdrop(clock) > 0) {
                    phone.buzz();
                    audio.play("airdrop", {.volume = phone.held() ? 1.0f : 0.6f});
                }
            } else if (action == "jimmy_hold") {
                jimmy.holdPlayer();   // the 23:00 bus: he grabs your arm
            } else if (action == "airdrop_decline") {
                // nothing arrives; dialogue.flags["airdrop"] remembers the answer
            } else {
                (van.active() ? van : bus).onAction(action, world);   // the driver who is talking
            }
        }
        // one thing at a time: an event that comes due while a rule's window or another event is on waits for it
        bus.hold = rules.windowOpen() || van.active() || grandma.cutscene();
        van.hold = rules.windowOpen() || bus.active() || grandma.cutscene();
        grandma.hold = rules.windowOpen() || bus.active() || van.active();
        bus.update(f.dt, clock, player.position, world, dialogue, audio);
        van.update(f.dt, clock, player.position, world, dialogue, audio);
        darkBus.update(f.dt, clock, player.position, world, dialogue, audio);
        if (van.active() && !vanWasActive) jimmy.onSignal("small_van");   // "free ride, let's go!"
        vanWasActive = van.active();
        if (busWasActive && !bus.active() && !bus.isFinal() && !bus.playerBoarded()) jimmy.onSignal("real_bus_gone");
        busWasActive = bus.active();
        grandma.update(f.dt, clock, cam.position, world, dialogue, audio);
        if (grandma.takeWokeUp()) {
            // caught in the forest: the player comes to in the shelter, not knowing how they got back
            player.position = {0.3f, player.position.y, 1.3f};
            player.yaw = 0;
            notice = "คุณสะดุ้งตื่นอยู่ในศาลา... จำไม่ได้ว่ากลับมาได้ยังไง";
            noticeTime = 5;
            app.redrawUi();
        }
        if (dialogue.changed()) app.redrawUi();

        // playtest collision audit: log whenever someone starts overlapping a collider (the shelter, the bench, a
        // post, a bus) or another character. Bodies are circles of radius 0.25 m on the ground plane.
        if (playtest.active()) {
            struct Body {
                std::string name;
                glm::vec3 p;
                bool on;
            };
            const glm::vec3 jim = world.companion ? world.companion->position : glm::vec3(0);
            std::vector<Body> bodies = {
                {"player", player.position, !hiding.current() && !grandma.cutscene() && !ending.active()},
                {"jimmy", jim, world.companion && world.companion->visible},
                {"driver", driverSearch.position(), driverSearch.active()},
            };
            for (const Grandma& g : world.grandmas) bodies.push_back({"grandma", g.root->position, g.root->visible});
            std::set<std::string> now;
            for (const Body& b : bodies) {
                if (!b.on) continue;
                for (size_t i = 0; i < world.colliders.size(); ++i) {
                    const ter::Box& box = world.colliders[i];
                    if (box.max.y < 0.1f || box.min.y > 1.8f || box.max.x < -1e3f) continue;   // parked / not body height
                    glm::vec3 c = (box.min + box.max) * 0.5f;
                    if (b.name == "jimmy" && glm::length(c - (jim + glm::vec3(0, 0.9f, 0))) < 0.05f) continue;   // his own
                    float dx = b.p.x - std::clamp(b.p.x, box.min.x, box.max.x), dz = b.p.z - std::clamp(b.p.z, box.min.z, box.max.z);
                    if (dx * dx + dz * dz >= 0.25f * 0.25f) continue;
                    std::string key = b.name + " in box #" + std::to_string(i);
                    if (!touching.count(key))   // first frame inside: say where (moving boxes = the buses)
                        std::printf("[overlap %.1f %s] %s (centre %.1f %.1f, size %.1f x %.1f)\n", playtest.time(), clock.text().c_str(),
                                    key.c_str(), c.x, c.z, box.max.x - box.min.x, box.max.z - box.min.z);
                    now.insert(key);
                }
                for (const Body& o : bodies)
                    if (o.on && o.name > b.name && glm::length(glm::vec2(b.p.x - o.p.x, b.p.z - o.p.z)) < 0.5f)
                        now.insert(b.name + " overlaps " + o.name);
            }
            for (const std::string& k : now)
                if (!touching.count(k) && k.find(" in box #") == std::string::npos)
                    std::printf("[overlap %.1f %s] %s\n", playtest.time(), clock.text().c_str(), k.c_str());
            touching = std::move(now);
        }

        // zones: print when the player enters one (rules/events will hook in here in M3/M6)
        std::string zone = zoneAt(world, player.position);
        if (zone != lastZone) {
            if (!zone.empty()) std::printf("entered zone: %s\n", zone.c_str());
            lastZone = zone;
        }

        // hint text: "click to start" while the mouse is free, controls for a few seconds after
        if (app.cursorLocked() || playtest.active()) playTime += f.dt;
        std::string want = grandma.lost()                      ? "คุณหายไปในหมอก"
                           : grandma.chasing()                 ? "วิ่ง! กลับไปที่ศาลา (Shift)"
                           : boardedTime > 2.5f && !bus.finalBoarded() ? "คุณขึ้นรถไปแล้ว"
                           : !app.cursorLocked() && !playtest.active() ? clickHint
                           : phone.reading() && phoneHints > 0 ? readingHint
                           : phone.held() && phoneHints > 0    ? phoneHint
                           : grandma.canHelp(player.position)  ? helpHint
                           : nearBag                           ? bagHint
                           : nearJimmy                         ? talkHint
                           : hiding.hidden() && !phone.held()  ? (hiding.spot()->sit ? standHint : unhideHint)
                           : hideSpot && !phone.held() && !hiding.amount() ? (hideSpot->sit ? sitHint : hideHint)
                           : playTime < 6                      ? controlsHint
                                                               : "";
        if (want != message) {
            message = want;
            app.redrawUi();
        }

        // fade in from black; getting on the bus fades out: the last bus goes on to the ending, an earlier one
        // is still a placeholder ending
        if (bus.playerBoarded()) {
            boardedTime += f.dt;
            app.overlay().fill({0, 0, 0, std::min(1.0f, boardedTime / 2.5f)});
            if (bus.finalBoarded() && boardedTime >= 2.5f) beginEnding();
            return;
        }
        fade = std::max(0.0f, fade - f.dt * 0.4f);
        // crouched in a hiding place: the view closes in a little
        float wrapBlack = wrapTime > 0 ? std::min(1.0f, (1.2f - std::abs(wrapTime - 1.2f)) / 0.8f) : 0.0f;
        bool seated = hiding.current() && hiding.current()->sit;   // also while standing up again
        float black = std::max({fade, grandma.blackout(), seated ? 0.0f : hiding.amount() * 0.25f, wrapBlack});
        if (black > 0) app.overlay().fill({0, 0, 0, black});
    });
}
