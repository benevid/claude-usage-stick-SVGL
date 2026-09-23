#!/usr/bin/env python3
"""
gen_logo_assets.py — rasteriza os SVGs oficiais do Claude Code (assets/brand/)
em imagens LVGL (ARGB8888) embutidas no firmware.

Gera firmware/claude_stick/logo_assets.h com:
  img_wordmark   — logotipo "CLAUDE / CODE" p/ o header (h=26)
  img_clawd_sm   — Clawd pixel p/ o header (h=26)
  img_clawd_big  — Clawd p/ telas de token/loading (h=90)
  img_clawd_xl   — Clawd p/ as animações de limiar (w=176)
  + defines com os retângulos dos olhos do XL (p/ as expressões)

Requisitos: rsvg-convert (brew install librsvg) e Pillow.
"""
import os
import subprocess
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "assets", "brand")
OUT = os.path.join(ROOT, "firmware", "claude_stick", "logo_assets.h")

ICON_SVG = os.path.join(BRAND, "claudecode-color.svg")
TEXT_SVG = os.path.join(BRAND, "claudecode-text.svg")

CORAL = "#D97757"
# olhos do Clawd no viewBox 24x24 do claudecode-color.svg (buracos no path)
EYES_VB = [(6.0, 8.102, 7.488, 10.949), (16.51, 8.102, 18.0, 10.949)]
VB = 24.0


def render(svg: str, *, height: int | None = None, width: int | None = None,
           color: str | None = None) -> Image.Image:
    """rsvg-convert -> PIL RGBA (fundo transparente)."""
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tf:
        png = tf.name
    cmd = ["rsvg-convert", svg, "-o", png]
    if height:
        cmd += ["-h", str(height)]
    if width:
        cmd += ["-w", str(width)]
    css = None
    if color:
        css = tempfile.NamedTemporaryFile(suffix=".css", delete=False, mode="w")
        css.write(f"svg{{color:{color}}}")
        css.close()
        cmd += ["--stylesheet", css.name]
    subprocess.run(cmd, check=True)
    im = Image.open(png).convert("RGBA")
    os.unlink(png)
    if css:
        os.unlink(css.name)
    return im


def to_c(im: Image.Image, name: str) -> str:
    """PIL RGBA -> lv_image_dsc_t ARGB8888 (bytes B,G,R,A little-endian)."""
    w, h = im.size
    px = im.tobytes()  # RGBA
    data = bytearray()
    for i in range(0, len(px), 4):
        r, g, b, a = px[i], px[i + 1], px[i + 2], px[i + 3]
        data += bytes((b, g, r, a))
    rows = []
    for i in range(0, len(data), 20):
        rows.append(",".join(str(v) for v in data[i:i + 20]))
    body = ",\n  ".join(rows)
    return (
        f"static const uint8_t {name}_map[] = {{\n  {body}\n}};\n"
        f"static const lv_image_dsc_t {name} = {{\n"
        f"  {{ LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_ARGB8888, 0, {w}, {h}, {w * 4}, 0 }},\n"
        f"  sizeof({name}_map), {name}_map\n"
        f"}};\n"
    )


def clawd(target_w: int) -> tuple[Image.Image, list[tuple[int, int, int, int]]]:
    """Renderiza o Clawd, corta no conteúdo e devolve os retângulos dos olhos."""
    big = render(ICON_SVG, width=target_w * 2, height=target_w * 2)
    bbox = big.getbbox()
    big = big.crop(bbox)
    scale = target_w / big.width
    im = big.resize((target_w, max(1, round(big.height * scale))), Image.LANCZOS)
    s2 = (target_w * 2) / VB          # viewBox -> px do render 2x
    eyes = []
    for (x0, y0, x1, y1) in EYES_VB:
        ex0 = (x0 * s2 - bbox[0]) * scale
        ey0 = (y0 * s2 - bbox[1]) * scale
        ex1 = (x1 * s2 - bbox[0]) * scale
        ey1 = (y1 * s2 - bbox[1]) * scale
        eyes.append((round(ex0), round(ey0), round(ex1 - ex0), round(ey1 - ey0)))
    return im, eyes


def main() -> None:
    parts = ["// GERADO por tools/gen_logo_assets.py — nao editar na mao.",
             "#pragma once", "#include <lvgl.h>", ""]

    wm = render(TEXT_SVG, color=CORAL, height=64)
    wm = wm.crop(wm.getbbox())
    wm = wm.resize((round(wm.width * 36 / wm.height), 36), Image.LANCZOS)
    parts.append(to_c(wm, "img_wordmark"))
    print(f"wordmark: {wm.size}")

    sm, _ = clawd(58)                # header de 56px: 58x36
    parts.append(to_c(sm, "img_clawd_sm"))
    print(f"clawd_sm: {sm.size}")

    md, meyes = clawd(88)
    parts.append(to_c(md, "img_clawd_md"))
    print(f"clawd_md: {md.size} olhos={meyes}")
    for i, (x, y, w, h) in enumerate(meyes):
        parts.append(f"#define CLAWD_MD_EYE{i}_X {x}")
        parts.append(f"#define CLAWD_MD_EYE{i}_Y {y}")
        parts.append(f"#define CLAWD_MD_EYE{i}_W {w}")
        parts.append(f"#define CLAWD_MD_EYE{i}_H {h}")
    parts.append("")

    big, _ = clawd(144)
    parts.append(to_c(big, "img_clawd_big"))
    print(f"clawd_big: {big.size}")

    xl, eyes = clawd(176)
    parts.append(to_c(xl, "img_clawd_xl"))
    print(f"clawd_xl: {xl.size} olhos={eyes}")
    for i, (x, y, w, h) in enumerate(eyes):
        parts.append(f"#define CLAWD_XL_EYE{i}_X {x}")
        parts.append(f"#define CLAWD_XL_EYE{i}_Y {y}")
        parts.append(f"#define CLAWD_XL_EYE{i}_W {w}")
        parts.append(f"#define CLAWD_XL_EYE{i}_H {h}")
    parts.append("")

    with open(OUT, "w") as f:
        f.write("\n".join(parts))
    print(f"gerado: {OUT} ({os.path.getsize(OUT) // 1024} KB)")


if __name__ == "__main__":
    main()
