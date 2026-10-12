# Lighting and post-processing

[← Back to README](../README.md) · Headers: `ter/scene/renderer.h`, `ter/scene/object.h`

There are four kinds of light. The first two are global and live in `ter::Environment`. The other two are objects you add to the scene.

| Light | Where | Casts shadows | Typical use |
|---|---|---|---|
| Hemisphere (ambient) | `Environment` | — | Base brightness so nothing is pitch black |
| Sun / moon (directional) | `Environment` | **Yes** | Outdoors, big rooms |
| Point light | Scene object | No | Candles, bulbs, glowing things |
| Spot light | Scene object | No | Flashlights, street lamps |

Nothing is lit by default: a fresh `Environment` is completely dark.

---

## Environment: ambient, sun and fog

```cpp
ter::Environment env;

// ambient: color from above (sky) blended with color from below (ground)
env.hemiSky = ter::hexc(0xbfd9ff);
env.hemiGround = ter::hexc(0x4a3c2a);
env.hemiIntensity = 0.6f;

// one sun (or moon) — the only light that casts shadows
env.sunColor = ter::hexc(0xfff1d6);
env.sunIntensity = 1.2f;            // 0 = no sun, and no time spent on shadows
env.sunPosition = {4, 8, 3};        // the light shines from here ...
env.sunTarget = {0, 0, 0};          // ... toward here
env.sunShadow = true;
env.shadowHalf = 14;                // shadows cover a square of 2 x 14 m around sunTarget

// linear fog: clear until fogNear, fully fogColor at fogFar
env.fogColor = ter::hexc(0x9cc2e8);
env.fogNear = 10;
env.fogFar = 30;

app.renderer().background = env.fogColor;   // make the "sky" match the fog
```

**Shadows look blocky or are missing?** The shadow area is a box around `sunTarget` with half-width `shadowHalf`. Make it just large enough to cover what the player sees, and move `sunTarget` (and `sunPosition`) with the player in large levels. Raise `renderer().shadowMapSize` for sharper shadows.

Objects only take part in shadows if you set `castShadow` / `receiveShadow` on them.

---

## Point lights

```cpp
// color, intensity, range in meters (0 = infinite), decay
auto lamp = ter::makePointLight(ter::hexc(0xffaa55), 2.0f, 6.0f);
lamp->position = {0, 2, 0};
scene->add(lamp);

lamp->intensity = 0;   // switch off
```

## Spot lights

```cpp
// color, intensity, range, cone half-angle (radians), edge softness 0..1
auto torch = ter::makeSpotLight(ter::hexc(0xfff2d8), 3.0f, 14.0f, 0.42f, 0.5f);
scene->add(torch);

// every frame: put it at the eye and aim it where the player looks
torch->position = eye;
torch->lookAt(eye + forward);
```

A spot light shines along its **local +Z axis**, and `lookAt` points that axis at a target. Because lights are ordinary objects, you can also attach one to a moving parent.

**Limits:** up to **16** point + spot lights are used at once (the closest to the camera). Neither type casts shadows.

**Flicker:** change `intensity` randomly in your update:

```cpp
lamp->intensity = ter::frand() < 0.03f ? 0.3f : 2.0f;
```

---

## Post-processing: `app.renderer()`

These settings affect the whole image after the 3D scene is drawn.

| Field | Default | Meaning |
|---|---|---|
| `background` | black | Color where nothing is drawn. Usually set equal to `fogColor`. |
| `vignette` | 0.35 | Darkened corners. 1 and above is a heavy horror-movie look. |
| `warm` | 0.04 | Positive = warmer/orange, negative = colder/blue |
| `grain` | 0 | Animated film noise; 0.03–0.08 works well |
| `shadows` | true | Global shadow on/off |
| `shadowMapSize` | 2048 | Shadow resolution |
| `tiltShift`, `tiltAmount` | false, 0.35 | Blur the top and bottom (miniature look) |

Resolution is set with `AppConfig::pixelScale` (or `--pixel N`). With 2 or 3 the scene is rendered smaller and scaled up with crisp pixels.

### Recipes

```cpp
// horror: dark, cold, grainy
ter::Renderer& r = app.renderer();
r.background = ter::hexc(0x020203);
r.vignette = 1.2f;
r.warm = -0.06f;
r.grain = 0.06f;
env.hemiSky = ter::hexc(0x20242c);
env.hemiGround = ter::hexc(0x050505);
env.hemiIntensity = 0.25f;
env.fogColor = r.background;
env.fogNear = 2;
env.fogFar = 14;
```

```cpp
// sunny day
env.hemiSky = ter::hexc(0xbfd9ff);
env.hemiGround = ter::hexc(0x4a3c2a);
env.hemiIntensity = 0.6f;
env.sunIntensity = 1.2f;
env.sunPosition = {4, 8, 3};
env.fogColor = ter::hexc(0x9cc2e8);
env.fogNear = 20;
env.fogFar = 60;
app.renderer().background = env.fogColor;
```
