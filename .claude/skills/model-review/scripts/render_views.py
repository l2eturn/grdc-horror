# Contact sheet of one .glb for review: 4 views (front, front 3/4, side, back) × 3 rows
#   row 1 textured  — base colour textures, soft studio light (shape + texture read)
#   row 2 AO        — vertex colour only, flat light (is the baked AO there and believable?)
#   row 3 silhouette — black on white (does the shape read from a distance in fog?)
# Run headless (no Blender UI needed):
#   /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python render_views.py -- \
#       game/assets/models/bus.glb /tmp/bus_sheet.png [--clip idle] [--frame 12] [--size 320]
# Without --clip the pose is whatever the glTF importer leaves active (usually one of the clips).
# Game models face +Z in glTF = -Y in Blender, so "front" looks at the model from -Y.
import bpy, math, sys, os, zlib, struct
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argv) < 2:
    raise SystemExit("usage: ... -- <model.glb> <out.png> [--clip NAME] [--frame N] [--size PX]")
glb, out = os.path.abspath(argv[0]), os.path.abspath(argv[1])
opt = {argv[i]: argv[i + 1] for i in range(2, len(argv) - 1, 2)}
size = int(opt.get("--size", 320))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
sc = bpy.context.scene
meshes = [o for o in sc.objects if o.type == "MESH"]

# pose: optional clip + frame (clips import as actions; NLA tracks too)
clip = opt.get("--clip")
if clip:
    act = bpy.data.actions.get(clip)
    if not act:
        print("clip not found:", clip, "available:", [a.name for a in bpy.data.actions])
    else:
        for o in sc.objects:
            if o.animation_data:
                for t in o.animation_data.nla_tracks: t.mute = True
                o.animation_data.action = act
                slot = act.slots.get("OB" + o.name) if hasattr(act, "slots") else None   # Blender 4.4+: slotted actions
                if slot: o.animation_data.action_slot = slot
sc.frame_set(int(opt.get("--frame", 1)))
bpy.context.view_layer.update()

# bounds in world space
pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center, ext = (lo + hi) / 2, hi - lo
radius = max(ext.length / 2, 0.01)

cam_data = bpy.data.cameras.new("cam"); cam_data.type = "ORTHO"; cam_data.ortho_scale = max(ext.z, ext.x, ext.y) * 1.15
cam_data.clip_start = radius * 0.01; cam_data.clip_end = radius * 20
cam = bpy.data.objects.new("cam", cam_data); sc.collection.objects.link(cam); sc.camera = cam

sc.render.engine = "BLENDER_WORKBENCH"
sc.render.resolution_x = sc.render.resolution_y = size
sc.render.film_transparent = False
sc.render.image_settings.file_format = "PNG"
sc.view_settings.view_transform = "Standard"
sh = sc.display.shading

VIEWS = [("front", 0), ("front 3/4", 35), ("side", 90), ("back", 180)]
ROWS = ["textured", "ao", "silhouette"]


def set_mode(mode):
    sh.show_cavity = False; sh.show_shadows = False; sh.show_object_outline = False
    sh.background_type = "VIEWPORT"
    if mode == "textured":
        sh.light = "STUDIO"; sh.color_type = "TEXTURE"; sh.background_color = (0.18, 0.19, 0.21)
    elif mode == "ao":
        sh.light = "FLAT"; sh.color_type = "VERTEX"; sh.background_color = (0.18, 0.19, 0.21)
    else:
        sh.light = "FLAT"; sh.color_type = "SINGLE"; sh.single_color = (0, 0, 0); sh.background_color = (1, 1, 1)


def place(deg, elev=8):
    a, e = math.radians(deg), math.radians(elev)
    d = radius * 4
    # deg 0 = in front of the model (Blender -Y), turning toward +X
    pos = center + Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e))) * d
    cam.location = pos
    cam.rotation_euler = (center - pos).to_track_quat("-Z", "Y").to_euler()


tiles = []
tmp = out + ".tile.png"
for mode in ROWS:
    set_mode(mode)
    row = []
    for _, deg in VIEWS:
        place(deg)
        sc.render.filepath = tmp
        bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(tmp)
        px = np.array(im.pixels[:], np.float32).reshape(size, size, 4)
        bpy.data.images.remove(im)
        row.append(px)
    tiles.append(np.concatenate(row, axis=1))
os.remove(tmp)
sheet = np.concatenate(tiles[::-1], axis=0)   # Blender pixels are bottom-up: last row first


def write_png(path, rgba):
    H, W = rgba.shape[:2]
    b = (np.clip(rgba[::-1], 0, 1) * 255 + 0.5).astype(np.uint8)
    raw = b"".join(b"\x00" + b[y].tobytes() for y in range(H))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


write_png(out, sheet)
print(f"sheet: {out}  rows: {', '.join(ROWS)}  cols: {', '.join(v for v, _ in VIEWS)}  "
      f"size: {ext.x:.2f}×{ext.y:.2f}×{ext.z:.2f} m (Blender x×y×z, z up)")
