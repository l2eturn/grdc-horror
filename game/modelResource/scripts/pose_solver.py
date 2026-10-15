# Helper for writing poses in npc_anims.py: finds shoulder/elbow angles that put a hand on a target point.
# Run inside Blender with the NPC built (npc_build.py), then call:
#   solve_hand(move=(0, 0, -0.33), torso=(8, 0, 0), target=(0.19, -0.55, 0.86), side="L")
# move/torso are the pose's "move" and "torso" values; target is in the NPC's own space (root on the ground,
# facing -Y, meters). Prints and returns (error_m, upperarm_angles, forearm_angles) in degrees.
import bpy, math
from mathutils import Vector as V, Matrix, Euler

# other characters: set SOLVER_COL / SOLVER_PFX / SOLVER_HIPS / SOLVER_PALM before exec (the grandma: "Grandma",
# "grandma_", 0.80, palm (0.013, -0.025, -0.26) from the elbow)
_PFX = globals().get("SOLVER_PFX", "npc_")
_O = {o.name[len(_PFX):]: o for o in bpy.data.collections[globals().get("SOLVER_COL", "NPC")].objects if o.name.startswith(_PFX)}
_HIPS = globals().get("SOLVER_HIPS", 0.95)
_PALM = globals().get("SOLVER_PALM", (0.017, -0.03, -0.29))


def _R(r): return Euler(tuple(math.radians(x) for x in r), "XYZ").to_matrix().to_4x4()


def hand_and_elbow(move, torso, up, fo, side):
    M = Matrix.Translation(V((0, 0, _HIPS)) + V(move)) @ _R(torso) @ Matrix.Translation(_O["upperarm_" + side].location) @ _R(up)
    elbow = (M @ Matrix.Translation(_O["forearm_" + side].location)).translation
    M = M @ Matrix.Translation(_O["forearm_" + side].location) @ _R(fo)
    s = 1 if side == "L" else -1
    return elbow, M @ V((s*_PALM[0], _PALM[1], _PALM[2]))      # middle of the palm


def solve_hand(move, torso, target, side="L", elbow_max_z=None):
    best = None
    for ux in range(-180, 31, 5):
        for uy in range(-30, 31, 10):
            for uz in range(-60, 61, 10):
                for fx in range(-140, 1, 5):
                    for fz in (-30, -15, 0, 15, 30):
                        e, h = hand_and_elbow(move, torso, (ux, uy, uz), (fx, 0, fz), side)
                        cost = (h - V(target)).length
                        if elbow_max_z is not None: cost += max(0, e.z - elbow_max_z)
                        if best is None or cost < best[0]: best = (cost, (ux, uy, uz), (fx, 0, fz))
    print(side, "error %.3f m" % best[0], "upperarm", best[1], "forearm", best[2])
    return best
