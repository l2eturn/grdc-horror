# game.json and rules.json

Two small data files next to `map.json` in `assets/data/`. Edit them, rebuild (`cmake --build build`), run.
Times are `"HH:MM"` on the night's clock.

## game.json — clock, battery, phone

```json
{
  "clock": { "start": "22:00", "end": "05:30", "secondsPerHour": 80 },
  "battery": { "start": 64, "idlePerMinute": 1.0, "screenPerMinute": 3.0, "flashPerMinute": 4.0 },
  "phone": { "sender": "ไม่ทราบชื่อ", "date": "วันศุกร์ที่ 13 ตุลาคม", "carrier": "ไม่มีบริการ" }
}
```

| key | meaning |
|---|---|
| `clock.start` / `end` | when the night starts / ends (the last bus comes at the end) |
| `clock.secondsPerHour` | real seconds per in-game hour (80 = the 7.5-hour night takes 10 minutes) |

Every `HH:MM` in the data files (here, `rules.json`, `bus.json`, `grandma.json`, `--time`) is a time of this one night:
**12:00–23:59 is the evening, 00:00–11:59 is after midnight**. So `"01:10"` comes after `"22:30"`.
| `battery.start` | starting charge, percent |
| `battery.*PerMinute` | percent lost per **real** minute: always (`idle`), plus while the phone is in hand (`screen`), plus while the flash shines (`flash`). At 0 the phone is dark: no flash, nothing to read |
| `breath.<level>` | the hold-your-breath mini-game per difficulty (`easy`, `hard`, add more): `arrowsMin`/`arrowsMax` arrows per round, `bpm` heartbeat (a round = 4 beats), `rounds`, `mark` (0..1, where on the bar Space must land), `perfect`/`good` (half-widths of the windows, in bar lengths), `missNoise` (noise per loud breath), `gaspNoise` (per wrong arrow), `drain` (noise lost per second). Noise 1 = heard |
| `phone.sender` | name shown for the unknown sender |
| `phone.date` | date line on the lock screen |
| `phone.carrier` | top-left of the status bar |

## rules.json — what arrives on the phone

```json
{
  "messages": [
    { "id": "hello", "at": "00:01", "text": "คุณกำลังรอรถเที่ยวแรกอยู่ใช่ไหม" }
  ],
  "rules": [
    {
      "id": "no_look_back",
      "at": "00:04",
      "text": "2. ถ้าได้ยินเสียงเรียกชื่อคุณจากด้านหลัง ห้ามหันไปมอง",
      "fuzzyText": "2. ถ้าได้ยินเสียง...ด้านหลัง ห้าม...",
      "readLimit": 2
    }
  ]
}
```

Both lists arrive in one thread from the sender, in time order. Each arrival vibrates the phone and plays the
message tone (heard from the pocket too); messages due before the start time are already there.

| key | meaning |
|---|---|
| `id` | name used by other systems (rule checks in M3, events) |
| `at` | arrival time |
| `text` | the message |
| `fuzzyText` | rules: shown once the rule has been read more than `readLimit` times. Missing = the message shows as deleted |
| `readLimit` | rules: how many times the thread can be opened before it blurs; leave it out for no limit |

### `airdrop` — the companion's fake rules

```json
"airdrop": { "at": "01:05", "script": "friend_airdrop", "from": "เพื่อน",
             "rules": [ { "id": "fake_grandma", "text": "1. ถ้ามียายแก่ขอให้ช่วยเข็นรถ ..." } ] }
```

At `at` the companion runs the dialogue `script` (dialogue.json). An answer with the action `airdrop_accept`
puts every `rules` entry in the thread at that moment, drawn as a photo of a paper note under the label
"AirDrop จาก <from>"; `airdrop_decline` sends nothing. The answer is in `dialogue.flags["airdrop"]`
(`accepted` / `declined` / `pushed`). AirDropped rules never blur. Comments (`//`) are allowed in this file.

Every time the player opens the thread counts as one read of every rule in it.
Rules no longer blur (2026-10-14, the player asked for clear rules): they have no `readLimit` / `fuzzyText`.
The code still supports both — add them back to a rule to make it fade after that many reads.

### `check` — how the game checks a rule (src/RuleSystem.*)

A rule entry may carry a `check` block. Breaking a rule never kills: it plays `onViolate`, counts a strike, and is
reported once per window.

```json
"check": { "at": "22:30", "seconds": 10, "signal": "phone_buzz", "type": "phone_held",
           "onViolate": { "sound": "sting" } }
```

- `at` + `seconds` — the window: from that in-game time, for that many real seconds. No `at` = all night.
- `signal` — what tells the player the rule applies now; played when the window opens. A signal the game
  can't make yet is logged and **the window is skipped**. Built: `phone_buzz`, `shadow_figure`, `bag`,
  `street_lights_off` (map lamps `lamp_west`/`lamp_east` off + a tape rewinding closer), `shelter_light_off`,
  `dragging_sound`, `scream`. `dark_bus` (rule 10: dark_bus.json, the driver walks round looking — `onViolate.scare: "driver"`).
- **One thing at a time:** a window that comes due while another event has the player (a bus or the van at the
  stop, a question on screen, the forest walk) waits until it is over and then opens late. The other way round,
  the buses, the van, the old woman and Jimmy's AirDrop wait while a rule's window is open. Only a clock jump
  (`--time`, a playtest `clock`) skips what was due before the new time.
- `repeat: true` — an all-night rule that can be broken again once the player is back in line (rule 2).
- `onViolate` — `sound` (a sound name), `wrap` (a map zone: fade to black, come out at the opposite edge of it),
  `hide` (a signal prop to take away), `dancer: true` (the ghost dancer comes out of the fog into the player's face —
  rule 5 has it, so she is met whether the player hides or not).

| type | params | broken when |
|---|---|---|
| `leave_zone` | `zone`, `grace` (3) | outside the map zone longer than `grace` s |
| `enter_zone` | `zone`, `grace` (0) | inside the zone longer than `grace` s |
| `must_be_in` | `zone`, `within` (10) | not in the zone from `within` s after the window opens |
| `phone_held` | — | the phone is taken out, or the messages are opened |
| `light_on` | — | the flash is on |
| `look_at` | `target` [x,y,z], `coneDeg` (12), `max` (3) | looked at the target longer than `max` s in total |
| `must_hide` | `spot` (any), `within` (8), `breath` (easy/hard), `phoneAway` (true), `noLight` (false) | the flash on (with `noLight`), not hidden in `spot` from `within` s on, phone out while hidden, or heard in the breath mini-game (it starts once hidden) |
| `still` | `maxMove` (0.2), `breath` | moved more than `maxMove` m, or heard in the breath mini-game (starts right away) |
| `flag` | `flag`, `value` (any) | a flag has that value: dialogue answers (`dialogue.json` `set`) and game state (`grandma_forest`) |

New types: add an evaluator in `makeEvaluators()` in src/RuleSystem.cpp.
Zones for the checks live in map.json (`stop_area`, `lamp_light`).

## Debug flags

```sh
build/grdc-horror.app/Contents/MacOS/grdc-horror --time 02:30 --phone read
```

- `--time HH:MM` start the night at that time
- `--phone held` / `--phone read` start with the phone in hand / open on the thread
- `--van` the rule-5 van arrives now (V in game)
- `--bus` the first bus arrives now (B in game); `--time 05:29` to get the last bus
- `--grandma normal|scary` the old woman appears now (G in game)
- `--ending` skip straight to the ending inside the last bus
- `--rule <id>` open that rule's check window now, even without its signal (J in game = the next waiting one);
  `` ` `` shows the rule overlay (state, timers, strikes)
- `--breath easy|hard` start the hold-your-breath mini-game (H in game = easy); `--hide under_bench|behind_shelter` start hidden
- `--mute` / `--sound`, `--fps`, `--shot file.png [--shot-delay s]`, `--pixel N`
- with `open`, pass them after `--args`: `open build/grdc-horror.app --args --ending`
