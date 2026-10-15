#!/usr/bin/env python3
"""Audit grdc-horror .glb models against the art rules (game-comcept.md §1.1) and what terEngine's glTF loader
actually uses. Standard library only.

    python3 audit_glb.py                      # every game/assets/models/*.glb
    python3 audit_glb.py bus grandma          # by name (or pass paths)
    python3 audit_glb.py --json ...           # machine-readable

Budgets per model live in budgets.json next to this script. Exit code 1 when any FAIL is found.
"""
import json, math, re, struct, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]                      # .claude/skills/model-review/scripts -> repo root
GAME = REPO / "game"
MODELS = GAME / "assets" / "models"

COMP = {5120: ("b", 1, 127.0), 5121: ("B", 1, 255.0), 5122: ("h", 2, 32767.0), 5123: ("H", 2, 65535.0),
        5125: ("I", 4, None), 5126: ("f", 4, None)}
NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
SUPPORTED_EXT = {"EXT_meshopt_compression", "KHR_materials_emissive_strength", "KHR_texture_transform"}


def load_glb(path):
    data = path.read_bytes()
    magic, _ver, _len = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF":
        raise ValueError("not a GLB file")
    off, gltf, binc = 12, None, b""
    while off < len(data):
        clen, ctype = struct.unpack_from("<I4s", data, off)
        chunk = data[off + 8: off + 8 + clen]
        if ctype == b"JSON":
            gltf = json.loads(chunk)
        elif ctype == b"BIN\x00":
            binc = chunk
        off += 8 + clen
    return gltf, binc


def view_bytes(g, binc, vi):
    v = g["bufferViews"][vi]
    o = v.get("byteOffset", 0)
    return binc[o: o + v["byteLength"]]


def read_accessor(g, binc, ai):
    """values as a list of tuples, normalized ints mapped to 0..1 (None when sparse / compressed)"""
    a = g["accessors"][ai]
    if "bufferView" not in a or "sparse" in a:
        return None
    v = g["bufferViews"][a["bufferView"]]
    if "extensions" in v:          # meshopt-compressed view: skip reading
        return None
    fmt, size, norm = COMP[a["componentType"]]
    n = NCOMP[a["type"]]
    stride = v.get("byteStride") or size * n
    base = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    out = []
    for i in range(a["count"]):
        vals = struct.unpack_from("<" + fmt * n, binc, base + i * stride)
        if a.get("normalized") and norm:
            vals = tuple(x / norm for x in vals)
        out.append(vals)
    return out


def image_size(b):
    if b[:8] == b"\x89PNG\r\n\x1a\n":
        return struct.unpack(">II", b[16:24])
    if b[:2] == b"\xff\xd8":
        i = 2
        while i < len(b):
            if b[i] != 0xFF:
                i += 1
                continue
            m = b[i + 1]
            if m in (0xC0, 0xC1, 0xC2):
                h, w = struct.unpack(">HH", b[i + 5: i + 9])
                return w, h
            i += 2 + struct.unpack(">H", b[i + 2: i + 4])[0]
    return None


# ------------------------------------------------------------------ transforms for world-space bounds
def mat_mul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def node_matrix(n):
    if "matrix" in n:
        m = n["matrix"]
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    tx, ty, tz = n.get("translation", [0, 0, 0])
    x, y, z, w = n.get("rotation", [0, 0, 0, 1])
    sx, sy, sz = n.get("scale", [1, 1, 1])
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[r[0][0] * sx, r[0][1] * sy, r[0][2] * sz, tx],
            [r[1][0] * sx, r[1][1] * sy, r[1][2] * sz, ty],
            [r[2][0] * sx, r[2][1] * sy, r[2][2] * sz, tz],
            [0, 0, 0, 1]]


def world_nodes(g):
    """(node index, world matrix) for every node reachable from the default scene"""
    nodes = g.get("nodes", [])
    scene = g.get("scenes", [{}])[g.get("scene", 0)] if g.get("scenes") else {"nodes": list(range(len(nodes)))}
    I = [[1 if r == c else 0 for c in range(4)] for r in range(4)]
    out, stack = [], [(i, I) for i in scene.get("nodes", [])]
    while stack:
        i, parent = stack.pop()
        m = mat_mul(parent, node_matrix(nodes[i]))
        out.append((i, m))
        stack.extend((c, m) for c in nodes[i].get("children", []))
    return out


# ------------------------------------------------------------------ source references
def referenced_literals():
    """node names the game looks up: string literals on findNode(...) lines in game/src, plus lists of two or
    more names on one line (e.g. `for (auto w : {"bus_wheel_fl", "bus_wheel_fr"}) findNode(...)`)"""
    lits = set()
    for f in list((GAME / "src").glob("*.cpp")) + list((GAME / "src").glob("*.h")):
        for line in f.read_text(errors="ignore").splitlines():
            found = re.findall(r'"([A-Za-z][A-Za-z0-9_]*_[A-Za-z0-9_]+)"', line)
            if "findNode" in line or len(found) >= 2:
                lits.update(found)
    return lits


# ------------------------------------------------------------------ audit
def audit(path, budgets, lits):
    name = path.stem
    cfg = budgets["models"].get(name)
    issues = []

    def add(level, msg):
        issues.append((level, msg))

    if cfg is None:
        add("WARN", f"no budget entry for '{name}' in budgets.json — add one (category + texMax)")
        cfg = {"category": "medium"}
    cat = budgets["categories"][cfg["category"]]
    tri_min, tri_max = cfg.get("tris", cat["tris"])
    tex_max = cfg.get("texMax", cat["texMax"])

    g, binc = load_glb(path)

    for ext in g.get("extensionsRequired", []):
        if ext not in SUPPORTED_EXT:
            add("FAIL", f"requires extension {ext}, which terEngine does not load")

    # ---- geometry
    tris = verts = prims = 0
    no_ao, flat_ao, no_uv_tex = [], [], []
    mats = g.get("materials", [])
    for mi, mesh in enumerate(g.get("meshes", [])):
        mname = mesh.get("name", f"mesh{mi}")
        for p in mesh["primitives"]:
            prims += 1
            attrs = p["attributes"]
            nv = g["accessors"][attrs["POSITION"]]["count"]
            verts += nv
            mode = p.get("mode", 4)
            ni = g["accessors"][p["indices"]]["count"] if "indices" in p else nv
            if mode == 4:
                tris += ni // 3
            elif mode in (5, 6):
                tris += max(0, ni - 2)
            if "COLOR_0" not in attrs:
                no_ao.append(mname)
            else:
                cols = read_accessor(g, binc, attrs["COLOR_0"])
                if cols and min(min(c[:3]) for c in cols) > 0.97:
                    flat_ao.append(mname)
            m = mats[p["material"]] if "material" in p and p["material"] < len(mats) else {}
            if "baseColorTexture" in m.get("pbrMetallicRoughness", {}) and "TEXCOORD_0" not in attrs:
                no_uv_tex.append(mname)

    if tris > tri_max * 1.15:
        add("FAIL", f"{tris} tris — over the {cfg['category']} budget {tri_min}–{tri_max}")
    elif tris > tri_max:
        add("WARN", f"{tris} tris — slightly over the {cfg['category']} budget {tri_min}–{tri_max}")
    elif tris < tri_min:
        add("INFO", f"{tris} tris — under the {cfg['category']} range {tri_min}–{tri_max}; room for silhouette detail")
    if cfg.get("ao") is False:
        no_ao, flat_ao = [], []
    if no_ao:
        add("WARN", f"no vertex colour (baked AO) on: {', '.join(sorted(set(no_ao))[:6])}")
    if flat_ao:
        add("WARN", f"vertex colour is all white (AO not baked?) on: {', '.join(sorted(set(flat_ao))[:6])}")
    if no_uv_tex:
        add("FAIL", f"textured but no TEXCOORD_0: {', '.join(sorted(set(no_uv_tex))[:6])}")
    if prims > budgets.get("maxPrimitives", 40):
        add("WARN", f"{prims} primitives (draw calls) — merge parts that share a material and never move")

    # ---- materials
    for m in mats:
        mn = m.get("name", "?")
        if m.get("doubleSided") and m.get("alphaMode") == "MASK" and not cfg.get("foliage"):
            add("FAIL", f"material {mn}: doubleSided + MASK is lit as foliage in mesh.frag (cutouts glow) — "
                        "make it single-sided with flipped duplicate faces")
        if m.get("alphaMode") == "BLEND":
            add("WARN", f"material {mn}: alphaMode BLEND (sorting issues in fog) — prefer MASK")
        unused = [k for k in ("normalTexture", "occlusionTexture") if k in m]
        if "metallicRoughnessTexture" in m.get("pbrMetallicRoughness", {}):
            unused.append("metallicRoughnessTexture")
        if unused:
            add("WARN", f"material {mn}: {', '.join(unused)} — terEngine ignores these, wasted bytes")

    # ---- textures
    for s in g.get("samplers", []):
        if s.get("magFilter") == 9728 or s.get("minFilter") in (9728, 9984, 9986):
            add("FAIL", f"sampler '{s.get('name', '?')}' uses NEAREST filtering — the art rules want bilinear (no PS1 look)")
    big = []
    for im in g.get("images", []):
        if "bufferView" not in im:
            add("WARN", f"image {im.get('name', '?')} is external (uri) — will not travel with the glb")
            continue
        sz = image_size(view_bytes(g, binc, im["bufferView"]))
        if not sz:
            continue
        w, h = sz
        iname = im.get("name", "?")
        limit = cfg.get("texMaxByImage", {}).get(iname, tex_max)
        if max(w, h) > limit * 2:
            add("FAIL", f"texture {iname} {w}×{h} — far over the {limit}px limit")
        elif max(w, h) > limit:
            big.append(f"{iname} {w}×{h}")
        if w & (w - 1) or h & (h - 1):
            add("WARN", f"texture {iname} {w}×{h} is not a power of two")
    if big:
        add("WARN", f"textures over {tex_max}px: {', '.join(big[:6])}" + (" …" if len(big) > 6 else ""))

    # ---- bounds (world space, glTF Y up)
    lo, hi = [math.inf] * 3, [-math.inf] * 3
    nodes = g.get("nodes", [])
    for i, m in world_nodes(g):
        if "mesh" not in nodes[i]:
            continue
        for p in g["meshes"][nodes[i]["mesh"]]["primitives"]:
            a = g["accessors"][p["attributes"]["POSITION"]]
            if "min" not in a:
                continue
            for cx in (a["min"][0], a["max"][0]):
                for cy in (a["min"][1], a["max"][1]):
                    for cz in (a["min"][2], a["max"][2]):
                        w = [sum(m[r][k] * v for k, v in enumerate((cx, cy, cz, 1))) for r in range(3)]
                        lo = [min(x, y) for x, y in zip(lo, w)]
                        hi = [max(x, y) for x, y in zip(hi, w)]
    dims = [round(h - l, 3) for l, h in zip(lo, hi)] if lo[0] != math.inf else None
    if dims and "height" in cfg:
        hmin, hmax = cfg["height"]
        if dims[1] > hmax * 1.5 or dims[1] < hmin / 1.5:
            add("FAIL", f"height {dims[1]} m — far outside the expected {hmin}–{hmax} m (scale or units wrong?)")
        elif not hmin <= dims[1] <= hmax:
            add("WARN", f"height {dims[1]} m outside the expected {hmin}–{hmax} m — check proportions")

    # ---- node names the game looks up
    names = {n.get("name", "") for n in nodes}
    prefixes = {n.split("_")[0] + "_" for n in names if "_" in n}
    prefixes &= set(cfg.get("nodePrefixes", [name + "_"])) | {name + "_"}
    missing = sorted(l for l in lits if any(l.startswith(p) for p in prefixes) and l not in names
                     and not any(l == p.rstrip("_") for p in prefixes))
    # only literals that look like node names of this model and are not material/texture/clip names
    known = {m.get("name") for m in mats} | {im.get("name") for im in g.get("images", [])} | \
        {a.get("name") for a in g.get("animations", [])}
    missing = [l for l in missing if l not in known]
    if missing:
        add("FAIL", f"node names the game looks up but the glb lacks: {', '.join(missing[:8])} "
                    "(renamed or deleted part? the game finds parts by name)")

    anims = [(a.get("name", "?"), len(a.get("channels", []))) for a in g.get("animations", [])]
    return {
        "model": name, "file": str(path.relative_to(REPO)) if path.is_relative_to(REPO) else str(path), "category": cfg["category"],
        "kb": round(path.stat().st_size / 1024), "tris": tris, "verts": verts, "primitives": prims,
        "materials": len(mats), "textures": len(g.get("images", [])), "dims_m": dims,
        "nodes": len(nodes), "animations": anims,
        "issues": [{"level": l, "msg": m} for l, m in issues],
    }


def main(argv):
    as_json = "--json" in argv
    args = [a for a in argv if not a.startswith("--")]
    budgets = json.loads((HERE / "budgets.json").read_text())
    paths = []
    for a in args or sorted(p.stem for p in MODELS.glob("*.glb")):
        p = Path(a)
        paths.append(p if p.suffix == ".glb" else MODELS / f"{a}.glb")
    lits = referenced_literals()
    reports = [audit(p, budgets, lits) for p in paths]
    fails = sum(1 for r in reports for i in r["issues"] if i["level"] == "FAIL")
    if as_json:
        print(json.dumps(reports, ensure_ascii=False, indent=1))
    else:
        for r in reports:
            d = "×".join(f"{x:g}" for x in r["dims_m"]) if r["dims_m"] else "?"
            head = "FAIL" if any(i["level"] == "FAIL" for i in r["issues"]) else \
                "WARN" if any(i["level"] == "WARN" for i in r["issues"]) else "OK"
            print(f"[{head}] {r['model']} ({r['category']}) — {r['tris']} tris, {r['primitives']} prims, "
                  f"{r['textures']} tex, {r['kb']} KB, size {d} m (x×y×z, y up)")
            if r["animations"]:
                print("       clips: " + ", ".join(f"{n}({c}ch)" for n, c in r["animations"]))
            for i in r["issues"]:
                print(f"   {i['level']:4}  {i['msg']}")
        print(f"\n{len(reports)} model(s), {fails} FAIL")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
