#!/usr/bin/env python3
"""
gen_social.py — gera a imagem de social preview do repositorio em
assets/social-preview.png (1280x640, o tamanho recomendado pelo GitHub).

E uma peca promocional, nao uma tela do device: usa a paleta e as scanlines do
SITE (papel claro, coral, linhas 1px a cada 3px), igual aos banner-*.png. A
tela do produto entra como um mockup dentro do device — reaproveitada de
assets/mock-agora.png, que sai do gen_mockups.py; rode-o antes se ela mudou.

Desenhado em 2x sobre uma base de 640x320, entao o arquivo final tem 1280x640 e
as scanlines de 2px com periodo 6 voltam a ser 1px a cada 3px quando o GitHub
exibe o cartao em ~640px.

Ao contrario dos banners, os cantos sao retos: o cartao preenche a moldura do
GitHub e do Twitter. O template oficial (repository-open-graph-template.png)
marca 40pt de borda segura, que nesta base de 640x320 sao 40 unidades: nada que
precise ser lido pode sair do retangulo (40, 40)-(600, 280). O que passa disso
e so sangria — a faixa coral do topo.

Requisitos: rsvg-convert + Pillow (mesmos do gen_banners.py).
"""
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "assets", "brand")
OUT = os.path.join(ROOT, "assets")

# paleta do site (a mesma do gen_banners.py)
PAPER, RAISED, SUNKEN = "#F5F3EF", "#FBFAF8", "#ECE8E1"
INK, INK_MUTED, INK_FAINT = "#1A1A20", "#5F5A52", "#8A8478"
RULE, RULE_STRONG = "#E3DFD8", "#CDC7BC"
CORAL, CORAL_DEEP, CORAL_WASH = "#D97757", "#B4614A", "#FBEEE8"

# o corpo do device usa o preto do firmware, nao o do texto
SHELL, SHELL_EDGE = "#141419", "#2C2C36"

SANS = "/System/Library/Fonts/HelveticaNeue.ttc"
MONO = "/System/Library/Fonts/Menlo.ttc"

S = 2  # fator de escala: desenhamos em 2x


def sans(size, bold=False):
    return ImageFont.truetype(SANS, size, index=1 if bold else 0)


def mono(size, bold=False):
    return ImageFont.truetype(MONO, size, index=1 if bold else 0)


def clawd(w):
    """Clawd oficial do assets/brand — nunca redesenhar a mao."""
    png = tempfile.mktemp(suffix=".png")
    subprocess.run(["rsvg-convert", os.path.join(BRAND, "claudecode-color.svg"),
                    "-w", str(w * 2), "-h", str(w * 2), "-o", png], check=True)
    im = Image.open(png).convert("RGBA")
    im = im.crop(im.getbbox())
    return im.resize((w, round(im.height * w / im.width)), Image.LANCZOS)


def scanlines(im):
    """O vidro CRT do site — vai por ultimo, sobre tudo, inclusive sobre a tela."""
    glass = Image.new("RGBA", im.size, (0, 0, 0, 0))
    g = ImageDraw.Draw(glass)
    for y in range(0, im.height, 3 * S):
        g.rectangle([0, y, im.width, y + S - 1], fill=(0, 0, 0, 41))                # 16%
        g.rectangle([0, y + S, im.width, y + 2 * S - 1], fill=(255, 255, 255, 10))  # 4%
    return Image.alpha_composite(im.convert("RGBA"), glass)


def kicker(d, x, y, txt, color=CORAL_DEEP, size=9):
    """Rotulo tecnico: mono, maiusculas, bem espacado — a assinatura do site."""
    f = mono(size * S)
    cx = x
    for ch in txt.upper():
        d.text((cx, y), ch, font=f, fill=color)
        cx += d.textlength(ch, font=f) + 2.6 * S
    return cx


def sombra(im, caixa, raio, desfoque=9, alpha=54):
    """Sombra difusa por baixo do device — o unico volume da peca."""
    x0, y0, x1, y1 = caixa
    camada = Image.new("RGBA", im.size, (0, 0, 0, 0))
    ImageDraw.Draw(camada).rounded_rectangle([x0, y0, x1, y1], radius=raio,
                                             fill=(26, 20, 16, alpha))
    camada = camada.filter(ImageFilter.GaussianBlur(desfoque * S))
    return Image.alpha_composite(im, camada)


# ── o device ────────────────────────────────────────────────────────────────
def tela_mock():
    """A tela vem do mock-agora.png; ele e o retrato canonico da tela 'Agora'."""
    caminho = os.path.join(OUT, "mock-agora.png")
    if not os.path.exists(caminho):
        raise SystemExit("assets/mock-agora.png nao existe — rode tools/gen_mockups.py antes")
    return Image.open(caminho).convert("RGBA")


def device(im, d, x, y, w):
    """
    Device de frente: a tela 480x320 dentro do case, apoiado na mesa.

    Sem pescoco nem pedestal — com eles a peca vira um monitor de desktop, e o
    que se vende aqui e um objeto pequeno de mesa. O volume vem so da sombra de
    contato logo abaixo do corpo.

    x, y sao o canto superior esquerdo do corpo; w e a largura do corpo. A
    altura sai da proporcao 3:2 da tela mais o bezel, para a tela nunca
    distorcer.
    """
    bezel = 11
    tw = w - 2 * bezel
    th = round(tw * 320 / 480)
    h = th + 2 * bezel

    corpo = [x * S, y * S, (x + w) * S, (y + h) * S]
    im = sombra(im, [corpo[0] + 10 * S, corpo[1] + 14 * S,
                     corpo[2] - 10 * S, corpo[3] + 10 * S], 18 * S)
    d = ImageDraw.Draw(im)

    # sombra de contato: fina, colada na base, e o que apoia o objeto na mesa
    contato = Image.new("RGBA", im.size, (0, 0, 0, 0))
    ImageDraw.Draw(contato).ellipse([(x + 6) * S, (y + h - 3) * S,
                                     (x + w - 6) * S, (y + h + 9) * S],
                                    fill=(26, 20, 16, 88))
    im = Image.alpha_composite(im, contato.filter(ImageFilter.GaussianBlur(4 * S)))
    d = ImageDraw.Draw(im)

    d.rounded_rectangle(corpo, radius=13 * S, fill=SHELL, outline=SHELL_EDGE, width=1 * S)

    tela = tela_mock().resize((tw * S, th * S), Image.LANCZOS)
    mascara = Image.new("L", tela.size, 0)
    ImageDraw.Draw(mascara).rounded_rectangle([0, 0, tela.width - 1, tela.height - 1],
                                              radius=5 * S, fill=255)
    im.paste(tela, ((x + bezel) * S, (y + bezel) * S), mascara)

    # USB-C na lateral esquerda, do lado em que ele fica de verdade (flush 270 CW)
    d.rounded_rectangle([(x - 2) * S, (y + h / 2 - 7) * S, (x + 2) * S, (y + h / 2 + 7) * S],
                        radius=2 * S, fill=SHELL_EDGE)
    return im, h


# ── textos ──────────────────────────────────────────────────────────────────
# Mesma logica dos banners: um desenho so, muda apenas o que esta escrito.
TEXTOS = {
    "en": {
        "sufixo": "",
        "kicker": "ESP32-S3 · LVGL · no backend",
        "titulo": ["Claude Usage", "Stick"],
        "corpo": [
            "A desk gadget that shows your Claude Code",
            "rate limits at a glance — 5-hour and weekly",
            "windows, per-model status, history and heatmap.",
        ],
        "specs": "3.5\" touch · 480×320 · the token never leaves the device",
        "rodape": "Open firmware — flash it yourself, or from the browser",
    },
    "pt": {
        "sufixo": "-pt",
        "kicker": "ESP32-S3 · LVGL · sem backend",
        "titulo": ["Claude Usage", "Stick"],
        "corpo": [
            "Um gadget de mesa que mostra seus limites do",
            "Claude Code de relance — janelas de 5 horas e",
            "semanal, status por modelo, histórico e heatmap.",
        ],
        "specs": "Touch de 3,5\" · 480×320 · o token não sai do device",
        "rodape": "Firmware aberto — grave você mesmo, ou pelo navegador",
    },
}


def social(t):
    W, H = 640, 320
    im = Image.new("RGBA", (W * S, H * S), PAPER)
    d = ImageDraw.Draw(im)

    # faixa coral no topo: a unica marca de cor que sobrevive ao thumbnail
    d.rectangle([0, 0, W * S, 3 * S], fill=CORAL)

    x = 44
    kicker(d, x * S, 52 * S, t["kicker"])

    f_tit = sans(38 * S, bold=True)
    for i, ln in enumerate(t["titulo"]):
        d.text((x * S, (72 + i * 42) * S), ln, font=f_tit, fill=INK)

    for i, ln in enumerate(t["corpo"]):
        d.text((x * S, (168 + i * 20) * S), ln, font=sans(13 * S), fill=INK_MUTED)

    # legenda tecnica: o registro de que isto e hardware real. Fica logo abaixo
    # do corpo, e nao no pe do cartao, para nao cair na faixa que o GitHub corta
    d.line([x * S, 230 * S, (x + 300) * S, 230 * S], fill=RULE, width=1 * S)
    d.text((x * S, 238 * S), t["specs"], font=mono(8 * S), fill=INK_FAINT)

    # a URL, tratada como o elemento que a pessoa deve levar embora
    d.rounded_rectangle([x * S, 248 * S, (x + 232) * S, 274 * S], radius=7 * S,
                        fill=CORAL_WASH, outline=CORAL, width=1 * S)
    d.text(((x + 14) * S, 254 * S), "usagestick.autom.my",
           font=mono(12 * S, bold=True), fill=CORAL_DEEP)

    marca = clawd(15 * S)
    im.paste(marca, ((x + 246) * S, 254 * S), marca)
    d.text(((x + 268) * S, 253 * S), t["rodape"], font=sans(10 * S), fill=INK_FAINT)

    # device centrado na vertical da area segura (40..280), colado na margem direita
    im, alt = device(im, d, 356, 76, 240)
    d = ImageDraw.Draw(im)

    nome = f"social-preview{t['sufixo']}.png"
    scanlines(im).convert("RGB").save(os.path.join(OUT, nome), "PNG", optimize=True)
    print(f"  {nome}  {im.width}x{im.height}")


def main():
    print("gerando social preview:")
    for t in TEXTOS.values():
        social(t)


if __name__ == "__main__":
    main()
