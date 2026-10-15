# Textures for the bus driver variant of the NPC: light-blue uniform shirt, navy trousers, a moustache and greying hair.
# Run inside Blender before npc_build.py with TEX_OVERRIDE = DRIVER_TEX (see the bottom of this file).
import bpy, numpy as np

OUT = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/textures/"


def fractal(N, seed, ax=1.0, ay=1.0, beta=1.8):
    r = np.random.default_rng(seed); w = r.standard_normal((N, N))
    f = np.fft.fftfreq(N)*N; fu, fv = np.meshgrid(f, f)
    k = np.sqrt((fu/ax)**2 + (fv/ay)**2); amp = np.where(k >= 1, np.maximum(k, 1e-6)**(-beta), 0)
    n = np.real(np.fft.ifft2(np.fft.fft2(w)*amp)); return (n-n.mean())/(n.std()+1e-9)


def save(name, rgb):
    H, W = rgb.shape[:2]
    old = bpy.data.images.get(name)          # a packed image keeps its old packed bytes after a pixel write
    if old: bpy.data.images.remove(old)
    im = bpy.data.images.new(name, W, H)
    px = np.ones((H, W, 4), np.float32); px[..., :3] = np.clip(rgb, 0, 1); im.pixels[:] = px.ravel()
    im.filepath_raw = OUT + name + ".png"; im.file_format = "PNG"; im.save(); im.pack()


N = 128
# washed-out uniform blue, darker than a new shirt so it doesn't glow behind the cab glass; sweat/grime blotches
shirt = np.array((0.40, 0.49, 0.58)) * (1 + 0.03*fractal(N, 41))[..., None]
shirt *= (1 - 0.12*np.clip(fractal(N, 42, ax=8, ay=0.7), 0, 3)/3)[..., None]
shirt *= (1 - 0.12*np.clip(fractal(N, 47, beta=2.3), 0, 3)/3)[..., None]
save("driver_shirt", shirt)
u = np.tile(np.arange(N)/N, (N, 1)); v = u.T
placket = np.array((0.37, 0.46, 0.55)) * (1 + 0.03*fractal(N, 43))[..., None]
buttons = (np.abs(u - 0.5) < 0.05) & ((v*8) % 1 < 0.12)
placket[buttons] = (0.85, 0.85, 0.82)
save("driver_placket", placket)
save("driver_trousers", np.array((0.11, 0.13, 0.2)) * (1 + 0.05*fractal(N, 44))[..., None])
save("driver_hair", np.array((0.26, 0.25, 0.24)) * (1 + 0.3*fractal(64, 45, ax=6, ay=0.4, beta=1.4))[..., None])
save("driver_skin", np.array((0.66, 0.47, 0.36)) * (1 + 0.03*fractal(64, 46))[..., None])

# face: clean-shaven except a moustache, heavier brows, a few lines, darker tan
N = 256
u = np.tile((np.arange(N)+0.5)/N, (N, 1)); v = u.T


def ss(e0, e1, x): t = np.clip((x-e0)/(e1-e0), 0, 1); return t*t*(3-2*t)
def ell(cu, cv, ru, rv, soft=0.25):
    d = np.sqrt(((u-cu)/ru)**2 + ((v-cv)/rv)**2); return 1 - ss(1-soft, 1+soft, d)
def mix(img, col, m): return img*(1-m[..., None]) + np.array(col)*m[..., None]


r = np.random.default_rng(47)
skin = np.array((0.66, 0.47, 0.36))
img = np.ones((N, N, 3)) * skin * (1 + 0.03*r.standard_normal((N, N)))[..., None]
for s in (-1, 1): img = mix(img, skin*0.75, 0.6*ell(0.5+s*0.165, 0.52, 0.13, 0.06, 0.6))
img = mix(img, skin*0.8, 0.5*ell(0.5, 0.40, 0.07, 0.025, 0.6))
for s in (-1, 1): img = mix(img, skin*0.85, 0.5*ell(0.5+s*0.05, 0.46, 0.02, 0.06, 0.7))
for s in (-1, 1): img = mix(img, (0.22, 0.13, 0.1), 0.8*ell(0.5+s*0.028, 0.415, 0.016, 0.009, 0.4))
for s in (-1, 1): img = mix(img, skin*0.82, 0.5*ell(0.5+s*0.15, 0.33, 0.012, 0.06, 0.8))     # nose-to-mouth lines
img = mix(img, skin*0.85, 0.35*ss(0.0, 0.2, 0.2 - v))                                            # shaved jaw shadow
img = mix(img, (0.2, 0.17, 0.15), 0.85*ell(0.5, 0.31, 0.11, 0.022, 0.4))                         # moustache
img = mix(img, (0.42, 0.25, 0.22), 0.6*ell(0.5, 0.25, 0.08, 0.014, 0.4))
img = mix(img, (0.16, 0.08, 0.07), 0.85*ell(0.5, 0.262, 0.085, 0.005, 0.5))
for s in (-1, 1):
    cu = 0.5 + s*0.165
    img = mix(img, (0.78, 0.74, 0.68), ell(cu, 0.512, 0.058, 0.018, 0.2))
    img = mix(img, (0.16, 0.11, 0.08), ell(cu, 0.512, 0.022, 0.018, 0.25))
    img = mix(img, (0.03, 0.02, 0.02), ell(cu, 0.512, 0.01, 0.01, 0.3))
    img = mix(img, (0.1, 0.07, 0.05), 0.9*ell(cu, 0.531, 0.066, 0.008, 0.5))
    img = mix(img, (0.2, 0.18, 0.16), 0.95*ell(cu, 0.572, 0.09, 0.017, 0.4))
for z in (0.66, 0.70): img = mix(img, skin*0.88, 0.5*ell(0.5, z, 0.15, 0.004, 0.6))               # forehead lines
save("driver_face", img)

DRIVER_TEX = {"jacket": "driver_shirt", "tee": "driver_placket", "shorts": "driver_trousers", "skin": "driver_skin",
              "face": "driver_face", "hair": "driver_hair"}
print("driver textures ok")
