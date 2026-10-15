# Clips for the ghost dancer. Run inside Blender after dancer_build.py. Conventions as npc_anims.py (read its header
# for the joint signs); hips rest at 0.90 m.
#   dance       (loop)  slow classical Thai dance: knees bent, one arm raised in a curve (ตั้งวง), the other low behind,
#                       head tilted away from the raised arm, changing sides; the lifted heel
#   dance_jerk  (loop)  the same dance like a puppet on strings: holds, then snaps to the next pose
#   glide       (loop)  arms reaching forward, clawed hands, head on one side (the game slides her toward you)
#   bow         (once)  hands pressed together (ไหว้), head bowed, then up: how she first appears
import bpy, math
from mathutils import Vector

PFX = "dancer_"
FPS = 30
PARTS = ["torso", "head", "upperarm_L", "upperarm_R", "forearm_L", "forearm_R", "thigh_L", "thigh_R", "shin_L", "shin_R"]
OBJ = {p: bpy.data.objects[PFX + p] for p in PARTS}
HIPS = Vector((0, 0, 0.90))


def mirror(pose):
    out = {}
    for k, v in pose.items():
        if k == "move": out[k] = v; continue
        k2 = k.replace("_L", "_X").replace("_R", "_L").replace("_X", "_R")
        out[k2] = (v[0], -v[1], -v[2])
    return out


def merge(*poses):
    out = {}
    for p in poses: out.update(p)
    return out


# knees bent, back straight (ย่อ)
SQUAT = {"move": (0, 0.02, -0.06), "torso": (4, 0, 0), "thigh_L": (-30, 0, -3), "thigh_R": (-30, 0, 3),
         "shin_L": (34, 0, 0), "shin_R": (34, 0, 0)}
HIGH_L = {"upperarm_L": (10, -110, 0), "forearm_L": (-70, 0, 90)}            # left arm raised in a curve (forearm twisted up)
LOW_R = {"upperarm_R": (30, 25, 0), "forearm_R": (-15, 0, 0)}                # right arm low behind
LIFT_R = {"thigh_R": (-48, 0, 6), "shin_R": (70, 0, 0)}                      # right heel up
A = merge(SQUAT, HIGH_L, LOW_R, {"head": (-4, -14, 8), "torso": (4, 4, -6)})
A2 = merge(A, LIFT_R, {"head": (-2, -18, 12), "torso": (5, 6, -8)})
MID = merge(SQUAT, {"upperarm_L": (-70, -30, 0), "forearm_L": (-90, 0, 0), "upperarm_R": (-70, 30, 0), "forearm_R": (-90, 0, 0),
                    "head": (4, 0, 0)})                                    # both arms curved in front
B, B2 = mirror(A), mirror(A2)
DANCE = [(0, A), (45, A2), (75, A), (100, MID), (130, B), (175, B2), (205, B), (230, MID), (260, A)]

# puppet: hold, snap, hold ...
J, f = [], 0
for pose, hold in ((A, 16), (merge(A2, {"head": (8, -35, 20)}), 14), (MID, 10), (B, 16),
                   (merge(B2, {"head": (8, 35, -20)}), 14), (MID, 10)):
    J += [(f, pose), (f + hold, pose)]
    f += hold + 2                 # 2 frames to snap to the next pose
J.append((f, A))
DANCE_JERK = J

REACH = {"torso": (10, 0, 0), "head": (8, 28, 0), "move": (0, 0, 0),
         "upperarm_L": (-78, -12, 0), "forearm_L": (-25, 0, 0), "upperarm_R": (-82, 12, 0), "forearm_R": (-20, 0, 0),
         "thigh_L": (-10, 0, 0), "thigh_R": (-10, 0, 0), "shin_L": (0, 0, 0), "shin_R": (0, 0, 0)}
GLIDE = [(0, REACH), (20, merge(REACH, {"head": (12, 34, 4), "upperarm_L": (-86, -12, 0)})),
         (40, merge(REACH, {"head": (6, 24, -4), "upperarm_R": (-90, 12, 0)})), (60, REACH)]

WAI = {"upperarm_L": (-35, 0, 22), "forearm_L": (-110, 0, -30), "upperarm_R": (-35, 0, -22), "forearm_R": (-110, 0, 30)}
BOW = [(0, merge(SQUAT, WAI, {"torso": (25, 0, 0), "head": (25, 0, 0)})), (30, merge(SQUAT, WAI, {"torso": (25, 0, 0), "head": (25, 0, 0)})),
       (60, merge(SQUAT, WAI, {"head": (-5, 0, 0)})), (80, A)]

CLIPS = {"dance": (True, DANCE), "dance_jerk": (True, DANCE_JERK), "glide": (True, GLIDE), "bow": (False, BOW)}

for o in OBJ.values():
    ad = o.animation_data or o.animation_data_create()
    ad.action = None
    for t in list(ad.nla_tracks): ad.nla_tracks.remove(t)
for a in list(bpy.data.actions):
    if a.name.startswith("dancer|"): bpy.data.actions.remove(a)
for clip, (loop, keys) in CLIPS.items():
    for p, o in OBJ.items():
        act = bpy.data.actions.new(f"dancer|{clip}|{p}")
        o.animation_data.action = act
        for frame, pose in keys:
            o.rotation_euler = tuple(math.radians(x) for x in pose.get(p, (0, 0, 0)))
            o.keyframe_insert("rotation_euler", frame=frame)
            if p == "torso":
                m = pose.get("move", (0, 0, 0))
                o.location = (HIPS.x + m[0], HIPS.y + m[1], HIPS.z + m[2])
                o.keyframe_insert("location", frame=frame)
        o.animation_data.action = None
        tr = o.animation_data.nla_tracks.new(); tr.name = clip
        tr.strips.new(clip, 0, act); tr.mute = True
    print(clip, "frames", keys[-1][0], "loop" if loop else "once")
for p, o in OBJ.items():
    o.rotation_euler = (0, 0, 0)
    if p == "torso": o.location = HIPS
bpy.context.scene.render.fps = FPS
