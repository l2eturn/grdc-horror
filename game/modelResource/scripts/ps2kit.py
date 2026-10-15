# Shared helpers for the prop scripts (shrine_build.py, dancer_build.py): numpy textures, texture x AO materials,
# bmesh parts and simple primitives with world-meter UVs. Run inside Blender with exec() before the build script:
#   exec(open(S + "ps2kit.py").read())
# Blender is Z up; props face -Y (= +Z in the game).
import bpy, bmesh, math, zlib, struct, numpy as np
from mathutils import Vector as V, Matrix, Euler

OUT = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/textures/"


# ---------------------------------------------------------------- textures
def fractal(N, seed, ax=1.0, ay=1.0, beta=1.8):
    r = np.random.default_rng(seed); w = r.standard_normal((N, N))
    f = np.fft.fftfreq(N)*N; fu, fv = np.meshgrid(f, f)
    k = np.sqrt((fu/ax)**2 + (fv/ay)**2); amp = np.where(k >= 1, np.maximum(k, 1e-6)**(-beta), 0)
    n = np.real(np.fft.ifft2(np.fft.fft2(w)*amp)); return (n-n.mean())/(n.std()+1e-9)


def grid(N):
    u = np.tile((np.arange(N)+0.5)/N, (N, 1)); return u, u.T


def ss(e0, e1, x): t = np.clip((x-e0)/(e1-e0), 0, 1); return t*t*(3-2*t)


def write_png(path, rgba):
    """straight-alpha RGBA PNG (Blender's own save turns fully transparent texels white)"""
    H, W = rgba.shape[:2]
    b = (np.clip(rgba[::-1], 0, 1)*255 + 0.5).astype(np.uint8)
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


# ---------------------------------------------------------------- baked AO -> vertex colour
AO_GAMMA = 1.5


def ao_curve(ao, floor=0.25):
    """Cycles AO (0 = fully occluded, 1 = open) -> the "AO" vertex colour every export script writes. The gamma
    keeps open surfaces near 1 but pulls creases and contact areas down hard, so parts look seated in each other
    instead of evenly washed; `floor` is the darkest a vertex gets. One curve for every model so they match."""
    return floor + (1 - floor)*np.clip(ao, 0, 1)**AO_GAMMA


# ---------------------------------------------------------------- materials
def make_materials(prefix, tex, cutout=(), emissive=None):
    """tex: {key: image name}. Materials prefix+key = image x vertex colour "AO". Cutouts: alpha -> Round -> Alpha
    (glTF MASK). Everything single-sided: the engine lights double-sided + alpha-test as foliage."""
    mats, mi = [], {}
    for k, img in tex.items():
        m = bpy.data.materials.get(prefix + k) or bpy.data.materials.new(prefix + k)
        m.use_nodes = True
        nt = m.node_tree
        for n in list(nt.nodes):
            if n.type not in ("BSDF_PRINCIPLED", "OUTPUT_MATERIAL"): nt.nodes.remove(n)
        b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
        b.inputs["Roughness"].default_value = 0.85
        t = nt.nodes.new("ShaderNodeTexImage"); t.image = bpy.data.images[img]
        vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "AO"
        mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"
        mx.inputs["Factor"].default_value = 1
        nt.links.new(t.outputs["Color"], mx.inputs[6]); nt.links.new(vc.outputs["Color"], mx.inputs[7])
        nt.links.new(mx.outputs[2], b.inputs["Base Color"])
        if k in cutout:
            rd = nt.nodes.new("ShaderNodeMath"); rd.operation = "ROUND"
            nt.links.new(t.outputs["Alpha"], rd.inputs[0]); nt.links.new(rd.outputs[0], b.inputs["Alpha"])
        if emissive and k in emissive:
            b.inputs["Emission Color"].default_value = (*emissive[k], 1); b.inputs["Emission Strength"].default_value = 1.0
        m.use_backface_culling = True
        mi[k] = len(mats); mats.append(m)
    return mats, mi


# ---------------------------------------------------------------- parts and primitives
class Part:
    def __init__(s):
        s.bm = bmesh.new(); s.uv = s.bm.loops.layers.uv.new("UVMap")


class Kit:
    """geometry helpers bound to a material index table"""
    def __init__(s, mi): s.mi = mi

    def add(s, part, b, mat, uvf):
        me = bpy.data.meshes.new("tmp"); b.to_mesh(me); b.free()
        n0 = len(part.bm.faces); part.bm.from_mesh(me); bpy.data.meshes.remove(me)
        part.bm.faces.ensure_lookup_table(); part.bm.normal_update()
        new = list(part.bm.faces)[n0:]
        for f in new:
            f.material_index = s.mi[mat]
            if uvf:   # None: keep the UVs the source bmesh already had (its layer is also called "UVMap")
                for l in f.loops: l[part.uv].uv = uvf(l.vert.co, f)
        return new

    @staticmethod
    def boxuv(tile):
        def f(c, face):
            n = face.normal; ax = max(range(3), key=lambda i: abs(n[i]))
            a, b = [(1, 2), (0, 2), (0, 1)][ax]
            return (c[a]/tile, c[b]/tile)
        return f

    def box(s, part, c, size, mat, rot=(0, 0, 0), tile=0.5):
        b = bmesh.new(); bmesh.ops.create_cube(b, size=1.0)
        M = Matrix.Translation(V(c)) @ Euler([math.radians(x) for x in rot]).to_matrix().to_4x4() @ Matrix.Diagonal((*size, 1))
        bmesh.ops.transform(b, matrix=M, verts=b.verts)
        return s.add(part, b, mat, s.boxuv(tile))

    def beam(s, part, a, b_, w, h, mat, tile=0.5):
        a, b_ = V(a), V(b_); d = b_ - a
        rot = d.to_track_quat("Y", "Z").to_euler()
        return s.box(part, (a+b_)/2, (w, d.length, h), mat, tuple(math.degrees(x) for x in rot), tile)

    def tube(s, part, pts, radii, mat, sides=6, tile=0.5, cap=False):
        """round tube along a polyline, radius per point (or one radius)"""
        b = bmesh.new(); pts = [V(p) for p in pts]; rings = []
        if not isinstance(radii, (list, tuple)): radii = [radii]*len(pts)
        X0 = None
        for i, p in enumerate(pts):
            t = (pts[min(i+1, len(pts)-1)] - pts[max(i-1, 0)]).normalized()
            X = (X0 - t*X0.dot(t)).normalized() if X0 is not None else t.orthogonal().normalized()
            X0 = X; Y = t.cross(X)
            rings.append([b.verts.new(p + (X*math.cos(2*math.pi*k/sides) + Y*math.sin(2*math.pi*k/sides))*radii[i])
                          for k in range(sides)])
        uvl = b.loops.layers.uv.new("UVMap")
        acc = [0.0]
        for i in range(1, len(pts)): acc.append(acc[-1] + (pts[i] - pts[i-1]).length)
        circ = 2*math.pi*max(radii)/tile
        for i in range(len(rings)-1):
            for k in range(sides):
                f = b.faces.new([rings[i][k], rings[i][(k+1) % sides], rings[i+1][(k+1) % sides], rings[i+1][k]])
                for l, (kk, ii) in zip(f.loops, ((k, i), (k+1, i), (k+1, i+1), (k, i+1))):
                    l[uvl].uv = (kk/sides*circ, acc[ii]/tile)
        if cap: b.faces.new(rings[-1])
        return s.add(part, b, mat, None)

    def blob(s, part, c, r, mat, rot=0.0, flat=1.0, us=10, vs=6, seed=0, lumpy=0.08, uvs=2.0, uvoff=(0, 0)):
        rng = np.random.default_rng(seed)
        b = bmesh.new(); bmesh.ops.create_uvsphere(b, u_segments=us, v_segments=vs, radius=1.0)
        R = Matrix.Rotation(math.radians(rot), 3, "Z")
        for vt in b.verts:
            p = vt.co.copy()
            if p.z < -flat: p.z = -flat + (p.z + flat)*0.2
            p *= 1 + lumpy*rng.standard_normal()
            vt.co = V(c) + R @ V((p.x*r[0], p.y*r[1], p.z*r[2]))
        c0 = V(c)
        return s.add(part, b, mat, lambda q, f: (uvoff[0] + ((q - c0).x + (q - c0).y)*uvs, uvoff[1] + (q - c0).z*uvs))

    def quad2(s, part, a, b_, c, d, mat, tile=0.5, uv=None):
        """a two-sided flat panel built as two single-sided faces (cutout cloth, leaves, wire)"""
        bm = bmesh.new()
        for pts in ((a, b_, c, d), (d, c, b_, a)):
            bm.faces.new([bm.verts.new(V(p)) for p in pts])
        a_, b2, d_ = V(a), V(b_), V(d); eu = (b2-a_).normalized(); ev = (d_-a_).normalized()
        if uv is None:
            uvf = lambda p, f: ((p-a_).dot(eu)/tile, (p-a_).dot(ev)/tile)
        else:   # uv = (u0, v0, u1, v1) mapped over the panel
            lu, lv = (b2-a_).length, (d_-a_).length
            uvf = lambda p, f: (uv[0] + (uv[2]-uv[0])*(p-a_).dot(eu)/lu, uv[1] + (uv[3]-uv[1])*(p-a_).dot(ev)/lv)
        return s.add(part, bm, mat, uvf)


def finish(col, prefix, parts, mats, piv, parent, root_name, cutout_mats=(), names=None):
    """bmesh parts -> mesh objects with an AO colour layer, pivots at piv, parented under an empty root"""
    root = bpy.data.objects.get(root_name) or bpy.data.objects.new(root_name, None)
    if root.name not in col.objects: col.objects.link(root)
    objs, tris = {}, 0
    for name, part in parts.items():
        full = (names or {}).get(name, prefix + name)
        old = bpy.data.meshes.get(full)
        if old: bpy.data.meshes.remove(old)
        me = bpy.data.meshes.new(full)
        idx = {i for i, m in enumerate(mats) if m.name[len(prefix):] in cutout_mats}
        bmesh.ops.recalc_face_normals(part.bm, faces=[f for f in part.bm.faces if f.material_index not in idx])
        part.bm.to_mesh(me); part.bm.free()
        for m in mats: me.materials.append(m)
        me.transform(Matrix.Translation(-piv.get(name, V())))
        a = me.color_attributes.new("AO", "BYTE_COLOR", "CORNER")
        for d in a.data: d.color = (1, 1, 1, 1)
        o = bpy.data.objects.new(full, me); col.objects.link(o)
        o.rotation_mode = "XYZ"
        objs[name] = o
        tris += sum(len(p.vertices)-2 for p in me.polygons)
    for name, o in objs.items():
        par = parent.get(name)
        o.parent = objs[par] if par else root
        o.location = piv.get(name, V()) - (piv.get(par, V()) if par else V())
    return root, objs, tris


def collection(name):
    col = bpy.data.collections.get(name)
    if col:
        for o in list(col.objects): bpy.data.objects.remove(o, do_unlink=True)
    else:
        col = bpy.data.collections.new(name); bpy.context.scene.collection.children.link(col)
    return col


def bake_and_export(col, glb, distance=0.4, ground=8, hide_others=True, keep_anim=False):
    """AO into the "AO" colour layers (on a temporary ground plane), then export col to modelResource/<glb>.glb and
    copy it into game/assets/models/"""
    import shutil
    ROOT = "/Users/smarttapon/coding_prac/grdc-horror/game/"
    meshes = [o for o in col.objects if o.type == "MESH"]
    others = [o for o in bpy.context.scene.objects if o.name not in col.objects and o.type == "MESH" and not o.hide_render] if hide_others else []
    for o in others: o.hide_render = True
    for o in col.objects:
        o.hide_set(False); o.hide_render = False
        if o.animation_data:
            for t in o.animation_data.nla_tracks: t.mute = True
    bpy.context.view_layer.update()
    sc = bpy.context.scene
    try: sc.render.engine = "CYCLES"
    except TypeError as e: print(e)
    sc.cycles.samples = 96
    if sc.world is None: sc.world = bpy.data.worlds.new("World")
    sc.world.light_settings.distance = distance
    root = next(o for o in col.objects if o.parent is None)
    bpy.ops.mesh.primitive_plane_add(size=ground, location=root.matrix_world.translation); gp = bpy.context.active_object
    for o in meshes:
        me = o.data; a = me.color_attributes["AO"]; me.color_attributes.active_color = a
        bpy.ops.object.select_all(action="DESELECT"); o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.ops.object.bake(type="AO", target="VERTEX_COLORS")
        c = np.empty(len(a.data)*4, np.float32); a.data.foreach_get("color", c); c = c.reshape(-1, 4)
        v = ao_curve(c[:, 0]); c[:, 0] = c[:, 1] = c[:, 2] = v; c[:, 3] = 1
        a.data.foreach_set("color", c.ravel())
    bpy.data.objects.remove(gp, do_unlink=True)
    for o in others: o.hide_render = False
    for o in col.objects:
        if o.animation_data:
            for t in o.animation_data.nla_tracks: t.mute = False
    bpy.ops.object.select_all(action="DESELECT")
    for o in col.objects: o.select_set(True)
    out = ROOT + "modelResource/" + glb + ".glb"
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_yup=True, export_apply=True,
                              export_image_format="AUTO", export_vertex_color="NAME", export_vertex_color_name="AO",
                              export_cameras=False, export_lights=False,
                              export_animations=True, export_animation_mode="NLA_TRACKS", export_frame_range=False,
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_optimize_animation_keep_anim_armature=True, export_optimize_animation_keep_anim_object=True)
    shutil.copy(out, ROOT + "assets/models/" + glb + ".glb")
    for o in col.objects:
        if o.animation_data:
            for t in o.animation_data.nla_tracks: t.mute = True
    bpy.ops.wm.save_mainfile()
    print("exported", out)
