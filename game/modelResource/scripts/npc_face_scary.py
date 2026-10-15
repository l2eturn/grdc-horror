# The companion's true face for the ending reveal: made from his normal face (textures/npc_face.png) so he is
# still recognisable — same beard and brows — but bloodless grey, black sunken sockets with white eyes (the game
# adds glowing eye dots on top) and the mouth torn open. Same layout as every NPC face: u = 0.5 + 0.395*x,
# v = 0.5 + 0.458*z on the unit head sphere (eyes at u 0.5 +- 0.165, v 0.51; mouth around v 0.25).
# Plain Python (numpy + Pillow), no Blender needed:
#   uv run --with pillow --with numpy python game/modelResource/scripts/npc_face_scary.py
# Writes modelResource/textures/npc_face_scary.png and assets/textures/npc_face_scary.png (loaded by the game).
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "../textures/npc_face.png")
OUTS = [os.path.join(HERE, "../textures/npc_face_scary.png"), os.path.join(HERE, "../../assets/textures/npc_face_scary.png")]

img = np.asarray(Image.open(SRC).convert("RGB")).astype(np.float32) / 255
N = img.shape[0]
v, u = np.meshgrid(1 - (np.arange(N) + 0.5) / N, (np.arange(N) + 0.5) / N, indexing="ij")   # v up, like the UVs


def ell(cu, cv, ru, rv, soft):
    """1 inside an ellipse, soft edge (soft = share of the radius that fades)"""
    d = np.sqrt(((u - cu) / ru) ** 2 + ((v - cv) / rv) ** 2)
    return np.clip((1 - d) / max(soft, 1e-3), 0, 1)[..., None]


def mix(a, col, w): return a * (1 - w) + np.asarray(col, np.float32) * w


rng = np.random.default_rng(91)
# bloodless: keep the light and dark of the face, drain the colour toward a cold grey-green
lum = img @ np.array([0.3, 0.59, 0.11], np.float32)
grey = lum[..., None] * np.array([0.86, 0.92, 0.88], np.float32)
img = mix(img, grey, 0.85) * 0.92
# dark veins spreading from the eyes and mouth
for (cu, cv), n in (((0.335, 0.51), 5), ((0.665, 0.51), 5), ((0.5, 0.24), 6)):
    for _ in range(n):
        a = rng.uniform(0, 2 * np.pi); L = rng.uniform(0.06, 0.16)
        for t in np.linspace(0, 1, 40):
            a += rng.normal(0, 0.12)
            pu, pv = cu + np.cos(a) * L * t, cv + np.sin(a) * L * t
            img = mix(img, (0.18, 0.2, 0.22), 0.35 * (1 - t) * ell(pu, pv, 0.006, 0.006, 0.8))
for s in (-1, 1):
    cu = 0.5 + s * 0.165
    img = mix(img, (0.2, 0.21, 0.22), 0.75 * ell(cu, 0.51, 0.13, 0.09, 0.6))     # bruised hollows
    img = mix(img, (0.02, 0.02, 0.025), ell(cu, 0.51, 0.08, 0.062, 0.3))          # black sockets
    img = mix(img, (0.97, 0.97, 0.94), ell(cu, 0.508, 0.03, 0.03, 0.25))          # small white eyes
# mouth torn open from v 0.13 to 0.33, wider than the lips were; a few teeth
img = mix(img, (0.22, 0.12, 0.12), ell(0.5, 0.235, 0.13, 0.115, 0.2))
img = mix(img, (0.03, 0.01, 0.01), ell(0.5, 0.235, 0.11, 0.095, 0.2))
for i, x in enumerate(np.linspace(-0.08, 0.08, 9)):
    if i in (1, 5): continue
    h = 0.03 + 0.025 * rng.random()
    img = mix(img, (0.78, 0.75, 0.62), ell(0.5 + x, 0.322 - h / 2, 0.008, h / 2, 0.3))
for i, x in enumerate(np.linspace(-0.065, 0.065, 7)):
    if i in (2, 3): continue
    h = 0.02 + 0.02 * rng.random()
    img = mix(img, (0.72, 0.68, 0.55), ell(0.5 + x, 0.148 + h / 2, 0.0075, h / 2, 0.3))
# dried blood running from the corners of the mouth
for s in (-1, 1):
    for t in np.linspace(0, 1, 30):
        img = mix(img, (0.25, 0.04, 0.04), 0.8 * ell(0.5 + s * (0.115 - 0.01 * t), 0.2 - 0.17 * t, 0.007, 0.01, 0.6))

out = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
for p in OUTS:
    out.save(p)
    print("wrote", os.path.normpath(p))
