# Builds the companion NPC (PS2 style) in Blender: run inside Blender with the textures npc_* already in the file
# (npc_source.blend has them packed). Z up, the character faces -Y (= +Z in glTF / the game), feet on z = 0.
# Ten rigid parts in a joint hierarchy (the engine has no skinning), each with its pivot at a joint:
#   npc_torso (hips) > npc_head (neck), npc_upperarm_L/R (shoulder) > npc_forearm_L/R (elbow),
#   npc_thigh_L/R (hip) > npc_shin_L/R (knee). Animations: scripts/npc_anims.py.
import bpy, bmesh, math
from mathutils import Vector as V, Matrix

col = bpy.data.collections.get("NPC")
if col:
    for o in list(col.objects): bpy.data.objects.remove(o, do_unlink=True)
else:
    col = bpy.data.collections.new("NPC"); bpy.context.scene.collection.children.link(col)

# ---------------------------------------------------------------- materials: texture x vertex AO
MN = ["jacket", "tee", "shorts", "skin", "face", "hair", "shoe", "sole"]
TEX = {"jacket": "npc_jacket", "tee": "npc_tee", "shorts": "npc_shorts", "skin": "npc_skin", "face": "npc_face",
       "hair": "npc_hair", "shoe": "npc_shoe", "sole": "npc_shoe"}
TEX.update(globals().get("TEX_OVERRIDE", {}))   # variants (the bus driver) swap textures: {"jacket": "driver_shirt", ...}
MAT_PREFIX = globals().get("MAT_PREFIX", "npc_")
MATS = []
for k in MN:
    m = bpy.data.materials.get(MAT_PREFIX + k) or bpy.data.materials.new(MAT_PREFIX + k)
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
    out = mx.outputs[2]
    if k == "sole":   # same texture as the shoe, a bit greyer
        mt = nt.nodes.new("ShaderNodeMix"); mt.data_type = "RGBA"; mt.blend_type = "MULTIPLY"
        mt.inputs["Factor"].default_value = 1
        nt.links.new(out, mt.inputs[6]); mt.inputs[7].default_value = (0.72, 0.72, 0.70, 1); out = mt.outputs[2]
    nt.links.new(out, b.inputs["Base Color"])
    MATS.append(m)
MI = {k: i for i, k in enumerate(MN)}


class Part:
    def __init__(s):
        s.bm = bmesh.new(); s.uv = s.bm.loops.layers.uv.new("UVMap")


P = {n: Part() for n in ("torso", "head", "upperarm_L", "upperarm_R", "forearm_L", "forearm_R",
                         "thigh_L", "thigh_R", "shin_L", "shin_R")}


def loft(part, secs, sides, matf, tile=0.5, cap0=False, cap1=False):
    """Rings joined into a tube. secs: (center, X, Y, rx, ry_front, ry_back).
    Ring point = c + X*rx*sin(a) - Y*ry*cos(a), so a = 0 is on the -Y side ("front")."""
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
    """horizontal ring for the torso"""
    return ((0, cy, z), (1, 0, 0), (0, 1, 0), rx, ryf, ryb if ryb is not None else ryf)


def tube(part, pts, radii, sides, mat, tile=0.5, side=V((1, 0, 0)), cap1=False):
    """loft along a polyline; radii are r or (r_side, r_front)"""
    secs = []
    for i, p in enumerate(pts):
        t = (pts[min(i+1, len(pts)-1)] - pts[max(i-1, 0)]).normalized()
        X = (side - t*side.dot(t)).normalized()
        Y = t.cross(X)
        if Y.y < 0: Y = -Y          # keep a = 0 on the -Y (front) side
        r = radii[i]; rx, ry = r if isinstance(r, tuple) else (r, r)
        secs.append((p, X, Y, rx, ry, ry))
    loft(part, secs, sides, (lambda i, a: mat) if isinstance(mat, str) else mat, tile, cap1=cap1)


# ---------------------------------------------------------------- torso: shorts, jacket open over a black tee
TS = [H(0.885, .120, .080, .092), H(0.91, .168, .100, .118), H(0.94, .172, .100, .114), H(0.975, .166, .096, .102),
      H(0.976, .184, .116, .121), H(1.03, .180, .116, .116), H(1.12, .173, .118, .110), H(1.22, .178, .128, .110, -0.005),
      H(1.31, .185, .133, .114, -0.01), H(1.38, .188, .118, .112, -0.005), H(1.425, .178, .100, .102),
      H(1.465, .125, .076, .080, 0.005), H(1.495, .074, .064, .066, 0.008)]


def torso_mat(i, a):
    if i < 3: return "shorts"
    if i == 3: return "jacket"          # the hem lip
    da = abs(((a + math.pi) % (2*math.pi)) - math.pi)
    if 4 <= i <= 10 and da < math.radians(17 if i < 9 else 24): return "tee"
    return "jacket"


loft(P["torso"], TS, 16, torso_mat, cap0=True)
loft(P["torso"], [H(1.455, .098, .084, .088, 0.008), H(1.525, .084, .075, .079, 0.012)], 12, lambda i, a: "jacket")  # collar
loft(P["torso"], [H(1.45, .056, .052, .056, 0.012), H(1.55, .056, .052, .055, 0.012), H(1.61, .052, .05, .052, 0.008)],
     10, lambda i, a: "skin")                                                                            # neck

# ---------------------------------------------------------------- head: shaped sphere, face projected from the front
HC = V((0, -0.004, 1.652))
hd = P["head"]


def add_sphere(part, us, vs, shape, mat, uvf):
    b = bmesh.new(); bmesh.ops.create_uvsphere(b, u_segments=us, v_segments=vs, radius=1.0)
    for vtx in b.verts: vtx.co = shape(vtx.co.copy())
    drop = [f for f in b.faces if shape.__dict__.get("drop", lambda c: False)(f.calc_center_median())]
    if drop: bmesh.ops.delete(b, geom=drop, context="FACES")
    me = bpy.data.meshes.new("tmp"); b.to_mesh(me); b.free()
    n0 = len(part.bm.faces); part.bm.from_mesh(me); bpy.data.meshes.remove(me)
    part.bm.faces.ensure_lookup_table()
    for f in list(part.bm.faces)[n0:]:
        f.material_index = MI[mat]
        for l in f.loops: l[part.uv].uv = uvf(l.vert.co)


def head_shape(p):
    x, y, z = p
    sx = 1 - 0.28*max(0, -z)**1.4                 # jaw narrower than the skull
    y = y*0.94 if y < 0 else y*1.06                # flatter face, fuller back
    if z < -0.5 and y < 0: y -= 0.08*(-z-0.5)     # chin
    if abs(x) < 0.25 and -0.45 < z < 0.15 and y < -0.85: y -= 0.17*(1 - abs(z+0.15)/0.3)   # nose
    return HC + V((x*0.079*sx, y*0.100, z*0.119))


# front projection; the back half samples plain forehead skin so the beard does not show through on the nape
add_sphere(hd, 16, 12, head_shape, "face",
           lambda c: (0.5 + c.x/0.20, 0.5 + (c.z - HC.z)/0.26) if c.y - HC.y < 0.01 else (0.5, 0.9))
for s in (-1, 1):   # ears
    add_sphere(hd, 6, 4, lambda p, s=s: V((s*0.079 + p.x*0.012, 0.008 + p.y*0.02, HC.z - 0.01 + p.z*0.03)), "skin",
               lambda c: (c.y*4, c.z*4))


def hair_shape(p):
    x, y, z = p
    yy = y*0.97 if y < 0 else y*1.08
    return HC + V((x*0.087, yy*0.108, z*(0.128 if z > 0 else 0.124))) + V((0, 0.004, 0.008 + 0.012*max(z, 0)))


def hair_drop(c):
    n = c - HC; n = V((n.x/0.087, n.y/0.108, n.z/0.128))
    thr = 0.42*(-n.y) if n.y < 0 else -0.62*n.y          # forehead hairline high, nape low
    if abs(n.x) > 0.55 and n.y < 0.35: thr = max(thr, 0.08)   # above the ears
    return n.z < thr


hair_shape.drop = hair_drop
add_sphere(hd, 16, 10, hair_shape, "hair", lambda c: (c.x*6, (c.y-HC.y)*5 - (c.z-HC.z)*3))

# ---------------------------------------------------------------- arms: upper sleeve | forearm sleeve + hand
def ball(part, c, r, mat):
    """small sphere at a joint: fills the gap on the outside of a bent elbow/knee"""
    add_sphere(part, 8, 6, lambda p: V(c) + p*r, mat, lambda q: (q.x*4, q.z*4))


for s, side in ((1, "L"), (-1, "R")):
    sx = V((s, 0, 0))
    up, fo = P["upperarm_" + side], P["forearm_" + side]
    # starts on top of the shoulder and curves down over the deltoid (no corner), ends just past the elbow
    sleeve = [V((s*0.155, 0.0, 1.425)), V((s*0.190, 0.0, 1.405)), V((s*0.208, 0.003, 1.355)), V((s*0.216, 0.006, 1.27)),
              V((s*0.230, 0.012, 1.14)), V((s*0.231, 0.012, 1.11))]
    tube(up, sleeve, [(.040, .055), (.058, .066), (.064, .068), .059, .053, .052], 10, "jacket", side=sx, cap1=True)
    ball(fo, (s*0.232, 0.012, 1.14), 0.05, "jacket")
    sleeve = [V((s*0.231, 0.012, 1.17)), V((s*0.240, 0.0, 1.02)), V((s*0.248, -0.018, 0.905)), V((s*0.249, -0.02, 0.90))]
    tube(fo, sleeve, [.051, .050, .046, .031], 10, "jacket", side=sx)
    hand = [V((s*0.249, -0.02, 0.915)), V((s*0.25, -0.022, 0.865)), V((s*0.252, -0.024, 0.82)), V((s*0.252, -0.028, 0.775)),
            V((s*0.25, -0.034, 0.735)), V((s*0.247, -0.04, 0.712))]
    tube(fo, hand, [(.023, .029), (.021, .033), (.019, .043), (.016, .041), (.013, .033), (.009, .022)], 8, "skin",
         tile=0.3, side=sx, cap1=True)
    tube(fo, [V((s*0.238, -0.04, 0.862)), V((s*0.236, -0.058, 0.825)), V((s*0.238, -0.064, 0.795))],
         [.012, .011, .008], 6, "skin", tile=0.3, side=sx, cap1=True)

# ---------------------------------------------------------------- legs: shorts + thigh | knee + shin + sneaker
for s, side in ((1, "L"), (-1, "R")):
    sx = V((s, 0, 0))
    th, sh = P["thigh_" + side], P["shin_" + side]
    shorts = [V((s*0.082, 0.003, 0.95)), V((s*0.095, 0.0, 0.84)), V((s*0.102, -0.004, 0.70)), V((s*0.103, -0.005, 0.625)),
              V((s*0.103, -0.005, 0.624))]
    tube(th, shorts, [(.102, .107), (.100, .104), .095, .092, .074], 12, "shorts", side=sx)
    tube(th, [V((s*0.10, 0.0, 0.70)), V((s*0.102, 0.0, 0.62)), V((s*0.103, -0.002, 0.55)), V((s*0.104, -0.003, 0.49))],
         [.075, .071, (.062, .064), .052], 10, "skin", side=sx, cap1=True)
    ball(sh, (s*0.104, -0.004, 0.50), 0.053, "skin")
    leg = [V((s*0.104, -0.004, 0.52)), V((s*0.104, 0.005, 0.42)), V((s*0.104, 0.007, 0.33)), V((s*0.104, 0.004, 0.22)),
           V((s*0.104, 0.0, 0.12)), V((s*0.104, 0.0, 0.07))]
    tube(sh, leg, [.053, (.060, .066), (.058, .063), .047, .037, .036], 10, "skin", side=sx)
    # sneaker: heel -> toe, rings in the XZ plane (a = 0 is the bottom), bottom faces are the sole
    xs = s*0.106
    sec = [(0.068, 0.046, .036, .046), (0.045, 0.052, .044, .052), (0.0, 0.05, .049, .05), (-0.06, 0.042, .052, .042),
           (-0.12, 0.034, .051, .034), (-0.165, 0.028, .045, .028), (-0.19, 0.024, .033, .022), (-0.198, 0.022, .018, .014)]
    loft(sh, [((xs, y, zc), (1, 0, 0), (0, 0, 1), w, h, h) for (y, zc, w, h) in sec], 10,
         lambda i, a: "sole" if math.cos(a) > 0.6 else "shoe", tile=0.3, cap0=True, cap1=True)

# ---------------------------------------------------------------- objects: a joint hierarchy, pivots at the joints
# npc (root, on the ground) > torso (hips) > head (neck), upperarm (shoulder) > forearm (elbow), thigh (hip) > shin (knee)
PIV = {"torso": V((0, 0, 0.95)), "head": V((0, 0.005, 1.56))}
PARENT = {"torso": None, "head": "torso"}
for s, side in ((1, "L"), (-1, "R")):
    PIV["upperarm_" + side] = V((s*0.20, 0, 1.40)); PARENT["upperarm_" + side] = "torso"
    PIV["forearm_" + side] = V((s*0.232, 0.012, 1.14)); PARENT["forearm_" + side] = "upperarm_" + side
    PIV["thigh_" + side] = V((s*0.095, 0, 0.93)); PARENT["thigh_" + side] = "torso"
    PIV["shin_" + side] = V((s*0.104, -0.004, 0.50)); PARENT["shin_" + side] = "thigh_" + side
root = bpy.data.objects.get("npc") or bpy.data.objects.new("npc", None)
if root.name not in col.objects: col.objects.link(root)
OBJ = {}
tris = 0
for name, part in P.items():
    old = bpy.data.meshes.get("npc_" + name)
    if old: bpy.data.meshes.remove(old)
    me = bpy.data.meshes.new("npc_" + name)
    bmesh.ops.recalc_face_normals(part.bm, faces=part.bm.faces)
    part.bm.to_mesh(me); part.bm.free()
    for m in MATS: me.materials.append(m)
    for p in me.polygons: p.use_smooth = True
    me.transform(Matrix.Translation(-PIV[name]))
    a = me.color_attributes.new("AO", "BYTE_COLOR", "CORNER")
    for d in a.data: d.color = (1, 1, 1, 1)
    o = bpy.data.objects.new("npc_" + name, me); col.objects.link(o)
    o.rotation_mode = "XYZ"
    OBJ[name] = o
    tris += sum(len(p.vertices)-2 for p in me.polygons)
for name, o in OBJ.items():
    par = PARENT[name]
    o.parent = OBJ[par] if par else root
    o.location = PIV[name] - (PIV[par] if par else V())
print("tris", tris)
