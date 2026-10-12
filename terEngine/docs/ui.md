# UI and overlay

[← Back to README](../README.md) · Headers: `ter/ui/canvas2d.h`, `ter/app/app.h`

There are two ways to draw on top of the 3D scene:

| | **UI canvas** (`app.onUi`) | **Overlay** (`app.overlay()`) |
|---|---|---|
| Draws | Text (Thai, emoji), shapes, gradients, images | Solid rectangles, full-screen fills, textures |
| When | Only when you call `app.redrawUi()` | You call it **every frame** |
| Cost | Expensive to redraw, free to keep showing | Cheap |
| Use for | Hints, dialogue, menus, HUD text | Fades, damage flashes, bars, crosshair |
| Layer | Below the overlay | On top of everything |

---

## UI canvas

Assign a function to `app.onUi`. It receives a `ter::Canvas2D` that works like an HTML `<canvas>` 2D context, sized to the window in **points** with y pointing down:

```cpp
std::string hint = "Press E to open";

app.onUi = [&](ter::Canvas2D& g, const ter::Frame& f) {
    // the canvas is already cleared
    g.setFont("Inter", 22, 600);              // family, size in points, weight 100..900
    g.setFill("#ffffff");                     // CSS colors: "#rgb", "#rrggbb", "rgba(r,g,b,a)", "white"
    g.setTextAlign(ter::TextAlign::Center);
    g.setShadow(6, ter::cssColor("rgba(0,0,0,0.6)"), 0, 2);   // blur, color, offset x, y
    g.fillText(hint, f.width / 2.0f, f.height - 40.0f);

    g.clearShadow();
    g.setFill("rgba(0,0,0,0.5)");
    g.beginPath();
    g.roundRect(20, 20, 200, 60, 12);
    g.fill();
};
```

### Redrawing

`onUi` is **not** called every frame. It runs once at start, on window resize, and whenever you request it:

```cpp
hint = "The door is locked...";
app.redrawUi();
```

A good habit is to compute the text you want each frame and redraw only when it differs:

```cpp
std::string want = lookingAtDoor ? "Press E to open" : "";
if (want != hint) { hint = want; app.redrawUi(); }
```

For something that changes every frame (a timer, a moving marker), redrawing every frame works but costs more. Prefer the overlay for that.

### Fonts

Bundled and ready to use:

| Family | Notes |
|---|---|
| `"Mali"` | Thai + Latin, friendly handwritten feel (weights 400–600) |
| `"Inter"` | Clean sans-serif UI font |
| `"Cormorant Garamond"` | Elegant serif, also italic — good for horror/old notes |

Any font installed on the Mac also works by its family name. To add your own file:

```cpp
ter::registerFont(ter::assetPath("fonts/Creepster-Regular.ttf"));   // then setFont("Creepster", 40)
ter::registerFonts(ter::assetPath("fonts"));                        // every font file in a folder
```

Thai text shaping (vowel and tone-mark placement) and emoji are handled by macOS CoreText.

### Canvas2D reference

| Group | Methods |
|---|---|
| State | `save`, `restore`, `translate`, `rotate`, `scale`, `setGlobalAlpha`, `setAdditive` |
| Style | `setFill`, `setStroke`, `setLineWidth`, `setLineCap(round)`, `setShadow`, `clearShadow` |
| Text | `setFont(family, size, weight, italic)`, `setTextAlign`, `setTextBaseline`, `setLetterSpacing(em)`, `fillText`, `strokeText`, `measureText`, `wrapText` |
| Rectangles | `fillRect`, `strokeRect`, `clearRect`, `clear` |
| Paths | `beginPath`, `moveTo`, `lineTo`, `quadTo`, `arc`, `ellipse`, `rect`, `roundRect`, `closePath`, `fill`, `stroke`, `clip` |
| Gradients | `fillLinearGradient(x0, y0, x1, y1, stops)`, `fillRadialGradient(...)` — fill the current path |
| Images | `drawImage(rgba, w, h, x, y, dw, dh, cover)`, `copyFrom(otherCanvas)` |

**Multi-line text** — `wrapText` breaks lines correctly even for Thai, which has no spaces between words:

```cpp
float y = 100;
for (const std::string& line : g.wrapText(longText, 400)) {
    g.fillText(line, 40, y);
    y += 28;
}
```

**Gradient:**

```cpp
g.beginPath();
g.rect(0, 0, f.width, 120);
g.fillLinearGradient(0, 0, 0, 120, {{0, ter::cssColor("rgba(0,0,0,0.8)")}, {1, ter::cssColor("transparent")}});
```

---

## Text in the 3D world (notes, signs)

A `Canvas2D` can also be drawn once and uploaded into a texture that you put on a mesh:

```cpp
ter::Canvas2D paper(512, 512);
paper.setFill("#e8dfc8");
paper.fillRect(0, 0, 512, 512);
paper.setFont("Cormorant Garamond", 48, 600);
paper.setFill("#3a2a1a");
paper.fillText("Don't turn around.", 40, 120);

auto noteTex = std::make_shared<ter::Texture>();
paper.upload(*noteTex);

auto note = ter::makeMesh(ter::geo::plane(0.4f, 0.4f), ter::lambert(noteTex));
note->position = {0, 1.5f, -2.85f};   // just in front of a wall
scene->add(note);
```

To change the text later, draw on the canvas again and call `upload` again.

---

## Overlay

The overlay is a list of screen rectangles drawn **after** everything else, including the UI canvas. It is emptied after each frame, so call it in your update every frame you want something visible. Coordinates are window points, y down; colors are RGBA in 0..1.

```cpp
app.overlay().fill({0, 0, 0, fade});                                    // whole screen
app.overlay().rect(20, 20, 200 * hp, 10, {0.8f, 0.1f, 0.1f, 1}, 4);    // x, y, w, h, color, corner radius
app.overlay().image(*tex, 100, 100, 64, 64);                            // a texture (optional tint, radius)
```

### Recipes

```cpp
// fade in from black over 2 seconds
fade = std::max(0.0f, fade - f.dt * 0.5f);
if (fade > 0) app.overlay().fill({0, 0, 0, fade});
```

```cpp
// red flash when hurt: set flash = 1 on hit
flash = std::max(0.0f, flash - f.dt * 2);
if (flash > 0) app.overlay().fill({0.6f, 0, 0, flash * 0.5f});
```

```cpp
// crosshair dot
app.overlay().rect(f.width / 2.0f - 2, f.height / 2.0f - 2, 4, 4, {1, 1, 1, 0.8f}, 2);
```

Because the overlay is drawn over the UI, a full-screen fade also hides your text. That is usually what you want for fades. If text must stay visible, draw it with the overlay's `image` from a separate canvas, or skip the fade.
