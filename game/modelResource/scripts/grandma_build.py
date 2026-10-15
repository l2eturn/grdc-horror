# Builds the old woman (ยาย, PS2 style) in Blender: run inside Blender after grandma_textures.py.
# Same rig layout as the companion (npc_build.py), so the same kind of clips work: ten rigid parts in a joint
# hierarchy, pivots at the joints, Z up, facing -Y (= +Z in the game), feet on z = 0. She is ~1.50 m tall standing
# straight; the stoop is in the clips (grandma_anims.py).
#   grandma > grandma_torso (hips) > grandma_neck (empty, neck base) > grandma_neck_skin, grandma_head (hair, bun, ears)
#   > grandma_face_normal | grandma_face_scary
#   (> grandma_eyes_glow), arms and legs as on the NPC.
# The two faces are separate meshes on the head; the game shows one of them (the rule: a scary face = go and help,
# an ordinary face = pretend you did not see her). Everything else is identical, so only the face tells them apart.
import bpy, bmesh, math
from mathutils import Vector as V, Matrix

PFX = "grandma_"
col = bpy.data.collections.get("Grandma")
if col:
    for o in list(col.objects): bpy.data.objects.remove(o, do_unlink=True)
else:
    col = bpy.data.collections.new("Grandma"); bpy.context.scene.collection.children.link(col)

# ---------------------------------------------------------------- materials: texture x vertex AO
MN = ["blouse", "skirt", "skin", "face", "face_scary", "hair", "sandal", "eyes_glow"]
TEX = {"blouse": "grandma_blouse", "skirt": "grandma_skirt", "skin": "grandma_skin", "face": "grandma_face",
       "face_scary": "grandma_face_scary", "hair": "grandma_hair", "sandal": "grandma_sandal"}
MATS = []
for k in MN:
    m = bpy.data.materials.get(PFX + k) or bpy.data.materials.new(PFX + k)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        if n.type not in ("BSDF_PRINCIPLED", "OUTPUT_MATERIAL"): nt.nodes.remove(n)
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Roughness"].default_value = 0.85
    if k == "eyes_glow":   # flat white light: the scary eyes stay visible in the dark
        b.inputs["Base Color"].default_value = (1, 1, 1, 1)
        b.inputs["Emission Color"].default_value = (0.92, 0.94, 0.88, 1)
        b.inputs["Emission Strength"].default_value = 1.0
    else:
        t = nt.nodes.new("ShaderNodeTexImage"); t.image = bpy.data.images[TEX[k]]
        vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "AO"
        mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"
        mx.inputs["Factor"].default_value = 1
        nt.links.new(t.outputs["Color"], mx.inputs[6]); nt.links.new(vc.outputs["Color"], mx.inputs[7])
        nt.links.new(mx.outputs[2], b.inputs["Base Color"])
    MATS.append(m)
MI = {k: i for i, k in enumerate(MN)}


class Part:
    def __init__(s):
        s.bm = bmesh.new(); s.uv = s.bm.loops.layers.uv.new("UVMap")


P = {n: Part() for n in ("torso", "neck_skin", "head", "face_normal", "face_scary", "eyes_glow", "upperarm_L", "upperarm_R",
                         "forearm_L", "forearm_R", "thigh_L", "thigh_R", "shin_L", "shin_R")}


def loft(part, secs, sides, matf, tile=0.5, cap0=False, cap1=False):
    """Rings joined into a tube. secs: (center, X, Y, rx, ry_front, ry_back); a = 0 is on the -Y side ("front")."""
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


def H(z, rx, ryf, ryb=None, cy=0.0):
    return ((0, cy, z), (1, 0, 0), (0, 1, 0), rx, ryf, ryb if ryb is not None else ryf)


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


def add_sphere(part, us, vs, shape, mat, uvf, drop=None):
    """uvf gets the point on the unit sphere (before shape), so a stretched shape keeps its texture layout"""
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


def ball(part, c, r, mat):
    add_sphere(part, 8, 6, lambda p: V(c) + p*r, mat, lambda q: (q.x*0.2 + 0.5, q.z*0.2 + 0.5))


# ---------------------------------------------------------------- torso: tube skirt below, loose blouse over it
TS = [H(0.62, .162, .112, .120), H(0.69, .156, .110, .118), H(0.76, .152, .106, .116), H(0.82, .146, .098, .108), H(0.855, .132, .088, .094),
      H(0.856, .160, .114, .120), H(0.92, .152, .108, .110), H(1.00, .143, .110, .104), H(1.06, .148, .114, .104, -0.006),
      H(1.12, .150, .108, .106, -0.004), H(1.17, .148, .098, .102), H(1.20, .128, .086, .092),
      H(1.235, .104, .068, .074, 0.004), H(1.255, .062, .055, .060, 0.008)]


def torso_mat(i, a):
    if i < 4: return "skirt"
    da = abs(((a + math.pi) % (2*math.pi)) - math.pi)
    if i >= 11 and da < math.radians(45): return "skin"         # scooped neckline
    return "blouse"


loft(P["torso"], TS, 14, torso_mat, cap0=True)
# the neck is its own part on an empty joint (grandma_neck) so the game can hide it and stretch a long one in its place
loft(P["neck_skin"], [H(1.215, .046, .044, .048, 0.008), H(1.30, .043, .041, .045, 0.008), H(1.345, .042, .04, .044, 0.008)],
     8, lambda i, a: "skin")

# ---------------------------------------------------------------- head: hair pulled back into a bun, two faces
HC = V((0, -0.004, 1.40))
RX, RY, RZ = 0.072, 0.092, 0.108


def head_shape(scary):
    def f(p):
        x, y, z = p
        sx = 1 - 0.30*max(0, -z)**1.4
        y = y*0.94 if y < 0 else y*1.06
        if z < -0.5 and y < 0: y -= 0.06*(-z-0.5)                        # small sunken chin
        if abs(x) < 0.25 and -0.45 < z < 0.15 and y < -0.85: y -= 0.17*(1 - abs(z+0.15)/0.3)    # nose
        if 0.35 < abs(x) < 0.8 and -0.6 < z < -0.1 and y < -0.3: y *= 0.95                      # hollow cheeks
        if scary and z < -0.42 and y < 0.35:                               # jaw hanging open
            w = min(1, (0.35 - y)/0.5)
            z -= 0.55*(-z - 0.42)*w
            y -= 0.05*w
        return HC + V((x*RX*sx, y*RY, z*RZ))
    return f


def face_uv(q): return (0.5 + 0.395*q.x, 0.5 + 0.458*q.z) if q.y < 0.1 else (0.5, 0.9)


add_sphere(P["face_normal"], 16, 12, head_shape(False), "face", face_uv)
add_sphere(P["face_scary"], 16, 12, head_shape(True), "face_scary", face_uv)
for s in (-1, 1):   # the glowing eyes sit just in front of the painted ones (u 0.5 +- 0.165, v 0.51)
    ex, ez = s*0.165/0.395, 0.01/0.458
    ey = -math.sqrt(1 - ex*ex - ez*ez)
    c = head_shape(True)(V((ex, ey, ez))) + V((0, -0.002, 0))
    add_sphere(P["eyes_glow"], 8, 6, lambda p, c=c: c + V((p.x*0.0095, p.y*0.003, p.z*0.0085)), "eyes_glow",
               lambda q: (0.5, 0.5))
for s in (-1, 1):   # ears (big, as old ears are)
    add_sphere(P["head"], 6, 4, lambda p, s=s: V((s*0.072 + p.x*0.012, 0.01 + p.y*0.022, HC.z - 0.02 + p.z*0.036)),
               "skin", lambda q: (q.y*0.3 + 0.5, q.z*0.3 + 0.5))


def hair_shape(p):
    x, y, z = p
    yy = y*0.98 if y < 0 else y*1.06
    return HC + V((x*0.079, yy*0.099, z*(0.117 if z > 0 else 0.113))) + V((0, 0.004, 0.006 + 0.008*max(z, 0)))


def hair_drop(n):   # n: face centre on the unit sphere
    thr = 0.40*(-n.y) if n.y < 0 else -0.55*n.y
    if abs(n.x) > 0.55 and n.y < 0.35: thr = max(thr, 0.12)
    return n.z < thr


hair_uv = lambda q: (q.x*0.6 + 0.5, q.y*0.5 - q.z*0.3 + 0.5)       # strands run front to back
add_sphere(P["head"], 16, 10, hair_shape, "hair", hair_uv, drop=hair_drop)
add_sphere(P["head"], 10, 6, lambda p: HC + V((0, 0.098, 0.035)) + V((p.x*0.042, p.y*0.034, p.z*0.036)), "hair", hair_uv)   # bun

# ---------------------------------------------------------------- arms: blouse sleeve to below the elbow, bony hands
for s, side in ((1, "L"), (-1, "R")):
    sx = V((s, 0, 0))
    up, fo = P["upperarm_" + side], P["forearm_" + side]
    sl = [V((s*0.112, 0.0, 1.215)), V((s*0.150, 0.0, 1.19)), V((s*0.170, 0.003, 1.15)), V((s*0.184, 0.006, 1.08)),
          V((s*0.196, 0.01, 0.97)), V((s*0.197, 0.01, 0.945))]
    tube(up, sl, [(.030, .042), (.046, .052), (.052, .055), .050, .045, .044], 10, "blouse", side=sx, cap1=True)
    ball(fo, (s*0.198, 0.01, 0.96), 0.043, "blouse")
    tube(fo, [V((s*0.197, 0.01, 0.985)), V((s*0.203, 0.0, 0.90)), V((s*0.208, -0.008, 0.845))],
         [.045, .049, .053], 10, "blouse", side=sx)
    tube(fo, [V((s*0.204, -0.002, 0.90)), V((s*0.208, -0.008, 0.82)), V((s*0.211, -0.013, 0.765))],
         [.030, .026, .022], 8, "skin", side=sx)
    hand = [V((s*0.211, -0.013, 0.775)), V((s*0.212, -0.015, 0.735)), V((s*0.213, -0.017, 0.70)), V((s*0.213, -0.021, 0.665)),
            V((s*0.211, -0.026, 0.633)), V((s*0.209, -0.031, 0.614))]
    tube(fo, hand, [(.019, .025), (.018, .028), (.016, .036), (.014, .035), (.011, .028), (.008, .019)], 8, "skin",
         tile=0.3, side=sx, cap1=True)
    tube(fo, [V((s*0.201, -0.031, 0.732)), V((s*0.199, -0.046, 0.70)), V((s*0.201, -0.052, 0.676))],
         [.010, .009, .007], 6, "skin", tile=0.3, side=sx, cap1=True)

# ---------------------------------------------------------------- legs: skirt over thigh and shin, bare ankles, flip-flops
for s, side in ((1, "L"), (-1, "R")):
    sx = V((s, 0, 0))
    th, sh = P["thigh_" + side], P["shin_" + side]
    tube(th, [V((s*0.062, 0.0, 0.80)), V((s*0.066, -0.002, 0.66)), V((s*0.072, -0.004, 0.52)), V((s*0.078, -0.004, 0.40))],
         [(.100, .106), (.098, .104), (.092, .096), .082], 12, "skirt", side=sx, cap1=True)
    ball(sh, (s*0.086, -0.004, 0.42), 0.08, "skirt")
    tube(sh, [V((s*0.084, -0.004, 0.45)), V((s*0.080, 0.0, 0.32)), V((s*0.078, 0.002, 0.19)), V((s*0.078, 0.002, 0.185))],
         [.080, (.088, .084), (.098, .090), .04], 12, "skirt", side=sx)
    tube(sh, [V((s*0.094, 0.006, 0.24)), V((s*0.094, 0.006, 0.12)), V((s*0.094, 0.004, 0.06))],
         [.034, .029, .028], 8, "skin", side=sx)
    xs = s*0.095
    sec = [(0.052, 0.036, .028, .034), (0.032, 0.040, .034, .040), (0.0, 0.036, .038, .035), (-0.05, 0.029, .041, .028),
           (-0.10, 0.023, .042, .022), (-0.135, 0.019, .038, .018), (-0.155, 0.017, .028, .014), (-0.162, 0.016, .016, .010)]
    loft(sh, [((xs, y, zc), (1, 0, 0), (0, 0, 1), w, h, h) for (y, zc, w, h) in sec], 10, lambda i, a: "skin",
         tile=0.3, cap0=True, cap1=True)
    sole = [(0.062, .024), (0.03, .034), (-0.06, .042), (-0.14, .040), (-0.175, .024)]
    loft(sh, [((xs, y, 0.006), (1, 0, 0), (0, 0, 1), w, .006, .006) for (y, w) in sole], 8, lambda i, a: "sandal",
         tile=0.3, cap0=True, cap1=True)
    for d in (-1, 1):   # the thong strap: from between the toes back to both sides of the foot
        tube(sh, [V((xs + s*0.004, -0.125, 0.012)), V((xs + d*0.022, -0.085, 0.036)), V((xs + d*0.040, -0.045, 0.014))],
             [.005, .005, .004], 4, "sandal", tile=0.2)

# ---------------------------------------------------------------- the stoop: a rounded upper back, built into the mesh
# (bending her at the hips instead would swing the skirt out behind her). Torso vertices move forward with height,
# the head and arms move with the point they hang from.
def hunch(z): return -0.09*min(1.2, max(0.0, (z - 0.95)/0.30))**1.6


HUNCH_AT = {"neck_skin": 1.30, "head": 1.30, "face_normal": 1.30, "face_scary": 1.30, "eyes_glow": 1.30}
for side in ("L", "R"): HUNCH_AT["upperarm_" + side] = HUNCH_AT["forearm_" + side] = 1.19
for name, part in P.items():
    if name == "torso":
        for vtx in part.bm.verts: vtx.co.y += hunch(vtx.co.z) + 0.025*max(0.0, 1 - abs(vtx.co.z - 1.12)/0.12)*(vtx.co.y > 0)
    elif name in HUNCH_AT:
        for vtx in part.bm.verts: vtx.co.y += hunch(HUNCH_AT[name])

# ---------------------------------------------------------------- objects
NECK = V((0, 0.004 + hunch(1.30), 1.30))
PIV = {"torso": V((0, 0, 0.80)), "head": NECK, "face_normal": NECK, "face_scary": NECK, "eyes_glow": NECK}
NECK_BASE = V((0, 0.004 + hunch(1.30), 1.235))
PIV.update({"neck": NECK_BASE, "neck_skin": NECK_BASE})
PARENT = {"torso": None, "neck": "torso", "neck_skin": "neck", "head": "neck", "face_normal": "head", "face_scary": "head", "eyes_glow": "face_scary"}
for s, side in ((1, "L"), (-1, "R")):
    PIV["upperarm_" + side] = V((s*0.165, hunch(1.19), 1.19)); PARENT["upperarm_" + side] = "torso"
    PIV["forearm_" + side] = V((s*0.198, 0.01 + hunch(1.19), 0.96)); PARENT["forearm_" + side] = "upperarm_" + side
    PIV["thigh_" + side] = V((s*0.080, 0, 0.79)); PARENT["thigh_" + side] = "torso"
    PIV["shin_" + side] = V((s*0.090, -0.004, 0.42)); PARENT["shin_" + side] = "thigh_" + side
root = bpy.data.objects.get("grandma") or bpy.data.objects.new("grandma", None)
if root.name not in col.objects: col.objects.link(root)
OBJ = {}
tris = 0
for name, part in P.items():
    old = bpy.data.meshes.get(PFX + name)
    if old: bpy.data.meshes.remove(old)
    me = bpy.data.meshes.new(PFX + name)
    bmesh.ops.recalc_face_normals(part.bm, faces=part.bm.faces)
    part.bm.to_mesh(me); part.bm.free()
    for m in MATS: me.materials.append(m)
    for p in me.polygons: p.use_smooth = True
    me.transform(Matrix.Translation(-PIV[name]))
    a = me.color_attributes.new("AO", "BYTE_COLOR", "CORNER")
    for d in a.data: d.color = (1, 1, 1, 1)
    o = bpy.data.objects.new(PFX + name, me); col.objects.link(o)
    o.rotation_mode = "XYZ"
    OBJ[name] = o
    n = sum(len(p.vertices)-2 for p in me.polygons)
    if name != "face_scary": tris += n          # one face shows at a time
neck = bpy.data.objects.new(PFX + "neck", None); col.objects.link(neck)
neck.empty_display_size = 0.05; neck.rotation_mode = "XYZ"
OBJ["neck"] = neck
for name, o in OBJ.items():
    par = PARENT[name]
    o.parent = OBJ[par] if par else root
    o.location = PIV[name] - (PIV[par] if par else V())
OBJ["face_scary"].hide_set(True); OBJ["eyes_glow"].hide_set(True)
print("tris (one face)", tris)
