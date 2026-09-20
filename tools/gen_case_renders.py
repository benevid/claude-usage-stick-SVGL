#!/usr/bin/env python3
"""
gen_case_renders.py — renderiza os STL de "3D Case/" em assets/case-*.png.

O GitHub ate mostra STL num visualizador 3D, mas so ao abrir o arquivo; no README
o leitor precisa ver a peca antes de decidir baixar. Este script e um rasterizador
minimo (projecao ortografica + z-buffer + sombreamento plano), sem dependencia de
trimesh/OpenGL: so numpy + Pillow, como os outros gen_*.py.

Paleta escura do device (mesma dos mock-*.png), peca em coral. Desenhado em 2x e
reduzido com LANCZOS — e o antialiasing.

Cada entrada de RENDERS pode juntar varios STL: as pecas do case articulado foram
exportadas ja na posicao de montagem, entao renderiza-las juntas mostra o conjunto.
"""
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = os.path.join(ROOT, "3D Case")
OUT = os.path.join(ROOT, "assets")

BG = (0x14, 0x14, 0x13)          # C_BG do firmware
CORAL = (0xD9, 0x77, 0x57)       # C_ACCENT
PAPER = (0xE8, 0xE6, 0xDC)
MUTED = (0x8A, 0x88, 0x80)

W, H, SS = 800, 600, 2           # saida, supersampling
MARGIN = 0.10

STL_DTYPE = np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])

# (saida, [(stl, cor)], azimute, elevacao, eixo "para cima" do modelo)
RENDERS = [
    ("case-simples.png", [("Case_JC3248W535C.stl", CORAL)], -28, 18, "y"),
    ("case-articulado.png", [
        ("Articulado/UsageStick-Base.stl", MUTED),
        ("Articulado/UsageStick-Juncao.stl", PAPER),
        ("Articulado/UsageStick-Case.stl", CORAL),
        ("Articulado/UsageStick-Case-Display-Holder.stl", CORAL),
    ], 145, 20, "z"),
]


def load_stl(path):
    with open(path, "rb") as f:
        data = f.read()
    n = int.from_bytes(data[80:84], "little")
    if len(data) != 84 + 50 * n:
        raise SystemExit(f"{path}: nao e STL binario")
    return np.frombuffer(data, dtype=STL_DTYPE, offset=84, count=n)["v"].astype(np.float64)


def rot(az, el):
    a, e = np.radians(az), np.radians(el)
    ry = np.array([[np.cos(a), 0, np.sin(a)], [0, 1, 0], [-np.sin(a), 0, np.cos(a)]])
    rx = np.array([[1, 0, 0], [0, np.cos(e), -np.sin(e)], [0, np.sin(e), np.cos(e)]])
    return rx @ ry


def render(parts, az, el, up):
    tris, cols = [], []
    for name, col in parts:
        t = load_stl(os.path.join(CASES, name))
        if up == "z":                       # converte para Y-para-cima
            t = t[..., [0, 2, 1]] * [1, 1, -1]
        tris.append(t)
        cols.append(np.tile(np.array(col, dtype=np.float64), (len(t), 1)))
    tris, cols = np.concatenate(tris), np.concatenate(cols)

    tris = tris @ rot(az, el).T             # camera olha por -Z; +Z = perto
    nrm = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    ln = np.linalg.norm(nrm, axis=1)
    keep = ln > 1e-12
    tris, cols, nrm = tris[keep], cols[keep], nrm[keep] / ln[keep, None]

    # luz principal + preenchimento; abs() porque ha STL com normal invertida
    key = np.array([-0.45, 0.65, 0.62]); key /= np.linalg.norm(key)
    fill = np.array([0.7, 0.1, 0.5]); fill /= np.linalg.norm(fill)
    shade = 0.22 + 0.62 * np.abs(nrm @ key) + 0.22 * np.clip(nrm @ fill, 0, 1)
    rgb = np.clip(cols * shade[:, None], 0, 255)

    w, h = W * SS, H * SS
    lo, hi = tris[..., :2].reshape(-1, 2).min(0), tris[..., :2].reshape(-1, 2).max(0)
    scale = min(w * (1 - 2 * MARGIN) / (hi[0] - lo[0]), h * (1 - 2 * MARGIN) / (hi[1] - lo[1]))
    ctr = (lo + hi) / 2
    px = (tris[..., 0] - ctr[0]) * scale + w / 2
    py = h / 2 - (tris[..., 1] - ctr[1]) * scale
    pz = tris[..., 2]

    img = np.empty((h, w, 3), dtype=np.float64); img[:] = BG
    zbuf = np.full((h, w), -np.inf)
    for i in range(len(tris)):
        x0, x1, x2 = px[i]; y0, y1, y2 = py[i]
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-9:
            continue
        xa, xb = max(int(min(x0, x1, x2)), 0), min(int(max(x0, x1, x2)) + 1, w - 1)
        ya, yb = max(int(min(y0, y1, y2)), 0), min(int(max(y0, y1, y2)) + 1, h - 1)
        if xa > xb or ya > yb:
            continue
        gx, gy = np.meshgrid(np.arange(xa, xb + 1) + 0.5, np.arange(ya, yb + 1) + 0.5)
        l0 = ((y1 - y2) * (gx - x2) + (x2 - x1) * (gy - y2)) / den
        l1 = ((y2 - y0) * (gx - x2) + (x0 - x2) * (gy - y2)) / den
        l2 = 1 - l0 - l1
        z = l0 * pz[i, 0] + l1 * pz[i, 1] + l2 * pz[i, 2]
        zs = zbuf[ya:yb + 1, xa:xb + 1]
        m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0) & (z > zs)
        zs[m] = z[m]
        img[ya:yb + 1, xa:xb + 1][m] = rgb[i]

    return Image.fromarray(img.astype(np.uint8)).resize((W, H), Image.LANCZOS)


def main():
    for out, parts, az, el, up in RENDERS:
        render(parts, az, el, up).save(os.path.join(OUT, out), optimize=True)
        print("ok", out)


if __name__ == "__main__":
    main()
