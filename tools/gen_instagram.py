#!/usr/bin/env python3
"""
gen_instagram.py — carrosseis do Instagram (1080x1350, 4:5) em assets/instagram/.

Cada slide e um SVG montado aqui e rasterizado pelo rsvg-convert. Os mockups
(assets/mock-*.png) e os renders dos cases entram como PNG embutido (base64).
Tema: fundo escuro do device (#141413), coral da marca, scanlines de CRT bem
sutis, pixel-art do Clawd, rotulos em JetBrains Mono e titulos em Helvetica
Neue — o mesmo universo visual do gadget, sem parecer template.

Conjuntos:
  1-projeto/    o que e + funcionalidades (8 slides)
  2-logo/       logo da empresa no header (6 slides)
  3-como-ter/   do zero a mesa: placa, gravador web, seguranca, case (6 slides)

Requisitos: rsvg-convert + Pillow. Roda gen_mockups.py antes se as telas mudaram.
"""
import base64
import os
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
BRAND = os.path.join(ASSETS, "brand")
OUT = os.path.join(ASSETS, "instagram")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import partner_logo as pl  # noqa: E402

W, H = 1080, 1350
BG, SURF, SURF2 = "#141413", "#1E1E1D", "#2A2A28"
CORAL, CORAL_DEEP = "#D97757", "#B4614A"
TEXT, MUTED, FAINT = "#F2EFE9", "#9A958C", "#5E5A54"
OK, WARN, BAD, BLUE = "#4ADE80", "#FBBF24", "#F87171", "#7DD3FC"

SANS = "Helvetica Neue"
MONO = "JetBrains Mono"

PARTNER_PNG = os.environ.get("PARTNER_PNG", "")   # logo de exemplo p/ o conjunto 2


# ---------------------------------------------------------------- utils
def b64png(im: Image.Image) -> str:
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tf:
        im.save(tf.name)
        data = open(tf.name, "rb").read()
    os.unlink(tf.name)
    return "data:image/png;base64," + base64.b64encode(data).decode()


def asset(name: str) -> Image.Image:
    return Image.open(os.path.join(ASSETS, name)).convert("RGBA")


def clawd(w: int) -> Image.Image:
    png = tempfile.mktemp(suffix=".png")
    subprocess.run(["rsvg-convert", os.path.join(BRAND, "claudecode-color.svg"),
                    "-w", str(w * 2), "-h", str(w * 2), "-o", png], check=True)
    im = Image.open(png).convert("RGBA"); os.unlink(png)
    im = im.crop(im.getbbox())
    return im.resize((w, round(im.height * w / im.width)), Image.LANCZOS)


def esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size, *, fill=TEXT, font=SANS, weight=700, anchor="start",
         spacing=0, opacity=1.0):
    return (f'<text x="{x}" y="{y}" font-family="{font}" font-size="{size}" '
            f'font-weight="{weight}" fill="{fill}" text-anchor="{anchor}" '
            f'letter-spacing="{spacing}" opacity="{opacity}">{esc(s)}</text>')


def lines(x, y, rows, size, lh=None, **kw):
    lh = lh or round(size * 1.12)
    return "".join(text(x, y + i * lh, r, size, **kw) for i, r in enumerate(rows))


def mono(x, y, s, size=26, fill=CORAL, **kw):
    return text(x, y, s, size, fill=fill, font=MONO, weight=600, spacing=2, **kw)


def image(im: Image.Image, x, y, w, h=None, radius=0):
    h = h or round(im.height * w / im.width)
    clip = ""
    if radius:
        cid = f"c{abs(hash((x, y, w, h)))}"
        clip = (f'<clipPath id="{cid}"><rect x="{x}" y="{y}" width="{w}" height="{h}" '
                f'rx="{radius}"/></clipPath>')
        return (f'<defs>{clip}</defs><image clip-path="url(#{cid})" x="{x}" y="{y}" '
                f'width="{w}" height="{h}" href="{b64png(im)}"/>')
    return f'<image x="{x}" y="{y}" width="{w}" height="{h}" href="{b64png(im)}"/>'


def device(im: Image.Image, x, y, w, tilt=0):
    """Moldura do gadget: bezel escuro, tela com raio, brilho leve no vidro."""
    h = round(im.height * w / im.width)
    b = round(w * 0.045)
    g = ""
    g += (f'<g transform="rotate({tilt} {x + w / 2} {y + h / 2})">'
          f'<rect x="{x - b}" y="{y - b}" width="{w + 2 * b}" height="{h + 2 * b}" rx="{b * 1.6}" '
          f'fill="#0B0B0B" stroke="#2E2E2C" stroke-width="3"/>')
    g += image(im, x, y, w, h, radius=round(b * 0.6))
    g += (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{round(b * 0.6)}" '
          f'fill="url(#glass)" opacity="0.5"/></g>')
    return g


def chip(x, y, s, fill=CORAL, ink=BG, size=24, w=None):
    w = w or round(len(s) * size * 0.66 + 40)
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{size + 22}" rx="{(size + 22) / 2}" fill="{fill}"/>'
            + text(x + w / 2, y + size + 4, s, size, fill=ink, font=MONO, weight=700, anchor="middle", spacing=1))


def frame(body: str, n: int, total: int, *, tag: str) -> str:
    """Esqueleto comum: fundo, scanlines, rodape com tag + contador."""
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs>
  <pattern id="scan" width="4" height="4" patternUnits="userSpaceOnUse">
    <rect width="4" height="1" fill="#000" opacity="0.28"/>
  </pattern>
  <radialGradient id="glow" cx="50%" cy="0%" r="80%">
    <stop offset="0" stop-color="{CORAL}" stop-opacity="0.22"/>
    <stop offset="1" stop-color="{CORAL}" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="glass" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#fff" stop-opacity="0.10"/>
    <stop offset="0.45" stop-color="#fff" stop-opacity="0"/>
  </linearGradient>
  <linearGradient id="fade" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="{BG}" stop-opacity="0"/>
    <stop offset="1" stop-color="{BG}" stop-opacity="1"/>
  </linearGradient>
</defs>
<rect width="{W}" height="{H}" fill="{BG}"/>
<rect width="{W}" height="{H}" fill="url(#glow)"/>
{body}
<rect width="{W}" height="{H}" fill="url(#scan)"/>
<line x1="60" y1="{H - 96}" x2="{W - 60}" y2="{H - 96}" stroke="{SURF2}" stroke-width="2"/>
{mono(60, H - 52, tag, 22, fill=MUTED)}
{mono(W - 60, H - 52, f"{n:02d} / {total:02d}", 22, fill=MUTED, anchor="end")}
</svg>'''


def render(svg: str, path: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = tempfile.mktemp(suffix=".svg")
    open(tmp, "w").write(svg)
    subprocess.run(["rsvg-convert", tmp, "-w", str(W), "-h", str(H), "-o", path], check=True)
    os.unlink(tmp)


# ---------------------------------------------------------------- blocos reutilizaveis
def kicker(y, s):
    return (f'<rect x="60" y="{y - 30}" width="8" height="34" fill="{CORAL}"/>'
            + mono(84, y, s, 24, fill=CORAL))


def headline(y, rows, size=88):
    return lines(60, y, rows, size, lh=round(size * 1.02), weight=700, spacing=-2)


def sub(y, rows, size=34, fill=MUTED):
    return lines(60, y, rows, size, lh=round(size * 1.3), weight=400, fill=fill)


def bullets(y, items, size=32, gap=None):
    gap = gap or round(size * 2.3)
    out = ""
    for i, (title, desc) in enumerate(items):
        yy = y + i * gap
        out += f'<rect x="60" y="{yy - 22}" width="14" height="14" fill="{CORAL}"/>'
        out += text(96, yy - 8, title, size, weight=700)
        out += text(96, yy + round(size * 1.15), desc, round(size * 0.8), weight=400, fill=MUTED)
    return out


def cta_slide(n, total, tag):
    c = clawd(300)
    body = ""
    body += image(c, W / 2 - 150, 220, 300)
    body += headline(660, ["Quer montar", "o seu?"], 104)
    body += sub(850, ["Firmware aberto, gravacao pelo navegador,", "case pra imprimir em 3D."], 32)
    body += f'<rect x="60" y="960" width="{W - 120}" height="150" rx="24" fill="{CORAL}"/>'
    body += text(W / 2, 1022, "Comente", 38, fill=BG, weight=500, anchor="middle")
    body += text(W / 2, 1084, '"CLAUDE"', 60, fill=BG, weight=800, anchor="middle", font=MONO)
    body += sub(1180, ["que te envio o link do projeto."], 32, fill=TEXT)
    return frame(body, n, total, tag=tag)


def mock_with_logo(logo_png: str) -> Image.Image:
    """mock-agora com o logo do parceiro no lugar do wordmark (mesmo layout do firmware)."""
    m = asset("mock-agora.png")
    from PIL import ImageDraw
    d = ImageDraw.Draw(m)
    d.rectangle((78, 8, 160, 48), fill=BG)
    lg = pl.fit(pl.load(logo_png, 36))
    m.alpha_composite(lg, (240 - lg.width // 2, 10 + (36 - lg.height) // 2))
    return m


# ---------------------------------------------------------------- conjunto 1: projeto
def set_projeto():
    T, tag = 8, "claude usage stick"
    out = []
    # 1 capa
    b = kicker(150, "PROJETO OPEN SOURCE")
    b += headline(260, ["Seu uso do", "Claude Code,", "na mesa."], 96)
    b += sub(600, ["Uma tela touch de 3,5\" que mostra, em tempo real,", "quanto da sua cota ja foi."], 32)
    b += device(asset("mock-agora.png"), 130, 690, 820, tilt=-3)
    out.append(frame(b, 1, T, tag=tag))
    # 2 problema
    b = kicker(150, "O PROBLEMA")
    b += headline(270, ["\"Quanto falta", "da minha", "janela de 5h?\""], 92)
    b += sub(640, ["Quem usa Claude Code no plano Pro ou Max vive", "com essa pergunta. A resposta fica escondida", "num comando de terminal — e voce so descobre", "que acabou quando trava."], 32)
    b += f'<rect x="60" y="900" width="{W - 120}" height="200" rx="24" fill="{SURF}"/>'
    b += mono(100, 970, "$ claude", 30, fill=MUTED)
    b += mono(100, 1030, "> rate limit reached. resets in 2h13m", 30, fill=BAD)
    out.append(frame(b, 2, T, tag=tag))
    # 3 agora
    b = kicker(150, "TELA 1 • AGORA")
    b += headline(260, ["Duas janelas,", "um olhar."], 88)
    b += device(asset("mock-agora.png"), 90, 420, 900)
    b += bullets(1110, [("5 horas e semana lado a lado", "porcentagem, medidor e quando cada uma reseta")], 30)
    out.append(frame(b, 3, T, tag=tag))
    # 4 modelos
    b = kicker(150, "TELA 2 • MODELOS")
    b += headline(260, ["Cada modelo,", "um Clawd."], 88)
    b += device(asset("mock-modelos.png"), 90, 420, 900)
    b += bullets(1110, [("Sonda real na API, 1 modelo por ciclo", "piscando quando esta tudo bem, suando quando o limite aperta")], 30)
    out.append(frame(b, 4, T, tag=tag))
    # 5 janela
    b = kicker(150, "TELA 3 • JANELA DE 5H")
    b += headline(260, ["Projecao de", "esgotamento."], 88)
    b += device(asset("mock-janela5h.png"), 90, 420, 900)
    b += bullets(1110, [("Uso real + linha pontilhada ate o 100%", "\"no ritmo atual, esgota as 16:12\" — antes de acontecer")], 30)
    out.append(frame(b, 5, T, tag=tag))
    # 6 ritmo
    b = kicker(150, "TELA 4 • RITMO")
    b += headline(260, ["Que horas voce", "queima mais?"], 88)
    b += device(asset("mock-ritmo.png"), 90, 420, 900)
    b += bullets(1110, [("Heatmap por hora local", "hoje, 7 dias, 30 dias ou tudo — historico fica no device")], 30)
    out.append(frame(b, 6, T, tag=tag))
    # 7 momentos + contas
    b = kicker(150, "E MAIS")
    b += headline(260, ["Avisa quando", "importa."], 88)
    b += device(asset("mock-momento.png"), 70, 420, 500, tilt=-4)
    b += device(asset("mock-contas.png"), 520, 560, 480, tilt=3)
    b += bullets(1010, [("Animacao nos 25 / 50 / 70 / 100%", "e ate 4 contas, cada uma com seu historico"),
                        ("PT-BR e EN, PIN, token cifrado AES-256", "nada sai do device alem da chamada a API")], 28, gap=104)
    out.append(frame(b, 7, T, tag=tag))
    out.append(cta_slide(8, T, tag))
    return out


# ---------------------------------------------------------------- conjunto 2: logo
def set_logo():
    T, tag = 6, "claude usage stick • sua marca"
    if not PARTNER_PNG or not os.path.isfile(PARTNER_PNG):
        raise SystemExit("defina PARTNER_PNG=<logo.png> para o conjunto 2")
    mk = mock_with_logo(PARTNER_PNG)
    lg = pl.fit(pl.load(PARTNER_PNG, 36), 36, 170)
    out = []
    # 1 capa
    b = kicker(150, "NOVIDADE")
    b += headline(260, ["A logo da sua", "empresa, no", "gadget."], 96)
    b += sub(600, ["O medidor de uso do Claude Code com a sua", "marca no lugar de honra: o header."], 32)
    b += device(mk, 130, 690, 820, tilt=-3)
    out.append(frame(b, 1, T, tag=tag))
    # 2 como fica (zoom no header)
    b = kicker(150, "COMO FICA")
    b += headline(260, ["Clawd de um lado,", "voce no centro."], 88)
    hdr = mk.crop((0, 0, 480, 60)).resize((1920, 240), Image.LANCZOS)
    b += f'<defs><clipPath id="hz"><rect x="60" y="440" width="{W - 120}" height="240" rx="20"/></clipPath></defs>'
    b += f'<image clip-path="url(#hz)" x="-360" y="440" width="1920" height="240" href="{b64png(hdr)}"/>'
    b += f'<rect x="60" y="440" width="{W - 120}" height="240" rx="20" fill="none" stroke="{SURF2}" stroke-width="3"/>'
    b += mono(60, 730, "zoom 4x no header do device", 22, fill=FAINT)
    b += bullets(860, [("Logo centralizada, 36 px de altura", "o mascote Claude continua a esquerda"),
                       ("Aparece tambem na tela Sobre", "o resto do gadget e exatamente o mesmo")], 30, gap=120)
    out.append(frame(b, 2, T, tag=tag))
    # 3 sem recompilar
    b = kicker(150, "SEM COMPLICACAO")
    b += headline(260, ["Manda o PNG.", "So isso."], 88)
    b += sub(470, ["A logo entra no firmware na hora de gravar —", "sem recompilar nada, sem mexer em codigo."], 32)
    b += f'<rect x="60" y="600" width="{W - 120}" height="330" rx="24" fill="{SURF}"/>'
    rows = [("01", "PNG com fundo transparente (ou SVG)"), ("02", "horizontal, tipo 400 x 120"),
            ("03", "gravamos e pronto: marca na tela")]
    for i, (n, s) in enumerate(rows):
        b += mono(100, 690 + i * 96, n, 44, fill=CORAL)
        b += text(190, 690 + i * 96, s, 32, weight=500)
    b += image(lg.resize((lg.width * 3, lg.height * 3), Image.LANCZOS), W / 2 - lg.width * 1.5, 1000, lg.width * 3)
    out.append(frame(b, 3, T, tag=tag))
    # 4 para quem
    b = kicker(150, "PARA QUEM")
    b += headline(260, ["Um presente que", "fica na mesa", "do time."], 88)
    b += bullets(600, [("Empresas que usam Claude Code", "cada dev com o medidor — e a sua marca — na mesa"),
                       ("Agencias e consultorias", "brinde tecnico que ninguem guarda na gaveta"),
                       ("Eventos e comunidades", "lote com a identidade do evento")], 32, gap=130)
    out.append(frame(b, 4, T, tag=tag))
    # 5 continua igual
    b = kicker(150, "O RESTO CONTINUA IGUAL")
    b += headline(260, ["Mesmo firmware.", "Mesmas telas."], 88)
    b += device(mk, 70, 430, 560, tilt=-3)
    b += device(asset("mock-modelos.png"), 470, 640, 520, tilt=3)
    b += sub(1080, ["Janelas, projecao, mascotes, historico, multi-conta:", "tudo la. So o header muda."], 30)
    out.append(frame(b, 5, T, tag=tag))
    out.append(cta_slide(6, T, tag))
    return out


# ---------------------------------------------------------------- conjunto 3: como ter
def set_como_ter():
    T, tag = 6, "claude usage stick • como ter o seu"
    out = []
    b = kicker(150, "DO ZERO A MESA")
    b += headline(260, ["Como ter", "o seu em", "3 passos."], 96)
    b += sub(600, ["Uma placa, um navegador e uma impressora 3D", "(essa ultima e opcional)."], 32)
    b += device(asset("mock-agora.png"), 130, 690, 820, tilt=-3)
    out.append(frame(b, 1, T, tag=tag))
    # 2 placa
    b = kicker(150, "PASSO 1 • A PLACA")
    b += headline(260, ["Guition", "JC4832W535"], 88)
    b += sub(470, ["ESP32-S3 com tela IPS touch de 3,5\" (480x320).", "Custa pouco, vem pronta, so liga no USB."], 32)
    b += f'<rect x="60" y="620" width="{W - 120}" height="420" rx="24" fill="{SURF}"/>'
    specs = [("chip", "ESP32-S3, WiFi, 8 MB PSRAM"), ("tela", "3,5\" IPS 480x320 touch"),
             ("alimentacao", "USB-C, 5V"), ("onde achar", "AliExpress, Mercado Livre")]
    for i, (k, v) in enumerate(specs):
        b += mono(100, 700 + i * 90, k, 24, fill=CORAL)
        b += text(360, 700 + i * 90, v, 32, weight=500)
    out.append(frame(b, 2, T, tag=tag))
    # 3 gravador web
    b = kicker(150, "PASSO 2 • GRAVAR")
    b += headline(260, ["Pelo navegador.", "Sem instalar", "nada."], 88)
    b += sub(600, ["Conecta no USB, abre o Chrome e o firmware", "vai pra placa em um minuto."], 32)
    b += image(asset("banner-web-pt.png"), 60, 760, W - 120, radius=20)
    b += mono(60, 1110, "usagestick.autom.my", 34, fill=CORAL)
    out.append(frame(b, 3, T, tag=tag))
    # 4 seguranca
    b = kicker(150, "SEGURO POR DESENHO")
    b += headline(260, ["Seu token", "fica so ali."], 88)
    b += bullets(500, [("Token cifrado com AES-256-GCM", "chave derivada de um PIN de 4 digitos que nunca e salvo"),
                       ("10 erros de PIN = apaga tudo", "e o tempo de espera dobra a cada erro"),
                       ("Fala so com api.anthropic.com", "sem backend, sem nuvem, sem conta")], 32, gap=150)
    out.append(frame(b, 4, T, tag=tag))
    # 5 case
    b = kicker(150, "PASSO 3 • O CASE")
    b += headline(260, ["Tres cases,", "escolha o seu."], 88)
    b += image(asset("case-simples.png"), 60, 440, 460, radius=20)
    b += image(asset("case-articulado.png"), 560, 440, 460, radius=20)
    b += image(asset("case-clawd-vinnialfonso.jpg"), 60, 800, 460, radius=20)
    b += f'<rect x="560" y="800" width="460" height="300" rx="20" fill="{SURF}"/>'
    b += lines(600, 870, ["Cunha de mesa,", "suporte articulado", "ou o Clawd de", "@vinnialfonso."], 34, lh=44, weight=500)
    b += mono(600, 1070, "STL no repositorio", 22, fill=MUTED)
    out.append(frame(b, 5, T, tag=tag))
    out.append(cta_slide(6, T, tag))
    return out


def main():
    sets = {"1-projeto": set_projeto, "3-como-ter": set_como_ter}
    if PARTNER_PNG:
        sets["2-logo"] = set_logo
    only = sys.argv[1:]
    for name, fn in sets.items():
        if only and name not in only:
            continue
        for i, svg in enumerate(fn(), 1):
            path = os.path.join(OUT, name, f"{i:02d}.png")
            render(svg, path)
            print("ok", os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
