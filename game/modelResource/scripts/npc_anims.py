# Animation clips for the companion. Run inside Blender after npc_build.py (it builds the jointed model).
# Each clip becomes one glTF animation with the same name (an NLA track per part, merged by the exporter).
#
# HOW TO ADD A CLIP: add an entry to CLIPS below: (loop?, [(frame, pose), ...]) at 30 fps.
#   A pose is a dict  part -> (rx, ry, rz) in degrees, rotation of that joint relative to its parent,
#   plus optional "move": (dx, dy, dz) = offset of the hips from standing (meters, Blender axes).
#   Parts not listed in a pose stay at 0 (standing straight). Parts: torso, head, upperarm_L/R, forearm_L/R,
#   thigh_L/R, shin_L/R  (L = the character's left = +X; the character faces -Y).
#   Signs (all parts start pointing down/up in a straight standing pose):
#     torso rx + = lean forward       head rx + = look down          head rz + = turn to its left
#     thigh rx - = leg swings forward shin rx + = knee bends         upperarm rx - = arm swings forward/up
#     forearm rx - = elbow bends      upperarm_L ry - / upperarm_R ry + = arm out to the side
#   Then run this file again and export (npc_export.py).
import bpy, math

FPS = 30
PARTS = ["torso", "head", "upperarm_L", "upperarm_R", "forearm_L", "forearm_R", "thigh_L", "thigh_R", "shin_L", "shin_R"]
OBJ = {p: bpy.data.objects["npc_" + p] for p in PARTS}
from mathutils import Vector
HIPS = Vector((0, 0, 0.95))   # rest position of the hips above the root (npc_build.py PIV["torso"])


def mirror(pose):
    """left <-> right, for symmetric cycles (walk)"""
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


ARMS_DOWN = {"upperarm_L": (2, 3, 0), "upperarm_R": (2, -3, 0), "forearm_L": (-8, 0, 0), "forearm_R": (-8, 0, 0)}

# ---------------------------------------------------------------- idle: breathing, small weight shift
IDLE = [(0, merge(ARMS_DOWN, {})),
        (60, merge(ARMS_DOWN, {"torso": (1.5, 0, 0), "head": (-2, 0, 3), "move": (0, 0, 0.004),
                               "forearm_L": (-10, 0, 0), "forearm_R": (-11, 0, 0)})),
        (120, merge(ARMS_DOWN, {}))]

# ---------------------------------------------------------------- walk: one cycle = two steps in 1 s, in place
W0 = {"torso": (3, 0, 4), "head": (-2, 0, -4), "move": (0, 0, -0.015),          # left heel strikes
      "thigh_L": (-28, 0, 0), "shin_L": (5, 0, 0), "thigh_R": (18, 0, 0), "shin_R": (15, 0, 0),
      "upperarm_L": (18, 3, 0), "upperarm_R": (-18, -3, 0), "forearm_L": (-12, 0, 0), "forearm_R": (-28, 0, 0)}
W1 = {"torso": (3, 0, 2), "head": (-2, 0, -2), "move": (0, 0, -0.02),           # loading, right pushes off
      "thigh_L": (-22, 0, 0), "shin_L": (18, 0, 0), "thigh_R": (25, 0, 0), "shin_R": (35, 0, 0),
      "upperarm_L": (12, 3, 0), "upperarm_R": (-12, -3, 0), "forearm_L": (-12, 0, 0), "forearm_R": (-22, 0, 0)}
W2 = {"torso": (3, 0, 0), "move": (0, 0, 0.012),                                # passing: right leg swings through
      "thigh_L": (-2, 0, 0), "shin_L": (5, 0, 0), "thigh_R": (-20, 0, 0), "shin_R": (70, 0, 0),
      "upperarm_L": (0, 3, 0), "upperarm_R": (0, -3, 0), "forearm_L": (-12, 0, 0), "forearm_R": (-14, 0, 0)}
W3 = {"torso": (3, 0, -2), "head": (-2, 0, 2), "move": (0, 0, 0.004),          # right reaches forward
      "thigh_L": (12, 0, 0), "shin_L": (8, 0, 0), "thigh_R": (-32, 0, 0), "shin_R": (30, 0, 0),
      "upperarm_L": (-12, 3, 0), "upperarm_R": (12, -3, 0), "forearm_L": (-22, 0, 0), "forearm_R": (-12, 0, 0)}
WALK = [(0, W0), (4, W1), (8, W2), (11, W3), (15, mirror(W0)), (19, mirror(W1)), (23, mirror(W2)), (26, mirror(W3)), (30, W0)]

# ---------------------------------------------------------------- talk (standing): right hand gestures, nods
TALK = [(0, merge(ARMS_DOWN, {"upperarm_R": (-18, 0, 0), "forearm_R": (-70, 0, 15), "head": (0, 0, 0)})),
        (18, merge(ARMS_DOWN, {"upperarm_R": (-26, -6, 0), "forearm_R": (-85, 0, -20), "head": (5, 0, 4), "torso": (1, 0, -2)})),
        (36, merge(ARMS_DOWN, {"upperarm_R": (-16, 0, 0), "forearm_R": (-60, 0, 10), "upperarm_L": (-10, 3, 0),
                               "forearm_L": (-55, 0, -10), "head": (-2, 0, -3)})),
        (54, merge(ARMS_DOWN, {"upperarm_R": (-24, -4, 0), "forearm_R": (-80, 0, -10), "head": (4, 0, 2), "torso": (1, 0, 2)})),
        (72, merge(ARMS_DOWN, {"upperarm_R": (-18, 0, 0), "forearm_R": (-70, 0, 15)})),
        (90, merge(ARMS_DOWN, {"upperarm_R": (-18, 0, 0), "forearm_R": (-70, 0, 15), "head": (0, 0, 0)}))]

# ---------------------------------------------------------------- sit on a bench (seat 0.44 m above the feet)
# Put the root at the middle of the seat (front-to-back) on the floor, facing away from the backrest.
SIT_BASE = {"move": (0, 0.0, -0.39), "torso": (4, 0, 0), "thigh_L": (-88, 0, -4), "thigh_R": (-88, 0, 4),
            "shin_L": (86, 0, 4), "shin_R": (86, 0, -4),
            "upperarm_L": (-8, 4, 0), "upperarm_R": (-8, -4, 0), "forearm_L": (-58, 0, 0), "forearm_R": (-58, 0, 0)}
SIT = [(0, SIT_BASE), (60, merge(SIT_BASE, {"torso": (6, 0, 0), "head": (3, 0, 4)})), (120, SIT_BASE)]
SIT_TALK = [(0, merge(SIT_BASE, {"forearm_R": (-80, 0, 15), "upperarm_R": (-25, -4, 0)})),
            (20, merge(SIT_BASE, {"forearm_R": (-95, 0, -20), "upperarm_R": (-32, -8, 0), "head": (4, 0, -6), "torso": (5, 0, -3)})),
            (40, merge(SIT_BASE, {"forearm_R": (-75, 0, 10), "upperarm_R": (-24, -4, 0), "head": (-2, 0, -8)})),
            (60, merge(SIT_BASE, {"forearm_R": (-90, 0, -10), "upperarm_R": (-30, -6, 0), "forearm_L": (-70, 0, -10),
                                  "head": (3, 0, -4)})),
            (90, merge(SIT_BASE, {"forearm_R": (-80, 0, 15), "upperarm_R": (-25, -4, 0)}))]

# ---------------------------------------------------------------- crouch (นั่งย่อ): squat, arms on the knees
# (legs hang off the torso, so the torso's 32 deg lean is taken off the thighs: thigh -127 = -95 in the world)
CROUCH_BASE = {"move": (0, 0.24, -0.49), "torso": (32, 0, 0), "head": (-26, 0, 0),
               "thigh_L": (-127, 0, -10), "thigh_R": (-127, 0, 10), "shin_L": (120, 0, 6), "shin_R": (120, 0, -6),
               "upperarm_L": (-20, 0, -20), "upperarm_R": (-20, 0, 20), "forearm_L": (-70, 0, 0), "forearm_R": (-70, 0, 0)}
CROUCH = [(0, CROUCH_BASE), (60, merge(CROUCH_BASE, {"torso": (34, 0, 0), "thigh_L": (-129, 0, -10), "thigh_R": (-129, 0, 10),
                                                    "head": (-24, 0, 6), "move": (0, 0.24, -0.495)})),
          (120, CROUCH_BASE)]

# ---------------------------------------------------------------- prone (นอนหมอบ): face down, propped on the forearms
PRONE_BASE = {"move": (0, 0.0, -0.83), "torso": (90, 0, 0), "head": (-55, 0, 0),
              "upperarm_L": (-135, 0, 60), "upperarm_R": (-135, 0, -60), "forearm_L": (-90, 0, -30), "forearm_R": (-90, 0, 30),
              "thigh_L": (0, 0, -6), "thigh_R": (0, 0, 6), "shin_L": (8, 0, -70), "shin_R": (8, 0, 70)}
PRONE = [(0, PRONE_BASE), (60, merge(PRONE_BASE, {"move": (0, 0, -0.825), "head": (-52, 0, 4)})), (120, PRONE_BASE)]

# ---------------------------------------------------------------- board the bus: one step up into a low-floor bus
# One-shot with root motion in "move": ends 0.9 m forward (-Y) and 0.34 m up (the bus floor). Put the root on the
# road just outside the door, facing into the bus.
REACH = {"upperarm_L": (-80, 6, 0), "forearm_L": (-35, 0, 0)}        # left hand on the door's grab pole
BOARD = [(0, merge(ARMS_DOWN, {})),
         (10, merge(ARMS_DOWN, REACH, {"torso": (8, 0, 0), "head": (15, 0, 0), "thigh_R": (-48, 0, 0), "shin_R": (45, 0, 0)})),
         (20, merge(REACH, {"torso": (12, 0, 0), "head": (12, 0, 0), "move": (0, -0.22, 0.16),
                            "thigh_R": (-28, 0, 0), "shin_R": (25, 0, 0), "thigh_L": (22, 0, 0), "shin_L": (25, 0, 0),
                            "upperarm_R": (10, -3, 0), "forearm_R": (-15, 0, 0)})),
         (30, merge(REACH, {"torso": (8, 0, 0), "head": (8, 0, 0), "move": (0, -0.40, 0.34),
                            "thigh_R": (-4, 0, 0), "shin_R": (5, 0, 0), "thigh_L": (-30, 0, 0), "shin_L": (75, 0, 0)})),
         (40, merge(ARMS_DOWN, {"torso": (4, 0, 0), "move": (0, -0.62, 0.33),
                                "thigh_L": (-26, 0, 0), "shin_L": (6, 0, 0), "thigh_R": (16, 0, 0), "shin_R": (18, 0, 0)})),
         (50, merge(ARMS_DOWN, {"torso": (3, 0, 0), "move": (0, -0.80, 0.355), "thigh_R": (-20, 0, 0), "shin_R": (60, 0, 0),
                                "thigh_L": (-2, 0, 0), "shin_L": (5, 0, 0)})),
         (60, merge(ARMS_DOWN, {"move": (0, -0.90, 0.34)}))]

# ---------------------------------------------------------------- bus driver (bus.glb: put the root at bus_driver_seat)
# Seat 0.50 m above the cab floor, hands on the steering wheel (angles from pose_solver.py), feet on the pedals.
DRIVE_BASE = {"move": (0, 0, -0.33), "torso": (8, 0, 0), "head": (-6, 0, 0),
              "thigh_L": (-93, 0, -3), "thigh_R": (-93, 0, 3), "shin_L": (65, 0, 3), "shin_R": (65, 0, -3),
              "upperarm_L": (-60, 10, 0), "forearm_L": (-25, 0, -15), "upperarm_R": (-60, -10, 0), "forearm_R": (-25, 0, 15)}
DRIVE = [(0, DRIVE_BASE), (60, merge(DRIVE_BASE, {"head": (-5, 0, 6)})), (120, DRIVE_BASE)]
# turned toward the front door (on his left), left hand off the wheel, talking to someone at the stop
ASK_BASE = merge(DRIVE_BASE, {"torso": (8, 0, -8), "head": (-5, 0, 55),
                              "upperarm_L": (-30, 20, 40), "forearm_L": (-95, 0, 0), "upperarm_R": (-75, -20, 10), "forearm_R": (0, 0, 0)})
DRIVE_ASK = [(0, ASK_BASE), (20, merge(ASK_BASE, {"forearm_L": (-80, 0, 15), "head": (0, 0, 52)})),
             (40, merge(ASK_BASE, {"head": (-8, 0, 58)})), (60, ASK_BASE)]

CLIPS = {
    "idle": (True, IDLE), "walk": (True, WALK), "talk": (True, TALK), "sit": (True, SIT), "sit_talk": (True, SIT_TALK),
    "crouch": (True, CROUCH), "prone": (True, PRONE), "board_bus": (False, BOARD),
    "drive": (True, DRIVE), "drive_ask": (True, DRIVE_ASK),
}

# ---------------------------------------------------------------- write the keys: one action per part per clip
for o in OBJ.values():
    ad = o.animation_data or o.animation_data_create()
    ad.action = None
    for t in list(ad.nla_tracks): ad.nla_tracks.remove(t)
for a in list(bpy.data.actions):
    if a.name.startswith("npc|"): bpy.data.actions.remove(a)

for clip, (loop, keys) in CLIPS.items():
    for p, o in OBJ.items():
        act = bpy.data.actions.new(f"npc|{clip}|{p}")
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
# back to the standing pose
for p, o in OBJ.items():
    o.rotation_euler = (0, 0, 0)
    if p == "torso": o.location = HIPS
bpy.context.scene.render.fps = FPS
