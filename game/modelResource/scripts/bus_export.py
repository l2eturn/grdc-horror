# Door clips + ambient occlusion + export of bus.glb. Run inside Blender after bus_build.py.
import bpy, math, shutil, numpy as np

ROOT = "/Users/smarttapon/coding_prac/grdc-horror/game/"
col = bpy.data.collections["Bus"]
meshes = [o for o in col.objects if o.type == "MESH"]

# tinted glass like the photos (dark film)
g = bpy.data.materials["bus_glass"]
b = next(n for n in g.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
b.inputs["Alpha"].default_value = 0.72
mx = next(n for n in g.node_tree.nodes if n.type == "MIX")
mx.inputs[6].default_value = (0.02, 0.028, 0.032, 1)

# ---------------------------------------------------------------- door clips (leaves swing inward 85 degrees)
OPEN = {"bus_door_f1": 85, "bus_door_f2": -85, "bus_door_m1": 85, "bus_door_m2": -85}
for name in OPEN:
    o = bpy.data.objects[name]
    ad = o.animation_data or o.animation_data_create()
    ad.action = None
    for t in list(ad.nla_tracks): ad.nla_tracks.remove(t)
for a in list(bpy.data.actions):
    if a.name.startswith("bus|"): bpy.data.actions.remove(a)
for clip, keys in {"doors_open": ((0, 0.0), (8, 0.7), (24, 1.0)), "doors_close": ((0, 1.0), (16, 0.3), (24, 0.0))}.items():
    for name, deg in OPEN.items():
        o = bpy.data.objects[name]
        act = bpy.data.actions.new(f"bus|{clip}|{name}")
        o.animation_data.action = act
        for frame, f in keys:
            o.rotation_euler = (0, 0, math.radians(deg*f)); o.keyframe_insert("rotation_euler", frame=frame)
        o.animation_data.action = None
        t = o.animation_data.nla_tracks.new(); t.name = clip; t.strips.new(clip, 0, act); t.mute = True
for name in OPEN: bpy.data.objects[name].rotation_euler = (0, 0, 0)
bpy.context.view_layer.update()

# ---------------------------------------------------------------- AO into vertex colours (doors closed)
sc = bpy.context.scene
try: sc.render.engine = "CYCLES"
except TypeError as e: print(e)
sc.cycles.samples = 96
sc.world.light_settings.distance = 0.5   # tighter AO: contact darkening at seams instead of a flat wash
bpy.ops.mesh.primitive_plane_add(size=30, location=(0, 0, 0)); ground = bpy.context.active_object
for o in meshes:
    me = o.data; a = me.color_attributes["AO"]; me.color_attributes.active_color = a
    bpy.ops.object.select_all(action="DESELECT"); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.bake(type="AO", target="VERTEX_COLORS")
    c = np.empty(len(a.data)*4, np.float32); a.data.foreach_get("color", c); c = c.reshape(-1, 4)
    v = ao_curve(c[:, 0])                                                         # ps2kit.py
    # road dirt: darker toward the ground (world height of each corner), strongest below the wheel arches
    z = np.array([(o.matrix_world @ me.vertices[l.vertex_index].co).z for l in me.loops], np.float32)
    t = np.clip((z - 0.3)/0.6, 0, 1); v *= 0.45 + 0.55*t*t*(3 - 2*t)
    c[:, 0] = c[:, 1] = c[:, 2] = v; c[:, 3] = 1
    a.data.foreach_set("color", c.ravel())
bpy.data.objects.remove(ground, do_unlink=True)

# ---------------------------------------------------------------- export
for name in OPEN:
    for t in bpy.data.objects[name].animation_data.nla_tracks: t.mute = False
bpy.ops.object.select_all(action="DESELECT")
for o in col.objects: o.select_set(True)
out = ROOT + "modelResource/bus.glb"
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_yup=True, export_apply=True,
                          export_image_format="AUTO", export_vertex_color="NAME", export_vertex_color_name="AO",
                          export_cameras=False, export_lights=False,
                          export_animations=True, export_animation_mode="NLA_TRACKS", export_frame_range=False,
                          export_force_sampling=True, export_optimize_animation_size=False)
shutil.copy(out, ROOT + "assets/models/bus.glb")
for name in OPEN:
    for t in bpy.data.objects[name].animation_data.nla_tracks: t.mute = True
bpy.ops.wm.save_mainfile()
print("exported", out)
