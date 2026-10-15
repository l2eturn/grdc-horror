# Animation clips for the old woman. Run inside Blender after grandma_build.py. Same conventions as npc_anims.py
# (read its header for the joint signs): a pose = part -> (rx, ry, rz) degrees, "move" = hip offset in meters.
# Her hips rest at 0.80 m. The rounded back is in the mesh; standing clips start from STOOP (a little more lean,
# head raised to look ahead, arms and legs turned back to hang straight, knees soft).
#
# Clips: idle, walk (slow shuffle), beckon (calls you over), fallen (sitting on the road holding her shin),
#        fallen_call (sitting, reaching out for help), lunge (one-shot: rears up and lunges at you, the scary reveal),
#        push (straining at the stuck cart), push_walk (pushing it away), neck_stretch (body pose while the game
#        stretches her neck; see Grandma.cpp).
# The face (normal / scary) is not animated: the game shows grandma_face_normal or grandma_face_scary.
import bpy, math
from mathutils import Vector

PFX = "grandma_"
FPS = 30
PARTS = ["torso", "head", "upperarm_L", "upperarm_R", "forearm_L", "forearm_R", "thigh_L", "thigh_R", "shin_L", "shin_R"]
OBJ = {p: bpy.data.objects[PFX + p] for p in PARTS}
HIPS = Vector((0, 0, 0.80))   # grandma_build.py PIV["torso"]


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


# ---------------------------------------------------------------- stooped standing
STOOP = {"move": (0, 0.015, -0.01), "torso": (8, 0, 0), "head": (-14, 0, 0),
         "upperarm_L": (-10, 4, 0), "upperarm_R": (-10, -4, 0), "forearm_L": (-30, 0, -10), "forearm_R": (-30, 0, 10),
         "thigh_L": (-16, 0, -2), "thigh_R": (-16, 0, 2), "shin_L": (8, 0, 0), "shin_R": (8, 0, 0)}

IDLE = [(0, STOOP),
        (45, merge(STOOP, {"torso": (9, 0, 1), "head": (-12, 0, -4), "move": (0, 0.015, -0.013)})),
        (75, merge(STOOP, {"head": (-15, 0, 3)})),
        (120, STOOP)]

# ---------------------------------------------------------------- walk: short shuffling steps, 1.6 s a cycle
def step(tl, sl, tr, sr, arm, tz, mz):
    return merge(STOOP, {"thigh_L": (-16 + tl, 0, -2), "shin_L": (8 + sl, 0, 0), "thigh_R": (-16 + tr, 0, 2),
                         "shin_R": (8 + sr, 0, 0), "upperarm_L": (-10 - arm, 4, 0), "upperarm_R": (-10 + arm, -4, 0),
                         "torso": (8, 0, tz), "move": (0, 0.015, -0.01 + mz)})


G0 = step(-14, 4, 10, 14, 6, 3, -0.012)     # left foot forward, both down
G1 = step(-6, 6, 14, 30, 3, 2, -0.008)      # right lifts off
G2 = step(4, 4, -10, 40, 0, 0, 0.0)         # right swings through
G3 = step(10, 10, -14, 10, -4, -2, -0.008)  # right lands
WALK = [(0, G0), (8, G1), (16, G2), (22, G3), (24, mirror(G0)), (32, mirror(G1)), (40, mirror(G2)), (46, mirror(G3)),
        (48, G0)]

# ---------------------------------------------------------------- beckon: right arm out, the hand curls "come here"
BK = merge(STOOP, {"upperarm_R": (-62, -6, 8), "forearm_R": (-20, 0, 10), "head": (-18, 0, 0)})
BECKON = [(0, BK), (8, merge(BK, {"forearm_R": (-75, 0, 10)})), (16, BK), (24, merge(BK, {"forearm_R": (-75, 0, 10)})),
          (32, BK), (60, merge(BK, {"head": (-16, 0, 5), "upperarm_R": (-58, -6, 8)})), (75, BK)]

# ---------------------------------------------------------------- fallen on the road: sitting, left leg out, right knee up
FALL = {"move": (0, 0.0, -0.66), "torso": (12, 0, 0), "head": (6, 0, -10),
        "thigh_L": (-100, 0, -6), "shin_L": (8, 0, 0),
        "thigh_R": (-140, 0, 8), "shin_R": (120, 0, 0),
        "upperarm_L": (40, 25, 0), "forearm_L": (-15, 0, 0),           # propped on the left hand behind
        "upperarm_R": (-50, -10, 0), "forearm_R": (-45, 0, 20)}        # right hand on the shin
FALLEN = [(0, FALL), (30, merge(FALL, {"forearm_R": (-55, 0, 20), "head": (10, 0, -8)})), (60, FALL),
          (90, merge(FALL, {"head": (4, 0, -14), "torso": (14, 0, 0)})), (120, FALL)]
CALL = merge(FALL, {"head": (-12, 0, 0), "torso": (8, 0, 0), "upperarm_R": (-80, -4, 0), "forearm_R": (-15, 0, 0)})
FALLEN_CALL = [(0, CALL), (20, merge(CALL, {"upperarm_R": (-86, -4, 0), "forearm_R": (-35, 0, 0), "head": (-14, 0, 3)})),
               (40, CALL), (70, merge(CALL, {"upperarm_R": (-76, -4, 0), "head": (-10, 0, -3)})), (90, CALL)]

# ---------------------------------------------------------------- lunge: straightens up, arms up, then throws herself forward
UP = merge(STOOP, {"torso": (-6, 0, 0), "head": (-6, 0, 0), "move": (0, 0.0, 0.0),
                   "thigh_L": (-2, 0, -2), "thigh_R": (-2, 0, 2), "shin_L": (8, 0, 0), "shin_R": (8, 0, 0),
                   "upperarm_L": (-150, 20, -10), "upperarm_R": (-150, -20, 10), "forearm_L": (-40, 0, 0), "forearm_R": (-40, 0, 0)})
LUNGE_P = merge(STOOP, {"torso": (30, 0, 0), "head": (-40, 0, 0), "move": (0, -0.30, -0.10),
                        "thigh_L": (-90, 0, -4), "shin_L": (60, 0, 0), "thigh_R": (-10, 0, 4), "shin_R": (30, 0, 0),
                        "upperarm_L": (-110, 10, -15), "upperarm_R": (-110, -10, 15), "forearm_L": (-20, 0, 0), "forearm_R": (-20, 0, 0)})
LUNGE = [(0, STOOP), (10, merge(STOOP, {"torso": (16, 0, 0), "move": (0, 0.03, -0.05)})), (22, UP), (30, UP),
         (38, LUNGE_P), (60, LUNGE_P)]

# ---------------------------------------------------------------- the cart (cart.glb): put her root 0.40 m behind the cart's
# origin, facing the same way. Hands on the grip bar (angles from pose_solver.py, grips at (+-0.30, -0.38, 0.84)).
PUSH_BASE = merge(STOOP, {"move": (0, 0.06, -0.04), "torso": (18, 0, 0), "head": (-28, 0, 0),
                          "upperarm_L": (-20, -10, 10), "forearm_L": (-70, 0, 0), "upperarm_R": (-20, 10, -10), "forearm_R": (-70, 0, 0),
                          "thigh_L": (-34, 0, -2), "shin_L": (20, 0, 0), "thigh_R": (-6, 0, 2), "shin_R": (12, 0, 0)})
# stuck: straining against the cart, resting, straining again
PUSH = [(0, PUSH_BASE),
        (15, merge(PUSH_BASE, {"torso": (21, 0, 0), "head": (-32, 0, 0), "move": (0, 0.05, -0.05), "forearm_L": (-66, 0, 0), "forearm_R": (-66, 0, 0)})),
        (30, PUSH_BASE),
        (45, merge(PUSH_BASE, {"torso": (22, 0, 1), "head": (-33, 0, 0), "move": (0, 0.05, -0.05), "forearm_L": (-66, 0, 0), "forearm_R": (-66, 0, 0)})),
        (60, PUSH_BASE),
        (90, merge(PUSH_BASE, {"torso": (15, 0, 0), "head": (-20, 0, 10), "move": (0, 0.065, -0.035)})),
        (120, PUSH_BASE)]


def pstep(tl, sl, tr, sr):
    return merge(PUSH_BASE, {"thigh_L": (-20 + tl, 0, -2), "shin_L": (8 + sl, 0, 0), "thigh_R": (-20 + tr, 0, 2), "shin_R": (8 + sr, 0, 0)})


# pushing the freed cart away: short steps, 1.6 s a cycle (move the cart and her together at ~0.5 m/s)
P0, P1, P2, P3 = pstep(-14, 6, 10, 18), pstep(-6, 8, 14, 32), pstep(4, 6, -10, 40), pstep(10, 12, -14, 12)
PUSH_WALK = [(0, P0), (8, P1), (16, P2), (22, P3), (24, mirror(P0)), (32, mirror(P1)), (40, mirror(P2)), (46, mirror(P3)), (48, P0)]
# ---------------------------------------------------------------- the long neck: the body just stands there, arms limp.
# The game hides grandma_neck_skin, stretches a long neck from grandma_neck and moves/turns the head itself.
LIMP = merge(STOOP, {"upperarm_L": (-10, 2, 0), "upperarm_R": (-10, -2, 0), "forearm_L": (-4, 0, 0), "forearm_R": (-4, 0, 0)})
NECK_STRETCH = [(0, LIMP), (40, merge(LIMP, {"torso": (9, 0, 1.5), "forearm_L": (-6, 0, 0)})), (80, merge(LIMP, {"torso": (8, 0, -1)})),
                (120, LIMP)]

CLIPS = {
    "idle": (True, IDLE), "walk": (True, WALK), "beckon": (True, BECKON),
    "fallen": (True, FALLEN), "fallen_call": (True, FALLEN_CALL), "lunge": (False, LUNGE),
    "push": (True, PUSH), "push_walk": (True, PUSH_WALK), "neck_stretch": (True, NECK_STRETCH),
}

# ---------------------------------------------------------------- write the keys: one action per part per clip
for o in OBJ.values():
    ad = o.animation_data or o.animation_data_create()
    ad.action = None
    for t in list(ad.nla_tracks): ad.nla_tracks.remove(t)
for a in list(bpy.data.actions):
    if a.name.startswith(PFX.rstrip("_") + "|"): bpy.data.actions.remove(a)

for clip, (loop, keys) in CLIPS.items():
    for p, o in OBJ.items():
        act = bpy.data.actions.new(f"{PFX.rstrip('_')}|{clip}|{p}")
        o.animation_data.action = act
        for frame, pose in keys:
            r = pose.get(p, (0, 0, 0))
            o.rotation_euler = tuple(math.radians(x) for x in r)
            o.keyframe_insert("rotation_euler", frame=frame)
            if p == "torso":
                m = pose.get("move", (0, 0, 0))
                o.location = (HIPS.x + m[0], HIPS.y + m[1], HIPS.z + m[2])
                o.keyframe_insert("location", frame=frame)
        o.animation_data.action = None
        track = o.animation_data.nla_tracks.new(); track.name = clip
        track.strips.new(clip, 0, act)
        track.mute = True
    print(clip, "frames", keys[-1][0], "loop" if loop else "once")
for p, o in OBJ.items():
    o.rotation_euler = (0, 0, 0)
    if p == "torso": o.location = HIPS
bpy.context.scene.render.fps = FPS


def show(clip, frame=0):
    """preview one clip in the viewport: show('fallen', 30)"""
    for o in OBJ.values():
        for t in o.animation_data.nla_tracks: t.mute = t.name != clip
    bpy.context.scene.frame_set(frame); bpy.context.view_layer.update()
