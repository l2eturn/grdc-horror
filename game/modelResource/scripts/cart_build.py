# The old woman's junk cart (รถเข็นเก็บของเก่า, after the user's photos): rusty steel-tube frame with wire-mesh sides,
# two big spoked wheels in the middle, long handles, piled high with sacks, flattened cardboard and plastic bags;
# one wheel sunk in a mud patch. Run inside Blender (grandma_source.blend). Builds textures cart_* (64-128 px),
# the "Cart" collection and the clips:
#   stuck  (loop)  the body rocks in the rut, left wheel down in the mud
#   free   (once)  the body rolls back level, wheels turn a little: it has been pushed out
# Origin = the ground under the handle grips, the cart points -Y (= +Z in the game) away from whoever pushes it.
# Nodes: cart (root) > cart_mud (static), cart_body (rocks; pivot on the right wheel's ground contact) > cart_wheel_L/R.
# Wire mesh and spokes are alpha-cutout materials (glTF MASK), single-sided with back faces built in. Export: cart_export.py.
import bpy, bmesh, math, numpy as np
from mathutils import Vector as V, Matrix, Euler

OUT = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/textures/"


# ---------------------------------------------------------------- textures
def fractal(N, seed, ax=1.0, ay=1.0, beta=1.8):
    r = np.random.default_rng(seed); w = r.standard_normal((N, N))
    f = np.fft.fftfreq(N)*N; fu, fv = np.meshgrid(f, f)
    k = np.sqrt((fu/ax)**2 + (fv/ay)**2); amp = np.where(k >= 1, np.maximum(k, 1e-6)**(-beta), 0)
    n = np.real(np.fft.ifft2(np.fft.fft2(w)*amp)); return (n-n.mean())/(n.std()+1e-9)


def write_png(path, rgba):
    """straight-alpha RGBA PNG written directly (Blender's own save turns fully transparent texels white, which then
    bleeds into the wires through the texture filtering)"""
    import zlib, struct
    H, W = rgba.shape[:2]
    b = (np.clip(rgba[::-1], 0, 1)*255 + 0.5).astype(np.uint8)          # Blender rows go bottom-up
    raw = b"".join(b"\x00" + b[y].tobytes() for y in range(H))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def save(name, rgb, alpha=None):
    H, W = rgb.shape[:2]
    if alpha is not None:
        path = OUT + name + ".png"
        write_png(path, np.dstack([rgb, alpha]))
        old = bpy.data.images.get(name)
        if old: bpy.data.images.remove(old)
        im = bpy.data.images.load(path); im.name = name; im.alpha_mode = "STRAIGHT"; im.pack()
        return
    im = bpy.data.images.get(name) or bpy.data.images.new(name, W, H)
    if im.size[0] != W: im.scale(W, H)
    px = np.ones((H, W, 4), np.float32); px[..., :3] = np.clip(rgb, 0, 1)
    im.pixels[:] = px.ravel()
    im.filepath_raw = OUT + name + ".png"; im.file_format = "PNG"; im.save(); im.pack()


def grid(N):
    u = np.tile((np.arange(N)+0.5)/N, (N, 1)); return u, u.T


def ss(e0, e1, x): t = np.clip((x-e0)/(e1-e0), 0, 1); return t*t*(3-2*t)


def rust(N, seed):
    base = np.array((0.30, 0.17, 0.10)) * (1 + 0.25*fractal(N, seed, beta=1.4))[..., None]
    paint = np.clip(fractal(N, seed+1, beta=1.6) - 0.6, 0, 1)[..., None]           # a little old dark paint left
    return base*(1-paint) + np.array((0.12, 0.11, 0.10))*paint


N = 128
u, v = grid(N)
save("cart_metal", rust(64, 80))
# wire mesh: 5 cm squares (tile 0.5 m = 10 cells), rusty wire, holes cut out
cell = np.minimum((u*10) % 1, (v*10) % 1)
wire = cell < 0.16
save("cart_mesh", rust(N, 82)*0.9, alpha=wire.astype(np.float32))
# wheel side: tyre ring, steel rim, 16 spokes, hub; everything else cut out (planar UV, 1 = the wheel's diameter)
x, y = u - 0.5, v - 0.5
d = np.sqrt(x*x + y*y)*2; ang = np.arctan2(y, x)
tyre = (d > 0.86) & (d < 1.0)
rim = (d > 0.80) & (d <= 0.86)
spokes = (np.abs(np.sin(ang*8))*d*np.pi*0.5 < 0.02) & (d < 0.80)
hub = d < 0.10
wcol = np.ones((N, N, 3))*(0.06, 0.06, 0.06)*(1 + 0.2*fractal(N, 83))[..., None]
wcol = np.where((rim | spokes | hub)[..., None], rust(N, 84), wcol)
save("cart_wheel", wcol, alpha=(tyre | rim | spokes | hub).astype(np.float32))
save("cart_tyre", np.array((0.07, 0.07, 0.07)) * (1 + 0.3*np.sin(u*64*np.pi)*0.5 + 0.1*fractal(N, 85))[..., None])
# woven plastic sacks (กระสอบ), off-white and dirty, a faded red print
sack = np.array((0.66, 0.63, 0.56)) * (1 + 0.05*np.sin(u*128*np.pi)*np.sin(v*128*np.pi))[..., None]
sack *= (1 - 0.5*np.clip(fractal(N, 86, beta=1.5) + 0.5, 0, 3)/3)[..., None]
print_ = (np.abs(v - 0.55) < 0.06) & (np.abs(u - 0.5) < 0.3) & (((u*40) % 1) < 0.6)
sack = np.where(print_[..., None], sack*np.array((0.9, 0.55, 0.5)), sack)
save("cart_sack", sack)
# flattened cardboard: brown with corrugation lines and darker damp patches
card = np.array((0.55, 0.40, 0.25)) * (1 + 0.05*np.sin(u*96*np.pi))[..., None]
card *= (1 - 0.3*np.clip(fractal(N, 87, beta=1.6), 0, 3)/3)[..., None]
save("cart_card", card)
# plastic bags: 2x2 atlas (white, black, orange, pink), crinkled
cols = [(0.82, 0.82, 0.80), (0.06, 0.06, 0.07), (0.85, 0.40, 0.10), (0.85, 0.55, 0.65)]
bag = np.zeros((N, N, 3))
for i, c in enumerate(cols):
    m = ((u >= (i % 2)*0.5) & (u < (i % 2)*0.5 + 0.5) & (v >= (i//2)*0.5) & (v < (i//2)*0.5 + 0.5))
    bag[m] = c
bag *= (1 + 0.25*fractal(N, 88, beta=1.2))[..., None]
save("cart_bags", bag)
# mud: dark wet centre, lighter dried rim, wheel ruts across it (planar UV, tile = the patch, 2.8 m)
d2 = np.sqrt((u-0.5)**2 + (v-0.5)**2)*2
mud = np.array((0.16, 0.12, 0.08)) * (1 + 0.12*fractal(N, 89))[..., None]
rr = ss(0.55, 0.95, d2)
mud = mud*(1-rr[..., None]) + np.array((0.33, 0.27, 0.20))*(1 + 0.1*fractal(N, 90))[..., None]*rr[..., None]
mud *= (1 + 0.25*np.clip(fractal(N, 91, ax=4, ay=0.5), -1, 3)/3)[..., None]
for xx in (0.34, 0.66):
    mud *= (1 - 0.35*(1 - ss(0.0, 0.025, abs(u - xx))))[..., None]
save("cart_mud", mud)

# ---------------------------------------------------------------- materials (texture x vertex AO; cutouts are double-sided)
TEX = {"metal": "cart_metal", "mesh": "cart_mesh", "wheel": "cart_wheel", "tyre": "cart_tyre", "sack": "cart_sack",
       "card": "cart_card", "bags": "cart_bags", "mud": "cart_mud"}
CUTOUT = {"mesh", "wheel"}
MN = list(TEX)
MATS = []
for k in MN:
    m = bpy.data.materials.get("cart_" + k) or bpy.data.materials.new("cart_" + k)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        if n.type not in ("BSDF_PRINCIPLED", "OUTPUT_MATERIAL"): nt.nodes.remove(n)
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Roughness"].default_value = 0.85
    t = nt.nodes.new("ShaderNodeTexImage"); t.image = bpy.data.images[TEX[k]]
    vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "AO"
    mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"
    mx.inputs["Factor"].default_value = 1
    nt.links.new(t.outputs["Color"], mx.inputs[6]); nt.links.new(vc.outputs["Color"], mx.inputs[7])
    nt.links.new(mx.outputs[2], b.inputs["Base Color"])
    if k in CUTOUT:   # texture alpha -> Round -> Alpha: the glTF exporter writes alphaMode MASK
        rd = nt.nodes.new("ShaderNodeMath"); rd.operation = "ROUND"
        nt.links.new(t.outputs["Alpha"], rd.inputs[0]); nt.links.new(rd.outputs[0], b.inputs["Alpha"])
        try: m.surface_render_method = "DITHERED"
        except Exception: pass
    # single-sided everywhere: the engine treats double-sided + alpha-test as foliage (light wrapped round, so the
    # wires glowed white in the torch); the cutout panels get a second, flipped copy of their faces instead
    m.use_backface_culling = True
    MATS.append(m)
MI = {k: i for i, k in enumerate(MN)}

col = bpy.data.collections.get("Cart")
if col:
    for o in list(col.objects): bpy.data.objects.remove(o, do_unlink=True)
else:
    col = bpy.data.collections.new("Cart"); bpy.context.scene.collection.children.link(col)


class Part:
    def __init__(s):
        s.bm = bmesh.new(); s.uv = s.bm.loops.layers.uv.new("UVMap")


P = {n: Part() for n in ("body", "wheel_L", "wheel_R", "mud")}


def add_bm(part, b, mat, uvf=None):
    me = bpy.data.meshes.new("tmp"); b.to_mesh(me); b.free()
    n0 = len(part.bm.faces); part.bm.from_mesh(me); bpy.data.meshes.remove(me)
    part.bm.faces.ensure_lookup_table(); part.bm.normal_update()
    new = list(part.bm.faces)[n0:]
    for f in new:
        f.material_index = MI[mat]
        for l in f.loops: l[part.uv].uv = uvf(l.vert.co, f) if uvf else (0, 0)
    return new


def tube(part, pts, r, mat, sides=6):
    """steel tube along a polyline"""
    b = bmesh.new(); pts = [V(p) for p in pts]; rings = []
    for i, p in enumerate(pts):
        t = (pts[min(i+1, len(pts)-1)] - pts[max(i-1, 0)]).normalized()
        X = t.orthogonal().normalized(); Y = t.cross(X)
        rings.append([b.verts.new(p + (X*math.cos(2*math.pi*k/sides) + Y*math.sin(2*math.pi*k/sides))*r) for k in range(sides)])
    for i in range(len(rings)-1):
        for k in range(sides):
            b.faces.new([rings[i][k], rings[i][(k+1) % sides], rings[i+1][(k+1) % sides], rings[i+1][k]])
    add_bm(part, b, mat, lambda c, f: (c.x*3 + c.y*3, c.z*3))


def quad(part, a, b_, c, d, mat, tile=0.5):
    """a flat panel (wire mesh), UVs in meters along its edges; both sides as separate faces (see CUTOUT)"""
    bm = bmesh.new()
    for pts in ((a, b_, c, d), (d, c, b_, a)):
        bm.faces.new([bm.verts.new(V(p)) for p in pts])
    a, b_, d = V(a), V(b_), V(d); eu = (b_-a).normalized(); ev = (d-a).normalized()
    add_bm(part, bm, mat, lambda p, f: ((p-a).dot(eu)/tile, (p-a).dot(ev)/tile))


def blob(part, c, r, mat, rot=0.0, flat=0.4, us=10, vs=6, uvoff=(0, 0), uvs=1.0, seed=0):
    """lumpy sack / bag: squashed sphere, flat underneath"""
    rng = np.random.default_rng(seed)
    b = bmesh.new(); bmesh.ops.create_uvsphere(b, u_segments=us, v_segments=vs, radius=1.0)
    R = Matrix.Rotation(math.radians(rot), 3, "Z")
    for vt in b.verts:
        p = vt.co.copy()
        if p.z < -flat: p.z = -flat + (p.z + flat)*0.2
        p *= 1 + 0.08*rng.standard_normal()
        vt.co = V(c) + R @ V((p.x*r[0], p.y*r[1], p.z*r[2]))
    add_bm(part, b, mat, lambda q, f: (uvoff[0] + ((q.x + q.y)*uvs) % 0.5, uvoff[1] + (q.z*uvs*1.5) % 0.5))


def slab(part, c, size, rot, mat):
    b = bmesh.new(); bmesh.ops.create_cube(b, size=1.0)
    M = Matrix.Translation(V(c)) @ Euler([math.radians(x) for x in rot]).to_matrix().to_4x4() @ Matrix.Diagonal((*size, 1))
    bmesh.ops.transform(b, matrix=M, verts=b.verts)
    add_bm(part, b, mat, lambda q, f: (q.x*1.5 + q.z*1.5, q.y*1.5 + q.z*0.5))


# ---------------------------------------------------------------- body: basket frame, wire panels, handles, legs, axle
Y0, Y1, Z0, Z1, HW = -0.40, -1.70, 0.40, 0.88, 0.36        # basket: back, front, bottom, top, half width
AX_Y, AX_Z, WR = -1.12, 0.34, 0.34                          # axle and wheel radius
B = P["body"]
for z in (Z0, Z1):                                          # bottom and top rings
    tube(B, [(-HW, Y0, z), (HW, Y0, z), (HW, Y1, z), (-HW, Y1, z), (-HW, Y0, z)], 0.013, "metal")
for y in np.linspace(Y0, Y1, 5):                            # posts
    for s in (-1, 1): tube(B, [(s*HW, y, Z0), (s*HW, y, Z1)], 0.011, "metal")
for xx in (-HW/2, 0, HW/2): tube(B, [(xx, Y0, Z0), (xx, Y1, Z0)], 0.011, "metal")   # floor slats
for s in (-1, 1):
    quad(B, (s*HW, Y0, Z0), (s*HW, Y1, Z0), (s*HW, Y1, Z1), (s*HW, Y0, Z1), "mesh")   # sides
    tube(B, [(s*HW, Y0, Z1 - 0.02), (s*0.33, -0.15, 0.86), (s*0.31, 0.02, 0.84)], 0.015, "metal")   # handles
    tube(B, [(s*HW, Y0 - 0.02, Z0), (s*HW, Y0 + 0.02, 0.0)], 0.013, "metal")        # back legs (the stand)
    tube(B, [(s*HW, AX_Y - 0.12, Z0), (s*(HW + 0.03), AX_Y, AX_Z), (s*HW, AX_Y + 0.12, Z0)], 0.012, "metal")  # wheel forks
for y in (Y0, Y1): quad(B, (-HW, y, Z0), (HW, y, Z0), (HW, y, Z1), (-HW, y, Z1), "mesh")   # ends
quad(B, (-HW, Y0, Z0), (HW, Y0, Z0), (HW, Y1, Z0), (-HW, Y1, Z0), "mesh")                  # floor
tube(B, [(-0.31, 0.02, 0.84), (0.31, 0.02, 0.84)], 0.017, "metal")                      # grip bar
tube(B, [(-(HW + 0.06), AX_Y, AX_Z), (HW + 0.06, AX_Y, AX_Z)], 0.012, "metal")         # axle
# the load, piled over the top: sacks, cardboard, bags (bags atlas: 0 white, 1 black, 2 orange, 3 pink)
AT = [(0, 0), (0.5, 0), (0, 0.5), (0.5, 0.5)]
blob(B, (0.0, -1.00, 0.70), (0.34, 0.60, 0.30), "sack", rot=4, seed=1)
blob(B, (-0.05, -0.85, 1.12), (0.36, 0.42, 0.30), "sack", rot=-15, seed=2)
blob(B, (0.10, -1.40, 1.05), (0.30, 0.34, 0.26), "sack", rot=25, seed=3)
blob(B, (0.05, -0.55, 0.98), (0.26, 0.20, 0.20), "bags", seed=4, uvoff=AT[1])
blob(B, (0.24, -1.20, 1.30), (0.14, 0.14, 0.13), "bags", seed=5, uvoff=AT[2])
blob(B, (-0.22, -1.45, 1.22), (0.13, 0.16, 0.12), "bags", seed=6, uvoff=AT[0])
blob(B, (-0.25, -1.10, 1.32), (0.12, 0.12, 0.10), "bags", seed=7, uvoff=AT[3])
for i, (y, z, rx, rz) in enumerate(((-1.25, 1.36, 6, 12), (-1.22, 1.40, -4, -8), (-1.30, 1.43, 3, 30))):
    slab(B, (-0.02 + 0.03*i, y, z), (0.62, 0.48, 0.02), (rx, 4, rz), "card")       # flattened boxes on top

# ---------------------------------------------------------------- wheels: spoked disc (cutout) + rubber tyre ring
for s, side in ((1, "L"), (-1, "R")):
    Wp = P["wheel_" + side]
    cx = s*(HW + 0.07)
    bm = bmesh.new()
    n = 20
    c0 = bm.verts.new((cx, AX_Y, AX_Z))
    ring = [bm.verts.new((cx, AX_Y + WR*math.cos(2*math.pi*k/n), AX_Z + WR*math.sin(2*math.pi*k/n))) for k in range(n)]
    for k in range(n): bm.faces.new([c0, ring[k], ring[(k+1) % n]])
    c1 = bm.verts.new((cx, AX_Y, AX_Z))                     # the back of the disc: its own faces (see CUTOUT)
    ring1 = [bm.verts.new(v.co) for v in ring]
    for k in range(n): bm.faces.new([c1, ring1[(k+1) % n], ring1[k]])
    add_bm(Wp, bm, "wheel", lambda p, f: (0.5 + (p.y - AX_Y)/(2*WR), 0.5 + (p.z - AX_Z)/(2*WR)))
    bm = bmesh.new(); rings = []
    for xo, rr in ((-0.022, WR*0.92), (-0.022, WR), (0.022, WR), (0.022, WR*0.92)):
        rings.append([bm.verts.new((cx + xo, AX_Y + rr*math.cos(2*math.pi*k/n), AX_Z + rr*math.sin(2*math.pi*k/n))) for k in range(n)])
    for i in range(3):
        for k in range(n):
            bm.faces.new([rings[i][k], rings[i][(k+1) % n], rings[i+1][(k+1) % n], rings[i+1][k]])
    add_bm(Wp, bm, "tyre", lambda p, f: (math.atan2(p.z - AX_Z, p.y - AX_Y)*2, p.x*8))

# ---------------------------------------------------------------- mud: an irregular flat patch around the left wheel
M = P["mud"]
rng = np.random.default_rng(92)
n = 18
MC = V((0.25, AX_Y, 0.006))
cen = M.bm.verts.new(MC)
rim = []
for k in range(n):
    a = 2*math.pi*k/n
    r = 1.0 + 0.15*rng.standard_normal()
    rim.append(M.bm.verts.new((MC.x + 1.1*r*math.cos(a), MC.y + 1.3*r*math.sin(a), 0.006)))
for k in range(n):
    f = M.bm.faces.new([cen, rim[k], rim[(k+1) % n]]); f.material_index = MI["mud"]
    for l in f.loops: l[M.uv].uv = (0.5 + (l.vert.co.x - MC.x)/2.8, 0.5 + (l.vert.co.y - MC.y)/2.8)

# ---------------------------------------------------------------- objects
CONTACT = V((-(HW + 0.07), AX_Y, 0.0))        # the right wheel's ground contact: the body rolls about it
PIV = {"body": CONTACT, "wheel_L": V((HW + 0.07, AX_Y, AX_Z)), "wheel_R": V((-(HW + 0.07), AX_Y, AX_Z)), "mud": V()}
PARENT = {"body": None, "mud": None, "wheel_L": "body", "wheel_R": "body"}
root = bpy.data.objects.get("cart") or bpy.data.objects.new("cart", None)
if root.name not in col.objects: col.objects.link(root)
OBJ, tris = {}, 0
for name, part in P.items():
    old = bpy.data.meshes.get("cart_" + name)
    if old: bpy.data.meshes.remove(old)
    me = bpy.data.meshes.new("cart_" + name)
    # the two-faced cutout panels keep the winding they were built with
    bmesh.ops.recalc_face_normals(part.bm, faces=[f for f in part.bm.faces if MN[f.material_index] not in CUTOUT])
    part.bm.to_mesh(me); part.bm.free()
    for m in MATS: me.materials.append(m)
    me.transform(Matrix.Translation(-PIV[name]))
    a = me.color_attributes.new("AO", "BYTE_COLOR", "CORNER")
    for d_ in a.data: d_.color = (1, 1, 1, 1)
    o = bpy.data.objects.new("cart_" + name, me); col.objects.link(o)
    o.rotation_mode = "XYZ"
    OBJ[name] = o
    tris += sum(len(p.vertices)-2 for p in me.polygons)
for name, o in OBJ.items():
    par = PARENT[name]
    o.parent = OBJ[par] if par else root
    o.location = PIV[name] - (PIV[par] if par else V())

# ---------------------------------------------------------------- clips
STUCK_ROLL = 9.0      # left side down: the left wheel sits ~0.12 m into the mud
for o in OBJ.values():
    ad = o.animation_data or o.animation_data_create(); ad.action = None
    for t in list(ad.nla_tracks): ad.nla_tracks.remove(t)
for a in list(bpy.data.actions):
    if a.name.startswith("cart|"): bpy.data.actions.remove(a)
CLIPS = {
    # frame: (body roll deg, body pitch deg, wheel turn deg)
    "stuck": [(0, (STUCK_ROLL, 2, 0)), (20, (STUCK_ROLL - 1.5, 1, 6)), (34, (STUCK_ROLL, 2, 0)), (70, (STUCK_ROLL - 1.0, 1.5, 4)),
              (90, (STUCK_ROLL, 2, 0))],
    "free": [(0, (STUCK_ROLL, 2, 0)), (12, (STUCK_ROLL - 3, 0, 15)), (30, (1.0, -1, 70)), (40, (0, 0, 90))],
}
for clip, keys in CLIPS.items():
    for name in ("body", "wheel_L", "wheel_R"):
        o = OBJ[name]
        act = bpy.data.actions.new(f"cart|{clip}|{name}")
        o.animation_data.action = act
        for frame, (roll, pitch, turn) in keys:
            o.rotation_euler = (math.radians(pitch), math.radians(roll), 0) if name == "body" else (math.radians(-turn), 0, 0)
            o.keyframe_insert("rotation_euler", frame=frame)
        o.animation_data.action = None
        tr = o.animation_data.nla_tracks.new(); tr.name = clip
        tr.strips.new(clip, 0, act); tr.mute = True
for o in OBJ.values(): o.rotation_euler = (0, 0, 0)
OBJ["body"].rotation_euler = (math.radians(2), math.radians(STUCK_ROLL), 0)
print("cart tris", tris)
