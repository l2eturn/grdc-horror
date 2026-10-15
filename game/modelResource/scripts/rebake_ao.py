# Re-bake the ambient occlusion of a finished model and re-export its glb, without rebuilding the geometry:
# opens nothing itself — run Blender on the model's source .blend:
#   /Applications/Blender.app/Contents/MacOS/Blender -b <blend> --python rebake_ao.py -- <model>
#   models: bus cart dancer driver grandma npc sala shrine   (blend for each: JOBS below)
# The phone is left out on purpose: at 12 cm the bake is all white, and the game draws its screen through the vertex colour.
# Uses the model's own export script where it has one (same bake settings, clips, rest pose), so after changing
# ps2kit.ao_curve every model can be brought in line with one loop over JOBS. Meshes of other collections in the
# same file (grandma_source.blend also holds the cart and old copies) are hidden from the bake.
import bpy, sys, numpy as np

S = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/scripts/"
JOBS = {   # model: (blend, collection, export script or None = ps2kit.bake_and_export, globals for the script)
    "bus":     ("bus_source.blend", "Bus", "bus_export.py", {}),
    "cart":    ("grandma_source.blend", "Cart", "cart_export.py", {}),
    "grandma": ("grandma_source.blend", "Grandma", "npc_export.py",
                {"COLLECTION": "Grandma", "PFX": "grandma_", "HIPS_Z": 0.80, "GLB_NAME": "grandma", "FEET_DIRT": False,
                 "AO_HIDE": {"grandma_face_normal": ["grandma_face_scary"], "grandma_face_scary": ["grandma_face_normal"]}}),
    "npc":     ("npc_source.blend", "NPC", "npc_export.py", {}),
    "driver":  ("driver_source.blend", "NPC", "npc_export.py", {"GLB_NAME": "driver"}),
    "dancer":  ("dancer_source.blend", "Dancer", None, {}),
    "shrine":  ("shrine_source.blend", "Shrine", None, {}),
    "sala":    ("sala_source.blend", "Sala", None, {}),
}

model = sys.argv[sys.argv.index("--") + 1]
blend, colname, script, glob = JOBS[model]
if not bpy.data.filepath.endswith(blend):
    raise SystemExit(f"open {blend} for {model} (got {bpy.data.filepath})")
exec(open(S + "ps2kit.py").read())
col = bpy.data.collections[colname]

# meshes without an AO layer get a white one (older models, parts added by hand)
for o in col.objects:
    if o.type == "MESH" and "AO" not in o.data.color_attributes:
        a = o.data.color_attributes.new("AO", "BYTE_COLOR", "CORNER")
        for d in a.data: d.color = (1, 1, 1, 1)
        print("added AO layer to", o.name)

hidden = [o for o in bpy.context.scene.objects
          if o.type == "MESH" and o.name not in col.objects and not o.hide_render]
for o in hidden: o.hide_render = True

if script:
    g = dict(globals()); g.update(glob)
    exec(open(S + script).read(), g)
else:
    bake_and_export(col, model)

for o in hidden: o.hide_render = False
bpy.ops.wm.save_mainfile()
print("rebaked", model)
