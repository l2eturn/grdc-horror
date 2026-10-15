# The spirit house under the old tree (ศาลเจ้าที่ใต้ต้นไม้, after the user's Ghost House stills). Run inside Blender
# after ps2kit.py. Builds textures shrine_*, the "Shrine" collection; export with
#   bake_and_export(bpy.data.collections["Shrine"], "shrine")
# Origin = the foot of the shrine's post, the house faces -Y (= +Z in the game, toward whoever comes down the path).
# Nodes for the game: shrine_door (empty, just inside the doorway: the old woman's head comes out of here).
# Parts: weathered wooden house on a post (gable roof, chofa horns, bargeboards), offerings (zebra statues, red soda
# bottles, figurines, garlands, incense), three-colour cloth round the post and the tree, a big old tree with buttress
# roots and hanging aerial roots (its crown is lost in the fog).
import bpy, math, numpy as np
from mathutils import Vector as V

# ---------------------------------------------------------------- textures
N = 128
u, v = grid(N)
wood = np.array((0.30, 0.25, 0.20)) * (1 + 0.14*fractal(N, 100, ax=0.15, ay=6))[..., None]
wood *= (1 - 0.4*(1 - ss(0.0, 0.03, np.minimum((u*4) % 1, 1 - (u*4) % 1))))[..., None]        # board gaps
paint = np.clip(fractal(N, 101, beta=1.6) - 0.5, 0, 1)                                            # flakes of old red paint
wood = wood*(1 - 0.7*paint[..., None]) + np.array((0.42, 0.12, 0.08))*0.7*paint[..., None]
wood *= (1 - 0.25*np.clip(fractal(N, 102, beta=1.3), 0, 3)/3)[..., None]
save("shrine_wood", wood)
tile_v = (v*10) % 1; tile_u = (u*8 + 0.5*np.floor(v*10)) % 1                                     # fish-scale roof tiles
scale = ss(0.0, 0.15, tile_v) * (1 - ss(0.35, 0.5, np.abs(tile_u - 0.5)) * (tile_v < 0.4))
roof = np.array((0.30, 0.10, 0.07)) * (0.55 + 0.45*scale)[..., None] * (1 + 0.15*fractal(N, 103))[..., None]
moss = np.clip(fractal(N, 104, beta=1.5) - 0.3, 0, 1)
roof = roof*(1 - 0.8*moss[..., None]) + np.array((0.12, 0.15, 0.07))*0.8*moss[..., None]
save("shrine_roof", roof)
uu, vv = grid(64)
gold = np.array((0.55, 0.42, 0.18)) * (1 + 0.25*fractal(64, 105, beta=1.2))[..., None]
gold = gold*(1 - 0.6*np.clip(fractal(64, 106) - 0.3, 0, 1)[..., None])                          # tarnish
save("shrine_gold", gold)
save("shrine_dark", np.array((0.03, 0.025, 0.02)) * (1 + 0.2*fractal(64, 107))[..., None])
# cloth: three bands (pink, yellow, green) across u, faded and dirty
cloth = np.where((uu < 1/3)[..., None], (0.75, 0.35, 0.45), np.where((uu < 2/3)[..., None], (0.80, 0.68, 0.25), (0.30, 0.55, 0.30)))
cloth = cloth * (1 + 0.08*np.sin(vv*40*np.pi))[..., None] * (1 - 0.35*np.clip(fractal(64, 108), 0, 3)/3)[..., None]
save("shrine_cloth", cloth)
bark = np.array((0.24, 0.21, 0.18)) * (1 + 0.18*fractal(N, 109, ax=0.12, ay=5))[..., None]
bark *= (1 - 0.5*np.clip(fractal(N, 110, ax=0.2, ay=4, beta=1.4) - 0.8, 0, 2)/2)[..., None]      # fissures
bark = bark*(1 - 0.5*moss[..., None]) + np.array((0.14, 0.17, 0.09))*0.5*moss[..., None]
save("shrine_bark", bark)
save("shrine_leaves", np.array((0.07, 0.11, 0.05)) * (1 + 0.45*fractal(64, 111, beta=1.0))[..., None])
# leaf card (alpha cutout, 256 px): a loose cluster of small oval ficus leaves, darker toward the middle, so the
# crown reads as foliage against the fog instead of a solid faceted lump
LN = 256
rl = np.random.default_rng(116)
leaf_rgb = np.zeros((LN, LN, 3)); leaf_a = np.zeros((LN, LN))
yy, xx = np.mgrid[0:LN, 0:LN] / LN
for _ in range(900):
    cx, cy = rl.random(2)
    rr = np.hypot(cx - 0.5, cy - 0.5)
    if rr > 0.48 or rl.random() < rr*1.4: continue             # denser in the middle, ragged edge
    ang = rl.random()*np.pi; L, W = 0.020 + 0.012*rl.random(), 0.009 + 0.004*rl.random()
    x0, x1 = int(max(0, (cx - 0.04)*LN)), int(min(LN, (cx + 0.04)*LN)); y0, y1 = int(max(0, (cy - 0.04)*LN)), int(min(LN, (cy + 0.04)*LN))
    dx, dy = xx[y0:y1, x0:x1] - cx, yy[y0:y1, x0:x1] - cy
    a_ = dx*np.cos(ang) + dy*np.sin(ang); b_ = -dx*np.sin(ang) + dy*np.cos(ang)
    m = (a_/L)**2 + (b_/W)**2 < 1
    shade = (0.55 + 0.45*rl.random()) * (1.0 - 0.45*(1 - rr/0.5))
    col_ = np.array((0.07, 0.115, 0.05))*shade
    leaf_rgb[y0:y1, x0:x1][m] = col_; leaf_a[y0:y1, x0:x1][m] = 1
for _ in range(6):                                               # a few twigs through the cluster
    t = np.linspace(0, 1, 200); a0 = rl.random()*2*np.pi
    px = (0.5 + (0.08 + 0.32*t)*np.cos(a0 + 0.3*t)) * LN; py = (0.5 + (0.08 + 0.32*t)*np.sin(a0 + 0.3*t)) * LN
    for x_, y_ in zip(px.astype(int), py.astype(int)):
        if 0 <= x_ < LN and 0 <= y_ < LN: leaf_rgb[y_, x_] = (0.09, 0.07, 0.05); leaf_a[y_, x_] = 1
save("shrine_leafcard", leaf_rgb, leaf_a)
# figures atlas: 0 white porcelain, 1 painted skin, 2 red / gold costume, 3 zebra stripes
fig = np.zeros((64, 64, 3))
q = lambda i: (uu >= (i % 2)*0.5) & (uu < (i % 2)*0.5 + 0.5) & (vv >= (i//2)*0.5) & (vv < (i//2)*0.5 + 0.5)
fig[q(0)] = (0.80, 0.78, 0.72); fig[q(1)] = (0.72, 0.52, 0.40); fig[q(2)] = (0.55, 0.12, 0.08)
stripes = np.sin((uu*4 + vv*1.5)*np.pi*6) > 0.2
fig[q(3)] = np.where(stripes[q(3)][..., None], (0.08, 0.08, 0.08), (0.82, 0.80, 0.76))
fig *= (1 - 0.3*np.clip(fractal(64, 112), 0, 3)/3)[..., None]
save("shrine_figure", fig)
garland = np.where((np.sin(uu*64)*np.sin(vv*64) > 0)[..., None], (0.85, 0.45, 0.08), (0.75, 0.55, 0.12))
garland = garland*(1 - 0.5*np.clip(fractal(64, 113) + 0.3, 0, 2)[..., None]/2)                 # wilted brown
save("shrine_garland", garland)
save("shrine_concrete", np.array((0.42, 0.41, 0.38)) * (1 + 0.12*fractal(64, 114))[..., None])
save("shrine_soda", np.array((0.62, 0.05, 0.06)) * (1 + 0.1*fractal(64, 115))[..., None])

TEX = {k: "shrine_" + k for k in ("wood", "roof", "gold", "dark", "cloth", "bark", "leaves", "leafcard", "figure", "garland",
                                   "concrete", "soda")}
MATS, MI = make_materials("shrine_", TEX, cutout=("leafcard",))
K = Kit(MI)
col = collection("Shrine")
P = {"shrine": Part(), "tree": Part()}
S, T = P["shrine"], P["tree"]

# ---------------------------------------------------------------- the house on its post
PZ = 1.22                     # platform height
K.box(S, (0, 0, 0.05), (0.36, 0.36, 0.10), "concrete")
K.box(S, (0, 0, PZ/2), (0.13, 0.13, PZ), "wood")
for sx in (-1, 1):            # brackets under the platform
    for sy in (-1, 1):
        K.beam(S, (sx*0.06, sy*0.06, PZ - 0.28), (sx*0.33, sy*0.33, PZ - 0.02), 0.035, 0.035, "wood")
K.box(S, (0, 0, PZ + 0.025), (0.80, 0.80, 0.05), "wood")
for x in np.linspace(-0.38, 0.38, 5):          # railing
    for y in (-0.38, 0.38):
        if y < 0 and abs(x) < 0.1: continue      # the gap in front of the door
        K.box(S, (x, y, PZ + 0.10), (0.03, 0.03, 0.10), "wood")
for y in (-0.38, 0.38):
    for x0, x1 in ((-0.38, -0.12), (0.12, 0.38)) if y < 0 else ((-0.38, 0.38),):
        K.beam(S, (x0, y, PZ + 0.15), (x1, y, PZ + 0.15), 0.025, 0.02, "gold")
for x in (-0.38, 0.38): K.beam(S, (x, -0.38, PZ + 0.15), (x, 0.38, PZ + 0.15), 0.025, 0.02, "gold")
FZ, WH = PZ + 0.05, 0.34      # house floor, wall height
K.box(S, (0, 0.20, FZ + WH/2), (0.44, 0.02, WH), "wood")                                       # back wall
K.box(S, (0, 0.18, FZ + WH/2), (0.40, 0.01, WH - 0.02), "dark")                                # dark inside
for sx in (-1, 1):
    K.box(S, (sx*0.21, 0.0, FZ + WH/2), (0.02, 0.40, WH), "wood")                              # side walls
    K.box(S, (sx*0.145, -0.20, FZ + WH/2), (0.13, 0.02, WH), "wood")                           # front wall
K.box(S, (0, -0.20, FZ + WH - 0.04), (0.16, 0.02, 0.08), "gold")                               # lintel over the door
K.box(S, (0, 0.0, FZ + 0.005), (0.40, 0.38, 0.01), "dark")                                     # dark floor inside
K.box(S, (0, 0.0, FZ + WH - 0.005), (0.42, 0.40, 0.01), "dark")                                # ceiling
for sx in (-1, 1):            # porch columns
    K.tube(S, [(sx*0.18, -0.33, FZ), (sx*0.18, -0.33, FZ + WH)], 0.018, "gold", sides=6)
# roof: steep gable along Y with a smaller upper tier, overhanging the porch
EZ, RZ = FZ + WH - 0.04, FZ + WH + 0.26
for tier, (y0, y1, dz, w) in enumerate(((-0.40, 0.28, 0.0, 0.34), (-0.30, 0.18, 0.07, 0.26))):
    for sx in (-1, 1):
        a = V((sx*w, (y0 + y1)/2, EZ + dz)); b = V((0, (y0 + y1)/2, RZ + dz))
        d = b - a; ang = math.degrees(math.atan2(d.z, abs(d.x)))
        K.box(S, (a + b)/2, (d.length + 0.02, y1 - y0, 0.025), "roof", rot=(0, sx*ang, 0), tile=0.3)
    for y in (y0, y1):        # bargeboards with hooked ends, and the chofa horn at the peak
        for sx in (-1, 1):
            K.beam(S, (sx*(w + 0.01), y - 0.01*np.sign(y), EZ + dz - 0.01), (0, y - 0.01*np.sign(y), RZ + dz + 0.01), 0.02, 0.025, "gold")
            K.tube(S, [(sx*(w + 0.01), y, EZ + dz - 0.01), (sx*(w + 0.05), y, EZ + dz + 0.01), (sx*(w + 0.06), y, EZ + dz + 0.06)],
                   [0.012, 0.01, 0.005], "gold", sides=4)
        K.tube(S, [(0, y, RZ + dz), (0, y - 0.03*np.sign(y), RZ + dz + 0.08), (0, y - 0.07*np.sign(y), RZ + dz + 0.13),
                   (0, y - 0.11*np.sign(y), RZ + dz + 0.12)], [0.016, 0.013, 0.009, 0.004], "gold", sides=5)
# front gable triangle (gold, carved) under the upper tier
import bmesh
b = bmesh.new()
tri = [b.verts.new(p) for p in ((-0.33, -0.39, EZ), (0.33, -0.39, EZ), (0, -0.39, RZ - 0.02))]
b.faces.new(tri)
K.add(S, b, "gold", lambda c, f: (c.x*3, c.z*3))
# offerings on the porch: figurines, soda bottles, incense; garlands on the columns
for i, x in enumerate((-0.12, 0.12)):
    K.blob(S, (x, -0.29, FZ + 0.05), (0.025, 0.02, 0.05), "figure", seed=i, uvoff=(0.25, 0.75), uvs=0.5, lumpy=0.02)   # dancer
    K.tube(S, [(x, -0.29, FZ + 0.09), (x, -0.29, FZ + 0.15)], [0.012, 0.002], "figure", sides=5)
    K.blob(S, (x*1.9, -0.25, FZ + 0.045), (0.03, 0.025, 0.045), "figure", seed=5 + i, uvoff=(0.25, 0.25), uvs=0.5, lumpy=0.03)  # old couple
    K.tube(S, [(x*2.6, -0.33, FZ), (x*2.6, -0.33, FZ + 0.1), (x*2.6, -0.33, FZ + 0.13)], [0.016, 0.016, 0.004], "soda", sides=6)
    K.tube(S, [(x*2.6, -0.33, FZ + 0.12), (x*2.6 + 0.01, -0.34, FZ + 0.19)], 0.002, "concrete", sides=3)    # straw
    K.tube(S, [(x*1.5, -0.33, FZ + WH*0.7)] + [(x*1.5 + 0.035*math.cos(a), -0.33 + 0.02*math.sin(a), FZ + WH*0.7 - 0.10 - 0.04*math.sin(a/2))
           for a in np.linspace(0, 2*math.pi, 9)][1:-1] + [(x*1.5, -0.33, FZ + WH*0.7)], 0.012, "garland", sides=4)
K.tube(S, [(0, -0.30, FZ), (0, -0.30, FZ + 0.05)], [0.03, 0.035], "concrete", sides=8, cap=True)          # incense pot
for k in range(5):
    a = k*1.3
    K.tube(S, [(0.01*math.cos(a), -0.30 + 0.01*math.sin(a), FZ + 0.05), (0.03*math.cos(a), -0.30 + 0.03*math.sin(a), FZ + 0.17)],
           0.0025, "soda", sides=3)
# cloth round the post: three bands, the ends hanging
for i, z in enumerate((0.95, 1.0, 1.05)):
    K.tube(S, [(0, 0, z), (0, 0, z + 0.045)], 0.078, "cloth", sides=8, tile=0.15)
for sx, lean in ((-1, 0.05), (1, -0.03)):
    K.quad2(S, (sx*0.05, -0.075, 1.03), (sx*0.10, -0.075, 1.03), (sx*0.14 + lean, -0.09, 0.55), (sx*0.08 + lean, -0.09, 0.55), "cloth",
            uv=(0.4 if sx < 0 else 0.0, 0, 0.6 if sx < 0 else 0.3, 1))
# zebra statues at the foot of the post (white with black stripes)
for i, (x, y, rot) in enumerate(((-0.45, -0.35, 20), (0.42, -0.40, -30))):
    R = lambda p, rot=rot: V((p[0]*math.cos(math.radians(rot)) - p[1]*math.sin(math.radians(rot)),
                               p[0]*math.sin(math.radians(rot)) + p[1]*math.cos(math.radians(rot)), p[2]))
    c = V((x, y, 0))
    K.blob(S, c + R((0, 0, 0.21)), (0.07, 0.15, 0.07), "figure", rot=rot, seed=20 + i, uvoff=(0.75, 0.75), uvs=0.6, lumpy=0.02)
    for lx, ly in ((-0.04, -0.09), (0.04, -0.09), (-0.04, 0.09), (0.04, 0.09)):
        K.tube(S, [c + R((lx, ly, 0.2)), c + R((lx, ly, 0.0))], 0.015, "figure", sides=4)
    K.tube(S, [c + R((0, -0.12, 0.24)), c + R((0, -0.17, 0.36)), c + R((0, -0.24, 0.36))], [0.03, 0.028, 0.02], "figure", sides=5, cap=True)
# a few dry vines hanging off the roof
for k, (x, y) in enumerate(((-0.3, -0.38), (0.25, -0.36), (0.33, 0.1), (-0.32, 0.25))):
    K.tube(S, [(x, y, EZ), (x + 0.02, y - 0.01, EZ - 0.2), (x - 0.01, y, EZ - 0.35 - 0.1*k)], [0.006, 0.005, 0.003], "bark", sides=3)

# ---------------------------------------------------------------- the old tree behind it
TC = V((0.95, 0.85, 0))
K.tube(T, [TC + V(p) for p in ((0, 0, -0.1), (0, 0, 0.6), (0.05, 0.02, 1.6), (0.1, 0.05, 2.8), (0.05, 0.1, 4.0), (0.0, 0.12, 4.6))],
       [0.62, 0.48, 0.42, 0.38, 0.34, 0.32], "bark", sides=10, tile=1.0)
rng = np.random.default_rng(120)
for k in range(7):            # buttress roots
    a = 2*math.pi*k/7 + 0.3*rng.standard_normal()
    d = V((math.cos(a), math.sin(a), 0))
    K.tube(T, [TC + d*0.35 + V((0, 0, 0.7)), TC + d*0.75 + V((0, 0, 0.25)), TC + d*(1.2 + 0.4*rng.random()) + V((0, 0, -0.05))],
           [0.20, 0.13, 0.05], "bark", sides=6, tile=1.0)
for k, (dx, dy) in enumerate(((1.6, 0.5), (-1.4, 0.9), (0.2, -1.5))):      # three great limbs into the fog
    top = TC + V((dx*2.0, dy*2.0, 9.5))
    K.tube(T, [TC + V((0, 0.1, 4.2)), TC + V((dx*0.6, dy*0.6, 5.6)), TC + V((dx*1.3, dy*1.3, 7.4)), top],
           [0.28, 0.22, 0.15, 0.06], "bark", sides=7, tile=1.0)
    # crown: a smaller dark core (fills the gaps) wrapped in criss-crossing leaf cards that break the outline
    K.blob(T, top + V((0, 0, 0.5)), (1.9, 1.9, 1.0), "leaves", seed=30 + k, lumpy=0.2, us=8, vs=5)
    for j in range(14):
        a1 = rng.random()*math.pi; tilt = math.radians(-35 + 70*rng.random())
        e1 = V((math.cos(a1), math.sin(a1), 0)); e2 = V((-math.sin(a1)*math.cos(tilt), math.cos(a1)*math.cos(tilt), math.sin(tilt)))
        c = top + V((2.0*rng.standard_normal()*0.6, 2.0*rng.standard_normal()*0.6, 0.4 + 0.7*rng.standard_normal()*0.6))
        h = 1.3 + 0.5*rng.random()
        K.quad2(T, c - e1*h - e2*h, c + e1*h - e2*h, c + e1*h + e2*h, c - e1*h + e2*h, "leafcard", uv=(0, 0, 1, 1))
    for j in range(5):        # aerial roots hanging from the limbs (a banyan)
        t = 0.3 + 0.12*j
        p = TC + V((dx*0.6*t*2, dy*0.6*t*2, 4.6 + 3*t))
        K.tube(T, [p, p + V((0.05, 0, -1.2)), p + V((0.02, 0.03, -(1.8 + 1.4*rng.random())))], [0.02, 0.015, 0.006], "bark", sides=4)
for z in (1.25, 1.33, 1.41):  # cloth tied round the trunk
    K.tube(T, [TC + V((0.05, 0.02, z)), TC + V((0.05, 0.02, z + 0.07))], 0.45, "cloth", sides=10, tile=0.5)
K.quad2(T, TC + V((-0.2, -0.42, 1.36)), TC + V((-0.08, -0.43, 1.36)), TC + V((-0.06, -0.47, 0.75)), TC + V((-0.2, -0.46, 0.8)), "cloth",
        uv=(0.0, 0, 0.32, 1))

# ---------------------------------------------------------------- objects
root, OBJ, tris = finish(col, "shrine_", P, MATS, {}, {}, "shrine", cutout_mats=("leafcard",),
                         names={"shrine": "shrine_house", "tree": "shrine_tree"})
door = bpy.data.objects.new("shrine_door", None); col.objects.link(door)
door.parent = root; door.location = (0, -0.12, FZ + 0.15); door.empty_display_size = 0.05
print("shrine tris", tris)
