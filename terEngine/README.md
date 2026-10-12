# terEngine

ไลบรารีเกม 3D ขนาดเล็ก เขียนด้วย **C++20 + OpenGL 4.1** สำหรับ **macOS** แยกออกมาจากเกม terPiece (Patty's Realms)
API หน้าตาเหมือน **three.js** (`Object3D`, `Mesh`, `Material`, `Camera`) และใช้แบบเดียวกับ raylib / MLX42 คือ link ไลบรารีแล้วเขียน loop เอง ไม่มี editor

```cpp
#include <ter/ter.h>

int main(int argc, char** argv) {
    ter::App app({.title = "Hello"}, argc, argv);
    auto scene = ter::makeGroup();
    auto cube = ter::makeMesh(ter::geo::box(), ter::lambertColor(ter::hexc(0xff8a3d)));
    scene->add(cube);
    ter::Environment env;
    env.hemiIntensity = 1;
    ter::Camera cam;
    cam.position = {2, 2, 3};
    cam.lookAt({0, 0, 0});
    app.setScene(*scene, cam, env);
    return app.run([&](ter::Frame& f) { cube->rotation.y += f.dt; });
}
```

---

## สารบัญ

1. [มีอะไรให้บ้าง](#มีอะไรให้บ้าง)
2. [ติดตั้งและ build](#ติดตั้งและ-build)
3. [ใช้ในโปรเจคของตัวเอง](#ใช้ในโปรเจคของตัวเอง)
4. [App และ game loop](#app-และ-game-loop)
5. [ฉาก: Object3D / Mesh / Geometry / Material](#ฉาก-object3d--mesh--geometry--material)
6. [แสงและบรรยากาศ](#แสงและบรรยากาศ)
7. [โมเดล glTF และ texture](#โมเดล-gltf-และ-texture)
8. [Input](#input)
9. [กล้องบุคคลที่หนึ่งและการชน](#กล้องบุคคลที่หนึ่งและการชน)
10. [UI ภาษาไทย (Canvas2D) และ Overlay](#ui-ภาษาไทย-canvas2d-และ-overlay)
11. [เสียง](#เสียง)
12. [Shader ของตัวเอง](#shader-ของตัวเอง)
13. [ทดสอบด้วยภาพหน้าจอ (`--shot`)](#ทดสอบด้วยภาพหน้าจอ---shot)
14. [โครงสร้างไฟล์](#โครงสร้างไฟล์)
15. [ข้อจำกัด](#ข้อจำกัด)

---

## มีอะไรให้บ้าง

| ส่วน | ไฟล์ | ทำอะไร |
|---|---|---|
| App | `ter/app/app.h` | เปิดหน้าต่าง, loop, วาดฉาก → UI → overlay, บันทึกภาพหน้าจอ |
| Input | `ter/app/input.h` | คีย์บอร์ด, เมาส์ (ล็อกเมาส์ได้), จอยเกม, พิมพ์ข้อความไทย |
| กล้อง FPS | `ter/app/fps_camera.h` | เดิน WASD + หันด้วยเมาส์/ก้านขวา |
| การชน | `ter/core/collide.h` | วงกลมชนกล่อง (กันเดินทะลุกำแพง), ray ชนกล่อง (มองอะไรอยู่) |
| ฉาก | `ter/scene/object.h` | Group, Mesh, InstancedMesh, Points, Sprite, PointLight, SpotLight |
| รูปทรง | `ter/scene/geometry.h` | box, sphere, cylinder, cone, plane, torus, extrude ฯลฯ (ตรงกับ three.js r151) |
| วัสดุ | `ter/scene/material.h` | Basic (ไม่รับแสง), Lambert, Standard (PBR), Sprite, Custom shader |
| Renderer | `ter/scene/renderer.h` | เงา PCF, fog, MSAA, pixel-art scale, tilt-shift, vignette, ปรับโทนสี, film grain |
| glTF | `ter/scene/gltf.h` | โหลด `.glb` / `.gltf` (รวม meshopt + WebP) + เล่น animation |
| Texture | `ter/gfx/texture.h` | โหลด PNG / JPEG / WebP, อัปโหลด pixel เอง, บันทึก PNG |
| Pixel canvas | `ter/gfx/pixel_canvas.h` | วาด pixel-art ลง buffer ในโค้ด แล้วทำเป็น texture |
| Canvas2D | `ter/ui/canvas2d.h` | วาดแบบ HTML canvas: ข้อความไทย/emoji, รูปทรง, gradient, เงา |
| เสียง | `ter/audio/audio.h` | เล่นไฟล์ (wav/mp3/flac), เสียง 3 มิติ, สร้างเสียง placeholder จาก synth |

---

## ติดตั้งและ build

ต้องมี:
- macOS ที่มี Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.20 ขึ้นไป (`brew install cmake`)
- GLFW 3.4 (`brew install glfw`) — ถ้าไม่มี CMake จะดาวน์โหลดมา build ให้เอง

ไลบรารีอื่น (glm, cgltf, meshoptimizer, nlohmann/json, miniaudio) CMake จะดาวน์โหลดเองตอน build ครั้งแรก

```sh
cd terEngine
cmake -S . -B build
cmake --build build -j
./build/examples/01_hello_cube
./build/examples/02_dark_room
./build/examples/03_model_viewer path/to/model.glb
```

| ตัวอย่าง | แสดงอะไร |
|---|---|
| `01_hello_cube` | โปรแกรมเล็กที่สุด: ลูกบาศก์มีเงา, fog, ข้อความไทย |
| `02_dark_room` | เดินแบบ FPS ในห้องมืด: ไฟฉาย (spot light), ไฟกะพริบ, film grain, ชนกำแพง, เสียง 3 มิติที่เดินตามหาได้, มองแล้วกด E, fade จากสีดำ |
| `03_model_viewer` | โหลด .glb, เล่น animation, ลากเมาส์หมุนรอบ, scroll ซูม |

---

## ใช้ในโปรเจคของตัวเอง

วาง `terEngine/` ไว้ในโปรเจค (หรือที่ไหนก็ได้) แล้วเขียน `CMakeLists.txt` ของเกม:

```cmake
cmake_minimum_required(VERSION 3.20)
project(mygame LANGUAGES C CXX OBJCXX)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(terEngine)            # หรือ path อื่น เช่น ../terEngine ${CMAKE_BINARY_DIR}/terEngine

add_executable(mygame src/main.cpp)
target_link_libraries(mygame PRIVATE ter::engine)
# ให้โค้ดหาไฟล์ asset ได้ไม่ว่ารันจากโฟลเดอร์ไหน
target_compile_definitions(mygame PRIVATE ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
```

- ต้องมี `OBJCXX` ใน `project(... LANGUAGES ...)` เพราะ engine มีไฟล์ `.mm` (ImageIO / CoreText)
- ตอนเป็น subproject ตัวอย่างจะไม่ถูก build (เปิดได้ด้วย `-DTER_BUILD_EXAMPLES=ON`)
- shader พื้นฐานถูกฝังไว้ในไลบรารีแล้ว ไม่ต้องก๊อปโฟลเดอร์ `shaders/` ไปไหน
- ทุกอย่างอยู่ใน namespace `ter` และ include ได้ทีละไฟล์ (`<ter/scene/object.h>`) หรือรวมทีเดียว (`<ter/ter.h>`)
- พิกัด: **Y ขึ้น, −Z คือข้างหน้า, หน่วยเป็นเมตร** (เหมือน three.js)
- สีเขียนเป็น hex ด้วย `ter::hexc(0xRRGGBB)` ค่าที่ได้ใช้เป็นสี linear ตรงๆ (แบบ three.js r151 legacy) แล้ว renderer แปลงเป็น sRGB ให้ตอนท้าย

---

## App และ game loop

```cpp
ter::AppConfig cfg;
cfg.title = "My Game";
cfg.width = 1280;          // ขนาดหน้าต่าง (หน่วย point — จอ Retina framebuffer จะใหญ่เป็น 2 เท่า)
cfg.height = 720;
cfg.pixelScale = 2;        // 1 = คม, 2-3 = ภาพแตกเป็น pixel แบบ pixel-art
cfg.vsync = true;
cfg.fullscreen = false;

ter::App app(cfg, argc, argv);   // argc/argv อ่าน --shot, --shot-delay, --pixel, --fullscreen
if (!app.ok()) return 1;         // สร้างหน้าต่าง / compile shader ไม่ได้ (สาเหตุพิมพ์ออก stderr)

app.setScene(*scene, camera, env);   // ฉากที่จะวาดทุกเฟรม (ตัวแปรต้องอยู่ตลอดที่ app รัน)

return app.run([&](ter::Frame& f) {
    // f.dt      วินาทีตั้งแต่เฟรมก่อน (สูงสุด 0.1)
    // f.time    วินาทีตั้งแต่เริ่ม
    // f.index   เฟรมที่เท่าไร
    // f.width / f.height       ขนาดหน้าต่าง (point) — ใช้กับ UI และเมาส์
    // f.fbWidth / f.fbHeight   ขนาด framebuffer (pixel), f.scale = pixel ต่อ point
    if (app.input().pressed(ter::Key::Escape)) app.quit();
});
```

ลำดับในแต่ละเฟรม: อ่าน input → **update ของเรา** → อัปเดตเสียง → วาดฉาก 3D → post (สี/vignette/grain) → UI canvas → overlay → แสดงผล

ฟังก์ชันอื่นของ `App`:

| | |
|---|---|
| `app.renderer()` | ตั้งค่าภาพ (ดู [แสงและบรรยากาศ](#แสงและบรรยากาศ)) |
| `app.input()` | input ของเฟรมนี้ |
| `app.audio()` | ระบบเสียง (เริ่มทำงานเมื่อเรียกครั้งแรก) |
| `app.overlay()` | สี่เหลี่ยม/รูปบนจอ (fade, flash, หลอดพลัง) |
| `app.onUi` / `app.redrawUi()` | UI ด้วย Canvas2D |
| `app.lockCursor(true)` | ซ่อนเมาส์และล็อกไว้ในหน้าต่าง (เกม FPS) |
| `app.screenshot("a.png")` | บันทึกเฟรมถัดไปเป็น PNG |
| `app.setTitle("...")` | เปลี่ยนชื่อหน้าต่าง |
| `app.clearScene()` | หยุดวาดฉาก (เช่นหน้าเมนู) — จอจะเป็นสี `renderer().background` |
| `app.window()` | `GLFWwindow*` ถ้าต้องเรียก GLFW เอง |

---

## ฉาก: Object3D / Mesh / Geometry / Material

ทุกอย่างในฉากเป็น `ter::ObjPtr` (`std::shared_ptr<Object3D>`) ซ้อนกันเป็นต้นไม้ แบบ three.js

```cpp
auto scene = ter::makeGroup("root");

// mesh = geometry + material
auto box = ter::makeMesh(ter::geo::box(1, 2, 1), ter::standard(ter::hexc(0x8899aa), 0.7f, 0.0f));
box->position = {0, 1, -3};
box->rotation = {0, ter::PI / 4, 0};     // Euler XYZ (เรเดียน)
box->scale = glm::vec3(1.5f);
box->castShadow = true;
box->receiveShadow = true;
scene->add(box);

// ซ้อนกัน: ลูกขยับตามพ่อ
auto door = ter::makeGroup();
door->add(ter::makeMesh(ter::geo::box(1, 2, 0.1f), ter::lambertColor(ter::hexc(0x5a3a22))));
scene->add(door);

box->visible = false;                 // ซ่อน
box->removeFromParent();              // เอาออกจากฉาก
box->lookAt({0, 0, 0});               // หัน +Z ไปที่จุด
glm::vec3 p = box->worldPosition();   // ตำแหน่งจริงในโลก
scene->traverse([](ter::Object3D& o) { o.castShadow = true; });
auto copy = box->clone();             // ใช้ geometry/material ร่วมกัน
```

**รูปทรง** (`ter::geo::`): `box`, `sphere`, `cylinder`, `cone`, `plane`, `circle`, `torus`, `dodecahedron`, `octahedron`, `icosahedron`, `shape` / `extrude` (รูปจาก `ter::Shape`), `fromPositions`, `merge`
ใช้ `->translate()`, `->rotateX()`, `->scale()` แก้ geometry ได้ก่อนวาดครั้งแรก

**วัสดุ** (`ter::`):

| ฟังก์ชัน | ลักษณะ |
|---|---|
| `basic(color, map)` | ไม่รับแสง (ป้ายเรืองแสง, ท้องฟ้า) |
| `lambertColor(color)` / `lambert(texture, color)` | ด้าน ราคาถูก |
| `standard(color, roughness, metalness)` | PBR มีแสงสะท้อน |
| `spriteMaterial(texture)` | ใช้กับ `makeSprite()` — รูปแบนที่หันหากล้องเสมอ |

ฟิลด์ที่ปรับได้ใน `Material`: `color`, `map`, `emissive` (เรืองแสงเอง), `emissiveMap`, `opacity` + `transparent`, `alphaTest`, `side` (`Front/Back/Double`), `blending` (`Additive` สำหรับแสงฟุ้ง), `depthWrite`, `fog`, `vertexColors`, `mapOffset` / `mapRepeat` (เลื่อน texture ทำ sprite sheet)

**ของจำนวนมาก** (หญ้า, ต้นไม้, เทียน): `makeInstanced(geo, {mat}, n)` แล้วใส่ `obj->instances[i] = ter::composeMatrix(pos, quat, scale)` วาดทั้งหมดใน draw call เดียว

---

## แสงและบรรยากาศ

แสงมี 4 แบบ:

```cpp
ter::Environment env;
// 1) แสงรอบทิศ: สีจากฟ้า (ด้านบน) ผสมสีพื้น (ด้านล่าง)
env.hemiSky = ter::hexc(0xbfd9ff);
env.hemiGround = ter::hexc(0x4a3c2a);
env.hemiIntensity = 0.6f;
// 2) ดวงอาทิตย์/ดวงจันทร์ 1 ดวง ทอดเงา
env.sunColor = ter::hexc(0xfff1d6);
env.sunIntensity = 1.2f;            // 0 = ไม่มี (ไม่เสียเวลาทำเงา)
env.sunPosition = {4, 8, 3};
env.sunTarget = {0, 0, 0};
env.shadowHalf = 14;                // ครึ่งความกว้างของพื้นที่มีเงา (เมตร)
// หมอก (เส้นตรงจาก near ถึง far)
env.fogColor = ter::hexc(0x9cc2e8);
env.fogNear = 10;
env.fogFar = 30;

// 3) ไฟจุด (เทียน, หลอดไฟ) — มีได้พร้อมกันสูงสุด 16 ดวง (เลือกดวงที่ใกล้กล้องสุด)
auto lamp = ter::makePointLight(ter::hexc(0xffaa55), 2.0f, 6.0f);   // สี, ความแรง, ระยะ
lamp->position = {0, 2, 0};
scene->add(lamp);

// 4) ไฟสปอต (ไฟฉาย) — ส่องตามแกน +Z ของ object, เล็งด้วย lookAt
auto torch = ter::makeSpotLight(ter::hexc(0xfff2d8), 3.0f, 14.0f, 0.42f, 0.5f);  // สี, แรง, ระยะ, ครึ่งมุมกรวย (rad), ขอบนุ่ม 0..1
scene->add(torch);
torch->position = eye;
torch->lookAt(eye + forward);
```

ไฟจุดกับไฟสปอตยังไม่ทอดเงา มีแค่ดวงอาทิตย์ที่ทำเงาได้

ปรับภาพทั้งจอที่ `app.renderer()`:

| ค่า | ค่าเริ่มต้น | ความหมาย |
|---|---|---|
| `background` | ดำ | สีด้านหลังที่ไม่มีอะไรวาด (มักตั้งเท่ากับ `fogColor`) |
| `vignette` | 0.35 | มุมจอมืด (1+ = มืดมากแบบหนังสยองขวัญ) |
| `warm` | 0.04 | + โทนอุ่น / − โทนเย็น |
| `grain` | 0 | noise แบบฟิล์ม (0.03–0.08 กำลังดี) |
| `shadows` | true | เปิด/ปิดเงา |
| `shadowMapSize` | 2048 | ความละเอียดเงา |
| `tiltShift` / `tiltAmount` | false / 0.35 | เบลอบนล่างให้ดูเหมือนโมเดลจิ๋ว |

ความคมของภาพตั้งที่ `AppConfig::pixelScale` หรือ flag `--pixel 2`

---

## โมเดล glTF และ texture

```cpp
// .glb / .gltf (export จาก Blender ได้เลย)
auto model = ter::loadGltf(ASSET_DIR "/models/ghost.glb");
if (model) {
    scene->add(model->root);
    ghostMixer = std::make_unique<ter::AnimationMixer>(model->animations);   // เล่นทุก clip วนลูป
}
// ในแต่ละเฟรม
ghostMixer->update(f.dt);
```

```cpp
// texture จากไฟล์ PNG / JPEG / WebP
auto tex = std::make_shared<ter::Texture>();
ter::TextureOptions opt;
opt.nearest = true;        // pixel-art คม (false = เนียน)
opt.repeat = true;
tex->load(ASSET_DIR "/textures/wall.png", opt);
auto wall = ter::makeMesh(ter::geo::box(4, 3, 0.2f), ter::lambert(tex));
```

วาด texture เองด้วยโค้ด: `ter::Canvas` (pixel_canvas.h) วาด pixel แล้ว `tex->upload(c.w, c.h, c.rgba)` หรือใช้ `ter::Canvas2D` (มีข้อความ) แล้ว `canvas.upload(*tex)`

---

## Input

```cpp
ter::Input& in = app.input();

in.down(ter::Key::W)            // กดค้างอยู่
in.pressed(ter::Key::E)         // เพิ่งกดเฟรมนี้ (true ครั้งเดียวต่อการกด)
in.released(ter::Key::Space)    // เพิ่งปล่อย
in.down(ter::Mouse::Left)  /  in.pressed(ter::Mouse::Right)
in.mouse()                      // ตำแหน่งเมาส์ (point, y ลง)
in.mouseDelta()                 // เมาส์ขยับเท่าไรตั้งแต่เฟรมก่อน (ใช้ได้ตอนล็อกเมาส์)
in.scroll()                     // ล้อเมาส์
in.text()                       // ตัวอักษรที่พิมพ์เฟรมนี้ (UTF-8, ภาษาไทยได้)
in.moveAxis()                   // WASD/ลูกศร/ก้านซ้าย รวมเป็น vec2 (x = ขวา, y = หน้า)

// จอย (layout Xbox; จอย PS / Switch ใช้ได้ผ่าน mapping ของ GLFW)
in.gamepad()  in.down(ter::Pad::A)  in.leftStick()  in.rightStick()  in.rightTrigger()

// ทดสอบแบบอัตโนมัติ: แกล้งกดปุ่ม
in.simulate(ter::Key::W, true);
```

ปุ่มที่ไม่มีใน `ter::Key` ใช้รหัส GLFW ได้ เช่น `ter::Key(GLFW_KEY_KP_ADD)`

---

## กล้องบุคคลที่หนึ่งและการชน

```cpp
std::vector<ter::Box> walls = {
    ter::Box::centered({0, 1.5f, -8}, {16, 3, 0.3f}),   // ศูนย์กลาง, ขนาด
};

ter::Camera cam;
cam.fov = 70;
ter::FirstPersonCamera player;
player.position = {0, 1.6f, 0};   // ระดับสายตา
player.walkSpeed = 2.5f;
player.runSpeed = 5;              // กด Shift
player.collide = [&](glm::vec3, glm::vec3 to) { return ter::resolveCircle(to, 0.3f, walls); };

// ในแต่ละเฟรม
if (in.pressed(ter::Mouse::Left)) app.lockCursor(true);
float moved = player.update(in, f.dt, app.cursorLocked());   // คืนระยะที่เดิน (ใช้เล่นเสียงเท้า)
player.apply(cam);

// มองอะไรอยู่ (ภายใน 2 เมตร)
float dist;
int hit = ter::raycast(player.position, player.forward(), walls, 2.0f, &dist);
if (hit == doorIndex && in.pressed(ter::Key::E)) openDoor();
```

`player.fly = true` ทำให้บินไปตามทิศที่มอง (ใช้ debug)

---

## UI ภาษาไทย (Canvas2D) และ Overlay

UI วาดด้วย `ter::Canvas2D` ซึ่งเรียกเหมือน HTML canvas และใช้ CoreText ของ macOS ตัดคำ/วางสระภาษาไทยได้ถูกต้อง
App จะเรียก `onUi` **เฉพาะเมื่อสั่ง `redrawUi()`** (และตอนหน้าต่างเปลี่ยนขนาด) ถ้า UI ไม่เปลี่ยนก็ไม่เสียเวลาวาดเลย

```cpp
std::string hint = "กด E เพื่อเปิดประตู";
app.onUi = [&](ter::Canvas2D& g, const ter::Frame& f) {
    // หน่วยเป็น point เหมือนขนาดหน้าต่าง
    g.setFont("Mali", 22, 600);            // ฟอนต์ที่มากับ engine: "Mali" (ไทย), "Inter", "Cormorant Garamond"
    g.setFill("#ffffff");                  // สีแบบ CSS: "#rgb", "rgba(...)", "white"
    g.setTextAlign(ter::TextAlign::Center);
    g.setShadow(6, ter::cssColor("rgba(0,0,0,0.6)"), 0, 2);
    g.fillText(hint, f.width / 2.0f, f.height - 40.0f);

    g.setFill("rgba(0,0,0,0.5)");
    g.beginPath();
    g.roundRect(20, 20, 200, 60, 12);
    g.fill();
};
// เปลี่ยนข้อความเมื่อไร ให้สั่งวาดใหม่
hint = "ประตูล็อกอยู่…";
app.redrawUi();
```

ฟอนต์อื่น: `ter::registerFont("path/font.ttf")` แล้วใช้ชื่อ family ใน `setFont`
ตัดบรรทัดข้อความยาว: `g.wrapText(text, maxWidth)` ตัดคำภาษาไทยที่ไม่มีเว้นวรรคได้

**Overlay** ใช้กับของที่เปลี่ยนทุกเฟรม เช่น fade จอ, แฟลชสีแดงตอนโดนผี, หลอดพลัง ต้องเรียกใหม่ทุกเฟรมเพราะวาดครั้งเดียวแล้วถูกล้าง:

```cpp
app.overlay().fill({0, 0, 0, fade});                          // ทั้งจอ (RGBA 0..1)
app.overlay().rect(20, 20, 200 * hp, 10, {0.8f, 0.1f, 0.1f, 1}, 4);   // x, y, w, h (point), สี, มุมมน
app.overlay().image(*tex, 100, 100, 64, 64);                  // รูปจาก Texture
```

---

## เสียง

```cpp
ter::Audio& audio = app.audio();

// เล่นไฟล์ (wav / mp3 / flac) — ไฟล์ถูกถอดรหัสครั้งเดียวแล้ว cache ไว้
audio.play(ASSET_DIR "/sfx/door.wav");
audio.play(ASSET_DIR "/sfx/step.wav", {.volume = 0.6f, .pitch = 0.9f + ter::frand() * 0.2f});

// เพลงยาวๆ: stream จาก disk
auto music = audio.stream(ASSET_DIR "/music/ambient.mp3", {.volume = 0.4f, .loop = true, .fadeIn = 3});

// เสียง 3 มิติ: ดังเบาตามระยะ และซ้าย/ขวาตามทิศที่หัน (หูอยู่ที่กล้องอัตโนมัติ)
auto whisper = audio.play(ASSET_DIR "/sfx/whisper.wav",
                          {.loop = true, .spatial = true, .position = ghost->position, .minDistance = 1, .maxDistance = 20});
whisper->setPosition(ghost->position);   // ย้ายตามผีทุกเฟรม
whisper->fadeTo(0, 2.0f, true);          // ค่อยๆ เงียบใน 2 วินาทีแล้วหยุด
whisper->setPitch(0.7f);

audio.setMasterVolume(0.8f);
audio.setMuted(true);
audio.stopAll();
```

- ถ้าเก็บ `SoundPtr` ไว้ จะควบคุมเสียงนั้นต่อได้ (หยุด, ย้ายตำแหน่ง, เปลี่ยนความดัง) ถ้าไม่เก็บ เสียงจะเล่นจนจบแล้วถูกเก็บกวาดเอง
- ถ้าจะวางตำแหน่งหูเอง ให้ตั้ง `app.listenerFollowsCamera = false` แล้วเรียก `audio.setListener(pos, forward)`

**เสียง placeholder จาก synth** ใช้ตอนยังไม่มีไฟล์เสียงจริง:

```cpp
audio.synth("step",  {.wave = ter::Wave::Noise, .duration = 0.09f, .volume = 0.12f});
audio.synth("hum",   {.wave = ter::Wave::Sine, .freq = 110, .duration = 2, .attack = 0.6f, .volume = 0.25f, .vibrato = 0.3f});
audio.synth("alarm", {.wave = ter::Wave::Square, .freq = 880, .freqEnd = 440, .duration = 0.4f});
// หลายเสียงซ้อนกัน (วินาทีที่เริ่ม, synth)
audio.synth("chord", std::vector<std::pair<float, ter::Synth>>{{0, {.freq = 220}}, {0.1f, {.freq = 233}}});
audio.play("step");   // เล่นด้วยชื่อ เหมือนไฟล์
```

`Wave`: `Sine`, `Triangle`, `Square`, `Sawtooth`, `Noise` · `freqEnd` = เสียงไถลไปความถี่นี้ · `vibrato` = สั่น (หน่วย semitone)

---

## Shader ของตัวเอง

```cpp
ter::addShaderPath(ASSET_DIR "/shaders");   // ค้นหา <name>.vert / <name>.frag ที่นี่ก่อน shader ในตัว
auto mat = std::make_shared<ter::Material>();
mat->type = ter::MaterialType::Custom;
mat->shader = ter::sharedShader("ghost");   // โหลด ghost.vert + ghost.frag ครั้งเดียวแล้ว cache
mat->transparent = true;
mat->uniforms = [&](const ter::Shader& s) { s.set("uFade", fade); };
auto ghost = ter::makeMesh(ter::geo::sphere(0.5f), mat);
```

ข้อกำหนดของ shader (`#version 410 core`):
- vertex attribute: `layout(location = 0) in vec3 aPos;` · `1 aNormal` · `2 aUV` · `3 aColor` · `4 aInstance` (mat4 ของ instanced mesh)
- uniform ที่ engine ตั้งให้ทุกครั้ง: `uModel`, `uView`, `uProj`, `uNormalMatrix`, `uInstanced`, `uCamPos`, `uTime`, `uResolution`, `uMap` (+ `uHasMap`), `uOpacity`, `uAlphaTest`, `uMapOffset`, `uMapRepeat`
- สีที่ออกจาก fragment shader เป็น **linear** (post pass แปลงเป็น sRGB ให้)
- ใส่ชื่อเดียวกับ shader ในตัว (`mesh`, `post`, …) ในโฟลเดอร์ของเรา จะใช้แทนของ engine ได้
- `ter::reloadSharedShaders()` โหลด shader ใหม่ทั้งหมดตอนรันอยู่ (ผูกกับปุ่มไว้ตอนปรับ shader สะดวกมาก)
- มี `shader.loadSource(vertText, fragText)` สำหรับ shader ที่เขียนเป็น string ในโค้ด

---

## ทดสอบด้วยภาพหน้าจอ (`--shot`)

ทุกเกมที่สร้าง `App` ด้วย `argc, argv` ได้ flag เหล่านี้ฟรี:

```sh
./build/mygame --shot out.png --shot-delay 2    # รัน 2 วินาที บันทึกภาพ แล้วปิด
./build/mygame --pixel 3                        # ภาพ pixel ใหญ่ขึ้น
./build/mygame --fullscreen
```

ใช้ตรวจว่าฉากออกมาถูกต้องโดยไม่ต้องนั่งเล่นเอง ถ้าจะทดสอบการเดิน ให้เรียก `app.input().simulate(ter::Key::W, true)` ในโค้ด

---

## โครงสร้างไฟล์

```
terEngine/
├── CMakeLists.txt          library target ter::engine (+ examples เมื่อ build เดี่ยว)
├── cmake/EmbedShaders.cmake    ฝัง shaders/*.vert|frag เข้าไลบรารี
├── shaders/                mesh, depth (เงา), sprite, post (สี/vignette/grain), tilt, overlay
├── fonts/                  Mali, Inter, Cormorant Garamond (สัญญาอนุญาต OFL — ดู OFL-*.txt)
├── examples/               01_hello_cube, 02_dark_room, 03_model_viewer
└── ter/
    ├── ter.h               include รวม
    ├── app/                app, input, fps_camera
    ├── audio/              audio (miniaudio)
    ├── core/               math (PI, hexc, Rng, frand), collide
    ├── gfx/                gl, shader, texture, image.mm (ImageIO), pixel_canvas
    ├── scene/              object, geometry, material, renderer, gltf
    └── ui/                 canvas2d (CoreText)
```

แก้ shader ใน `shaders/` แล้ว build ใหม่ CMake จะฝังให้อัตโนมัติ

---

## ข้อจำกัด

- **macOS เท่านั้น** เพราะใช้ ImageIO (อ่านรูป), CoreText (ตัวอักษร) และ OpenGL 4.1 ที่ Apple หยุดพัฒนาแล้ว (ยังใช้ได้ แต่มี warning ว่า deprecated ซึ่งปิดไว้แล้ว)
  ถ้าจะย้ายไป Windows/Linux ต้องเปลี่ยน `gfx/image.mm` → stb_image, `ui/canvas2d.mm` → FreeType + HarfBuzz และ `gfx/gl.h` → glad
- ไม่มีระบบฟิสิกส์ มีแค่การชนแบบวงกลมกับกล่อง (`collide.h`)
- มีแค่ดวงอาทิตย์ที่ทอดเงา ไฟจุดและไฟสปอตยังไม่ทอดเงา
- ไฟจุด/สปอตใช้พร้อมกันได้สูงสุด 16 ดวง
- ไม่มี editor และไม่มีระบบจัดการ scene/asset ต้องเขียนเองด้วยโค้ด
- API ยังเปลี่ยนได้ (version 0.1)
