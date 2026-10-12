# Scene, meshes and materials

[← Back to README](../README.md) · Headers: `ter/scene/object.h`, `ter/scene/geometry.h`, `ter/scene/material.h`

If you know three.js you already know this page: terEngine copies its scene graph, geometry generators and material model.

---

## Objects and the scene tree

Everything in a scene is a `ter::Object3D`, held by a `ter::ObjPtr` (`std::shared_ptr<Object3D>`). Objects have children, and a child moves, rotates and scales with its parent.

```cpp
auto scene = ter::makeGroup("root");   // an empty container

auto box = ter::makeMesh(ter::geo::box(1, 2, 1), ter::standard(ter::hexc(0x8899aa), 0.7f, 0.0f));
box->position = {0, 1, -3};            // meters; Y up, -Z forward
box->rotation = {0, ter::PI / 4, 0};   // Euler angles, XYZ order, radians
box->scale = glm::vec3(1.5f);
scene->add(box);
```

### Creating objects

| Function | Creates |
|---|---|
| `ter::makeGroup(name)` | Empty node for grouping |
| `ter::makeMesh(geometry, material)` | A visible mesh |
| `ter::makeMesh(geometry, {mat0, mat1, ...})` | A mesh with one material per geometry group |
| `ter::makeInstanced(geometry, {material}, count)` | Many copies in one draw call (see below) |
| `ter::makeSprite(material)` | A flat image that always faces the camera |
| `ter::makePointLight(...)`, `ter::makeSpotLight(...)` | Lights (see [Lighting](lighting.md)) |

### Common fields and methods

```cpp
obj->visible = false;                  // hide (and its children)
obj->castShadow = true;                // casts a shadow from the sun
obj->receiveShadow = true;             // shadows fall on it
obj->renderOrder = 1;                  // draw order among transparent objects

parent->add(child);                    // also accepts several: scene->add(a, b, c)
obj->removeFromParent();
parent->remove(child.get());

obj->lookAt({0, 0, 0});                // turn local +Z toward a world point
glm::vec3 p = obj->worldPosition();    // position after all parents are applied
glm::vec3 w = obj->localToWorld({0, 1, 0});

scene->traverse([](ter::Object3D& o) { o.castShadow = true; });   // visit every descendant
auto copy = obj->clone();              // shares geometry and materials, copies the transform and children

obj->onBeforeRender = [](ter::Object3D& o) { /* runs right before drawing */ };
```

`worldPosition()` and `localToWorld()` read `matrixWorld`, which the renderer updates each frame. If you need fresh values for an object you just moved in the same frame, call `obj->updateWorldFromParents()` first.

---

## Geometry

Generators live in `ter::geo::` and take the same parameters as their three.js r151 versions:

| Function | Shape |
|---|---|
| `box(w, h, d)` | Box centered on the origin |
| `plane(w, h)` | Flat rectangle in the XY plane, facing +Z. **Rotate `x = -PI/2` to make a floor.** |
| `sphere(radius, widthSegments, heightSegments)` | Sphere |
| `cylinder(radiusTop, radiusBottom, height, radialSegments)` | Cylinder / truncated cone |
| `cone(radius, height, radialSegments)` | Cone |
| `circle(radius, segments)` | Flat disc |
| `torus(radius, tube, radialSegments, tubularSegments)` | Ring |
| `dodecahedron(r)`, `octahedron(r)`, `icosahedron(r)` | Low-poly solids (rocks, gems) |
| `shape(ter::Shape)` | Flat filled 2D outline |
| `extrude(ter::Shape, ExtrudeOptions)` | 2D outline pushed into 3D, with optional bevel |
| `fromPositions({x,y,z, ...})` | Raw triangles from a list of floats |
| `merge({g1, g2, ...})` | Combine several geometries into one (fewer draw calls) |

```cpp
// an arch outline, extruded 0.3 m deep
ter::Shape s;
s.moveTo(-1, 0).lineTo(-1, 2).quadTo(0, 3, 1, 2).lineTo(1, 0);
auto arch = ter::geo::extrude(s, {.depth = 0.3f, .bevelEnabled = false});
```

Geometries can be edited **before their first draw** (the CPU copy is freed after upload):

```cpp
auto g = ter::geo::box(1, 1, 1);
g->translate(0, 0.5f, 0);   // pivot at the bottom instead of the center
g->rotateY(ter::PI / 4);
g->scale(2, 1, 1);
```

For geometry you change every frame (particles, a waving cloth), set `g->dynamic = true` before the first draw, edit `g->position` etc., then call `g->markDirty()`.

---

## Materials

| Constructor | Lighting | Good for |
|---|---|---|
| `ter::basic(color, texture = nullptr)` | None (unlit) | Glowing signs, skies, UI-like objects in 3D |
| `ter::lambertColor(color)` | Matte | Most things; cheapest lit material |
| `ter::lambert(texture, color = white)` | Matte, textured | Textured walls and floors |
| `ter::standard(color, roughness, metalness)` | PBR | Shiny or metallic surfaces |
| `ter::spriteMaterial(texture, color, opacity)` | Unlit | Use with `makeSprite` |

Constructors return a `ter::MatPtr` (`shared_ptr<Material>`), and you can change any field afterwards:

| Field | Meaning |
|---|---|
| `color` | Base color (`ter::hexc(0xRRGGBB)`) |
| `map` | Color texture |
| `emissive`, `emissiveMap`, `emissiveIntensity` | Light the surface gives off itself (visible in the dark) |
| `roughness`, `metalness` | PBR parameters (Standard only) |
| `opacity` + `transparent = true` | See-through |
| `alphaTest` | Cut out pixels whose alpha is below this value (leaves, fences) |
| `blending = ter::Blending::Additive` | Glows, fire, light beams |
| `side` | `Front` (default), `Back`, `Double` |
| `depthWrite`, `depthTest` | Turn off for overlays and glows |
| `fog` | `false` to ignore the scene's fog |
| `vertexColors` | Use per-vertex colors from the geometry |
| `mapOffset`, `mapRepeat` | Scroll or tile the texture (sprite-sheet animation) |

```cpp
auto ghostMat = ter::basic(ter::hexc(0xaaccff));
ghostMat->transparent = true;
ghostMat->opacity = 0.4f;
ghostMat->blending = ter::Blending::Additive;
ghostMat->depthWrite = false;
```

Several meshes can share one material. Changing it changes all of them. Use `mat->clone()` to get an independent copy.

---

## Many copies: instancing

For dozens or thousands of identical objects (candles, grass, trees), use one instanced mesh instead of many meshes:

```cpp
const int n = 200;
auto candles = ter::makeInstanced(ter::geo::cylinder(0.03f, 0.03f, 0.2f), {ter::lambertColor(ter::hexc(0xeee6d0))}, n);
for (int i = 0; i < n; ++i) {
    glm::vec3 pos(ter::frand() * 20 - 10, 0.1f, ter::frand() * 20 - 10);
    glm::quat rot = ter::eulerToQuat({0, ter::frand() * ter::PI * 2, 0});
    candles->instances[i] = ter::composeMatrix(pos, rot, glm::vec3(1));
}
scene->add(candles);
// after changing instances later: candles->instancesDirty = true;
```

---

## Sprites

A sprite is a textured quad that always faces the camera. Its size is its `scale`:

```cpp
auto tex = std::make_shared<ter::Texture>();
tex->load(ter::assetPath("textures/eye.png"));
auto eye = ter::makeSprite(ter::spriteMaterial(tex));
eye->position = {0, 1.7f, -5};
eye->scale = {0.5f, 0.5f, 1};
scene->add(eye);
```
