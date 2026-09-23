#!/usr/bin/env python3
"""
partner_logo.py — grava o logo de um parceiro num firmware JA COMPILADO.

    python3 tools/partner_logo.py firmware.bin parceiro.png            # patch in-place
    python3 tools/partner_logo.py firmware.bin parceiro.svg -o out.bin
    python3 tools/partner_logo.py firmware.bin --clear                 # volta ao wordmark
    python3 tools/partner_logo.py firmware.bin --info                  # mostra o slot

O firmware traz um slot constante na flash (firmware/claude_stick/partner_slot.h:
magic "USAGESTICK-LOGO\\0", w, h, 12 bytes reservados, pixels ARGB8888 em B,G,R,A
para no maximo 170x36). Este script:

  1. prepara a imagem (PNG com alpha ou SVG via rsvg-convert): recorta pelo
     alpha > 8, escala para 36px de altura mantendo proporcao, limita a 170px
     de largura;
  2. localiza o magic no .bin e escreve w, h e os pixels no slot;
  3. recalcula o que o bootloader do ESP32-S3 valida no boot: o byte de
     checksum (XOR de todos os bytes de segmento, semente 0xEF) e o SHA-256
     anexado ao app image. Sem isso a imagem alterada nao sobe.

Nao depende de esptool — o formato do app image e simples e esta descrito em
parse_image(). Requer Pillow; SVG requer rsvg-convert (brew install librsvg).
"""
import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

from PIL import Image

MAGIC = b"USAGESTICK-LOGO\0"
MAX_W, MAX_H = 170, 36
HDR = 32                          # magic(16) + w(2) + h(2) + reserved(12)
SLOT = HDR + MAX_W * MAX_H * 4


# ---------------------------------------------------------------- imagem
def load(path: str, height: int) -> Image.Image:
    ext = os.path.splitext(path)[1].lower()
    if ext == ".svg":
        with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tf:
            png = tf.name
        subprocess.run(["rsvg-convert", path, "-h", str(height * 2), "-o", png], check=True)
        im = Image.open(png).convert("RGBA")
        os.unlink(png)
        return im
    if ext == ".png":
        im = Image.open(path)
        if im.mode not in ("RGBA", "LA", "P"):
            print(f"aviso: {path} nao tem transparencia — o header e escuro, "
                  "um fundo branco vai aparecer como retangulo", file=sys.stderr)
        return im.convert("RGBA")
    raise SystemExit(f"erro: {path}: use .png ou .svg")


def fit(im: Image.Image, height: int = MAX_H, max_width: int = MAX_W) -> Image.Image:
    # bbox por alpha com limiar: sombras/brilhos quase invisiveis (alpha 1..8)
    # chegam a cobrir a imagem inteira e o logo sairia minusculo
    box = im.split()[3].point(lambda v: 255 if v > 8 else 0).getbbox()
    if not box:
        raise SystemExit("erro: imagem vazia (tudo transparente)")
    im = im.crop(box)
    w, h = im.size
    scale = height / h
    if w * scale > max_width:
        scale = max_width / w
    return im.resize((max(1, round(w * scale)), max(1, round(h * scale))), Image.LANCZOS)


def to_bgra(im: Image.Image) -> bytes:
    """RGBA -> bytes B,G,R,A (ARGB8888 little-endian do LVGL)."""
    r, g, b, a = im.split()
    return Image.merge("RGBA", (b, g, r, a)).tobytes()


# ---------------------------------------------------------------- app image
def parse_image(d: bytes):
    """Devolve (fim_dos_segmentos, hash_appended). Formato do app image ESP32:
    header 24 bytes (0xE9, n_seg, ..., entry, header estendido de 16 bytes cujo
    ultimo byte e hash_appended); n_seg segmentos de [addr u32][size u32][dados];
    padding ate (pos+1) % 16 == 0; 1 byte de checksum; e, se hash_appended,
    32 bytes de SHA-256 de tudo que veio antes."""
    if d[0] != 0xE9:
        raise SystemExit("erro: nao e um app image ESP32 (magic 0xE9)")
    nseg = d[1]
    hash_appended = d[23]
    p = 24
    for _ in range(nseg):
        size = struct.unpack_from("<I", d, p + 4)[0]
        p += 8 + size
    return p, hash_appended


def checksum(d: bytes, end: int) -> int:
    nseg = d[1]
    p = 24
    ck = 0xEF
    for _ in range(nseg):
        size = struct.unpack_from("<I", d, p + 4)[0]
        seg = d[p + 8:p + 8 + size]
        for b in seg:
            ck ^= b
        p += 8 + size
    return ck


def reseal(d: bytearray) -> None:
    """Recalcula checksum e SHA-256 depois de alterar dados de segmento."""
    end, hash_appended = parse_image(d)
    pad = (16 - (end + 1) % 16) % 16
    ck_pos = end + pad
    d[ck_pos] = checksum(d, end)
    if hash_appended:
        d[ck_pos + 1:ck_pos + 33] = hashlib.sha256(d[:ck_pos + 1]).digest()


def find_slot(d: bytes) -> int:
    i = d.find(MAGIC)
    if i < 0:
        raise SystemExit("erro: slot de logo nao encontrado — firmware antigo (sem partner_slot)?")
    if d.find(MAGIC, i + 1) >= 0:
        raise SystemExit("erro: magic do slot aparece mais de uma vez no .bin")
    return i


# ---------------------------------------------------------------- CLI
def main() -> None:
    ap = argparse.ArgumentParser(description="grava o logo de parceiro num firmware compilado")
    ap.add_argument("firmware", help="app image (.bin) compilado")
    ap.add_argument("logo", nargs="?", help="arquivo .png (com transparencia) ou .svg")
    ap.add_argument("-o", "--out", help="saida (padrao: sobrescreve o firmware)")
    ap.add_argument("--clear", action="store_true", help="esvazia o slot (volta ao wordmark)")
    ap.add_argument("--info", action="store_true", help="so mostra o estado do slot")
    ap.add_argument("--height", type=int, default=MAX_H)
    ap.add_argument("--max-width", type=int, default=MAX_W)
    a = ap.parse_args()

    d = bytearray(open(a.firmware, "rb").read())
    i = find_slot(d)
    w, h = struct.unpack_from("<HH", d, i + 16)
    if a.info:
        print(f"slot em 0x{i:x}: " + (f"logo {w}x{h}" if w else "vazio (wordmark)"))
        return

    if a.clear:
        d[i + 16:i + SLOT] = bytes(SLOT - 16)
        print("slot esvaziado")
    else:
        if not a.logo:
            ap.error("informe o logo (ou --clear / --info)")
        if not os.path.isfile(a.logo):
            raise SystemExit(f"erro: {a.logo}: arquivo nao encontrado")
        if a.height > MAX_H or a.max_width > MAX_W:
            raise SystemExit(f"erro: o slot suporta no maximo {MAX_W}x{MAX_H}")
        im = fit(load(a.logo, a.height), a.height, a.max_width)
        w, h = im.size
        px = to_bgra(im)
        d[i + 16:i + SLOT] = struct.pack("<HH", w, h) + bytes(12) + px + bytes(MAX_W * MAX_H * 4 - len(px))
        print(f"{os.path.basename(a.logo)} -> {w}x{h} no slot 0x{i:x}")

    reseal(d)
    out = a.out or a.firmware
    open(out, "wb").write(d)
    print(f"gravado: {out}")


if __name__ == "__main__":
    main()
