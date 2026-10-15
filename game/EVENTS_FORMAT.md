# Events and dialogue data

Two files in `assets/data/` (comments with `//` are allowed in both).

## bus.json — the bus that stops at the shelter

```jsonc
{
  "route": { "laneZ": -2.6, "fromX": 80, "stopX": 4.5, "toX": -90, "speed": 9, "talkRadius": 7 },
  "arrivals": [
    { "at": "23:00", "script": "bus_driver", "sign": "8  ดอยหมอก - ตัวเมือง", "headlights": true, "wait": 15 }
  ]
}
```

| Key | Meaning |
|---|---|
| `route.laneZ` | World Z of the lane's centre line. The bus drives west (−X) with its doors toward the shelter (+Z) |
| `route.fromX` / `stopX` / `toX` | Where it appears, where it stops (`stopX` 4.5 puts the front door in front of the shelter), where it disappears |
| `route.speed` | Cruising speed, m/s. It brakes evenly to the stop and pulls away over 4 s |
| `route.talkRadius` | The driver starts talking when the player is this close to the front door (m) |
| `arrivals[].at` | In-game time (`HH:MM`, the clock in `game.json`) |
| `arrivals[].script` | Dialogue script the driver runs (`dialogue.json`). Empty = the bus just stops, waits and leaves |
| `arrivals[].sign` | Text on the orange LED destination sign |
| `arrivals[].headlights` | `false` = a bus driving with its lights off (rule 10) |
| `arrivals[].wait` | Seconds it waits at the stop if nobody comes to the door |
| `arrivals[].final` | `true` = the last bus (05:30): the companion gets on by himself when the doors open, walking away does not end the talk, and it never leaves without the player. Boarding it starts the ending (`src/Ending.cpp`, dialogue script `ending`: action `reveal` = he turns with his true face, `end` = cut to the title) |
| `arrivals[].dragScript` | Final bus: script the companion runs when the player hasn't come to the door after `wait` seconds (ends with `player_board`: he pulls them on) |

Testing: **B** in game, or `--bus` on the command line, calls the first arrival right away.

Flags it sets (in the dialogue's flags, for the rules later): `bus_missed` (nobody came), `bus_walked_away`
(the player left during the conversation).

## van.json — the small van of rule 6

Same format and code as bus.json, plus `"model": { "scale": [width, height, length], "frontText": ["", ""] }`:
the bus model shrunk into a van (placeholder; keep height = length so the wheels stay round). The driver's script
`van_driver` sets `van_answer` = `accept` / `refuse`; accept breaks rule 6. `--van` / V calls it now.

## dialogue.json — conversations

```jsonc
{
  "scripts": {
    "bus_driver": {
      "start": "greet",
      "nodes": {
        "greet": { "speaker": "คนขับรถ", "text": "...", "next": "ask" },
        "ask":   { "speaker": "คนขับรถ", "text": "จะขึ้นไหม?",
                   "choices": [
                     { "text": "ขึ้นครับ", "next": "count", "set": { "bus_answer": "board" } },
                     { "text": "ไม่ขึ้น", "action": "leave" }
                   ],
                   "timeout": 15, "timeoutNext": "silent" }
      }
    }
  }
}
```

A node is a **line** (no `choices`) or a **question** (with `choices`, answered with the number keys 1–9).

| Key | Meaning |
|---|---|
| `speaker`, `text` | Shown in the subtitle box |
| `next` | Node that follows (a line moves on by itself; Enter skips). Missing = the conversation ends |
| `duration` | Seconds a line stays up (default: from its length) |
| `choices[]` | `text`, `next`, `set`, `action` per answer |
| `timeout`, `timeoutNext` | Questions: seconds to answer, and where silence leads |
| `set` | Flags written when the node is reached / the answer is picked, e.g. `{"bus_answer": "board"}` |
| `action` | Something the game does. Bus actions: `leave`, `player_board` (fade out, placeholder ending), `companion_board` (the companion walks to the door, gets on, and rides away) |

The lines in the file now are placeholders to test the system.

## grandma.json — the old woman with the stuck cart (rule 7)

```jsonc
{
  "appear": "01:10", "face": "random", "pos": [9.0, 0, -0.8], "rotY": -90,
  "helpRadius": 2.2, "callEvery": 14, "stretchAfter": 25, "giveUpAfter": 40,
  "neck": { "speed": 2.5, "stare": 7, "stopShort": 0.8, "maxReach": 25 },
  "forest": { "path": [[9.0, -0.8], [6.6, -0.7], [5.4, 0.6], [5.1, 3.0], [5.0, 10.0], [5.0, 16.0]], "speed": 0.75,
              "fogVisibility": 3.5, "fogHeight": 12, "headSpeed": 0.35, "stare": 3,
              "chaseSpeed": 3.0, "catchRadius": 0.75, "safeZone": "shelter", "safeMargin": 2.0 }
}
```

At `appear` she is on the roadside straining at her junk cart (`cart.glb`), one wheel in a mud patch, calling
`grandma_call` (dialogue.json) every `callEvery` seconds and looking over at the player while she does.
Her face (`normal` / `scary` / `random`) decides what the right move is:

| Face | Player | What happens | Flag `grandma` |
|---|---|---|---|
| scary | **E** within `helpRadius` | cart comes free, `grandma_thanks`, she pushes it away and is gone | `helped_scary` |
| scary | does nothing for `giveUpAfter` s | `grandma_giveup`, she is gone | `ignored_scary` |
| normal | **E** within `helpRadius` | the forest walk and the chase (below) | `helped_normal`, then `escaped` / `caught` |
| normal | does nothing for `stretchAfter` s | her neck stretches out to the player (wherever they are, up to `maxReach` m), her face stops `stopShort` m in front of theirs for `stare` s (`grandma_neck`), then it pulls back and she is gone | `ignored_normal` |

`neck.speed` is how fast the head travels (m/s). The long neck is built in code (src/GrandmaEvent.cpp): 28 tube
segments along a curve from `grandma_neck` to the head, swaying; the model's short neck (`grandma_neck_skin`) is hidden meanwhile.

**The forest walk** (ordinary face, helped): `grandma_escort`, then the player walks beside her (on her left, a hand on
the cart; the mouse still looks around) while she pushes it along `forest.path` (her feet, x/z) at `forest.speed`.
The fog closes in to `fogVisibility` / `fogHeight` on the way. At the end `grandma_arrive`; the fog swallows everything,
and when it thins she and the cart are gone and the view turns to the spirit house (map type `spirit_shrine`). Her
head comes out of its door on the long neck (`headSpeed`, `grandma_shrine`) and stops in front of the player's face;
After `stare` s it comes after the player up the path on its neck (slowly at first, `chaseSpeed` after 4 s).
The player has control again ("วิ่ง! กลับไปที่ศาลา") and must run back; the fog thins the nearer they get.
Reaching the `safeZone` map zone (or anywhere within `safeMargin` metres of it, e.g. the stop sign or the road edge in front): the head goes back into the shrine, the fog lifts, the night goes on (flag `grandma=escaped`).
Caught within `catchRadius`: black, "คุณหายไปในหมอก" (placeholder, flag `grandma=caught`).

Testing: **G** calls her right away (and swaps her face once she is there); `--grandma normal|scary` on the command
line, `--grandma help` = an ordinary face already helped (straight into the forest walk).
Flags land in the dialogue's flags like the bus's, for the rule checks (M3).

## companion.json — Jimmy (src/Companion.cpp)

He sits on his `seat`, gets up and follows when the player is more than `followAfter` m away (keeping `followDist`,
minus `closerPerStrike` per broken rule, never under `minDist`), and sits again after the player has been back in the
`shelter` zone for `sitAfter` s. `footsteps: false` = he makes no sound when he walks (a clue).

- `chatter` — `every` [min, max] seconds, within `radius` m, a script from the current phase (`until` = in-game time;
  the first phase whose `until` is still ahead). Lines go round before any repeats.
- `talk` — E while looking at him within `radius` m: a script from the current phase (choices allowed, flags as usual).
- `signals` — a rule's signal (rules.json `check.signal`) -> the script he says when it shows up. Use it to nudge the
  player toward breaking the rule.
- `rules` — while that rule's window is open: `do` = `sit` (his seat), `crouch` / `stand` at `pos` facing `rotY`
  (`stand` without `pos`: freeze where he is, staring at the player); `say` when it opens, `after` when it closes.
- `strikes` — the scripts after the 1st, 2nd, ... broken rule (the last repeats).

The scripts live in dialogue.json (`jimmy_*`). Lines queued while something else is said wait up to 8 s, then drop.
