---
name: model-polish
description: Make an existing grdc-horror model look better (สวยขึ้น / เนียนขึ้น / ดูจริงขึ้น) inside the PS2-realistic art direction and terEngine's limits — review it first, change its Blender build script (not the .blend by hand), rebuild headless, export the .glb, then prove the improvement with before/after images. Use when the user asks to enhance, polish, improve, fix the look of, or upgrade a model or prop (sala, bus, phone, cart, shrine, npc/Jimmy, driver, grandma, dancer, or a new one).
---

# Model polish (grdc-horror)

Goal: the biggest visible improvement **in the game's fog and darkness** for the least work, without breaking
anything the game relies on. Reply to the user in Thai; code and comments in English. Before editing, tell the
user in two or three lines what you will change and why.

## 0. Before: baseline review

Run the `model-review` skill on the model (audit + contact sheet + in-game shots) and keep the images as
`<name>_before_*`. Its "Fix first" list is the work list. If the user named a specific complaint, that goes first.

## 1. Read before touching

- The model's scripts in `game/modelResource/scripts/`: `<name>_build.py` (geometry, often textures too),
  `<name>_textures.py`, `<name>_anims.py`, the export/runner (`<name>_export.py`, `npc_export.py` with globals,
  or `ps2kit.bake_and_export`). Shared helpers: `ps2kit.py` (fractal noise textures, `make_materials`, `Kit`
  primitives with world-metre UVs, `bake_and_export`).
- Where the game uses it: `grep -n '<name>' game/src/*.cpp game/assets/data/*.json` — node names, pivots,
  clips, scale and position the code assumes.
- game-comcept.md §1.1 (art direction) and §6 (pipeline, gotchas). Characters share one face UV layout
  (u = 0.5 + 0.395x, v = 0.5 + 0.458z; eyes u 0.5±0.165, v 0.51).

All changes go into the scripts so the model can be rebuilt; a hand edit in the .blend is lost on the next build.

## 2. What actually shows in terEngine

| Works | Ignored or harmful |
|---|---|
| Base-colour texture × vertex colour "AO" (baked) | Normal, roughness, metallic, occlusion maps (not loaded) |
| Emissive colour/texture (+ strength) | Specular/clearcoat/glossy looks (engine shading is matte) |
| Geometry that changes the silhouette | Tiny geometry detail (lost at pixelScale 2 in fog) |
| Alpha cutout (MASK), **single-sided** + flipped duplicate faces | doubleSided + MASK (lit as foliage → cutouts glow white) |
| Bilinear 64–256 px tiling textures, world-metre UVs | NEAREST filtering, > 256 px (except readable text 512) |
| Rigid parts with pivots + glTF clips | Skinning/armature deformation (no skinning) |

## 3. Techniques, in order of payoff at night in fog

1. **Silhouette** — fix wrong proportions against a real reference first; add the few shapes that make it
   recognisable from 15 m (roof overhang, bevelled edges on the outline, a sag, a broken part). Spend triangles
   here, not on flat interiors.
2. **Value and grounding** — contact AO where parts meet and at the ground (bake `distance` 0.3–0.6, temporary
   ground plane); a darker bottom gradient for dirt/splash. Every export maps AO through `ps2kit.ao_curve`
   (floor + gamma, shared so all models match); only pass a different floor for a reason (characters use 0.3). Overall albedo stays dark and desaturated (night scene, cold tone).
3. **Texture storytelling** — in the numpy textures: grime in crevices and along the bottom, rust at edges and
   bolts, water streaks under ledges, sun-faded paint, moss on the shaded side, edge wear. Use `fractal` with
   anisotropy (ax/ay) for grain and streaks; keep texel density even between parts.
4. **Material separation** — distinct base values and hue per material so metal, wood, cloth and skin read apart.
5. **Small story props** — only if the budget allows: a sticker, a cable, a dented panel, an alpha-cutout decal.

Stay inside the audit budget (`.claude/skills/model-review/scripts/budgets.json`). Trade triangles from hidden
or flat areas before asking for more; if the budget truly blocks the improvement, ask the user.

## 4. Must not break

- Node names and pivots the game looks up (`findNode`, the rig's `PFX` names) and clip names. The audit FAILs
  on missing `findNode` names; for clips, check `play("...")` calls in `src/`.
- Origin, facing (Blender −Y = game +Z) and real-world scale; colliders in `Props.cpp`/map data assume them.
- Characters: every face mesh and texture stays on the shared face layout; both grandma faces keep the same place.
- Other sessions may be editing the same scripts: re-read files right before editing and change only what you need.
- Do not touch `terEngine/` without asking.

## 5. Rebuild and export (headless — the user's open Blender stays untouched)

First copy `game/modelResource/<name>_source.blend` into the scratchpad as a backup (the runner overwrites
it). Then write a runner in the scratchpad:

```python
import bpy
S = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/scripts/"
bpy.ops.wm.read_factory_settings()
bpy.ops.wm.save_as_mainfile(filepath=S + "../<name>_source.blend")
for f in ["ps2kit.py", "<name>_textures.py", "<name>_build.py", "<name>_anims.py"]:   # the ones that exist
    exec(open(S + f).read())
bake_and_export(bpy.data.collections["<Collection>"], "<name>")   # or exec the model's own export script
```

```bash
perl -e 'alarm 600; exec @ARGV' /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python runner.py
cd game && cmake --build build -j      # copies the new glb into the app
```

Some models are built in steps or from another model (driver = npc_build with TEX_OVERRIDE/MAT_PREFIX; npc
characters export via `npc_export.py` with COLLECTION/PFX/HIPS_Z/AO_HIDE globals). Follow the header comment of
each script. If a rebuild from factory settings doesn't reproduce the current model, stop and tell the user
instead of guessing — the .blend may hold manual work.

## 6. After: prove it

Run `model-review` again with the same camera positions and name the images `<name>_after_*`. Show the user
before and after side by side (paths), with the score change and the audit lines. If something got worse
(budget, a missing node, a clip gap), fix it or report it plainly. Then tell the user how to see it themselves
(the flag or the place to walk to in the game).
