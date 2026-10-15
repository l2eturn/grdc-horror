# Bakes AO into the cart's vertex colours (level, on a ground plane) and exports cart.glb with the clips stuck / free.
# Run inside Blender after cart_build.py. Copies the result into game/assets/models/.
import bpy, shutil, numpy as np

ROOT = "/Users/smarttapon/coding_prac/grdc-horror/game/"
col = bpy.data.collections["Cart"]
meshes = [o for o in col.objects if o.type == "MESH"]
others = [o for o in bpy.context.scene.objects if o.name not in col.objects and o.type == "MESH" and not o.hide_render]
for o in others: o.hide_render = True                 # the grandma must not shadow the cart's bake
for o in col.objects:
    if o.animation_data:
        for t in o.animation_data.nla_tracks: t.mute = True
    o.rotation_euler = (0, 0, 0)
loc = bpy.data.objects["cart"].location.copy(); bpy.data.objects["cart"].location = (0, 0, 0)
bpy.context.view_layer.update()

sc = bpy.context.scene
try: sc.render.engine = "CYCLES"
except TypeError as e: print(e)
sc.cycles.samples = 128
if sc.world is None: sc.world = bpy.data.worlds.new("World")
sc.world.light_settings.distance = 0.4
bpy.ops.mesh.primitive_plane_add(size=8, location=(0, -1, 0)); ground = bpy.context.active_object
for o in meshes:
    me = o.data; a = me.color_attributes["AO"]; me.color_attributes.active_color = a
    bpy.ops.object.select_all(action="DESELECT"); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.bake(type="AO", target="VERTEX_COLORS")
    c = np.empty(len(a.data)*4, np.float32); a.data.foreach_get("color", c); c = c.reshape(-1, 4)
    v = ao_curve(c[:, 0]); c[:, 0] = c[:, 1] = c[:, 2] = v; c[:, 3] = 1             # ps2kit.py
    a.data.foreach_set("color", c.ravel())
bpy.data.objects.remove(ground, do_unlink=True)
for o in others: o.hide_render = False

for o in col.objects:
    if o.animation_data:
        for t in o.animation_data.nla_tracks: t.mute = False
bpy.ops.object.select_all(action="DESELECT")
for o in col.objects: o.select_set(True)
out = ROOT + "modelResource/cart.glb"
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_yup=True, export_apply=True,
                          export_image_format="AUTO", export_vertex_color="NAME", export_vertex_color_name="AO",
                          export_cameras=False, export_lights=False,
                          export_animations=True, export_animation_mode="NLA_TRACKS", export_frame_range=False,
                          export_force_sampling=True, export_optimize_animation_size=False,
                          export_optimize_animation_keep_anim_armature=True, export_optimize_animation_keep_anim_object=True)
shutil.copy(out, ROOT + "assets/models/cart.glb")
for o in col.objects:
    if o.animation_data:
        for t in o.animation_data.nla_tracks: t.mute = True
bpy.data.objects["cart"].location = loc
bpy.ops.wm.save_mainfile()
print("exported", out)
