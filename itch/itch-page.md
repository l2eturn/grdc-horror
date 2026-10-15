# itch.io page — copy into the fields of "Edit game"

## Title
ผีหลอก

## Short description / tagline
คืนเดียวที่ศาลารอรถบนเขา ทำตามกฎให้ครบทุกข้อ จนกว่ารถเที่ยวแรกจะมา

## Classification
- Kind of project: Downloadable
- Classification: Games
- Release status: Released
- Genre: Horror (tags: horror, first-person, psychological-horror, short, atmospheric, thai, ps2, rules-horror)
- Pricing: No payments (or "$0 or donate")
- Uploads: `grdc-horror-macOS.zip` → tick **macOS** (do NOT tick "This file will be played in the browser")
- Languages: Thai
- Inputs: Keyboard, Mouse
- Average session: About a half-hour or less

## Description (paste into "Description")

**ผีหลอก** — เกมสยองขวัญมุมมองบุคคลที่หนึ่ง สไตล์ PS2 (เล่นจบราว 10–12 นาที)

คุณกับ **จิมมี่** เพื่อนสนิทที่รู้จักกันมานาน ตกรถอยู่ที่ศาลารอรถในหมู่บ้านบนเขา
หมอกลงหนา โทรศัพท์ไม่มีสัญญาณ แต่จู่ ๆ ก็มีข้อความเข้ามา…

**"กฎ"** 12 ข้อ ที่คุณต้องทำตามให้ได้จนถึง 05:30 ตอนรถเที่ยวแรกมารับ

- อ่านกฎในโทรศัพท์ ซึ่งเป็นแสงเดียวที่คุณมี — แบตเตอรี่มีจำกัด
- ฟังให้ดี เสียงคือสัญญาณ: โทรศัพท์สั่น เสียงเทปกรอกลับ เสียงลากของ เสียงกรีดร้อง
- ซ่อนตัวแล้ว **กลั้นหายใจ** ให้ตรงจังหวะหัวใจ ก่อนที่ "เธอ" จะได้ยิน
- รถ คนขับ หญิงชรา… ไม่ใช่ทุกคนที่อยากให้คุณกลับบ้าน

กฎข้อแรกมีแค่ว่า **ห้ามไว้ใจใคร**

ใส่หูฟัง ปิดไฟ แล้วรอรถไปด้วยกัน

---

*A first-person PS2-style horror game (Thai language only, ~10–12 min). Stranded at a mountain bus stop with your
best friend Jimmy, your phone has no signal — yet rules keep arriving. Follow every one of them until the first bus at
05:30. Rule one: trust no one.*

### Controls
| | |
|---|---|
| Click | start / capture the mouse |
| WASD · Shift | walk · run |
| Mouse | look |
| Tab | phone out / away |
| E | read messages, talk, hide, sit, pick up |
| F | flashlight (drains the battery) |
| 1–9 | answer |
| Arrows + Space | hold-your-breath mini-game |
| M | mute · Esc free the mouse (twice = quit) |

### macOS: first launch
The game isn't notarized by Apple. Double-click once, then go to **System Settings → Privacy & Security → Open Anyway**
(macOS 14 and older: right-click the app → Open). Still blocked? Run in Terminal:
`xattr -dr com.apple.quarantine /Applications/grdc-horror.app`

Requires macOS 11+, Apple Silicon or Intel.

### Credits
Made for a 4-day horror game jam on **terEngine** (our own C++/OpenGL engine). Every model, texture and sound is made
in code or in Blender scripts — no outside assets.

## Jam restrictions (for the jam submission form, if it asks)
1. **Misleading signs** — the rules arrive from an unknown sender; Jimmy AirDrops a "new set of rules" that are fakes and
   lead you into breaking the real ones.
2. **One rule** — one objective: follow the rules and survive until the 05:30 bus.
3. **Something is pretending** — (spoiler) your friend is not who he seems.
4. **Haunting sounds** — sounds are the signals the rules react to (buzzing phone, a rewinding tape, dragging, a
   scream), and in hiding your own breathing gives you away.

## Images
- Cover image (630 × 500): `cover.png`
- Screenshots (1280 × 720): `screenshots/*.png` — upload in number order

## Rebuilding the zip (after a code/data change)
```
cd game && cmake --build build -j
S=$(mktemp -d); mkdir "$S/grdc-horror"; ditto build/grdc-horror.app "$S/grdc-horror/grdc-horror.app"
codesign --force --deep --sign - "$S/grdc-horror/grdc-horror.app"; cp ../itch/README.txt "$S/grdc-horror/"
rm -f ../itch/grdc-horror-macOS.zip; ditto -c -k --norsrc --noextattr --noqtn --keepParent "$S/grdc-horror" ../itch/grdc-horror-macOS.zip
```
