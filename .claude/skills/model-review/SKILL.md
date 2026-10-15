---
name: model-review
description: Review a grdc-horror 3D model (.glb in game/assets/models) against the PS2-realistic art direction and terEngine's limits — automatic budget/format audit, a Blender contact sheet (textured / AO / silhouette from 4 sides), an in-game screenshot in the real fog and light, then a scored visual critique with ranked fixes. Use after building or changing any model, before calling model work done, or when the user asks to check / ตรวจ / รีวิว a model, asks "สวยยัง", or wants to know which models need work.
---

# Model review (grdc-horror)

Judge a model the way the player will see it: low-res, at night, in volumetric fog, lit by a phone flash and a
few lamps. A model can pass every number and still look wrong in the scene, so always do all three passes.
Reply to the user in Thai.

Paths below are relative to the repo root (`grdc-horror/`). `S=.claude/skills/model-review/scripts`.

## 1. Automatic audit (seconds)

```bash
python3 $S/audit_glb.py                 # every model
python3 $S/audit_glb.py bus grandma     # some models; --json for machine output
```

It checks, per model: triangle budget for its category, texture sizes and power-of-two, NEAREST filtering (forbidden),
baked AO present in vertex colour (missing or all-white), textured meshes without UVs, doubleSided + MASK
(lit as foliage in mesh.frag → glowing cutouts), BLEND materials, normal/roughness/occlusion maps (terEngine
ignores them — wasted bytes), unsupported required extensions, world height vs. expected, draw-call count,
and node names that `game/src` looks up with `findNode` but the glb lacks (FAIL — the game breaks).

Budgets and expected heights live in `$S/budgets.json`. A new model needs an entry there (category from
game-comcept.md §1.1: small 50–300, medium 300–1,500, character 1,500–3,000 tris; textures 64–128 px general,
256 foliage/characters, 512 only for readable text). Change a budget only with a reason written in `_note`.

## 2. Contact sheet (≈5 s, headless Blender — does not touch the user's open Blender)

```bash
OUT=<scratchpad>/review; mkdir -p $OUT
perl -e 'alarm 120; exec @ARGV' /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
  --python $S/render_views.py -- game/assets/models/<name>.glb $OUT/<name>_sheet.png [--clip walk --frame 12]
```

Rows: **textured** (studio light), **AO** (vertex colour only), **silhouette** (black on white). Columns: front,
front 3/4, side, back. For animated models also render one or two extreme frames of the main clips
(`--clip`) to catch gaps or intersections at the rigid joints (the engine has no skinning).
Open the PNG with Read and actually look at it.

## 3. In-game shot (the one that matters)

Write a throwaway playtest into the scratchpad and run it (commands: `src/Playtest.h`; positions: the model's
entry in `game/assets/data/map.json`, or the event's json for bus/grandma/dancer/companion):

```text
@1  goto <x> <z>            # 3–6 m from the model, roughly where the player stands
+0  lookat <x> <y> <z>      # the model's centre
+1  shot <name>_mid
+0  goto <x2> <z2>          # far: 12–20 m, does it still read in the fog?
+0  lookat <x> <y> <z>
+1  shot <name>_far
+0  quit
```

```bash
cd game && perl -e 'alarm 60; exec @ARGV' build/grdc-horror.app/Contents/MacOS/grdc-horror --mute \
  --play $OUT/<name>.txt --play-out $OUT -ApplePersistenceIgnoreState YES
```

Models that only appear in events need their flag/clock: `--bus`, `--van`, `--grandma normal|scary`,
`--ending`, `--time HH:MM`, `clock HH:MM` in the script (see game-comcept.md §6). Rebuild first
(`cd game && cmake --build build -j`) only if C++ changed — a new .glb is copied into the app by the build,
so after an export run the build too. Also take one shot with the phone flash on the model when it matters
(`key Tab`, `key F` in the script) — that is how the player usually sees things.

## 4. Score it

Rate each line 1–3 (3 = good) and write one sentence of evidence from the images:

| # | Criterion | What good looks like |
|---|---|---|
| 1 | Silhouette | Readable and specific from front and side in the silhouette row and in the far shot; no blob, no paper-thin parts |
| 2 | Proportion & scale | Matches the real object (check `size` from the audit against a real reference); sits right next to the player and other props |
| 3 | Texture read | Detail (dirt, rust, stains, wood grain) lives in the texture; even texel density across parts; not blurry-smeared nor noisy at 128–256 px |
| 4 | Grounding & AO | Visible contact darkening where parts meet and where it touches the ground; no floating; AO row is not plain white |
| 5 | Material & wear | Distinct materials read as different (metal / wood / cloth / skin); believable age and grime for a mountain roadside at night |
| 6 | Fits the scene | In the in-game shot: not too bright or saturated, emissive only where light really is, no hard CG edges popping out of the fog |
| 7 | Art direction | PS2-realistic per §1.1: low-poly but real forms; no cartoon proportions; no PS1 tricks (nearest, jitter, affine) |
| 8 | Technical | No z-fighting, holes, flipped faces, joint gaps in clips, wrong pivots; node names intact |

## 5. Report

```
## <model> — PASS | NEEDS WORK | FAIL
Audit: <tris>/<budget>, <tex>, <size m>, FAIL/WARN lines (or "clean")
Score: silhouette 2 · proportion 3 · texture 1 · AO 2 · material 2 · scene 2 · art 3 · tech 3  (18/24)
Seen: <what the images show, 2–4 lines, cite which image>
Fix first (highest visible impact):
1. <fix> — where: <script>:<function or line>  (expected effect)
2. ...
Not checked: <e.g. phone-flash shot, animation clips, far view>
```

Verdict: **FAIL** if the audit has a FAIL or any score is 1 on criteria 1, 2 or 8; **NEEDS WORK** if the total
is under 19 or any other 1; otherwise **PASS**. Attach or link the sheet and in-game shot paths. Never claim
something looks good from numbers alone — say which images you looked at. To fix the findings, use the
`model-polish` skill.
