# Pink low-floor electric city bus (PS2 style) for the bus-stop event. Run inside Blender; textures are made here.
# Z up, the bus faces -Y (= +Z in glTF / the game), origin on the road under the middle of the bus.
# Thai traffic keeps left: the doors are on the bus's left (+X here), the driver sits on the right (-X).
#
# Nodes the game uses:
#   bus_door_f1/f2, bus_door_m1/m2  door leaves (pivot at the hinge, clip "doors_open" / "doors_close")
#   bus_wheel_FL/FR/RL/RR           wheels (pivot at the hub, the game spins them around X)
#   bus_driver_seat                 empty: put the driver's root here (feet on the cab floor, facing -Y)
#   bus_door_front                  empty: where a boarding NPC starts (clip board_bus), facing into the bus (-X)
#   bus_headlight_L/R, bus_cabin_light  empties for the game's lights
#   materials bus_sign (destination LED, UV 0..1) and bus_front_text (black panel, UV 0..1): painted by the game
import bpy, bmesh, math, numpy as np
from mathutils import Vector as V, Matrix

OUT = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/textures/"
col = bpy.data.collections.get("Bus")
if col:
    for o in list(col.objects): bpy.data.objects.remove(o, do_unlink=True)
else:
    col = bpy.data.collections.new("Bus"); bpy.context.scene.collection.children.link(col)


# ---------------------------------------------------------------- textures (tileable, 64-128 px)
def fractal(N, seed, ax=1.0, ay=1.0, beta=1.8):
    r = np.random.default_rng(seed); w = r.standard_normal((N, N))
    f = np.fft.fftfreq(N)*N; fu, fv = np.meshgrid(f, f)
    k = np.sqrt((fu/ax)**2 + (fv/ay)**2); amp = np.where(k >= 1, np.maximum(k, 1e-6)**(-beta), 0)
    n = np.real(np.fft.ifft2(np.fft.fft2(w)*amp)); return (n-n.mean())/(n.std()+1e-9)


def image(name, rgb):
    H, W = rgb.shape[:2]
    im = bpy.data.images.get(name) or bpy.data.images.new(name, W, H)
    if im.size[0] != W: im.scale(W, H)
    px = np.ones((H, W, 4), np.float32); px[..., :3] = np.clip(rgb, 0, 1); im.pixels[:] = px.ravel()
    im.filepath_raw = OUT + name + ".png"; im.file_format = "PNG"; im.save(); im.pack()
    return im


N = 128
# night-dulled pink, sun-faded blotches, grime streaks running down from every ledge (v = height, 2 m tile)
paint = np.array((0.66, 0.30, 0.43)) * (1 + 0.04*fractal(N, 31))[..., None]
fade = np.clip(fractal(N, 36, beta=2.4), -1, 2)/2
paint = paint*(1 - 0.18*fade[..., None]) + 0.18*fade[..., None]*np.array((0.55, 0.47, 0.48))
paint *= (1 - 0.30*np.clip(fractal(N, 32, ax=10, ay=0.4), 0, 3)/3)[..., None]   # long road-grime streaks
paint *= (1 - 0.15*np.clip(fractal(N, 37, ax=6, ay=0.6, beta=1.2), 0, 3)/3)[..., None]   # fine streaks
image("bus_paint", paint)
uu = np.tile(np.arange(N)/N, (N, 1)); vv = uu.T
dots = ((np.sin(uu*np.pi*2*16) > 0.9) & (np.sin(vv*np.pi*2*16) > 0.9))
image("bus_floor", np.array((0.24, 0.24, 0.25)) * (1 + 0.05*fractal(N, 33))[..., None] * (1 - 0.25*dots)[..., None])
image("bus_seat", np.array((0.16, 0.22, 0.42)) * (1 + 0.08*fractal(64, 34, beta=1.0))[..., None])
image("bus_dark", np.array((0.035, 0.035, 0.04)) * (1 + 0.15*fractal(64, 35))[..., None])


# ---------------------------------------------------------------- materials
def mat(name, rgb=(1, 1, 1), tex=None, rough=0.6, metal=0.0, alpha=1.0, emit=None):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True; nt = m.node_tree
    for n in list(nt.nodes):
        if n.type not in ("BSDF_PRINCIPLED", "OUTPUT_MATERIAL"): nt.nodes.remove(n)
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "AO"
    mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs["Factor"].default_value = 1
    if tex:
        t = nt.nodes.new("ShaderNodeTexImage"); t.image = bpy.data.images[tex]
        nt.links.new(t.outputs["Color"], mx.inputs[6])
    else:
        mx.inputs[6].default_value = (*rgb, 1)
    nt.links.new(vc.outputs["Color"], mx.inputs[7]); nt.links.new(mx.outputs[2], b.inputs["Base Color"])
    if alpha < 1:
        b.inputs["Alpha"].default_value = alpha
        try: m.surface_render_method = "BLENDED"
        except Exception: m.blend_method = "BLEND"
    if emit:
        b.inputs["Emission Color"].default_value = (*emit[0], 1); b.inputs["Emission Strength"].default_value = emit[1]
    return m


M = dict(
    paint=mat("bus_paint", tex="bus_paint", rough=0.45),
    black=mat("bus_black", tex="bus_dark", rough=0.35),
    glass=mat("bus_glass", (0.05, 0.07, 0.08), rough=0.1, alpha=0.55),
    tire=mat("bus_tire", (0.04, 0.04, 0.04), rough=0.9),
    hub=mat("bus_hub", (0.55, 0.55, 0.56), rough=0.4),
    floor=mat("bus_floor", tex="bus_floor", rough=0.8),
    seat=mat("bus_seat", tex="bus_seat", rough=0.9),
    yellow=mat("bus_yellow", (0.85, 0.62, 0.08), rough=0.4),
    grey=mat("bus_grey", (0.32, 0.33, 0.34), rough=0.6),
    head=mat("bus_headlight", (0.9, 0.9, 0.85), rough=0.2, emit=((1, 0.95, 0.85), 0.0)),
    tail=mat("bus_taillight", (0.5, 0.03, 0.03), rough=0.3, emit=((1, 0.1, 0.05), 0.0)),
    lamp=mat("bus_cabin_lamp", (0.9, 0.95, 1.0), rough=0.3, emit=((0.85, 0.95, 1.0), 2.0)),
    sign=mat("bus_sign", (0.02, 0.02, 0.02), rough=0.3),
    front=mat("bus_front_text", (0.03, 0.03, 0.035), rough=0.35),
)
TILE = {"paint": 2.0, "black": 1.0, "floor": 1.0, "seat": 0.6}

# ---------------------------------------------------------------- geometry helpers (one bmesh per part/material)
PARTS = {}


def bm_of(part):
    if part not in PARTS:
        b = bmesh.new(); b.loops.layers.uv.new("UVMap"); PARTS[part] = (b, {})
    return PARTS[part]


def face(part, k, pts, uvs):
    b, mats = bm_of(part); uvl = b.loops.layers.uv["UVMap"]
    if k not in mats: mats[k] = len(mats)
    f = b.faces.new([b.verts.new(p) for p in pts]); f.material_index = mats[k]
    for l, t in zip(f.loops, uvs): l[uvl].uv = t
    return f


def box(k, mn, mx, part="body", skip=(), uvbox=False):
    """axis-aligned box; world-meter UVs (or 0..1 per face with uvbox)"""
    mn, mx = V(mn), V(mx); T = TILE.get(k, 1.0)
    c = (mn+mx)/2; H = (mx-mn)/2; A = [V((1, 0, 0)), V((0, 1, 0)), V((0, 0, 1))]
    for i in range(3):
        for s in (1, -1):
            tag = ("+" if s > 0 else "-") + "xyz"[i]
            if tag in skip: continue
            j, l = (i+1) % 3, (i+2) % 3
            ua, va = (j, l) if i != 2 else (0, 1)
            if i == 0: ua, va = 1, 2
            if i == 1: ua, va = 0, 2
            pts = [c + A[i]*s*H[i] + A[ua]*a*H[ua] + A[va]*bb*H[va] for a, bb in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            flip = (pts[1]-pts[0]).cross(pts[2]-pts[0]).dot(A[i]*s) < 0
            if uvbox: uvs = [((a+1)/2, (bb+1)/2) for a, bb in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            else: uvs = [(p[ua]/T, p[va]/T) for p in pts]
            if flip: pts, uvs = pts[::-1], uvs[::-1]
            face(part, k, pts, uvs)


def cyl(k, c, axis, r, length, sides=12, part="body", caps=True):
    c, axis = V(c), V(axis).normalized()
    a = axis.orthogonal().normalized(); b2 = axis.cross(a)
    rings = [[c + axis*(s*length/2) + (a*math.cos(2*math.pi*i/sides) + b2*math.sin(2*math.pi*i/sides))*r for i in range(sides)] for s in (-1, 1)]
    for i in range(sides):
        j = (i+1) % sides
        face(part, k, [rings[0][i], rings[0][j], rings[1][j], rings[1][i]], [(i/sides, 0), ((i+1)/sides, 0), ((i+1)/sides, 1), (i/sides, 1)])
    if caps:
        face(part, k, rings[1], [(0.5+0.5*math.cos(2*math.pi*i/sides), 0.5+0.5*math.sin(2*math.pi*i/sides)) for i in range(sides)])
        face(part, k, rings[0][::-1], [(0.5, 0.5)]*sides)


def quad(k, pts, part="body", uv01=True):
    pts = [V(p) for p in pts]
    face(part, k, pts, [(0, 0), (1, 0), (1, 1), (0, 1)] if uv01 else [(p.x+p.y, p.z) for p in pts])


# ================================================================ body
L0, L1 = -4.3, 4.3          # front, rear
W = 1.2                     # half width
FLOOR = 0.36                # low floor (one step from the road)
BELT, ROOFB, ROOF = 1.2, 2.55, 3.0
AX_F, AX_R, WR = -2.2, 2.6, 0.47            # axles, wheel radius
DOOR_F, DOOR_M = (-3.95, -3.05), (0.9, 2.0)  # door openings on the +X side
ARCH = lambda ax: (ax-0.55, ax+0.55)

# floor slab + under-body
box("black", (-W+0.02, L0+0.05, 0.28), (W-0.02, L1-0.05, FLOOR), skip=("-z",))
box("floor", (-W+0.06, L0+0.1, FLOOR), (W-0.06, L1-0.1, FLOOR+0.005), skip=("-z",))


def side_panels(x, sgn, gaps):
    """lower body on one side between y0..y1, leaving gaps (doors full height, arches only below 1.0)"""
    ys = sorted({L0+0.12, L1-0.08} | {g for gg in gaps for g in gg[:2]})
    for y0, y1 in zip(ys[:-1], ys[1:]):
        mid = (y0+y1)/2
        kind = next((g[2] for g in gaps if g[0] <= mid <= g[1]), None)
        xi, xo = (x-0.05, x) if sgn > 0 else (x, x+0.05)
        if kind == "door": continue
        z0 = 1.0 if kind == "arch" else 0.30
        if z0 < 0.7:    # split at 0.7 m so the baked road-dirt gradient (bus_export.py) has a vertex row to land on
            box("paint", (xi, y0, z0), (xo, y1, 0.7), skip=("+z",)); box("paint", (xi, y0, 0.7), (xo, y1, BELT), skip=("-z",))
        else:
            box("paint", (xi, y0, z0), (xo, y1, BELT))


side_panels(W, 1, [(*DOOR_F, "door"), (*DOOR_M, "door"), (*ARCH(AX_F), "arch"), (*ARCH(AX_R), "arch")])
side_panels(-W, -1, [(*ARCH(AX_F), "arch"), (*ARCH(AX_R), "arch")])
# arch liners (dark) so you can't see into the body through the wheel wells
for ax in (AX_F, AX_R):
    for sx in (1, -1):
        x0, x1 = sorted((sx*(W-0.05), sx*(W-0.38)))
        box("black", (x0, ARCH(ax)[0], 0.98), (x1, ARCH(ax)[1], 1.0))          # top of the wheel well
        xi = sx*(W-0.38)
        box("black", (min(xi, xi+0.02*sx), ARCH(ax)[0], 0.30), (max(xi, xi+0.02*sx), ARCH(ax)[1], 1.0))   # inner wall
# front: pink bumper, black band with the "ELECTRIC BUS" panel, headlights, plate
box("paint", (-W, L0, 0.30), (W, L0+0.14, 0.6), skip=("+z",)); box("paint", (-W, L0, 0.6), (W, L0+0.14, 0.86), skip=("-z",))
box("black", (-W, L0-0.02, 0.86), (W, L0+0.14, BELT))
quad("front", [(-0.75, L0-0.025, 0.89), (0.75, L0-0.025, 0.89), (0.75, L0-0.025, 1.17), (-0.75, L0-0.025, 1.17)][::-1], part="front_text")
for sx in (1, -1):
    box("head", (sx*0.78 - 0.2, L0-0.02, 0.58), (sx*0.78 + 0.2, L0+0.02, 0.72), part="body")
    box("black", (sx*0.78 - 0.22, L0-0.01, 0.56), (sx*0.78 + 0.22, L0+0.02, 0.74))
box("hub", (-0.26, L0-0.02, 0.36), (0.26, L0, 0.48))                     # plate
box("grey", (-0.25, L0-0.015, 0.62), (0.25, L0, 0.66))                   # grille bar
# rear: pink, tail lights, rear window
box("paint", (-W, L1-0.1, 0.30), (W, L1, 0.7), skip=("+z",)); box("paint", (-W, L1-0.1, 0.7), (W, L1, BELT), skip=("-z",))
for sx in (1, -1): box("tail", (sx*1.0 - 0.12, L1, 0.6), (sx*1.0 + 0.12, L1+0.02, 1.0))

# ---------------------------------------------------------------- window band: pillars + glass, windshield
def pillar(x, y, w=0.12, xi=0.05):
    box("black", (min(x, x - xi*(1 if x > 0 else -1)), y-w/2, BELT), (max(x, x - xi*(1 if x > 0 else -1)), y+w/2, ROOFB))


for sx in (1, -1):
    x = sx*W
    ys = [L0+0.12, -2.45, -1.2, 0.0, 0.9, 2.0, 3.0, L1-0.08] if sx > 0 else [L0+0.12, -3.0, -1.8, -0.6, 0.6, 1.8, 3.0, L1-0.08]
    for y in ys: pillar(x, y)
    # glass between pillars (doors have their own glass)
    for y0, y1 in zip(ys[:-1], ys[1:]):
        if sx > 0 and (DOOR_F[0] - 0.01 <= (y0+y1)/2 <= DOOR_F[1] + 0.01 or DOOR_M[0] <= (y0+y1)/2 <= DOOR_M[1]): continue
        pts = [(x, y0+0.06, BELT), (x, y1-0.06, BELT), (x, y1-0.06, ROOFB), (x, y0+0.06, ROOFB)]
        quad("glass", pts if sx > 0 else pts[::-1], part="glass")
# front door's post toward the windshield, and the band above the front door is black
pillar(W, DOOR_F[0]-0.0, 0.08)
# windshield (slightly raked), A-pillars, destination sign
box("black", (-W, L0, BELT), (-W+0.1, L0+0.12, ROOFB))
box("black", (W-0.1, L0, BELT), (W, L0+0.12, ROOFB))
quad("glass", [(-W+0.1, L0+0.04, BELT), (W-0.1, L0+0.04, BELT), (W-0.1, L0+0.1, ROOFB-0.22), (-W+0.1, L0+0.1, ROOFB-0.22)][::-1], part="glass")
box("black", (-W, L0+0.02, ROOFB-0.22), (W, L0+0.14, ROOFB+0.05))
quad("sign", [(-0.95, L0+0.015, ROOFB-0.19), (0.75, L0+0.015, ROOFB-0.19), (0.75, L0+0.015, ROOFB+0.02), (-0.95, L0+0.015, ROOFB+0.02)][::-1], part="sign")
box("black", (-W, L1-0.06, BELT), (W, L1, ROOFB))                          # rear window frame (dark)

# ---------------------------------------------------------------- roof: rounded edge block + battery/AC pods
rb = bmesh.new(); bmesh.ops.create_cube(rb, size=1)
for v in rb.verts: v.co = V((v.co.x*2*W, v.co.y*(L1-L0) + (L0+L1)/2, v.co.z*(ROOF-ROOFB) + (ROOF+ROOFB)/2))
edges = [e for e in rb.edges if not (abs(e.verts[0].co.z - ROOFB) < 1e-4 and abs(e.verts[1].co.z - ROOFB) < 1e-4)]
bmesh.ops.bevel(rb, geom=edges, offset=0.14, segments=2, affect="EDGES", profile=0.5)
b, mats = bm_of("body"); uvl = b.loops.layers.uv["UVMap"]
if "paint" not in mats: mats["paint"] = len(mats)
rm = bpy.data.meshes.new("tmp"); rb.to_mesh(rm); rb.free()
n0 = len(b.faces); b.from_mesh(rm); bpy.data.meshes.remove(rm); b.faces.ensure_lookup_table()
for f in list(b.faces)[n0:]:
    f.material_index = mats["paint"]
    for l in f.loops: l[uvl].uv = ((l.vert.co.x + l.vert.co.y)/2, (l.vert.co.z + l.vert.co.y)/2)
for (y0, y1) in ((-3.7, -1.3), (1.4, 3.9)):
    pb = bmesh.new(); bmesh.ops.create_cube(pb, size=1)
    for v in pb.verts: v.co = V((v.co.x*1.9, v.co.y*(y1-y0) + (y0+y1)/2, v.co.z*0.26 + ROOF + 0.11))
    bmesh.ops.bevel(pb, geom=[e for e in pb.edges], offset=0.1, segments=2, affect="EDGES", profile=0.5)
    pm = bpy.data.meshes.new("tmp"); pb.to_mesh(pm); pb.free()
    n0 = len(b.faces); b.from_mesh(pm); bpy.data.meshes.remove(pm); b.faces.ensure_lookup_table()
    for f in list(b.faces)[n0:]:
        f.material_index = mats["paint"]
        for l in f.loops: l[uvl].uv = ((l.vert.co.x + l.vert.co.y)/2, (l.vert.co.z + l.vert.co.y)/2)
    for i in range(5):   # vent slots on the pods
        yy = y0 + 0.35 + i*(y1-y0-0.7)/4
        box("black", (-0.7, yy-0.05, ROOF+0.24), (0.7, yy+0.05, ROOF+0.245), skip=("-z",))

# mirrors: arms forward from the front corners, mirror heads hanging down
for sx in (1, -1):
    cyl("black", (sx*1.22, L0-0.12, 2.35), (0, 1, 0.3), 0.025, 0.3, 6)
    box("black", (sx*1.24 - 0.09, L0-0.32, 1.75), (sx*1.24 + 0.09, L0-0.24, 2.3))

# ---------------------------------------------------------------- interior: ceiling, lamps, poles, seats, cab
box("grey", (-W+0.05, L0+0.15, ROOFB-0.02), (W-0.05, L1-0.1, ROOFB), skip=("+z",), part="body")
for y in (-2.5, -0.5, 1.5, 3.3):
    for sx in (1, -1): box("lamp", (sx*0.6 - 0.05, y-0.5, ROOFB-0.05), (sx*0.6 + 0.05, y+0.5, ROOFB-0.02), part="lamps")
for sx in (1, -1): cyl("yellow", (sx*0.55, 0.3, ROOFB-0.25), (0, 1, 0), 0.02, 7.6, 8, caps=False)       # hand rails
for (x, y) in ((0.9, DOOR_F[1]+0.12), (0.9, DOOR_M[0]-0.12), (0.9, DOOR_M[1]+0.12), (0.0, -0.8), (0.0, 1.0), (-0.45, -3.4)):
    cyl("yellow", (x, y, (FLOOR+ROOFB)/2), (0, 0, 1), 0.02, ROOFB-FLOOR, 8, caps=False)
def seat(x, y, facing=-1):
    box("seat", (x-0.22, y-0.2, FLOOR+0.42), (x+0.22, y+0.2, FLOOR+0.5), part="body")
    yb = y + 0.2*(-facing)
    box("seat", (x-0.22, yb-0.05, FLOOR+0.5), (x+0.22, yb+0.05, FLOOR+1.05), part="body")
    box("grey", (x-0.03, y-0.03, FLOOR), (x+0.03, y+0.03, FLOOR+0.42), skip=("-z", "+z"))
for y in (-1.6, -0.8, 0.0, 0.8, 1.6, 2.6, 3.4):
    for x in (-0.95, -0.5): seat(x, y)
for y in (-0.6, 0.2, 3.0, 3.8): seat(0.75, y)
# driver's cab (front right): dashboard, seat, wheel column; a low partition
box("black", (-W+0.05, L0+0.14, FLOOR+0.55), (W-0.05, L0+0.75, 1.15))
box("seat", (-0.98, -3.25, FLOOR+0.40), (-0.52, -2.85, FLOOR+0.50))
box("seat", (-0.98, -2.85, FLOOR+0.50), (-0.52, -2.75, FLOOR+1.15))
box("grey", (-0.78, -3.08, FLOOR), (-0.72, -3.02, FLOOR+0.40), skip=("-z", "+z"))
box("yellow", (-0.45, -3.3, FLOOR+0.7), (-0.42, -2.6, FLOOR+1.1))
cyl("black", (-0.75, -3.75, 1.05), (0, 0.55, 0.85), 0.03, 0.35, 6)                    # steering column
wheel_c = V((-0.75, -3.62, 1.22)); wheel_n = V((0, 0.55, 0.85)).normalized()
a = wheel_n.orthogonal().normalized(); b2 = wheel_n.cross(a)
pts = [wheel_c + (a*math.cos(2*math.pi*i/12) + b2*math.sin(2*math.pi*i/12))*0.21 for i in range(13)]
for p0, p1 in zip(pts[:-1], pts[1:]): cyl("black", (p0+p1)/2, p1-p0, 0.018, (p1-p0).length+0.01, 5, caps=False)

# ---------------------------------------------------------------- separate nodes: wheels, door leaves
def node_part(name):
    return name


for nm, (sx, ax) in {"bus_wheel_FL": (1, AX_F), "bus_wheel_FR": (-1, AX_F), "bus_wheel_RL": (1, AX_R), "bus_wheel_RR": (-1, AX_R)}.items():
    c = V((sx*(W-0.16), ax, WR))
    cyl("tire", c, (1, 0, 0), WR, 0.3, 14, part=nm)
    cyl("hub", c + V((sx*0.155, 0, 0)), (1, 0, 0), 0.26, 0.02, 10, part=nm)
DOORS = {"bus_door_f1": (DOOR_F[0], +1), "bus_door_f2": (DOOR_F[1], -1), "bus_door_m1": (DOOR_M[0], +1), "bus_door_m2": (DOOR_M[1], -1)}
for nm, (yh, d) in DOORS.items():
    wleaf = ((DOOR_F[1]-DOOR_F[0]) if "_f" in nm else (DOOR_M[1]-DOOR_M[0]))/2 - 0.01
    y0, y1 = sorted((yh, yh + d*wleaf))
    box("black", (W-0.05, y0, FLOOR+0.02), (W-0.01, y1, ROOFB-0.05), part=nm, skip=("+x", "-x"))
    box("black", (W-0.05, y0, FLOOR+0.02), (W-0.01, y1, FLOOR+0.35), part=nm)
    quad("glass", [(W-0.03, y0+0.05, FLOOR+0.38), (W-0.03, y1-0.05, FLOOR+0.38), (W-0.03, y1-0.05, ROOFB-0.12), (W-0.03, y0+0.05, ROOFB-0.12)], part=nm)
    quad("glass", [(W-0.03, y0+0.05, FLOOR+0.38), (W-0.03, y1-0.05, FLOOR+0.38), (W-0.03, y1-0.05, ROOFB-0.12), (W-0.03, y0+0.05, ROOFB-0.12)][::-1], part=nm)
    box("black", (W-0.05, y0, ROOFB-0.12), (W-0.01, y1, ROOFB-0.05), part=nm)

# ---------------------------------------------------------------- objects
PIVOTS = {"bus_wheel_FL": V((W-0.16, AX_F, WR)), "bus_wheel_FR": V((-W+0.16, AX_F, WR)),
          "bus_wheel_RL": V((W-0.16, AX_R, WR)), "bus_wheel_RR": V((-W+0.16, AX_R, WR)),
          "bus_door_f1": V((W-0.03, DOOR_F[0], 0)), "bus_door_f2": V((W-0.03, DOOR_F[1], 0)),
          "bus_door_m1": V((W-0.03, DOOR_M[0], 0)), "bus_door_m2": V((W-0.03, DOOR_M[1], 0))}
root = bpy.data.objects.new("bus", None); col.objects.link(root)
tris = 0
for part, (b, mats) in PARTS.items():
    bmesh.ops.remove_doubles(b, verts=b.verts, dist=1e-5)
    name = part if part.startswith("bus_") else "bus_" + part
    old = bpy.data.meshes.get(name)
    if old: bpy.data.meshes.remove(old)
    me = bpy.data.meshes.new(name); b.to_mesh(me); b.free()
    for k, i in sorted(mats.items(), key=lambda kv: kv[1]): me.materials.append(M[k])
    piv = PIVOTS.get(name, V())
    me.transform(Matrix.Translation(-piv))
    a = me.color_attributes.new("AO", "BYTE_COLOR", "CORNER")
    for d in a.data: d.color = (1, 1, 1, 1)
    o = bpy.data.objects.new(name, me); o.location = piv; o.parent = root; o.rotation_mode = "XYZ"; col.objects.link(o)
    tris += sum(len(p.vertices)-2 for p in me.polygons)
for name, loc in {"bus_driver_seat": (-0.75, -3.05, FLOOR), "bus_door_front": (W+0.35, (DOOR_F[0]+DOOR_F[1])/2, 0),
                  "bus_headlight_L": (0.78, L0-0.05, 0.65), "bus_headlight_R": (-0.78, L0-0.05, 0.65),
                  "bus_cabin_light": (0, 0, ROOFB-0.3)}.items():
    e = bpy.data.objects.new(name, None); e.location = loc; e.parent = root; e.empty_display_size = 0.2; col.objects.link(e)
print("tris", tris)
