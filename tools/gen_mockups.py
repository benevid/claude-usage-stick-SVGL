#!/usr/bin/env python3
"""
gen_mockups.py — gera mockups 480x320 pixel-accurate das telas do firmware
(layout v2.1) em assets/mock-*.png, usando os SVGs oficiais de assets/brand/.

Sao mockups para o README ate existirem fotos reais do device. A fonte usa
HelveticaNeue como substituta da Montserrat do LVGL.

Requisitos: rsvg-convert + Pillow (mesmos do gen_logo_assets.py).
"""
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "assets", "brand")
OUT = os.path.join(ROOT, "assets")

# paleta do firmware
BG, SURF, SURF2, TRACK, GRID = "#0F0F12", "#1A1A20", "#24242C", "#26262E", "#232329"
BORDER, TEXT, MUTED, FAINT = "#30303A", "#F2F0EC", "#8C8C98", "#5C5C68"
ACCENT, OK, WARN, BAD, BLUE, LILAC = "#D97757", "#4ADE80", "#FBBF24", "#F87171", "#7DD3FC", "#C4B5FD"

FONT = "/System/Library/Fonts/HelveticaNeue.ttc"


def F(size):
    return ImageFont.truetype(FONT, size)


def hexrgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(c1, c2, t):
    a, b = hexrgb(c1), hexrgb(c2)
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def grad(p):
    """verde -> ambar -> vermelho, igual ao grad_color() do firmware."""
    p = max(0, min(100, p))
    return mix(OK, WARN, p / 50) if p <= 50 else mix(WARN, BAD, (p - 50) / 50)


def render_svg(name, w):
    png = tempfile.mktemp(suffix=".png")
    subprocess.run(["rsvg-convert", os.path.join(BRAND, name), "-w", str(w * 2),
                    "-h", str(w * 2), "-o", png], check=True)
    im = Image.open(png).convert("RGBA")
    im = im.crop(im.getbbox())
    return im.resize((w, round(im.height * w / im.width)), Image.LANCZOS)


def render_word(h):
    png = tempfile.mktemp(suffix=".png")
    css = tempfile.mktemp(suffix=".css")
    open(css, "w").write(f"svg{{color:{ACCENT}}}")
    subprocess.run(["rsvg-convert", os.path.join(BRAND, "claudecode-text.svg"),
                    "-h", "64", "--stylesheet", css, "-o", png], check=True)
    im = Image.open(png).convert("RGBA")
    im = im.crop(im.getbbox())
    return im.resize((round(im.width * h / im.height), h), Image.LANCZOS)


CLAWD_SM = render_svg("claudecode-color.svg", 42)
CLAWD_MD = render_svg("claudecode-color.svg", 88)
CLAWD_XL = render_svg("claudecode-color.svg", 176)
WORDMARK = render_word(26)


def canvas():
    im = Image.new("RGBA", (480, 320), hexrgb(BG) + (255,))
    return im, ImageDraw.Draw(im)


def gear(d, cx, cy):
    d.ellipse((cx - 9, cy - 9, cx + 9, cy + 9), outline=hexrgb(TEXT), width=3)
    for k in range(8):
        import math
        a = k * math.pi / 4
        x, y = cx + 11 * math.cos(a), cy + 11 * math.sin(a)
        d.rectangle((x - 2, y - 2, x + 2, y + 2), fill=hexrgb(TEXT))
    d.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill=hexrgb(SURF2))


def header(im, d, status="atualizado ha 24s"):
    im.alpha_composite(CLAWD_SM, (14, 8))
    im.alpha_composite(WORDMARK, (66, 8))
    d.text((398, 16), status, font=F(12), fill=hexrgb(MUTED), anchor="rm")
    d.rounded_rectangle((416, 2, 474, 42), 10, fill=hexrgb(SURF2))
    gear(d, 445, 22)
    d.rectangle((0, 40, 480, 43), fill=hexrgb(SURF))
    d.rectangle((0, 40, 300, 43), fill=hexrgb(ACCENT))


def dots(d, active):
    cx0 = 240 - 1.5 * 18
    for i in range(4):
        x = cx0 + i * 18
        if i == active:
            d.rounded_rectangle((x - 9, 308, x + 9, 316), 4, fill=hexrgb(ACCENT))
        else:
            d.ellipse((x - 4, 308, x + 4, 316), fill=hexrgb(BORDER))


def chip(d, x, y, txt, color, font=None):
    font = font or F(12)
    w = d.textlength(txt, font=font)
    bg = mix(color, BG, 0.76)
    d.rounded_rectangle((x, y, x + w + 20, y + 24), 12, fill=bg)
    d.text((x + 10, y + 12), txt, font=font, fill=hexrgb(color), anchor="lm")
    return w + 20


def meter(d, x, y, pct, n=18):
    filled = round(pct / 100 * n)
    col = grad(pct)
    for i in range(n):
        c = col if i < filled else hexrgb(TRACK)
        d.rounded_rectangle((x + i * 11, y, x + i * 11 + 8, y + 16), 2, fill=c)


# ---------------- tela 1: Agora ----------------
def mock_agora():
    im, d = canvas()
    header(im, d)
    for (x, title, pct, cd, at) in ((8, "5 HORAS", 41, "1h 40m", "RESETA EM • qui 16:30"),
                                    (244, "SEMANA", 16, "5d 3h", "RESETA EM • ter 11:59")):
        d.rounded_rectangle((x, 50, x + 228, 260), 18, fill=hexrgb(SURF))
        d.text((x + 14, 64), title, font=F(14), fill=hexrgb(MUTED))
        d.text((x + 14, 84), f"{pct}%", font=F(48), fill=grad(pct))
        meter(d, x + 14, 146, pct)
        d.text((x + 14, 170), at, font=F(12), fill=hexrgb(FAINT))
        d.text((x + 14, 188), cd, font=F(40), fill=hexrgb(TEXT))
    chip(d, 8, 266, "OK", OK)
    d.text((472, 278), "tokens na janela: 1.2M entrada • 88k saida",
           font=F(12), fill=hexrgb(MUTED), anchor="rm")
    dots(d, 0)
    return im


# ---------------- tela 2: Modelos ----------------
def acc(d, ox, oy, model):
    r = lambda x, y, w, h, rad, c: d.rounded_rectangle((ox + x, oy + y, ox + x + w, oy + y + h), rad, fill=hexrgb(c))
    if model == 0:
        r(46, 0, 8, 7, 1, WARN); r(41, 5, 8, 7, 1, WARN); r(46, 10, 8, 7, 1, WARN)
    elif model == 1:
        r(50, 0, 10, 4, 1, BLUE); r(50, 0, 4, 13, 1, BLUE); r(44, 10, 8, 7, 3, BLUE)
    elif model == 2:
        r(30, 4, 7, 8, 1, WARN); r(41, 1, 7, 11, 1, WARN); r(52, 4, 7, 8, 1, WARN); r(30, 12, 29, 6, 1, WARN)
    else:
        r(41, 0, 6, 17, 2, LILAC); r(36, 6, 16, 6, 2, LILAC)


def mock_modelos():
    im, d = canvas()
    header(im, d)
    centers = (60, 180, 300, 420)
    names = ("Haiku", "Sonnet", "Opus", "Fable")
    for i, cx in enumerate(centers):
        ox, oy = cx - 44, 60
        im.alpha_composite(CLAWD_MD, (ox, oy + 20))
        acc(d, ox, oy, i)
        if i > 0:  # limitados: gota de suor
            d.rounded_rectangle((ox + 70, oy + 30, ox + 76, oy + 40), 3, fill=hexrgb(BLUE))
        d.text((cx, 152), names[i], font=F(16),
               fill=hexrgb(TEXT if i == 0 else MUTED), anchor="mm")
        txt, col = ("OK 0.9s", OK) if i == 0 else ("LIMITADO", WARN)
        w = d.textlength(txt, font=F(12)) + 20
        chip(d, cx - w / 2, 168, txt, col)
    d.text((14, 216), "sonda real na API • 1 modelo por ciclo", font=F(12), fill=hexrgb(FAINT))
    d.text((14, 240), "status.claude.com: OK • sem incidentes", font=F(14), fill=hexrgb(FAINT))
    dots(d, 1)
    return im


# ---------------- tela 3: Janela de 5h ----------------
def mock_janela():
    im, d = canvas()
    header(im, d)
    d.text((14, 48), "Janela de 5h", font=F(16), fill=hexrgb(TEXT))
    d.text((320, 52), "uso real + projecao", font=F(12), fill=hexrgb(FAINT))
    d.rounded_rectangle((8, 72, 472, 242), 18, fill=hexrgb(SURF))
    x0, y0, w, h = 20, 82, 440, 126
    y = lambda p: y0 + h - p * h / 100
    for p in (25, 50, 75):
        d.line((x0, y(p), x0 + w, y(p)), fill=hexrgb(GRID))
    d.text((x0 + w - 4, y0 - 2), "100", font=F(12), fill=hexrgb(FAINT), anchor="ra")
    d.text((x0 + w - 4, y0 + h - 12), "0", font=F(12), fill=hexrgb(FAINT), anchor="ra")
    # historico (sobe de 8% a 47% ate 62% da janela)
    pts = []
    for i in range(40):
        t = i / 39
        pts.append((x0 + t * w * 0.62, y(8 + 39 * (t ** 1.4))))
    d.line(pts, fill=hexrgb(ACCENT), width=3, joint="curve")
    # projecao pontilhada ate 100%
    import math
    px, py = pts[-1], None
    ex, ey = x0 + w * 0.93, y(100)
    steps = int(math.hypot(ex - pts[-1][0], ey - pts[-1][1]) / 12)
    for k in range(steps):
        t0, t1 = k / steps, (k + 0.5) / steps
        d.line((pts[-1][0] + (ex - pts[-1][0]) * t0, pts[-1][1] + (ey - pts[-1][1]) * t0,
                pts[-1][0] + (ex - pts[-1][0]) * t1, pts[-1][1] + (ey - pts[-1][1]) * t1),
               fill=mix(ACCENT, BG, 0.25), width=2)
    d.ellipse((pts[-1][0] - 4, pts[-1][1] - 4, pts[-1][0] + 4, pts[-1][1] + 4), fill=hexrgb(TEXT))
    d.text((x0, y0 + h + 8), "11:40", font=F(12), fill=hexrgb(FAINT))
    d.text((x0 + w, y0 + h + 8), "16:40", font=F(12), fill=hexrgb(FAINT), anchor="ra")
    d.text((14, 256), "No ritmo atual, esgota as 16:12 (em 1h32m)", font=F(16), fill=hexrgb(WARN))
    dots(d, 2)
    return im


# ---------------- tela 4: Ritmo por hora ----------------
def mock_ritmo():
    im, d = canvas()
    header(im, d)
    d.text((14, 52), "Ritmo por hora", font=F(16), fill=hexrgb(TEXT))
    for i, name in enumerate(("Hoje", "7d", "30d", "Tudo")):
        x = 246 + i * 56
        on = (i == 3)
        d.rounded_rectangle((x, 46, x + 52, 76), 15, fill=hexrgb(ACCENT if on else SURF2))
        d.text((x + 26, 61), name, font=F(14), fill=hexrgb(BG if on else MUTED), anchor="mm")
    prof = [0, 0, 0, 0, 0, 0, 1, 2, 5, 9, 12, 10, 4, 6, 11, 14, 12, 8, 5, 3, 2, 1, 0, 0]
    mx = max(prof)
    for hh in range(24):
        r = prof[hh] / mx
        hgt = 4 + r * 114
        x = 18 + hh * 18
        col = hexrgb(TEXT) if hh == 15 else mix(ACCENT, BG, (1 - r) * 0.55)
        d.rounded_rectangle((x, 222 - hgt, x + 13, 222), 3, fill=col)
    for hh in (0, 6, 12, 18, 23):
        d.text((14 + hh * 18, 232), f"{hh}h", font=F(12), fill=hexrgb(MUTED))
    d.text((14, 260), "quota da janela 5h queimada em cada hora local", font=F(12), fill=hexrgb(FAINT))
    dots(d, 3)
    return im


# ---------------- tela: Ajustes -> Contas ----------------
# Espelha ui_accounts() do firmware: titulo em (14,10) f20; botao Voltar 100x32
# no canto sup. direito; lista em (8,44) 464x240, linhas de 444x44 com pad_row 8.
# Com >1 conta cabe o lixo, entao o botao do nome tem 324 px (senao 384) e os dois
# icones ficam em x=332 e x=392, ambos 52x44.
def pencil(d, cx, cy, color):
    """LV_SYMBOL_EDIT — lapis diagonal."""
    d.line((cx - 6, cy + 6, cx + 5, cy - 5), fill=hexrgb(color), width=3)
    d.polygon([(cx - 8, cy + 8), (cx - 7, cy + 3), (cx - 3, cy + 7)], fill=hexrgb(color))
    d.line((cx + 4, cy - 7, cx + 7, cy - 4), fill=hexrgb(color), width=3)


def trash(d, cx, cy, color):
    """LV_SYMBOL_TRASH — lixeira."""
    c = hexrgb(color)
    d.rectangle((cx - 7, cy - 7, cx + 7, cy - 5), fill=c)
    d.rectangle((cx - 3, cy - 10, cx + 3, cy - 8), fill=c)
    d.rounded_rectangle((cx - 6, cy - 4, cx + 6, cy + 9), 2, outline=c, width=2)
    for dx in (-2, 2):
        d.line((cx + dx, cy - 1, cx + dx, cy + 6), fill=c, width=1)


def mock_contas():
    im, d = canvas()
    d.text((14, 10), "Contas", font=F(20), fill=hexrgb(TEXT))

    # Voltar (100x32, canto superior direito)
    d.rounded_rectangle((368, 6, 468, 38), 10, fill=hexrgb(SURF2))
    d.polygon([(388, 22), (395, 16), (395, 28)], fill=hexrgb(MUTED))
    d.text((402, 22), "Voltar", font=F(14), fill=hexrgb(MUTED), anchor="lm")

    rows = (("ClaudeMAX", True), ("Trabalho", False), ("Faculdade", False))
    y = 44
    for name, active in rows:
        d.rounded_rectangle((8, y, 8 + 324, y + 44), 12, fill=hexrgb(SURF))
        txt = f"{name}  •  ativa" if active else name
        d.text((22, y + 22), txt, font=F(16),
               fill=hexrgb(ACCENT if active else TEXT), anchor="lm")
        d.rounded_rectangle((340, y, 392, y + 44), 12, fill=hexrgb(SURF2))
        pencil(d, 366, y + 22, MUTED)
        d.rounded_rectangle((400, y, 452, y + 44), 12, fill=hexrgb(SURF2))
        trash(d, 426, y + 22, MUTED)
        y += 52

    # + Adicionar conta (ocupa a linha inteira; some quando os 4 slots estao cheios)
    d.rounded_rectangle((8, y, 452, y + 44), 12, fill=hexrgb(SURF2))
    d.line((26, y + 22, 38, y + 22), fill=hexrgb(ACCENT), width=3)
    d.line((32, y + 16, 32, y + 28), fill=hexrgb(ACCENT), width=3)
    d.text((50, y + 22), "Adicionar conta", font=F(16), fill=hexrgb(ACCENT), anchor="lm")

    d.text((14, 300), "So a conta ativa e consultada na API (as outras ficam dormentes).",
           font=F(12), fill=hexrgb(FAINT), anchor="lm")
    return im


# ---------------- momento de limiar (70% na 5h) ----------------
def mock_momento():
    im = Image.new("RGBA", (480, 320), (13, 13, 16, 255))
    d = ImageDraw.Draw(im)
    im.alpha_composite(CLAWD_XL, (36, 110))
    # olhos arregalados + gotas de suor
    for ex in (44, 121):
        d.rounded_rectangle((36 + ex - 3, 110 + 23 - 4, 36 + ex + 11 + 3, 110 + 23 + 21 + 4), 2, fill=(13, 13, 16))
    d.rounded_rectangle((36 + 150, 110 + 14, 36 + 158, 110 + 26), 4, fill=hexrgb(BLUE))
    d.rounded_rectangle((36 + 18, 110 + 24, 36 + 26, 110 + 36), 4, fill=mix(BLUE, BG, 0.4))
    d.text((240, 42), "JANELA DE 5 HORAS", font=F(20), fill=hexrgb(MUTED))
    d.text((240, 70), "70%", font=F(48), fill=grad(70))
    d.text((240, 148), "Atencao • uso alto", font=F(16), fill=hexrgb(TEXT))
    meter(d, 240, 196, 70)
    d.text((240, 228), "reseta em 1h 32m", font=F(14), fill=hexrgb(FAINT))
    d.text((352, 296), "toque para fechar", font=F(12), fill=hexrgb(FAINT))
    return im


def main():
    outs = {
        "mock-agora.png": mock_agora(),
        "mock-modelos.png": mock_modelos(),
        "mock-janela5h.png": mock_janela(),
        "mock-ritmo.png": mock_ritmo(),
        "mock-contas.png": mock_contas(),
        "mock-momento.png": mock_momento(),
    }
    for name, im in outs.items():
        p = os.path.join(OUT, name)
        im.convert("RGB").save(p)
        print(f"gerado: {p}")


if __name__ == "__main__":
    main()
