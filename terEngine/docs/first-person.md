# First-person camera and collision

[← Back to README](../README.md) · Headers: `ter/app/fps_camera.h`, `ter/core/collide.h`

terEngine has no physics engine. It gives you exactly what a walking game needs: a first-person controller, circle-vs-box collision so you can't walk through walls, and ray-vs-box tests for "what am I looking at".

---

## The camera

```cpp
ter::Camera cam;
cam.fov = 70;          // vertical field of view in degrees
cam.near = 0.05f;      // small near plane so walls don't clip when you stand close

ter::FirstPersonCamera player;
player.position = {0, 1.6f, 0};   // eye position; 1.6 m is a standing adult
player.yaw = 0;                   // 0 looks toward -Z; positive turns left
player.pitch = 0;                 // positive looks up
player.walkSpeed = 2.5f;          // m/s
player.runSpeed = 5;              // while Shift (or left stick click) is held
player.mouseSensitivity = 0.0025f;
player.apply(cam);
```

Each frame:

```cpp
if (in.pressed(ter::Mouse::Left)) app.lockCursor(true);
float moved = player.update(in, f.dt, app.cursorLocked());   // mouse look only while locked
player.apply(cam);                                           // copy the view into the rendered camera
```

`update` returns the distance moved this frame. Add it up to trigger footstep sounds or head bob:

```cpp
walked += moved;
if (walked > 0.7f) { walked = 0; audio.play("step"); }
```

Useful helpers:

| | |
|---|---|
| `player.forward()` | Unit vector you are looking along (includes pitch) |
| `player.right()` | Unit vector to your right (flat) |
| `player.fly = true` | Move along the view direction, ignoring the ground (noclip for debugging) |

Head bob, crouching or a scripted camera are just changes to `player.position` or `cam` before/after `apply`.

---

## Collision with walls

Walls are axis-aligned boxes:

```cpp
std::vector<ter::Box> walls;
walls.push_back(ter::Box::centered({0, 1.5f, -8}, {16, 3, 0.3f}));   // center, full size
walls.push_back({{-1, 0, -1}, {1, 2, 1}});                           // or min corner, max corner
```

Tell the player how to collide:

```cpp
player.collide = [&](glm::vec3 from, glm::vec3 to) {
    return ter::resolveCircle(to, 0.3f, walls);   // player = circle of radius 0.3 m
};
```

`resolveCircle(p, radius, boxes, y0 = 0.1, y1 = 1.8)` pushes the point out of every box whose height overlaps `y0..y1`. That means low boxes (under 0.1 m, e.g. a rug) and high boxes (above 1.8 m, e.g. a ceiling beam) don't block you.

### Pattern: one helper for mesh + collider

Keep the visible mesh and its collider together:

```cpp
auto addBlock = [&](glm::vec3 center, glm::vec3 size, const ter::MatPtr& mat) {
    auto mesh = ter::makeMesh(ter::geo::box(size.x, size.y, size.z), mat);
    mesh->position = center;
    mesh->castShadow = mesh->receiveShadow = true;
    scene->add(mesh);
    walls.push_back(ter::Box::centered(center, size));
    return walls.size() - 1;   // index, useful for raycasts
};
```

A door that opens: remove its collider (or move it away) and hide/rotate the mesh:

```cpp
walls[doorIndex] = ter::Box{{0, -10, 0}, {0, -10, 0}};   // a box far below the floor never blocks
doorMesh->rotation.y = ter::PI / 2;
```

---

## What am I looking at? Raycasts

```cpp
float dist;
int hit = ter::raycast(player.position, player.forward(), walls, 2.0f, &dist);   // max 2 m
if (hit == doorIndex && in.pressed(ter::Key::E)) openDoor();
```

`raycast` returns the index of the **nearest** box hit within the distance, or `-1`. Because a wall in front of an object is nearer, you can't interact through walls.

The same call answers line-of-sight questions ("can the monster see the player?"):

```cpp
glm::vec3 toPlayer = player.position - monsterEye;
float d = glm::length(toPlayer);
bool blocked = ter::raycast(monsterEye, toPlayer / d, walls, d) != -1;
```

Interactable objects that shouldn't block movement can live in their own `std::vector<ter::Box>` that you raycast against but don't pass to `resolveCircle`.

For a single box use `ter::rayBox(origin, dir, box, maxDist)`, which returns the distance or `-1`. `box.contains(point)` checks whether a point is inside, which is handy for trigger zones:

```cpp
ter::Box exitZone = ter::Box::centered({0, 1, -9}, {2, 2, 1});
if (exitZone.contains(player.position)) win();
```
