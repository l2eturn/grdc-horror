# Cloth textures for the companion (Jimmy) that the night scene needs darker and worn: his khaki jacket and the
# shoes. Run inside Blender before npc_build.py (it replaces the packed images npc_jacket / npc_shoe in place;
# the other npc_* images stay as they are in npc_source.blend). Tileable, world-metre UVs (tile = 0.5 m).
import bpy, numpy as np

OUT = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/textures/"


def fractal(N, seed, ax=1.0, ay=1.0, beta=1.8):
    r = np.random.default_rng(seed); w = r.standard_normal((N, N))
    f = np.fft.fftfreq(N)*N; fu, fv = np.meshgrid(f, f)
    k = np.sqrt((fu/ax)**2 + (fv/ay)**2); amp = np.where(k >= 1, np.maximum(k, 1e-6)**(-beta), 0)
    n = np.real(np.fft.ifft2(np.fft.fft2(w)*amp)); return (n-n.mean())/(n.std()+1e-9)


def save(name, rgb):
    H, W = rgb.shape[:2]
    old = bpy.data.images.get(name)          # a packed image keeps its old packed bytes after a pixel write:
    if old: bpy.data.images.remove(old)       # start fresh (npc_build.py relinks the materials by name)
    im = bpy.data.images.new(name, W, H)
    px = np.ones((H, W, 4), np.float32); px[..., :3] = np.clip(rgb, 0, 1); im.pixels[:] = px.ravel()
    im.filepath_raw = OUT + name + ".png"; im.file_format = "PNG"; im.save(); im.pack()


N = 128
# jacket: faded olive-khaki cotton (was a near-white cream that glowed in the fog), vertical fold shading,
# a fine weave, damp darker blotches and grime
jacket = np.array((0.36, 0.34, 0.27)) * (1 + 0.035*fractal(N, 51, beta=0.9))[..., None]          # weave
jacket *= (1 + 0.10*fractal(N, 52, ax=0.35, ay=3.0))[..., None]                                   # folds (vertical)
jacket *= (1 - 0.18*np.clip(fractal(N, 53, beta=2.3), 0, 3)/3)[..., None]                         # damp blotches
jacket *= (1 - 0.10*np.clip(fractal(N, 54, ax=6, ay=0.5), 0, 3)/3)[..., None]                     # creases across
save("npc_jacket", jacket)

# shoes: canvas sneakers gone grey-brown with mud
n = 64
shoe = np.array((0.42, 0.40, 0.36)) * (1 + 0.05*fractal(n, 55, beta=1.0))[..., None]
mud = np.clip(fractal(n, 56, beta=2.0), 0, 3)/3
shoe = shoe*(1 - 0.6*mud[..., None]) + 0.6*mud[..., None]*np.array((0.16, 0.12, 0.09))
save("npc_shoe", shoe)
print("npc textures ok")
