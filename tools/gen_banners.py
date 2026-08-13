#!/usr/bin/env python3
"""
gen_banners.py — gera os banners horizontais do README em assets/banner-*.png.

Sao dois:
  banner-steps.png  os tres passos ("como funciona"), logo no inicio do README
  banner-web.png    o cartao do gravador web (usagestick.autom.my)

Ao contrario dos mock-*.png, que reproduzem a tela do device (paleta escura),
estes usam a paleta e as scanlines do SITE — papel claro, coral, linhas 1px a
cada 3px. E o site que eles anunciam, entao o visual e o de la.

Desenhado em 2x e exibido a ~800px no GitHub, entao tudo aqui vale metade na
tela: a scanline de 2px com periodo 6 vira 1px a cada 3px, que e a spec.

Requisitos: rsvg-convert + Pillow (mesmos do gen_mockups.py).
"""
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "assets", "brand")
OUT = os.path.join(ROOT, "assets")

# paleta do site (frontend/src/index.css)
PAPER, RAISED, SUNKEN = "#F5F3EF", "#FBFAF8", "#ECE8E1"
INK, INK_MUTED, INK_FAINT = "#1A1A20", "#5F5A52", "#8A8478"
RULE, RULE_STRONG = "#E3DFD8", "#CDC7BC"
CORAL, CORAL_DEEP, CORAL_WASH = "#D97757", "#B4614A", "#FBEEE8"
OK_GREEN = "#1F7A4D"

SANS = "/System/Library/Fonts/HelveticaNeue.ttc"
MONO = "/System/Library/Fonts/Menlo.ttc"

S = 2  # fator de escala: desenhamos em 2x


def sans(size, bold=False):
    # HelveticaNeue.ttc: 0 = regular, 1 = bold
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
    """
    O vidro CRT do site: duas camadas de 1px a cada 3px, preto 16% e branco 4%.

    Vai por ultimo, sobre tudo — e um vidro na frente da tela, nao textura de
    fundo. Em 2x as linhas tem 2px e o periodo e 6px.
    """
    glass = Image.new("RGBA", im.size, (0, 0, 0, 0))
    g = ImageDraw.Draw(glass)
    for y in range(0, im.height, 3 * S):
        g.rectangle([0, y, im.width, y + S - 1], fill=(0, 0, 0, 41))          # 16%
        g.rectangle([0, y + S, im.width, y + 2 * S - 1], fill=(255, 255, 255, 10))  # 4%
    return Image.alpha_composite(im.convert("RGBA"), glass)


def canvas(w, h):
    im = Image.new("RGBA", (w * S, h * S), PAPER)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, w * S - 1, h * S - 1], radius=14 * S,
                        fill=PAPER, outline=RULE, width=1 * S)
    return im, d


def kicker(d, x, y, txt, color=CORAL_DEEP):
    """Rotulo tecnico: mono, maiusculas, bem espacado — a assinatura do site."""
    f = mono(9 * S)
    cx = x
    for ch in txt.upper():
        d.text((cx, y), ch, font=f, fill=color)
        cx += d.textlength(ch, font=f) + 2.6 * S
    return cx


def save(im, nome):
    caminho = os.path.join(OUT, nome)
    scanlines(im).convert("RGB").save(caminho, "PNG", optimize=True)
    print(f"  {nome}  {im.width}x{im.height}")


# ── textos ─────────────────────────────────────────────────────────────────
# O desenho e um so; muda apenas o que esta escrito. Assim as duas versoes nao
# divergem de layout com o tempo — e a razao de o texto viver aqui em cima.
TEXTOS = {
    "en": {
        "sufixo": "",
        "passos": [
            ("01", "Buy the board", ["Guition JC4832W535 — ESP32-S3", "with a 3.5\" touch screen."]),
            ("02", "Plug it into USB", ["An ordinary data cable.", "Nothing to install."]),
            ("03", "Flash and set up", ["One click writes the firmware.", "WiFi, token and PIN on-screen."]),
        ],
        "rodape": "No arduino-cli, no repo clone, no compiling.",
        "kicker": "No install needed",
        "titulo": "Flash it from your browser.",
        "corpo": [
            "Short on time, or not comfortable with arduino-cli?",
            "Plug the board into USB and flash it from Chrome in",
            "about a minute. Small one-off fee per board.",
        ],
        "livre": "The firmware stays free and open — you can always flash it yourself.",
        "gravando": "Writing the firmware…",
        "nao_tire": "Do not unplug the board.",
    },
    "pt": {
        "sufixo": "-pt",
        "passos": [
            ("01", "Compre a placa", ["Guition JC4832W535 — ESP32-S3", "com tela touch de 3,5\"."]),
            ("02", "Conecte no USB", ["Um cabo de dados comum.", "Nada para instalar."]),
            ("03", "Grave e configure", ["Um clique grava o firmware.", "WiFi, token e PIN na tela."]),
        ],
        "rodape": "Sem arduino-cli, sem clonar o repositório, sem compilar.",
        "kicker": "Nada para instalar",
        "titulo": "Grave pelo navegador.",
        "corpo": [
            "Sem tempo, ou sem vontade de lidar com o arduino-cli?",
            "Conecte a placa no USB e grave pelo Chrome em",
            "cerca de um minuto. Cobrança única por placa.",
        ],
        "livre": "O firmware continua livre e aberto — você sempre pode gravar sozinho.",
        "gravando": "Gravando o firmware…",
        "nao_tire": "Não desconecte a placa.",
    },
}


# ── banner 1: os tres passos ────────────────────────────────────────────────
def banner_steps(t):
    W, H = 800, 196
    im, d = canvas(W, H)
    RODAPE = H - 50  # a regua do rodape; as reguas das colunas param nela

    passos = t["passos"]

    col = W // 3
    for i, (n, titulo, linhas) in enumerate(passos):
        x = i * col + 34
        if i:  # regua entre colunas: para na do rodape, nao a cruza
            d.line([(i * col) * S, 26 * S, (i * col) * S, RODAPE * S], fill=RULE, width=1 * S)

        kicker(d, x * S, 38 * S, n)
        d.text((x * S, 58 * S), titulo, font=sans(19 * S, bold=True), fill=INK)
        for j, ln in enumerate(linhas):
            d.text((x * S, (90 + j * 20) * S), ln, font=sans(12 * S), fill=INK_MUTED)

    # rodape: o que o conjunto significa
    d.line([34 * S, RODAPE * S, (W - 34) * S, RODAPE * S], fill=RULE, width=1 * S)
    marca = clawd(15 * S)
    im.paste(marca, (34 * S, (RODAPE + 16) * S), marca)
    d.text((56 * S, (RODAPE + 15) * S), t["rodape"], font=sans(12 * S), fill=INK_FAINT)

    save(im, f"banner-steps{t['sufixo']}.png")


# ── banner 2: o gravador web ────────────────────────────────────────────────
def janela_navegador(im, d, t, x, y, w, h):
    """Janela de navegador com a barra de endereco e a gravacao em andamento."""
    d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], radius=9 * S,
                        fill=RAISED, outline=RULE_STRONG, width=1 * S)
    # barra de titulo
    d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + 26) * S], radius=9 * S, fill=SUNKEN)
    d.rectangle([x * S, (y + 17) * S, (x + w) * S, (y + 26) * S], fill=SUNKEN)
    d.line([x * S, (y + 26) * S, (x + w) * S, (y + 26) * S], fill=RULE, width=1 * S)
    for k in range(3):
        cx = (x + 14 + k * 11) * S
        d.ellipse([cx - 3 * S, (y + 10) * S, cx + 3 * S, (y + 16) * S], fill=RULE_STRONG)
    # endereco
    d.rounded_rectangle([(x + 50) * S, (y + 7) * S, (x + w - 12) * S, (y + 19) * S],
                        radius=6 * S, fill=PAPER, outline=RULE, width=1 * S)
    d.text(((x + 58) * S, (y + 9) * S), "usagestick.autom.my",
           font=mono(8 * S), fill=INK_MUTED)

    # conteudo: a gravacao acontecendo
    d.text(((x + 16) * S, (y + 42) * S), t["gravando"],
           font=sans(12 * S, bold=True), fill=INK)
    pct = 0.64
    bx0, bx1 = (x + 16) * S, (x + w - 16) * S
    by = (y + 64) * S
    d.rounded_rectangle([bx0, by, bx1, by + 7 * S], radius=4 * S, fill=SUNKEN)
    d.rounded_rectangle([bx0, by, bx0 + (bx1 - bx0) * pct, by + 7 * S], radius=4 * S, fill=CORAL)
    d.text(((x + 16) * S, (y + 80) * S), t["nao_tire"],
           font=sans(9 * S), fill=INK_FAINT)
    d.text(((x + w - 16) * S, (y + 42) * S), "64%", font=mono(12 * S, bold=True),
           fill=CORAL_DEEP, anchor="ra")


def banner_web(t):
    W, H = 800, 270
    im, d = canvas(W, H)

    x = 40
    kicker(d, x * S, 38 * S, t["kicker"])
    d.text((x * S, 60 * S), t["titulo"], font=sans(27 * S, bold=True), fill=INK)

    for i, ln in enumerate(t["corpo"]):
        d.text((x * S, (100 + i * 19) * S), ln, font=sans(13 * S), fill=INK_MUTED)

    # a URL, tratada como o elemento que a pessoa deve levar embora
    d.rounded_rectangle([x * S, 168 * S, (x + 232) * S, 194 * S], radius=7 * S,
                        fill=CORAL_WASH, outline=CORAL, width=1 * S)
    d.text(((x + 14) * S, 174 * S), "usagestick.autom.my",
           font=mono(12 * S, bold=True), fill=CORAL_DEEP)

    # o essencial, e o que mais importa dizer
    d.text((x * S, 214 * S), t["livre"], font=sans(11 * S), fill=INK_FAINT)

    janela_navegador(im, d, t, 452, 72, 308, 126)
    save(im, f"banner-web{t['sufixo']}.png")


def main():
    print("gerando banners:")
    for lang, t in TEXTOS.items():
        print(f" [{lang}]")
        banner_steps(t)
        banner_web(t)


if __name__ == "__main__":
    main()
