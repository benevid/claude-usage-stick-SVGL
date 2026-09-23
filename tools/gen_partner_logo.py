#!/usr/bin/env python3
"""
gen_partner_logo.py — logo de parceiro (PNG ou SVG) -> partner_logo.h para o build.

Chamado pelo firmware/claude_stick/build.sh quando recebe --logo; o header vai
para um diretorio temporario e entra no sketch por -I + -DPARTNER_LOGO, entao
nada gerado fica dentro de firmware/claude_stick/.

    python3 tools/gen_partner_logo.py parceiro.png -o /tmp/x/partner_logo.h

A imagem e recortada pelo alpha, escalada para HEIGHT px de altura (a faixa do
wordmark no header) e, se ainda passar de MAX_WIDTH, escalada pela largura.
Sai em ARGB8888 (mesmo formato do logo_assets.h) como `img_partner`, mais os
defines PARTNER_LOGO_W / PARTNER_LOGO_H que o .ino usa para posicionar.

SVG exige rsvg-convert (brew install librsvg), como o gen_logo_assets.py.
"""
import argparse
import os
import sys

from PIL import Image

from gen_logo_assets import render, to_c

HEIGHT = 26       # altura do img_wordmark que o logo substitui
MAX_WIDTH = 120   # ate x=186: deixa o badge de conta e o botao de refresh (x=202) livres


def load(path: str, height: int) -> Image.Image:
    ext = os.path.splitext(path)[1].lower()
    if ext == ".svg":
        return render(path, height=height * 2)     # 2x e reduz depois: bordas mais limpas
    if ext == ".png":
        im = Image.open(path)
        if im.mode not in ("RGBA", "LA", "P"):
            print(f"aviso: {path} nao tem transparencia — o header e escuro, "
                  "um fundo branco vai aparecer como retangulo", file=sys.stderr)
        return im.convert("RGBA")
    raise SystemExit(f"erro: {path}: use .png ou .svg")


def fit(im: Image.Image, height: int, max_width: int) -> Image.Image:
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
    size = (max(1, round(w * scale)), max(1, round(h * scale)))
    return im.resize(size, Image.LANCZOS)


def main() -> None:
    ap = argparse.ArgumentParser(description="logo de parceiro -> partner_logo.h")
    ap.add_argument("logo", help="arquivo .png (com transparencia) ou .svg")
    ap.add_argument("-o", "--out", required=True, help="caminho do partner_logo.h")
    ap.add_argument("--height", type=int, default=HEIGHT)
    ap.add_argument("--max-width", type=int, default=MAX_WIDTH)
    a = ap.parse_args()

    if not os.path.isfile(a.logo):
        raise SystemExit(f"erro: {a.logo}: arquivo nao encontrado")
    im = fit(load(a.logo, a.height), a.height, a.max_width)
    w, h = im.size

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w") as f:
        f.write(f"// GERADO por tools/gen_partner_logo.py a partir de {os.path.basename(a.logo)} — nao editar.\n")
        f.write("#pragma once\n#include <lvgl.h>\n\n")
        f.write(f"#define PARTNER_LOGO_W {w}\n#define PARTNER_LOGO_H {h}\n\n")
        f.write(to_c(im, "img_partner"))
    print(f"{os.path.basename(a.logo)} -> {w}x{h}")


if __name__ == "__main__":
    main()
