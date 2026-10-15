# Bakes ambient occlusion into the NPC's vertex colours (standing pose) and exports npc.glb (or GLB_NAME.glb) with every clip.
# Run inside Blender after npc_build.py and npc_anims.py. Copies the result into game/assets/models/.
# Other characters on the same rig set globals first: COLLECTION ("Grandma"), PFX ("grandma_"), HIPS_Z (0.80),
# AO_HIDE ({"grandma_face_normal": ["grandma_face_scary", ...]}: meshes hidden while that one bakes, e.g. a second
# face in the same place).
import bpy, shutil, numpy as np

ROOT = "/Users/smarttapon/coding_prac/grdc-horror/game/"
col = bpy.data.collections[globals().get("COLLECTION", "NPC")]
PFX = globals().get("PFX", "npc_")
AO_HIDE = globals().get("AO_HIDE", {})
FEET_DIRT = globals().get("FEET_DIRT", True)
meshes = [o for o in col.objects if o.type == "MESH"]
for o in col.objects: o.hide_set(False); o.hide_render = False      # hidden parts (a second face) must export too

# standing pose for the bake: mute every clip
for o in col.objects:
    if o.animation_data:
        for t in o.animation_data.nla_tracks: t.mute = True
    o.rotation_euler = (0, 0, 0)
bpy.data.objects[PFX + "torso"].location = (0, 0, globals().get("HIPS_Z", 0.95))
bpy.context.view_layer.update()

sc = bpy.context.scene
try: sc.render.engine = "CYCLES"
except TypeError as e: print(e)
sc.cycles.samples = 128
if sc.world is None: sc.world = bpy.data.worlds.new("World")
sc.world.light_settings.distance = 0.25
bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, 0)); ground = bpy.context.active_object
for o in meshes:
    me = o.data; a = me.color_attributes["AO"]; me.color_attributes.active_color = a
    bpy.ops.object.select_all(action="DESELECT"); o.select_set(True); bpy.context.view_layer.objects.active = o
    for n in AO_HIDE.get(o.name, []): bpy.data.objects[n].hide_render = True
    bpy.ops.object.bake(type="AO", target="VERTEX_COLORS")
    for n in AO_HIDE.get(o.name, []): bpy.data.objects[n].hide_render = False
    c = np.empty(len(a.data)*4, np.float32); a.data.foreach_get("color", c); c = c.reshape(-1, 4)
    v = ao_curve(c[:, 0], 0.3)                    # ps2kit.py; a lighter floor: faces and hands stay readable
    if FEET_DIRT:   # road dirt and darker shins: the feet sit in the fog layer, a clean white shoe pops out of it
        z = np.array([(o.matrix_world @ me.vertices[l.vertex_index].co).z for l in me.loops], np.float32)
        t = np.clip(z/0.55, 0, 1); v *= 0.62 + 0.38*t*t*(3 - 2*t)
    c[:, 0] = c[:, 1] = c[:, 2] = v; c[:, 3] = 1
    a.data.foreach_set("color", c.ravel())
bpy.data.objects.remove(ground, do_unlink=True)

# every NLA track becomes a glTF animation; tracks with the same name on different parts merge into one clip
for o in col.objects:
    if o.animation_data:
        for t in o.animation_data.nla_tracks: t.mute = False
bpy.ops.object.select_all(action="DESELECT")
for o in col.objects: o.select_set(True)
GLB = globals().get("GLB_NAME", "npc")
out = ROOT + "modelResource/" + GLB + ".glb"
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_yup=True, export_apply=True,
                          export_image_format="AUTO", export_vertex_color="NAME", export_vertex_color_name="AO",
                          export_cameras=False, export_lights=False,
                          export_animations=True, export_animation_mode="NLA_TRACKS", export_frame_range=False,
                          export_force_sampling=True, export_optimize_animation_size=False,
                          export_optimize_animation_keep_anim_armature=True, export_optimize_animation_keep_anim_object=True)


def standing_rest(path):
    """The exporter leaves each node at whatever pose the last NLA track evaluated to (e.g. "drive" = seated).
    Things that load the glb without playing a clip (the rule-4 shadow figure) need it standing: rewrite the
    joints' rest transform to the standing pose (every joint rotation 0, hips at HIPS_Z)."""
    import json, struct
    d = open(path, "rb").read()
    n = struct.unpack("<I", d[12:16])[0]; j = json.loads(d[20:20+n]); rest = d[20+n:]
    joints = {ch["target"]["node"] for a in j.get("animations", []) for ch in a["channels"]}
    for i, node in enumerate(j["nodes"]):
        if i not in joints: continue
        node.pop("rotation", None)
        if node["name"] == PFX + "torso": node["translation"] = [0, globals().get("HIPS_Z", 0.95), 0]
    js = json.dumps(j, separators=(",", ":")).encode(); js += b" "*((4 - len(js) % 4) % 4)
    body = struct.pack("<II", len(js), 0x4E4F534A) + js + rest
    open(path, "wb").write(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)


standing_rest(out)
shutil.copy(out, ROOT + "assets/models/" + GLB + ".glb")
for o in col.objects:
    if o.animation_data:
        for t in o.animation_data.nla_tracks: t.mute = True
bpy.ops.wm.save_mainfile()
print("exported", out)
