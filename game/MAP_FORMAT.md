# map.json format

The level is `assets/data/map.json`. Edit it and rebuild (`cmake --build build`, which only re-copies the file) to see the change. No C++ changes are needed.

- Units are **meters**. Axes: **+X = east (right)**, **−Z = north (forward)**, **+Y = up**. The shelter is at the origin.
- Colors are `"#rrggbb"`. Angles are in **degrees**.
- `//` and `/* */` comments are allowed.

```
          −Z  north: cliff → valley (z < −8.6), power line, guardrail
   ─────────────────────────────── road (z = −7 … −1)
            [sign]  ┌shelter┐
   (lamp)           │   ●   │       ● = companion
                    └───────┘  ║ path down to the village
          +Z  south: forest     ║
```

## Top level

| Key | Meaning |
|---|---|
| `atmosphere` | Ambient light, fog and post-processing (see below) |
| `ground` | `min` / `max` [x, z], `texture` (default `grass`), `tile` (meters per repeat), `color` (tint), `seed` (light/dark patches) |
| `sky` | Sky dome, mountains and moon (see below) |
| `bounds` | `min` / `max` [x, z]: invisible walls the player can't cross |
| `spawn` | `pos` [x, y, z] on the ground (eye height is added), `yawDeg` (0 = facing north, 90 = west, 180 = south, −90 = east) |
| `objects` | Everything placed in the world, each with a `type` |
| `hideSpots` | Hiding places (E with the phone away): `id`, `eye` [x, y, z] where the view goes, `yawDeg` the way it faces (0 = north, −Z), `radius` how close to get in, `lookDeg` how far the view may turn, `kind`: `"sit"` = a seat instead (rule 9: the bench; the view doesn't darken) |
| `lights` | Extra point lights |
| `zones` | Named boxes used by rules and events (`min`, `max` as [x, y, z]) |
| `sounds` | Looping sounds; with `pos` they are 3D, without it they play everywhere |
| `ambient` | Random one-shot sounds around the player |

## `atmosphere`

The fog is volumetric (computed in `assets/shaders/mesh.frag`): thick near the ground, thinner higher up, rolling like smoke and drifting with the wind, lit by lamps and the flashlight, and fading into darkness far away (like DOOM / the cub3D fog). `smoke` objects add visible drifting puffs on top.

| Key | Default | Meaning |
|---|---|---|
| `hemiSky`, `hemiGround`, `hemiIntensity` | dark blue, black, 0.25 | Ambient light from above and below |
| `fogColor` | `#0b0d10` | Colour of unlit fog (keep it equal to `sky.horizon`) |
| `visibility` | 25 | Meters of ground fog you can see through before it's ~95% opaque |
| `fogHeight` | 4 | How quickly the fog thins with height (meters). Small = a low layer you can see over; large = fog everywhere |
| `vignette`, `warm`, `grain` | 1.1, −0.05, 0.06 | Dark corners, color temperature, film noise. Keep `grain` ≤ 0.015: it is added before the sRGB conversion, so dark areas turn into TV static quickly |

Finer control (patch size, wind, how strongly lights glow, flashlight beam strength, quality) is in the `FOG_*` constants at the top of `assets/shaders/mesh.frag`. Rebuild to apply.

## Object types

Every object has `"type"` and `"pos"` [x, y, z]. An unknown type prints a warning and is skipped.

| Type | Keys | Notes |
|---|---|---|
| `box` | `size` [x, y, z], `color`, `rot` [x, y, z] degrees, `collide` (default true), `unlit` (glows, ignores light), `fog` (false = visible through fog), `texture` + `tile` | `pos` is the **center** of the box. Building block for everything. With a texture, `color` tints it. |
| `tree` | `scale`, `rotY`, `variant` (0–2 pines, 3 dead tree) | `pos` is the base of the trunk. Pines are 10–14 m tall at scale 1. |
| `forest` | scatter keys, `dead` (share of dead trees, default 0.08), `saplings` (share of young small pines, default 0.15) | Scatters trees; a different `seed` gives a different forest. The ground under trees darkens automatically. |
| `bus_shelter` | `rotY` (multiples of 90), `title`, `subtitle`, `posters` (up to 2, list of `{kind: "timetable" or "missing", title, lines}`), `logo` (image path under assets/, default `"textures/jam_logo.png"`, shown as a poster on a plank board in the right bay of the back; `""` = none), `light` `{intensity, range, flicker, color}` | `pos` = centre of the floor. Model `assets/models/sala.glb` (source: `modelResource/sala_source.blend`): Thai gable roof over a 5.8 × 3.2 m floor, eaves reach ±3.4 × ±2.1 m. Opens toward local −Z (rotY 0 = facing the road to the north). Has its own tube light and colliders |
| `model` | `file` (glb path under assets/), `rotY`, `scale`, `anim` (clip to loop), `hide` (list of node names to hide), `collide` [w, h, d] | Any glTF model as a decorative object; `pos` = its origin. No collider unless `collide` is given |
| `lamp_post` | `rotY` (direction of the arm), `height`, `reach`, `color`, `intensity`, `range`, `flicker` (chance per frame), `on` | Steel pole on a footing, curved arm, cobra-head lamp with a halo. `"on": false` = dead lamp with a drooping head |
| `sign` | `rotY` (the face looks along +Z at 0, toward the road at 180), `lines`: list of text, `background`, `glow` | Bus symbol + text. First line is the big title. Thai works |
| `bin` | `color` | Steel drum with an open lid |
| `bag` | `rotY`, `color`, `visible` (false) | Rule 11: a backpack like the player's. Hidden until the rule's window (signal `bag`); E picks it up (flag `took_bag`) |
| `shadow_figure` | `rotY`, `visible` (false) | Rule 4: a flat-black person standing still (npc.glb). Shown for the rule's window (signal `shadow_figure`) |
| `km_post` | `route`, `place`, `km`, `band` (top colour), `rotY` | Thai kilometre stone, text on both faces |
| `road_sign` | `shape` (`diamond`, `circle`, `rect`), `symbol` (`curve_left`, `curve_right`, `animal`, `number`, `none`), `text`, `size`, `height`, `glow`, `rotY` (face looks along +Z at 0; −90 faces west) | Reflective road sign on a steel pole |
| `companion` | `rotY`, `anim` (clip to start with: `idle`, `walk`, `talk`, `sit`, `sit_talk`, `crouch`, `prone`, `board_bus`), `facePlayer` (default true) | The companion (model `assets/models/npc.glb`, 1.8 m tall, feet at `pos`). Once the game runs, `assets/data/companion.json` takes him over (his seat, following, talking; see EVENTS_FORMAT.md). For `sit`, put `pos` on the floor under the middle of the seat (seat 0.44 m above `pos`) with `rotY` facing away from the backrest |
| `grandma` | `id` (default `grandma`), `rotY`, `anim` (`idle`, `walk`, `beckon`, `fallen`, `fallen_call`, `lunge`, `push`, `push_walk`, `neck_stretch`), `face` (`normal` default / `scary`), `facePlayer` (default true), `visible` (default true) | The old woman (model `assets/models/grandma.glb`, 1.5 m, stooped, feet at `pos`). Two faces on one head; code swaps them with `world.grandmas[i].setScary(bool)` (rule 6: scary face = help her, ordinary face = pretend not to see). The scripted encounter with the stuck cart is not a map object: see `grandma.json` in EVENTS_FORMAT.md. **G** swaps the faces of map grandmas too. No collider |
| `spirit_shrine` | `rotY` | The old spirit house on its post under a big banyan (model `assets/models/shrine.glb`, the house faces +Z, `pos` = the foot of the post; the tree stands 0.95 m to its left and 0.85 m behind). Offerings, zebra statues, three-colour cloth. Colliders on the post and the trunk. Its door is where the old woman's head comes out (grandma.json, forest walk) |

Landscape types take a line `from` → `to` ([x, z]) or a scatter area instead of `pos`:

| Type | Keys | Notes |
|---|---|---|
| `strip` | `from`, `to`, `width`, `texture`, `tile` (m along), `tileWidth` (m across, default = width), `color`, `layer`, `y`, `roughness` (glossy, e.g. 0.3 = damp asphalt) | Flat band on the ground: road (`asphalt`), shoulders and paths (`dirt`). Higher `layer` draws on top. |
| `cliff` | `from`, `to` (top edge), `out` [x, z] (direction it drops toward), `depth`, `lean`, `tile`, `seed` | Jagged rock face into the valley |
| `guardrail` | `from`, `to`, `height`, `spacing` (posts), `color` | Real W-beam profile on spacer blocks, amber reflectors. Keep it axis-aligned (the collider is a box around it) |
| `puddles` | scatter keys, `size` [min, max], `roughness`, `color` | Dark glossy water patches; lamps and the flashlight glint in them |
| `decals` | scatter keys, `texture` (`litter`, `stain`, `skid`), `size` [min, max], `aspect`, `align` (1 = along X), `opacity`, `color`, `y` | Flat details lying on the ground |
| `gutter` | `from`, `to`, `width` | Concrete drainage channel (no collider) |
| `delineators` | `from`, `to`, `spacing`, `reflector` (color) | White roadside posts with a faintly glowing reflector |
| `power_line` | `from`, `to`, `spacing`, `height`, `sag` | Concrete poles, cross-arms, three sagging wires |
| `rocks` | scatter keys, `scale` [min, max], `color` | Rocks over 0.55 scale block the player |
| `grass` | scatter keys, `color` | Grass tufts (crossed alpha cards) |
| `smoke` | scatter keys, `size` [min, max] m, `height` [min, max] of the puff centres, `opacity`, `color`, `wind` [x, z] m/s | Drifting smoke puffs (lit sprites). They glow near lamps, fade when you walk into them, and wrap around inside their area. Keep `wind` equal to `FOG_WIND` in mesh.frag |
| `mist` | `pos` (center, y = height), `size` [x, z], `tile`, `speed` [u, v], `opacity`, `color` | Optional flat fog sheet. Not used now (the volumetric fog replaces it); keep it below 0.3 m if you add one |

**Scatter keys** (`forest`, `rocks`, `grass`): `count`, `seed`, `min` / `max` [x, z], `clear`: list of `{min, max}` rectangles to keep empty.

Textures available for `texture`: `asphalt`, `grass`, `dirt`, `rock`, `cliff`, `bark`, `needles`, `concrete`, `metal`, `steel`, `paint`, `wood`, `corrugated` (all generated in `src/Textures.cpp`, 64–256 px, bilinear).

## `sky`

```json
"sky": {
  "horizon": "#10151b", "zenith": "#06080b", "below": "#0b0e12",
  "mountains": [ { "radius": 80, "height": [4, 20], "color": "#0f1319", "seed": 4, "fog": false } ],
  "moon": { "dir": [0.5, 0.42, -1], "size": 3.2, "color": "#aab6c4", "brightness": 0.45, "halo": 0.2 }
}
```

Keep `horizon` equal to `atmosphere.fogColor` so the fog melts into the sky. Mountains are rings of silhouettes around the player; nearer rings should be darker. `"fog": false` lets a ring's peaks rise above the fog — but with the current dark night fog an unfogged ring looks pasted on from the road, so all rings are fogged now (only the moon shows through). The sky moves with the player, so it always looks infinitely far away.

## Lights, zones, sounds

```json
"lights":  [ { "pos": [0, 2.25, 1.5], "color": "#cfe4ff", "intensity": 1.3, "range": 6, "flicker": 0.06 } ],
"zones":   [ { "id": "shelter", "min": [-2.5, 0, 0], "max": [2.5, 3, 3] } ],
"sounds":  [ { "sound": "buzz", "pos": [0, 2.4, 1.5], "volume": 0.8, "maxDistance": 10 } ],
"ambient": [ { "sound": "cricket", "every": [0.6, 2.5], "radius": [4, 14], "volume": 1.0 } ]
```

`sound` is either a placeholder name from `src/Sounds.cpp` (`wind`, `buzz`, `cricket`, `step`, `click`) or a file in `assets/`, e.g. `"sfx/wind.ogg"`. Switching to real recordings is only a data change.

At most 16 point lights (lamps + the flashlight) are used at once, the ones nearest to the camera.

## Finding coordinates

Stand at the shelter (origin) facing the road: that direction is north (−Z). Each meter east adds 1 to x, each meter toward the road subtracts 1 from z. The road's center line is z = −4.
