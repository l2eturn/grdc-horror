# TODO

## Decisions / content for Ter
- [ ] Real map layout (current one is a placeholder blockout; edit `assets/data/map.json`)
- [ ] Ground is flat. The concept mentions a road up the mountain: ramps/slopes need ground height (player eye height following the terrain, collision height range) — design the layout first, then decide if slopes are worth it
- [ ] Sign text (currently "ป้ายรถโดยสาร / บ้านดอยหมอก / รถเที่ยวแรก 05:30")
- [x] Companion look: Blender model npc.glb (generic face, not a real person's likeness)
- [ ] Real sounds: replace placeholder names in map.json with files in `assets/sfx/` (phone: "notify", "vibrate" in src/Sounds.cpp)

## Next milestones (game-comcept.md §4)
- [x] M2 Clock + Phone: night clock (game.json), phone out/away (Tab), lock screen with clock + notifications, message thread close-up (E / click, wheel), readLimit -> fuzzy / deleted, battery, vibrate + tone
- [ ] Message / rule texts in `assets/data/rules.json` are placeholders — write the real ones
- [ ] Not yet: "typing…" indicator, messages fired by events (M6), battery pickups / low-battery flicker
- [x] M3 RuleSystem (src/RuleSystem.*, rules.json "check", DATA_FORMAT.md): windows on the clock, signals, evaluator
      registry, strikes, ` debug overlay, J / --rule. Working now: rules 2 (wrap), 3 (phone_buzz), 7 (forest walk flag);
      4, 10, 12 work with --rule / J but wait for their signals (M6)
- [x] Rule props: shadow figure (rule 4, map `shadow_figure`), bag on the bench (rule 11, map `bag`, E = took_bag),
      bench seat (rule 9, hideSpots `bench_seat` kind sit), east lamp post at x 10 (rule 2)
- [x] Rule 6 van, short version: the bus shrunk (van.json "model" scale), driver script van_driver (he has your face)
- [ ] A real van model (Blender) instead of the shrunk bus; rule 7 "don't take food" needs a dialogue answer from the
      old woman
- [x] M5 Jimmy (src/Companion.*, assets/data/companion.json): sits / follows / sits again, chatter by the hour, E to
      talk, nudges on rule signals, crouches for rule 5, stands in the open for rule 10, closer + a line per strike,
      no footsteps (clue). All lines in dialogue.json `jimmy_*` are PLACEHOLDERS
- [ ] Jimmy ideas: no reflection/shadow flag, he knows a rule before it arrives, glances at things before they happen
- [ ] Balance: breath "easy" can't be lost by doing nothing (4 misses x 0.3 - drain 0.05 x 16 s < 1). Raise
      missNoise or lower drain in game.json if rule 5 should be failable
- [ ] M4 Interaction + Dialogue
- [ ] M5 Companion behaviour (follow, state machine, tactics)
- [ ] M6 EventDirector (the night is 22:00 -> 05:30, ~10 min; timeline in game-comcept.md §1.0)
- [x] Hiding places (map.json hideSpots, E) + hold-your-breath mini-game, Audition style (src/Hiding.*, src/Breath.*,
      game.json "breath"); not yet triggered by events, being heard only shows a notice until M3
- [x] The ghost dancer (dancer.glb v2, scarier) is who comes looking during the breath game (src/Searcher.*):
      circles in the fog, closer with the noise, jump-scare when heard. Under the bench she clips into the floor
- [x] 01:05 the companion AirDrops fake rules (rules.json "airdrop", dialogue friend_airdrop): photos in the thread
- [x] Ending: the 05:30 bus, board with the companion (pulled on if you refuse), inside the bus he turns with his
      true face: "นี่แหละ ที่เขาเรียกว่า ผีหลอก" -> title. src/Ending.*, `--ending` jumps straight to it.
      Lines in dialogue.json "ending" are placeholders
- [ ] M7 Polish hooks

## Environment (art pass)
- [x] Volumetric fog shader (assets/shaders/mesh.frag): smooth rolling smoke, lit by lamps and the flashlight, fades to darkness far away
- [x] Drifting smoke puffs (`smoke` objects, assets/shaders/sprite.frag)
- [x] Realistic pines (alpha-card branches), dead trees, saplings, darker ground under trees
- [x] Phone in hand (src/Phone.cpp, assets/models/phone.glb): back flash = torch (F), sway, walk bob, pulls back near walls; head-bob + footsteps synced
- [x] Bus shelter, street lamps, bus-stop sign, bin rebuilt as detailed prefabs (src/Props.cpp)
- [x] Bus shelter replaced by a Blender model: Thai gable-roof sala (assets/models/sala.glb, source modelResource/sala_source.blend, ~1,200 tris, 128-256 px textures, AO in vertex colours)
- [ ] Sala interior is darker than the old plastered shelter (dark wood, no walls): raise the tube light or lighten the wood texture if it reads too dark in play
- [x] Player's right hand + sleeve holding the phone (fingers round the left edge, thumb on the right)
- [x] Main road: damp glossy asphalt (wheel tracks, patch, cracks), puddles, needles/stains/skid decals, W-beam guardrail with reflectors, gutter, km stone, warning signs, a third lamp
- [ ] Ideas for the road: a parked/abandoned car, a phone booth, a shrine (ศาลพระภูมิ) by the road, rain
- [ ] Poster / name-board texts are placeholders (bus_shelter "posters", "title") — real content + misleading signs later
- [x] Companion model (assets/models/npc.glb): PS2-style man in a cream overshirt, black tee, grey shorts, white sneakers; parts pivot at the joints for simple animation later
- [ ] Ideas: fallen logs, ferns/bushes, wet road reflections, fireflies/moths around the lamp

## M6 signals (2026-10-14)
- [x] street_lights_off (rule 5, + guaranteed ghost dancer), shelter_light_off (8), dragging_sound (9), scream (12)
- [ ] dark_bus (rule 10): a bus with no lights at 03:00 whose driver walks around looking
- [ ] Rule 9 "lights dim" is one lamp off for now (no real dimming)

## Dev tools
- [x] `--play script.txt --play-out dir`: scripted playtests (src/Playtest.*, game/playtests/night_a|b|c.txt)
- [ ] The "SHOWCASE (temporary)" light in map.json (5.8, 2.6, -2.6) makes the fog glow white around the spawn — remove it?
- [ ] `--debug`: fly mode + print position (P) to find coordinates for the map
- [ ] F5: reload map.json without restarting (in debug, read straight from the source folder)

## Bus + NPC animation (2026-10-14)
- [x] NPC: 10 jointed parts + clips idle, walk, talk, sit, sit_talk, crouch, prone, board_bus (+ drive, drive_ask for the driver).
      Add clips in modelResource/scripts/npc_anims.py (pose_solver.py finds arm angles), export with npc_export.py
- [x] Bus (assets/models/bus.glb) + driver (driver.glb), event in assets/data/bus.json, dialogue system + assets/data/dialogue.json (EVENTS_FORMAT.md)
- [ ] Driver's lines are placeholders — real script
- [x] The 23:00 bus is the only real way down; the player can't board it: Jimmy grabs their arm (action jimmy_hold)
      and quotes rule 6 ("only the big OLD bus"); after it leaves he says "good thing you didn't get on"
- [ ] Hook dialogue flags (bus_answer, bus_party, ...) into the rule system (M3 "answer_choice")
- [ ] The bus has no collider while driving (the player can walk through it); companion's collider stays at its start spot when it walks off

## Old woman / rule 7 (2026-10-14)
- [x] grandma.glb (source modelResource/grandma_source.blend, scripts grandma_textures.py / grandma_build.py / grandma_anims.py,
      exported with npc_export.py + COLLECTION/PFX/HIPS_Z/AO_HIDE): ~3,000 tris, two faces on one head (normal / scary with
      glowing eyes), clips idle, walk, beckon, fallen, fallen_call, lunge. Map type `grandma`, G swaps faces for testing
- [x] Rule 7 text in rules.json ("grandma_face")
- [x] Her event (src/GrandmaEvent.cpp, assets/data/grandma.json): junk cart stuck in mud (cart.glb, after the user's
      photos), E to help, normal face ignored -> long neck to the player, scary helped -> she leaves, flags `grandma=...`
- [x] helped_normal = the forest walk: shrine (shrine.glb + map spirit_shrine at the end of the path), the head out of
      the shrine, then it chases the player back to the shelter on its long neck
- [ ] Ghost dancer (assets/models/dancer.glb, scripts dancer_build.py / dancer_anims.py: dance, dance_jerk, glide, bow) is
      built but taken out of the scene for now (user, 2026-10-14)
- [x] The player runs back to the shelter (escaped = the night goes on)
- [ ] Being caught is a placeholder (black + text); decide what it does (M6)
- [ ] What the flags lead to (rule broken / kept) in M3
- [ ] Her lines in dialogue.json are first drafts
- [ ] No collider on her or the cart; the long neck passes through the shelter roof
- [ ] The forest walk ignores trees off the path (the path is clear); the cart clips the bus-stop sign at the first bend
