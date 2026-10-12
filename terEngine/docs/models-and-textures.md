# Models and textures

[← Back to README](../README.md) · Headers: `ter/scene/gltf.h`, `ter/gfx/texture.h`, `ter/gfx/pixel_canvas.h`

---

## glTF models (`.glb` / `.gltf`)

glTF is the format to use. Blender exports it directly (**File → Export → glTF 2.0**, choose `.glb`).

```cpp
auto model = ter::loadGltf(ter::assetPath("models/ghost.glb"));
if (!model) return 1;                       // file missing or invalid (reason printed)
scene->add(model->root);

model->root->position = {0, 0, -4};
model->root->traverse([](ter::Object3D& o) { o.castShadow = true; });
```

`loadGltf` returns a `std::optional<ter::GltfScene>` with:

- `root`: an `ObjPtr` holding the whole model, built from ordinary `Object3D`s. You can move it, scale it, find children by `name`, swap materials, etc.
- `animations`: the animation clips in the file.

Materials become `Standard` (PBR) materials with their textures. Meshopt-compressed meshes and WebP textures are supported.

### Animations

```cpp
ter::AnimationMixer mixer(model->animations);   // plays every clip, looping

// every frame
mixer.update(f.dt);
```

The mixer is intentionally minimal: all clips play at once on a loop. For a model with several actions (walk, idle, attack), export each action as its own `.glb` and swap which one is visible, or keep one clip per file.

### Finding parts of a model

```cpp
ter::Object3D* jaw = nullptr;
model->root->traverse([&](ter::Object3D& o) { if (o.name == "Jaw") jaw = &o; });
```

Use `examples/03_model_viewer.cpp` to check that a model loads and animates before putting it in your game:

```sh
./build/examples/03_model_viewer assets/models/ghost.glb
```

---

## Textures from image files

PNG, JPEG and WebP are supported.

```cpp
auto tex = std::make_shared<ter::Texture>();
ter::TextureOptions opt;
opt.nearest = true;    // crisp pixels (pixel art); false = smooth filtering
opt.repeat = true;     // tile when UVs go beyond 0..1
if (!tex->load(ter::assetPath("textures/wall.png"), opt)) { /* missing file */ }

auto wall = ter::makeMesh(ter::geo::box(4, 3, 0.2f), ter::lambert(tex));
```

| `TextureOptions` | Default | Meaning |
|---|---|---|
| `srgb` | true | The image holds colors (true) or data like masks (false) |
| `nearest` | true | Nearest-neighbor (pixel art) vs. smooth filtering |
| `mipmaps` | true | Smoother at a distance |
| `repeat` | true | Tile vs. clamp at the edges |
| `flipY` | true | Image row 0 at the top of the UVs |
| `maxSize` | 0 | Halve larger images until they fit (0 = keep full size) |

Tiling: `mat->mapRepeat = {4, 2};` repeats the texture 4 times across and 2 times up.
Sprite-sheet animation: set `mapRepeat` to one frame's size and move `mapOffset` each frame.

---

## Textures drawn in code

### Pixel art: `ter::Canvas`

```cpp
ter::Canvas c(16, 16);
c.fill(0, 0, 16, 16, 0x3a3430);   // x, y, w, h, 0xRRGGBB
c.px(4, 5, 0xff0000);             // single pixel
c.circle(8, 8, 3, 0xffffff);      // anti-aliased disc
c.blend(0, 12, 16, 4, 0x000000, 0.5f);   // translucent overlay

auto tex = std::make_shared<ter::Texture>();
tex->upload(c.w, c.h, c.rgba);
```

### Text, shapes and gradients: `ter::Canvas2D`

See [Text in the 3D world](ui.md#text-in-the-3d-world-notes-signs). Draw on a `Canvas2D`, then call `canvas.upload(*tex)`.

### Raw pixels

```cpp
std::vector<uint8_t> rgba(w * h * 4);   // rows top to bottom
// ... fill ...
tex->upload(w, h, rgba);                // re-uploading the same size reuses GPU memory
```

Save pixels to disk with `ter::savePNG(path, w, h, rgba)`.
