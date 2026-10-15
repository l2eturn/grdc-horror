# The ghost dancer (ผีนางรำ, after the user's references: a dead classical dancer in tarnished costume).
# v2 (scarier): corpse-grey skin with bruises, a grin cut from cheek to cheek and sockets that bleed black down the
# face, a neck stretched too long, long wet black hair hanging from under the crown (strands over the face too),
# bony arms with longer forearms and long black-gold claws, the skirt hem torn into hanging strips. Run inside
# Blender after ps2kit.py. Same jointed rig as the companion / old woman (no skinning): Z up, faces -Y (= +Z in the
# game), feet on z = 0, ~1.62 m to the top of the head, the crown's spike to ~2.0 m.
#   dancer > dancer_torso (hips) > dancer_head (neck: face, crown, flower), dancer_upperarm_L/R > dancer_forearm_L/R
#   (bare arms, bracelets, hands bent back with long gold nails), dancer_thigh_L/R > dancer_shin_L/R (the skirt)
# Clips: dancer_anims.py. Export: bake_and_export(bpy.data.collections["Dancer"], "dancer").
import bpy, bmesh, math, numpy as np
from mathutils import Vector as V, Matrix

PFX = "dancer_"

# ---------------------------------------------------------------- textures
N = 128
u, v = grid(N)
# bodice: red brocade, a gold lattice of diamonds with a flower in each, worn and dirty
lat = np.minimum(np.abs(((u*8 + v*8) % 1) - 0.5), np.abs(((u*8 - v*8) % 1) - 0.5))
d = np.sqrt((((u*8 + v*8) % 1) - 0.5)**2 + (((u*8 - v*8) % 1) - 0.5)**2)
brocade = np.ones((N, N, 3))*(0.42, 0.07, 0.07)
brocade = np.where((lat < 0.06)[..., None], (0.62, 0.46, 0.18), brocade)
brocade = np.where((d < 0.12)[..., None], (0.70, 0.55, 0.22), brocade)
brocade *= (1 + 0.12*fractal(N, 130))[..., None]
brocade *= (1 - 0.4*np.clip(fractal(N, 131, beta=1.4), 0, 3)/3)[..., None]
brocade *= (1 - 0.55*ss(0.6, 1.6, fractal(N, 150, beta=1.6)))[..., None]                       # dark stains
save("dancer_brocade", brocade)
# skirt: gold-green brocade, vertical pattern bands, a gold border low down
band = (u*6) % 1
skirt = np.ones((N, N, 3))*(0.20, 0.22, 0.11)
skirt = np.where(((band > 0.42) & (band < 0.58))[..., None], (0.55, 0.45, 0.18), skirt)
dots = (np.sin(u*48*np.pi)*np.sin(v*48*np.pi)) > 0.6
skirt = np.where(dots[..., None], skirt*1.5, skirt)
skirt = np.where((((v + 0.02) % 1.0) < 0.04)[..., None], (0.62, 0.48, 0.18), skirt)          # one border band
skirt *= (1 + 0.1*fractal(N, 132))[..., None] * (1 - 0.35*np.clip(fractal(N, 133, beta=1.4), 0, 3)/3)[..., None]
skirt *= (1 - 0.5*ss(0.5, 1.5, fractal(N, 151, beta=1.6)))[..., None] * (0.55 + 0.45*v)[..., None]   # mud up the hem
save("dancer_skirt", skirt)
# gold: chased metal at 128 px: fine horizontal ridges, a small diamond lattice punched in, black-green tarnish
# settled in the grooves and in broad soft patches (old stage costume, years in the damp)
uu, vv = grid(128)
ridge = 0.5 + 0.5*np.cos(vv*2*np.pi*16)
lat = np.minimum(np.abs(((uu*16 + vv*16) % 1) - 0.5), np.abs(((uu*16 - vv*16) % 1) - 0.5))
groove = np.maximum(ss(0.25, 0.0, ridge)*0.6, 0.35*ss(0.05, 0.015, lat))                          # 1 in the grooves
gold = np.array((0.50, 0.37, 0.15)) * (1 + 0.15*fractal(128, 134, beta=2.0))[..., None]
gold *= (1 + 0.18*ss(0.6, 1.0, ridge))[..., None]                                                # ridge tops catch light
tarn = np.clip(0.5*groove + 0.8*ss(-0.2, 1.2, fractal(128, 135, beta=2.2)), 0, 1)
gold = gold*(1 - 0.75*tarn[..., None]) + np.array((0.06, 0.07, 0.04))*0.75*tarn[..., None]
save("dancer_gold", gold)
uu, vv = grid(64)
sabai = np.where((np.abs(uu - 0.5) > 0.4)[..., None], (0.62, 0.48, 0.2), (0.12, 0.30, 0.18))
save("dancer_sabai", sabai * (1 + 0.12*fractal(64, 136))[..., None])
SKIN = np.array((0.40, 0.43, 0.42))                                                            # corpse grey-blue
sk = np.ones((64, 64, 3))*SKIN*(1 + 0.10*fractal(64, 137))[..., None]
sk = sk*(1 - 0.55*np.clip(fractal(64, 138, ax=0.3, ay=3, beta=1.2) - 1.0, 0, 2)[..., None]/2)    # dark veins
sk = sk*(1 - 0.35*ss(0.8, 1.8, fractal(64, 152, beta=1.8)))[..., None] + np.array((0.06, 0.0, 0.05))*ss(0.8, 1.8, fractal(64, 152, beta=1.8))[..., None]   # bruises
save("dancer_skin", sk)
save("dancer_hair", np.array((0.04, 0.035, 0.035)) * (1 + 0.3*fractal(64, 139, ax=6, ay=0.4))[..., None])
# hair card: wet clumped strands running along v (v = 1 at the root, 0 at the tips), thinning into ragged ends and
# ragged at the sides so overlapping cards don't show straight edges
hu, hv = np.meshgrid((np.arange(64) + 0.5)/64, (np.arange(128) + 0.5)/128)
strands = fractal(128, 161, ax=3, ay=0.25)[:, ::2]                                               # 128 x 64, streaks along v
clump = fractal(128, 162, ax=3, ay=0.5)[:, ::2]
thr = -1.9 + 2.6*ss(0.6, 0.0, hv) + 2.4*ss(0.32, 0.5, np.abs(hu - 0.5)) - 0.5*clump
hair_a = (strands > thr).astype(float)
hair_rgb = np.array((0.035, 0.03, 0.03)) * (1 + 0.9*np.clip(strands - 0.8, 0, 1.5))[..., None]  # wet glints on clumps
save("dancer_hair_card", hair_rgb, hair_a)
save("dancer_flower", np.array((0.55, 0.05, 0.08)) * (1 + 0.35*fractal(64, 140, beta=1.0))[..., None])
# face (same layout as the other faces: eyes u 0.5 +- 0.165, v 0.51; mouth v 0.26): cracked white make-up over grey
# rotten skin, black sockets with tiny pale pupils (they glow), black tears, dark red lips in a thin smile
N = 256
u, v = grid(N)
def ell(cu, cv, ru, rv, soft=0.25):
    dd = np.sqrt(((u-cu)/ru)**2 + ((v-cv)/rv)**2); return 1 - ss(1-soft, 1+soft, dd)
def mix(img, col, m): return img*(1-m[..., None]) + np.array(col)*m[..., None]
def line(img, p0, p1, w, col, a):
    p0, p1 = np.array(p0), np.array(p1); dd = p1 - p0; L2 = dd @ dd
    t = np.clip(((u-p0[0])*dd[0] + (v-p0[1])*dd[1]) / L2, 0, 1)
    dist = np.sqrt((u - p0[0] - t*dd[0])**2 + (v - p0[1] - t*dd[1])**2)
    return mix(img, col, a*(1 - ss(0.3*w, w, dist)))
r = np.random.default_rng(141)
img = np.ones((N, N, 3))*SKIN*(1 + 0.06*fractal(N, 142))[..., None]
makeup = np.clip(0.75 + 0.6*fractal(N, 143, beta=1.3), 0, 1)                                    # flaking white paint
img = mix(img, (0.82, 0.80, 0.74), 0.85*makeup*ell(0.5, 0.42, 0.36, 0.42, 0.3))
for _ in range(40):                                                                            # cracks in the paint
    p0 = np.array((0.2 + 0.6*r.random(), 0.1 + 0.75*r.random())); a = r.random()*np.pi*2
    pts = [p0]
    for k in range(4):
        a += 0.8*r.standard_normal(); pts.append(pts[-1] + 0.03*np.array((np.cos(a), np.sin(a))))
    for q0, q1 in zip(pts, pts[1:]): img = line(img, q0, q1, 0.004, SKIN*0.45, 0.8)
for s in (-1, 1):
    cu = 0.5 + s*0.165
    img = mix(img, (0.03, 0.02, 0.02), ell(cu, 0.51, 0.085, 0.06, 0.35))                      # black sockets
    img = mix(img, (0.85, 0.85, 0.80), ell(cu, 0.505, 0.014, 0.014, 0.3))                     # pinpoint pupils
    img = mix(img, (0.10, 0.04, 0.05), 0.8*ell(cu, 0.50, 0.12, 0.085, 0.5))                   # bruised hollows
    img = mix(img, (0.0, 0.0, 0.0), ell(cu, 0.51, 0.095, 0.07, 0.3))                          # deeper sockets
    img = mix(img, (0.92, 0.92, 0.86), ell(cu, 0.505, 0.016, 0.016, 0.3))
    for k, (dx, w, end) in enumerate(((-0.03, 0.016, 0.08), (0.0, 0.011, 0.16), (0.035, 0.009, 0.24))):   # black blood running down
        img = line(img, (cu + dx*s, 0.47), (cu + (dx*1.3 - 0.01)*s, end), w, (0.02, 0.0, 0.0), 0.9)
    img = line(img, (cu - 0.08*s, 0.60), (cu + 0.08*s, 0.585), 0.008, (0.05, 0.04, 0.04), 0.9)  # painted brows, long
for s in (-1, 1): img = mix(img, (0.22, 0.13, 0.1), 0.6*ell(0.5+s*0.028, 0.415, 0.013, 0.007, 0.4))
img = mix(img, (0.35, 0.03, 0.05), ell(0.5, 0.262, 0.07, 0.016, 0.3))                         # dark red lips
# the grin: cut from cheek to cheek, curving up, black inside with a row of small teeth
pts = [(0.5 + x, 0.262 + 0.9*x*x + (0.004 if abs(x) > 0.05 else 0)) for x in np.linspace(-0.2, 0.2, 21)]
for q0, q1 in zip(pts, pts[1:]): img = line(img, q0, q1, 0.016, (0.22, 0.0, 0.02), 0.9)
for q0, q1 in zip(pts, pts[1:]): img = line(img, q0, q1, 0.008, (0.02, 0.0, 0.0), 1.0)
for x in np.linspace(-0.09, 0.09, 13): img = mix(img, (0.75, 0.70, 0.55), 0.9*ell(0.5 + x, 0.266 + 0.9*x*x, 0.004, 0.006, 0.3))
for s in (-1, 1): img = line(img, (0.5 + s*0.19, 0.30), (0.5 + s*0.17, 0.12), 0.006, (0.25, 0.0, 0.02), 0.8)   # blood from the cuts
save("dancer_face", img)

TEX = {k: "dancer_" + k for k in ("brocade", "skirt", "gold", "sabai", "skin", "hair", "flower", "face")}
TEX["hair"] = "dancer_hair_card"          # cutout strand cards (single-sided, back faces built in)
TEX["hairbase"] = "dancer_hair"           # solid black: the hair mass at the nape, the nails
TEX["eyes_glow"] = "dancer_skin"
MATS, MI = make_materials(PFX, TEX, cutout=("hair",), emissive={"eyes_glow": (0.85, 0.9, 0.85)})
col = collection("Dancer")


class Part:
    def __init__(s):
        s.bm = bmesh.new(); s.uv = s.bm.loops.layers.uv.new("UVMap")


P = {n: Part() for n in ("torso", "head", "upperarm_L", "upperarm_R", "forearm_L", "forearm_R",
                         "thigh_L", "thigh_R", "shin_L", "shin_R")}


def loft(part, secs, sides, matf, tile=0.5, cap0=False, cap1=False):
    bm, uvl = part.bm, part.uv
    rings = []
    for c, X, Y, rx, ryf, ryb in secs:
        c, X, Y = V(c), V(X).normalized(), V(Y).normalized()
        ring = []
        for k in range(sides):
            a = 2*math.pi*k/sides
            ry = ryf if math.cos(a) > 0 else ryb
            ring.append(bm.verts.new(c + X*rx*math.sin(a) - Y*ry*math.cos(a)))
        rings.append((ring, c, max(rx, ryf)))
    vacc = [0.0]
    for i in range(1, len(rings)): vacc.append(vacc[-1] + (rings[i][1]-rings[i-1][1]).length)
    for i in range(len(rings)-1):
        r0, r1 = rings[i][0], rings[i+1][0]
        circ = 2*math.pi*rings[i][2]
        for k in range(sides):
            k1 = (k+1) % sides
            f = bm.faces.new([r0[k], r0[k1], r1[k1], r1[k]])
            f.material_index = MI[matf(i, 2*math.pi*(k+0.5)/sides)]
            for l, (kk, ii) in zip(f.loops, ((k, i), (k+1, i), (k+1, i+1), (k, i+1))):
                l[uvl].uv = (kk/sides*circ/tile, vacc[ii]/tile)
    for cap, idx in ((cap0, 0), (cap1, -1)):
        if not cap: continue
        ring = rings[idx][0]
        f = bm.faces.new(ring if idx == -1 else ring[::-1])
        f.material_index = MI[matf(0 if idx == 0 else len(rings)-2, math.pi)]
        for l in f.loops: l[uvl].uv = (l.vert.co.x/tile, l.vert.co.y/tile)


def H(z, rx, ryf, ryb=None, cy=0.0, cx=0.0):
    return ((cx, cy, z), (1, 0, 0), (0, 1, 0), rx, ryf, ryb if ryb is not None else ryf)


def tube(part, pts, radii, sides, mat, tile=0.5, side=V((1, 0, 0)), cap1=False):
    secs = []
    for i, p in enumerate(pts):
        t = (pts[min(i+1, len(pts)-1)] - pts[max(i-1, 0)]).normalized()
        X = (side - t*side.dot(t)).normalized()
        Y = t.cross(X)
        if Y.y < 0: Y = -Y
        r = radii[i]; rx, ry = r if isinstance(r, tuple) else (r, r)
        secs.append((p, X, Y, rx, ry, ry))
    loft(part, secs, sides, (lambda i, a: mat) if isinstance(mat, str) else mat, tile, cap1=cap1)


def card(part, pts, widths, side, mat):
    """a hanging strip (hair) as quads, width across `side`, u 0..1 across, v 1 at pts[0] -> 0 at the end; built
    twice with opposite winding (cutouts must be single-sided in terEngine)"""
    bm, uvl = part.bm, part.uv
    L = [0.0]
    for a, b in zip(pts, pts[1:]): L.append(L[-1] + (b - a).length)
    for flip in (False, True):
        rows = []
        for i, p in enumerate(pts):
            t = (pts[min(i+1, len(pts)-1)] - pts[max(i-1, 0)]).normalized()
            X = (side - t*side.dot(t)).normalized()*widths[i]*0.5
            rows.append((bm.verts.new(p - X), bm.verts.new(p + X), 1 - L[i]/L[-1]))
        for (a0, a1, v0), (b0, b1, v1) in zip(rows, rows[1:]):
            q = [(a0, (0, v0)), (a1, (1, v0)), (b1, (1, v1)), (b0, (0, v1))]
            if flip: q = q[::-1]
            f = bm.faces.new([vv for vv, _ in q]); f.material_index = MI[mat]
            for l, (_, uv) in zip(f.loops, q): l[uvl].uv = uv


def add_sphere(part, us, vs, shape, mat, uvf, drop=None):
    b = bmesh.new(); bmesh.ops.create_uvsphere(b, u_segments=us, v_segments=vs, radius=1.0)
    unit = {vtx: vtx.co.copy() for vtx in b.verts}
    if drop:
        gone = [f for f in b.faces if drop(f.calc_center_median())]
        if gone: bmesh.ops.delete(b, geom=gone, context="FACES")
    uvl = b.loops.layers.uv.new("UVMap")
    for f in b.faces:
        for l in f.loops: l[uvl].uv = uvf(unit[l.vert])
    for vtx in b.verts: vtx.co = shape(unit[vtx])
    me = bpy.data.meshes.new("tmp"); b.to_mesh(me); b.free()
    n0 = len(part.bm.faces); part.bm.from_mesh(me); bpy.data.meshes.remove(me)
    part.bm.faces.ensure_lookup_table()
    for f in list(part.bm.faces)[n0:]: f.material_index = MI[mat]


def ball(part, c, r, mat, us=8, vs=6):
    add_sphere(part, us, vs, lambda p: V(c) + V((p.x*r[0], p.y*r[1], p.z*r[2])) if isinstance(r, tuple) else V(c) + p*r, mat,
               lambda q: (q.x*0.3 + 0.5, q.z*0.3 + 0.5))


# ---------------------------------------------------------------- torso: skirt top, belt, bodice with sash, collar
TS = [H(0.70, .150, .105, .115), H(0.80, .152, .100, .112), H(0.88, .148, .095, .105), H(0.95, .128, .085, .092),
      H(0.975, .118, .080, .084), H(1.025, .120, .080, .084),
      H(1.10, .126, .086, .084), H(1.17, .140, .106, .088, -0.004), H(1.23, .145, .098, .090), H(1.29, .152, .084, .088),
      H(1.33, .120, .066, .072), H(1.36, .052, .046, .050, 0.006)]


def torso_mat(i, a):
    if i < 3: return "skirt"
    if i in (3, 4): return "gold"        # belt
    return "brocade"


loft(P["torso"], TS, 14, torso_mat, cap0=True)
loft(P["torso"], [H(1.33, .052, .046, .05, 0.006), H(1.42, .040, .036, .039, 0.006)], 8, lambda i, a: "skin")   # neck
loft(P["torso"], [H(1.27, .172, .112, .104), H(1.315, .135, .088, .090), H(1.36, .075, .062, .064, 0.004)], 14,
     lambda i, a: "gold")                                                                          # collar (กรองคอ)
# the sash (สไบ): faces of the bodice on a band from the left shoulder to the right hip turn green
A, B = V((0.13, 0, 1.31)), V((-0.13, 0, 1.03))
for f in P["torso"].bm.faces:
    c = f.calc_center_median()
    if 1.0 < c.z < 1.30 and MATS[f.material_index].name == PFX + "brocade":
        p = V((c.x, 0, c.z)); t = max(0, min(1, (p - A).dot(B - A)/(B - A).length_squared))
        if (A + (B - A)*t - p).length < 0.045: f.material_index = MI["sabai"]
ball(P["torso"], (0, -0.098, 1.20), (0.035, 0.012, 0.045), "gold", 6, 4)                          # breast plate
ball(P["torso"], (0, -0.092, 1.0), (0.045, 0.015, 0.032), "gold", 6, 4)                           # belt buckle
for s in (-1, 1):                                                                                  # shoulder flares
    tube(P["torso"], [V((s*0.11, 0, 1.33)), V((s*0.19, 0, 1.335)), V((s*0.235, 0.0, 1.37)), V((s*0.25, 0.01, 1.43))],
         [(.012, .045), (.010, .05), (.008, .035), (.004, .01)], 4, "gold", side=V((0, 0, 1)))
# front panel and hanging tails of the skirt
P_t = P["torso"]
tube(P_t, [V((0, -0.112, 0.97)), V((0, -0.118, 0.82)), V((0, -0.12, 0.70))], [(.035, .006), (.04, .006), (.045, .006)], 4,
     "gold", side=V((1, 0, 0)))
tube(P_t, [V((0.02, -0.10, 0.99)), V((0.03, -0.13, 0.80)), V((0.025, -0.135, 0.55))], [(.02, .004), (.025, .004), (.03, .004)], 4,
     "sabai", side=V((1, 0, 0)))

# ---------------------------------------------------------------- head: face, hair bun, crown (ชฎา), ear pieces, flower
HC = V((0, -0.004, 1.60))     # 9.5 cm higher than a person's: the neck is stretched
RX, RY, RZ = 0.064, 0.086, 0.106


def head_shape(p):
    x, y, z = p
    sx = 1 - 0.32*max(0, -z)**1.4
    y = y*0.93 if y < 0 else y*1.05
    if z < -0.5 and y < 0: y -= 0.07*(-z-0.5)
    if abs(x) < 0.22 and -0.45 < z < 0.15 and y < -0.85: y -= 0.14*(1 - abs(z+0.15)/0.3)
    if 0.35 < abs(x) < 0.85 and -0.65 < z < -0.05 and y < -0.2: y *= 0.9                          # sunken cheeks
    if 0.25 < abs(x) < 0.6 and -0.05 < z < 0.25 and y < -0.6: y *= 0.94                            # deep-set eyes
    return HC + V((x*RX*sx, y*RY, z*RZ))


add_sphere(P["head"], 16, 12, head_shape, "face",
           lambda q: (0.5 + 0.395*q.x, 0.5 + 0.458*q.z) if q.y < 0.1 else (0.5, 0.95))
for s in (-1, 1):
    ex, ez = s*0.165/0.395, 0.0
    ey = -math.sqrt(1 - ex*ex)
    c = head_shape(V((ex, ey, ez))) + V((0, -0.002, -0.0005))
    add_sphere(P["head"], 6, 4, lambda p, c=c: c + V((p.x*0.0045, p.y*0.002, p.z*0.0045)), "eyes_glow", lambda q: (0.5, 0.5))
ball(P["head"], HC + V((0, 0.07, -0.01)), (0.06, 0.05, 0.07), "hairbase", 6, 5)                   # hair at the nape (ears are under the hair)
# the stretched neck belongs to the head (it bends with it), thin, with the tendons showing
loft(P["head"], [H(1.40, .040, .036, .039, 0.006), H(1.47, .033, .030, .034, 0.004), H(HC.z - 0.07, .036, .034, .036, 0.0)],
     8, lambda i, a: "skin")
# long black hair from under the crown: a curtain down the back to the waist, strands hanging past the face
r = np.random.default_rng(160)
def hair_top(a): return HC + V((0.07*math.sin(a), 0.075*abs(math.cos(a))*0.9 + 0.02, 0.05))
for k in range(9):                                          # strand cards around the back of the head, overlapping
    a = math.radians(-150 + 300*k/8)
    top = hair_top(a)
    side = (hair_top(a + 0.05) - hair_top(a - 0.05)).normalized()
    out = V((math.sin(a), max(0.25, abs(math.cos(a))), 0)).normalized()
    L = 0.55 + 0.25*r.random()
    pts = [top, top + out*0.05 + V((0, 0, -0.10)), top + out*0.07 + V((0, 0.02, -0.30)), top + out*0.08 + V((0.01*r.standard_normal(), 0.03, -L))]
    card(P["head"], pts, [0.055, 0.07, 0.075, 0.06], side, "hair")
for sgn in (-1, 1):                                        # over the face, leaving the middle (the grin) showing
    for k, dx in enumerate((0.052, 0.066)):
        top = HC + V((sgn*dx, -0.05, 0.075))
        pts = [top, top + V((sgn*0.012, -0.045, -0.06)), top + V((sgn*0.008, -0.05, -0.20)), top + V((sgn*0.015, -0.035, -0.42 - 0.08*k))]
        card(P["head"], pts, [0.018, 0.024, 0.026, 0.02], V((1, 0, 0)), "hair")
# crown: a band round the brow, then stacked tiers tapering into a long spike
crown = [H(HC.z + 0.02, .072, .092, .088, 0.004), H(HC.z + 0.06, .076, .094, .090, 0.006)]
z, r = HC.z + 0.075, 0.072
for k in range(7):            # each tier: a flared lip, then a waist
    crown += [H(z + 0.004, r*1.18, r*1.24, r*1.2, 0.012), H(z + 0.036, r*0.80, r*0.86, r*0.82, 0.012)]
    z += 0.04; r *= 0.80
crown += [H(z + 0.04, 0.010, 0.010, 0.010, 0.014), H(z + 0.20, 0.003, 0.003, 0.003, 0.02)]
loft(P["head"], crown, 8, lambda i, a: "gold", tile=0.2, cap1=False)
for s in (-1, 1):                                                                                   # ear pieces (กรรเจียก)
    tube(P["head"], [HC + V((s*0.074, 0.01, 0.01)), HC + V((s*0.088, 0.035, 0.08)), HC + V((s*0.09, 0.07, 0.15)),
                     HC + V((s*0.085, 0.09, 0.19))], [(.006, .02), (.006, .028), (.005, .02), (.003, .006)], 4, "gold",
         side=V((1, 0, 0)))
ball(P["head"], HC + V((-0.078, -0.01, 0.07)), 0.026, "flower", 6, 4)                              # red flower, her right
ball(P["head"], HC + V((-0.072, -0.03, 0.05)), 0.016, "flower", 6, 4)
tube(P["head"], [HC + V((-0.086, -0.02, 0.04)), HC + V((-0.087, -0.02, -0.02)), HC + V((-0.086, -0.02, -0.08))],   # tassel (อุบะ)
     [.008, .007, .004], 4, "gold", tile=0.1)

# ---------------------------------------------------------------- arms: bare, armlets, bracelets, hands bent back, gold nails
for s, side in ((1, "L"), (-1, "R")):
    sx = V((s, 0, 0))
    up, fo = P["upperarm_" + side], P["forearm_" + side]
    tube(up, [V((s*0.12, 0.0, 1.335)), V((s*0.155, 0.0, 1.32)), V((s*0.17, 0.003, 1.27)), V((s*0.18, 0.006, 1.18)),
              V((s*0.19, 0.01, 1.08)), V((s*0.191, 0.01, 1.06))], [(.026, .034), (.032, .034), .030, .026, .024, .028], 8, "skin",
         side=sx, cap1=True)
    tube(up, [V((s*0.178, 0.005, 1.215)), V((s*0.181, 0.006, 1.19))], [.033, .031], 8, "gold", side=sx)            # armlet
    ball(fo, (s*0.19, 0.01, 1.08), 0.03, "skin", 6, 5)                                             # bony elbow
    # forearm 6 cm longer than a person's, thin as a stick
    tube(fo, [V((s*0.19, 0.01, 1.10)), V((s*0.196, 0.0, 0.96)), V((s*0.20, -0.01, 0.87)), V((s*0.20, -0.01, 0.81))],
         [.024, .019, .016, .015], 8, "skin", side=sx)
    for zb in (0.825, 0.845):
        tube(fo, [V((s*0.20, -0.01, zb + 0.01)), V((s*0.20, -0.01, zb - 0.01))], [.022, .022], 8, "gold", side=sx)  # bracelets
    # the hand bends back at the wrist and the fingers curl out (palm facing in): a flat tube, wide across Y
    hand = [V((s*0.20, -0.01, 0.815)), V((s*0.207, -0.012, 0.765)), V((s*0.228, -0.014, 0.725)), V((s*0.262, -0.016, 0.705)),
            V((s*0.295, -0.018, 0.708))]
    tube(fo, hand, [(.026, .011), (.034, .012), (.034, .010), (.028, .008), (.018, .006)], 6, "skin", tile=0.3,
         side=V((0, 1, 0)))
    for k, dy in enumerate((-0.022, -0.007, 0.008, 0.022)):                                         # long claws
        tip = V((s*0.295, -0.018 + dy, 0.708))
        tube(fo, [tip, tip + V((s*0.05, dy*0.4, 0.015)), tip + V((s*0.09, dy*0.7, 0.05)), tip + V((s*0.11, dy*0.9, 0.10))],
             [.005, .0045, .003, .001], 3, "hairbase", tile=0.2, side=V((0, 1, 0)))

# ---------------------------------------------------------------- legs: the long skirt over thigh and shin, bare feet
for s, side in ((1, "L"), (-1, "R")):
    sx = V((s, 0, 0))
    th, sh = P["thigh_" + side], P["shin_" + side]
    tube(th, [V((s*0.060, 0.0, 0.90)), V((s*0.062, -0.002, 0.76)), V((s*0.068, -0.004, 0.60)), V((s*0.075, -0.004, 0.45))],
         [(.098, .104), (.096, .102), (.088, .092), .078], 12, "skirt", side=sx, cap1=True)
    ball(sh, (s*0.08, -0.004, 0.47), 0.078, "skirt", 8, 5)
    tube(sh, [V((s*0.08, -0.004, 0.50)), V((s*0.075, 0.0, 0.30)), V((s*0.072, 0.002, 0.10)), V((s*0.072, 0.002, 0.095))],
         [.078, .080, (.090, .086), .04], 12, "skirt", side=sx)
    tube(sh, [V((s*0.085, 0.006, 0.16)), V((s*0.085, 0.006, 0.06))], [.03, .027], 8, "skin", side=sx)
    tube(sh, [V((s*0.085, 0.006, 0.075)), V((s*0.085, 0.006, 0.06))], [.032, .032], 8, "gold", side=sx)        # anklet
    # the hem torn into strips that hang below it
    for k in range(7):
        a = 2*math.pi*(k + 0.5*s)/7
        top = V((s*0.075 + 0.085*math.sin(a), 0.002 - 0.085*math.cos(a), 0.12))
        n = V((math.sin(a), -math.cos(a), 0))
        L = 0.06 + 0.07*((k*37 % 11)/10)
        tube(sh, [top, top + n*0.012 + V((0, 0, -L*0.5)), top + n*0.02 + V((0, 0, -L))], [(.018, .003), (.014, .003), (.006, .002)], 4,
             "skirt", tile=0.3, side=V((math.cos(a), math.sin(a), 0)))
    xs = s*0.085
    sec = [(0.045, 0.034, .026, .032), (0.028, 0.038, .031, .038), (0.0, 0.034, .034, .033), (-0.05, 0.026, .036, .026),
           (-0.10, 0.02, .036, .019), (-0.135, 0.016, .03, .014), (-0.15, 0.014, .02, .009)]
    loft(sh, [((xs, y, zc), (1, 0, 0), (0, 0, 1), w, h, h) for (y, zc, w, h) in sec], 8, lambda i, a: "skin",
         tile=0.3, cap0=True, cap1=True)

# ---------------------------------------------------------------- objects
PIV = {"torso": V((0, 0, 0.90)), "head": V((0, 0.004, 1.40))}   # the head bends at the base of the long neck
PARENT = {"torso": None, "head": "torso"}
for s, side in ((1, "L"), (-1, "R")):
    PIV["upperarm_" + side] = V((s*0.155, 0, 1.31)); PARENT["upperarm_" + side] = "torso"
    PIV["forearm_" + side] = V((s*0.19, 0.01, 1.08)); PARENT["forearm_" + side] = "upperarm_" + side
    PIV["thigh_" + side] = V((s*0.075, 0, 0.88)); PARENT["thigh_" + side] = "torso"
    PIV["shin_" + side] = V((s*0.08, -0.004, 0.47)); PARENT["shin_" + side] = "thigh_" + side
root, OBJ, tris = finish(col, PFX, P, MATS, PIV, PARENT, "dancer", cutout_mats=("hair",))
for o in OBJ.values():
    for p in o.data.polygons: p.use_smooth = True
print("dancer tris", tris)
