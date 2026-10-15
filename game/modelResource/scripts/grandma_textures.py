# Textures for the old woman (ยาย): faded blouse, striped tube skirt (ผ้าถุง), aged skin, grey hair,
# and two faces: grandma_face (an ordinary tired old face) and grandma_face_scary (pallid, sunken white eyes,
# jaw hanging open). Run inside Blender before grandma_build.py. Saves PNGs to textures/ and packs them.
# Face layout matches the NPC faces: u = 0.5 + 0.395*x, v = 0.5 + 0.458*z on the unit head sphere (eyes at
# u 0.5 +- 0.165, v 0.512; mouth v 0.26). The scary head's jaw is stretched in the mesh, its UVs are not.
import bpy, numpy as np

OUT = "/Users/smarttapon/coding_prac/grdc-horror/game/modelResource/textures/"


def fractal(N, seed, ax=1.0, ay=1.0, beta=1.8):
    r = np.random.default_rng(seed); w = r.standard_normal((N, N))
    f = np.fft.fftfreq(N)*N; fu, fv = np.meshgrid(f, f)
    k = np.sqrt((fu/ax)**2 + (fv/ay)**2); amp = np.where(k >= 1, np.maximum(k, 1e-6)**(-beta), 0)
    n = np.real(np.fft.ifft2(np.fft.fft2(w)*amp)); return (n-n.mean())/(n.std()+1e-9)


def save(name, rgb):
    H, W = rgb.shape[:2]
    im = bpy.data.images.get(name) or bpy.data.images.new(name, W, H)
    if im.size[0] != W: im.scale(W, H)
    px = np.ones((H, W, 4), np.float32); px[..., :3] = np.clip(rgb, 0, 1); im.pixels[:] = px.ravel()
    im.filepath_raw = OUT + name + ".png"; im.file_format = "PNG"; im.save(); im.pack()


def grid(N):
    u = np.tile((np.arange(N)+0.5)/N, (N, 1)); return u, u.T


def ss(e0, e1, x): t = np.clip((x-e0)/(e1-e0), 0, 1); return t*t*(3-2*t)


# ---------------------------------------------------------------- cloth
N = 128
u, v = grid(N)
r = np.random.default_rng(60)
blouse = np.array((0.74, 0.71, 0.62)) * (1 + 0.035*fractal(N, 61))[..., None]
blouse *= (1 - 0.10*np.clip(fractal(N, 62, ax=6, ay=0.6), 0, 3)/3)[..., None]          # creases hanging down
for _ in range(70):                                                                       # small faded flowers
    cu, cv = r.random(2); col = np.array([(0.55, 0.6, 0.7), (0.72, 0.55, 0.55)][r.integers(2)])
    du = np.minimum(abs(u-cu), 1-abs(u-cu)); dv = np.minimum(abs(v-cv), 1-abs(v-cv))
    m = 0.55*(1 - ss(0.5, 1.2, np.sqrt(du**2 + dv**2)*N/2.2))
    blouse = blouse*(1-m[..., None]) + col*m[..., None]
blouse *= (1 - 0.12*np.clip(fractal(N, 63, beta=2.2), 0, 3)/3)[..., None]                # grime
save("grandma_blouse", blouse)

# tube skirt: dark indigo-brown with woven bands (period 1/4 of the tile = 12.5 cm)
band = (v*4) % 1
skirt = np.ones((N, N, 3)) * (0.16, 0.12, 0.13)
skirt = np.where((band < 0.12)[..., None], (0.30, 0.18, 0.12), skirt)
skirt = np.where(((band > 0.18) & (band < 0.24))[..., None], (0.12, 0.13, 0.22), skirt)
zig = (np.abs(((u*16) % 1) - 0.5) * 0.1 + 0.55 < band) & (band < 0.75)                  # mudmee-like zigzag
skirt = np.where(zig[..., None], skirt*1.35, skirt)
skirt *= (1 + 0.06*fractal(N, 64, ax=0.4, ay=4))[..., None]                               # weft streaks
skirt *= (1 + 0.05*fractal(N, 65))[..., None]
save("grandma_skirt", skirt)

save("grandma_hair", np.array((0.56, 0.55, 0.53)) * (1 + 0.32*fractal(64, 66, ax=6, ay=0.4, beta=1.4))[..., None])
SKIN = np.array((0.64, 0.50, 0.41))
sk = np.ones((64, 64, 3)) * SKIN * (1 + 0.05*fractal(64, 67))[..., None]
sk *= (1 - 0.15*np.clip(fractal(64, 68, beta=1.4) - 1.2, 0, 2)/2)[..., None]               # age spots
save("grandma_skin", sk)
save("grandma_sandal", np.array((0.70, 0.71, 0.70)) * (1 + 0.05*fractal(64, 69))[..., None])

# ---------------------------------------------------------------- faces
N = 256
u, v = grid(N)


def ell(cu, cv, ru, rv, soft=0.25):
    d = np.sqrt(((u-cu)/ru)**2 + ((v-cv)/rv)**2); return 1 - ss(1-soft, 1+soft, d)


def mix(img, col, m): return img*(1-m[..., None]) + np.array(col)*m[..., None]


def line(img, p0, p1, w, col, a):
    """soft stroke from p0 to p1 (uv), width w"""
    p0, p1 = np.array(p0), np.array(p1); d = p1 - p0; L2 = d @ d
    t = np.clip(((u-p0[0])*d[0] + (v-p0[1])*d[1]) / L2, 0, 1)
    dist = np.sqrt((u - p0[0] - t*d[0])**2 + (v - p0[1] - t*d[1])**2)
    return mix(img, col, a*(1 - ss(0.3*w, w, dist)))


def wrinkles(img, skin, k):
    """the lines every old face shares; k = depth"""
    for z in (0.64, 0.675, 0.71, 0.745):                                                   # forehead
        img = line(img, (0.34, z - 0.012), (0.5, z + 0.004), 0.005, skin*0.8, 0.4*k)
        img = line(img, (0.5, z + 0.004), (0.66, z - 0.010), 0.005, skin*0.8, 0.4*k)
    for s in (-1, 1):
        cu = 0.5 + s*0.165
        for a in (-0.03, 0.0, 0.03):                                                       # crow's feet
            img = line(img, (cu + s*0.075, 0.512 + a*0.5), (cu + s*0.115, 0.512 + a*1.4), 0.004, skin*0.75, 0.6*k)
        img = line(img, (cu - 0.05, 0.478), (cu + 0.05, 0.470), 0.005, skin*0.72, 0.6*k)     # bags under the eyes
        img = line(img, (cu - 0.04, 0.458), (cu + 0.055, 0.448), 0.005, skin*0.8, 0.4*k)
        img = line(img, (0.5 + s*0.07, 0.385), (0.5 + s*0.135, 0.255), 0.009, skin*0.68, 0.8*k)   # nose to mouth
        img = line(img, (0.5 + s*0.11, 0.24), (0.5 + s*0.12, 0.13), 0.008, skin*0.72, 0.6*k)     # marionette
        img = line(img, (0.5 + s*0.24, 0.40), (0.5 + s*0.21, 0.24), 0.012, skin*0.8, 0.5*k)      # sunken cheek
    for x in (-0.02, 0.0, 0.02):                                                           # frown between the brows
        img = line(img, (0.5 + x, 0.55), (0.5 + x*1.3, 0.60), 0.004, skin*0.75, 0.45*k)
    return img


def base_face(skin, seed):
    r = np.random.default_rng(seed)
    img = np.ones((N, N, 3)) * skin * (1 + 0.035*r.standard_normal((N, N)))[..., None]
    for _ in range(18):                                                                    # liver spots
        cu, cv = 0.25 + 0.5*r.random(), 0.3 + 0.5*r.random()
        img = mix(img, skin*0.82, 0.5*ell(cu, cv, 0.008 + 0.01*r.random(), 0.008 + 0.01*r.random(), 0.7))
    for s in (-1, 1): img = mix(img, skin*0.82, 0.55*ell(0.5+s*0.05, 0.46, 0.02, 0.06, 0.7))   # nose sides
    for s in (-1, 1): img = mix(img, (0.22, 0.13, 0.1), 0.75*ell(0.5+s*0.028, 0.415, 0.014, 0.008, 0.4))  # nostrils
    return wrinkles(img, skin, 1.0)


# ordinary face: tired half-closed eyes, thin pressed lips, sparse grey brows
img = base_face(SKIN, 70)
for s in (-1, 1):
    cu = 0.5 + s*0.165
    img = mix(img, SKIN*0.74, 0.55*ell(cu, 0.515, 0.12, 0.055, 0.6))                      # soft sockets
    img = mix(img, (0.70, 0.66, 0.60), ell(cu, 0.508, 0.05, 0.010, 0.25))                  # narrow eye
    img = mix(img, (0.10, 0.07, 0.05), ell(cu - s*0.004, 0.506, 0.018, 0.010, 0.3))         # iris under the lid
    img = mix(img, SKIN*0.55, 0.9*ell(cu, 0.522, 0.06, 0.008, 0.5))                         # heavy upper lid
    img = line(img, (cu - 0.07, 0.565 - 0.005*s), (cu + 0.07, 0.575 + 0.002*s), 0.008, (0.55, 0.53, 0.5), 0.7)  # brows
img = mix(img, SKIN*0.88, 0.4*ell(0.5, 0.40, 0.07, 0.03, 0.6))
img = mix(img, (0.45, 0.28, 0.26), 0.55*ell(0.5, 0.262, 0.075, 0.010, 0.5))                 # thin lips
img = mix(img, (0.20, 0.10, 0.09), 0.85*ell(0.5, 0.262, 0.08, 0.0035, 0.5))
for x in (-0.04, -0.015, 0.015, 0.04):                                                     # pursed-lip lines
    img = line(img, (0.5 + x, 0.275), (0.5 + x*1.1, 0.295), 0.003, SKIN*0.78, 0.5)
save("grandma_face", img)

# scary face (after the user's Ghost House stills): rotten, puffy, deeply folded flesh, mottled grey-brown with dark
# lesions, small pale eyes sunk in swollen lids (they glow in the model), the jaw hanging open on a black hole with a
# few broken brown teeth, lips cracked and dark
ROT = np.array((0.50, 0.42, 0.35))                     # the neck and ears keep her own skin; the face has gone bad
img = base_face(ROT, 71)
r = np.random.default_rng(73)
img *= (1 + 0.16*fractal(N, 74, beta=1.3))[..., None]                                       # mottling
img = mix(img, (0.30, 0.25, 0.20), 0.5*np.clip(fractal(N, 75, beta=1.7) - 0.6, 0, 1))      # dark rot patches
img = mix(img, (0.62, 0.58, 0.50), 0.35*np.clip(fractal(N, 76, beta=1.5) - 1.0, 0, 1))     # pale dead skin
for _ in range(14):                                                                         # lesions and scabs
    cu, cv = 0.22 + 0.56*r.random(), 0.15 + 0.7*r.random()
    rr = 0.006 + 0.014*r.random()
    img = mix(img, (0.22, 0.12, 0.09), 0.75*ell(cu, cv, rr, rr*(0.6 + 0.8*r.random()), 0.5))


def fold(img, cu, cv, ru, rv, a0, a1, w, k=1.0):
    """a deep fold of skin: a dark crease with a lighter ridge just above it"""
    for da, col, a in ((0.0, ROT*0.35, 0.85*k), (0.008, ROT*1.25, 0.45*k)):
        n = 10
        pts = [(cu + ru*np.cos(a0 + (a1-a0)*t/n), cv + da + rv*np.sin(a0 + (a1-a0)*t/n)) for t in range(n+1)]
        for p0, p1 in zip(pts, pts[1:]): img = line(img, p0, p1, w, col, a)
    return img


def lump(img, cu, cv, ru, rv, k=1.0):
    """a swelling lit from above: pale on top, a dark crease under it"""
    img = mix(img, ROT*0.45, 0.7*k*ell(cu, cv - rv*0.55, ru*1.05, rv*0.7, 0.7))
    return mix(img, ROT*1.3, 0.55*k*ell(cu, cv + rv*0.15, ru, rv*0.8, 0.8))


img = wrinkles(img, ROT, 1.0)
for _ in range(60):                                                                         # the whole face lumpy
    cu, cv = 0.2 + 0.6*r.random(), 0.05 + 0.75*r.random()
    if abs(cu - 0.5) < 0.12 and 0.08 < cv < 0.32: continue                                  # not over the mouth
    img = lump(img, cu, cv, 0.025 + 0.03*r.random(), 0.02 + 0.025*r.random(), 0.7)
for s in (-1, 1):                                                                           # heavy swellings: cheeks, bags, brow
    img = lump(img, 0.5 + s*0.24, 0.30, 0.07, 0.09)
    img = lump(img, 0.5 + s*0.17, 0.43, 0.08, 0.035)
    img = lump(img, 0.5 + s*0.18, 0.395, 0.07, 0.03, 0.8)
    img = lump(img, 0.5 + s*0.16, 0.60, 0.09, 0.035)
for z in (0.64, 0.70, 0.76): img = fold(img, 0.5, z - 0.18, 0.2, 0.18, 1.1, 2.05, 0.010, 0.6)   # forehead
for s in (-1, 1):
    cu = 0.5 + s*0.165
    for k, rv in enumerate((0.05, 0.075, 0.10)):                                            # swollen bags, ring on ring
        img = fold(img, cu, 0.52, 0.07 + 0.02*k, rv, np.pi*1.15, np.pi*1.85, 0.011, 0.7 - 0.15*k)
    img = fold(img, cu, 0.49, 0.075, 0.06, np.pi*0.15, np.pi*0.85, 0.007)                  # heavy upper lid
    for k in range(3):                                                                      # cheek folds round the mouth
        img = fold(img, 0.5, 0.20, 0.17 + 0.04*k, 0.20 + 0.03*k, np.pi*(0.5 + s*0.22), np.pi*(0.5 + s*0.42), 0.012, 0.7)
    img = mix(img, (0.05, 0.04, 0.04), ell(cu, 0.51, 0.065, 0.045, 0.35))                   # deep sockets
    img = mix(img, (0.80, 0.78, 0.66), ell(cu, 0.51, 0.030, 0.026, 0.2))                   # small pale eyes
    img = line(img, (cu - 0.08*s, 0.585), (cu + 0.05*s, 0.565), 0.008, (0.80, 0.80, 0.76), 0.5)   # thin white brows
for k in range(4):                                                                          # puckered chin
    img = fold(img, 0.5, 0.0 + 0.02*k, 0.08 + 0.02*k, 0.06, np.pi*0.2, np.pi*0.8, 0.006, 0.6)
img = mix(img, ROT*0.6, 0.5*ell(0.5, 0.40, 0.07, 0.03, 0.6))
# mouth from v 0.08 to 0.30: cracked dark lips round a black hole
img = mix(img, (0.20, 0.10, 0.09), ell(0.5, 0.19, 0.125, 0.118, 0.15))
img = mix(img, (0.012, 0.008, 0.008), ell(0.5, 0.19, 0.105, 0.098, 0.15))
for i, x in enumerate(np.linspace(-0.08, 0.08, 9)):                                         # upper teeth: few, brown, broken
    if i in (1, 2, 5, 7): continue
    h = 0.02 + 0.03*r.random()
    img = mix(img, (0.45, 0.36, 0.22), ell(0.5 + x, 0.285 - h/2, 0.009, h/2, 0.35))
for i, x in enumerate(np.linspace(-0.065, 0.065, 7)):                                       # lower teeth
    if i in (0, 2, 3, 6): continue
    h = 0.02 + 0.02*r.random()
    img = mix(img, (0.40, 0.32, 0.20), ell(0.5 + x, 0.095 + h/2, 0.008, h/2, 0.35))
for a in np.linspace(0.3, 2.85, 9):                                                         # cracks in the lips
    img = line(img, (0.5 + 0.11*np.cos(a), 0.19 + 0.105*np.sin(a)), (0.5 + 0.13*np.cos(a), 0.19 + 0.125*np.sin(a)),
               0.004, (0.08, 0.04, 0.04), 0.8)
save("grandma_face_scary", img)
print("grandma textures ok")
