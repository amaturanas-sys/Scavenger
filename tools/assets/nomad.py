#!/usr/bin/env python3
"""Inventario de modelos editable en Nomad Sculpt (arte/nomad/).

Genera un modelo de partida (blockout low-poly) en GLB para cada objeto de
assets/inventario.tsv, todos mapeados sobre UN atlas de materiales compartido
que se puede pintar a mano (arte/nomad/atlas_materiales.png).

Uso:
  python3 tools/assets/nomad.py atlas [--forzar]             atlas (solo si falta), guia y TSV de celdas
  python3 tools/assets/nomad.py modelos [--forzar] [prefijo]  GLB de cada id (solo los que faltan)
  python3 tools/assets/nomad.py indice                       INVENTARIO_MODELOS.md e inventario_modelos.tsv
  python3 tools/assets/nomad.py hoja [prefijo]               hojas de miniaturas (hoja_modelos*.png)
  python3 tools/assets/nomad.py verificar                    relee cada GLB y comprueba medidas, tris y origen
  python3 tools/assets/nomad.py todo [--forzar]              atlas + modelos + indice

Reglas:
  - Nunca sobrescribe atlas_materiales.png ni un .glb existente sin --forzar: son
    archivos para pintar y esculpir a mano.
  - Ejes como en el juego (src/world/props.c dibuja el marcador con
    size = {largo, alto, ancho}): largo -> X (+X es el frente), alto -> Y, ancho -> Z.
  - Origen en el suelo, en el centro de la base; los objetos con etiqueta manos:*
    (armas, escudos, herramientas) tienen el origen en el punto de agarre.
Dependencias: Python 3 y Pillow.
"""
import json
import math
import os
import random
import struct
import sys
import zlib
from collections import OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import inventario as inv  # noqa: E402

ROOT = inv.ROOT
DIR = os.path.join(ROOT, "arte", "nomad")  # fuera de assets/: no va en el APK ni junto al ejecutable
DIR_MODELOS = os.path.join(DIR, "modelos")
ATLAS = os.path.join(DIR, "atlas_materiales.png")
ATLAS_GUIA = os.path.join(DIR, "atlas_materiales_guia.png")
ATLAS_TSV = os.path.join(DIR, "atlas_materiales.tsv")
INDICE_MD = os.path.join(DIR, "INVENTARIO_MODELOS.md")
INDICE_TSV = os.path.join(DIR, "inventario_modelos.tsv")
GENERADOR = "ESTEPA tools/assets/nomad.py"

CELDA = 32          # px por celda del atlas
REJILLA = 8         # 8x8 celdas
LADO = CELDA * REJILLA
MARGEN_UV = 2       # px de margen dentro de cada celda (evita sangrado entre celdas)

# ---------------------------------------------------------------------------
# Atlas de materiales: 64 celdas (nombre, uso, color RGB, estilo del ruido).
# Los colores de oro, plata, turquesa, cornalina, lapislazuli, cuero y hueso son
# los de la paleta de docs/ESTILO_VISUAL.md.
# ---------------------------------------------------------------------------
CELDAS = [
    ("fieltro_blanco", "Paredes de yurta, fieltro claro", (236, 230, 214), "fieltro"),
    ("fieltro_crema", "Techos de yurta, fieltro envejecido", (214, 196, 160), "fieltro"),
    ("fieltro_gris", "Fieltro gris, armaduras de fieltro", (150, 146, 138), "fieltro"),
    ("fieltro_oscuro", "Fieltro oscuro, chamanes", (72, 66, 62), "fieltro"),
    ("fieltro_rojo", "Fieltro rojo, adornos y deel", (168, 52, 40), "fieltro"),
    ("fieltro_azul", "Fieltro azul, deel", (52, 74, 132), "fieltro"),
    ("fieltro_ocre", "Fieltro ocre, pastores", (186, 138, 64), "fieltro"),
    ("madera_pintada", "Puertas de yurta, madera pintada", (198, 92, 44), "madera"),
    ("cuero_claro", "Cuero curtido claro, odres", (176, 128, 82), "cuero"),
    ("cuero", "Cuero, armaduras laminares, arreos", (124, 82, 50), "cuero"),
    ("cuero_oscuro", "Cuero viejo (paleta: cuero)", (54, 37, 26), "cuero"),
    ("piel_pardo", "Pieles con pelo, oso, jabali", (112, 82, 56), "pelaje"),
    ("piel_blanca", "Pieles de invierno, leopardo de las nieves", (226, 222, 210), "pelaje"),
    ("lana", "Lana de oveja", (220, 210, 186), "lana"),
    ("cuerda", "Cuerdas, cinchas, crin trenzada", (170, 140, 96), "cuerda"),
    ("fuego", "Fuego y brasas", (232, 120, 32), "fuego"),
    ("madera_clara", "Madera nueva, astas, mangos", (196, 156, 104), "madera"),
    ("madera", "Madera, armazones, ruedas", (138, 96, 58), "madera"),
    ("madera_oscura", "Madera vieja o tratada", (84, 58, 36), "madera"),
    ("corteza", "Corteza de troncos y pinos", (96, 76, 58), "corteza"),
    ("abedul", "Corteza de abedul", (226, 222, 212), "abedul"),
    ("mimbre", "Mimbre trenzado, cestos, escudo de mimbre", (192, 160, 100), "tejido"),
    ("hierba_seca", "Paja, techos de paja, hierba seca", (204, 176, 98), "hierba"),
    ("carbon", "Carbon, madera quemada, hollin", (40, 36, 34), "piedra"),
    ("bronce", "Bronce", (176, 124, 58), "metal"),
    ("cobre", "Cobre", (184, 98, 58), "metal"),
    ("hierro", "Hierro forjado", (104, 104, 108), "metal"),
    ("acero", "Acero, hojas pulidas", (160, 166, 172), "metal"),
    ("oro", "Oro (paleta)", (212, 166, 72), "metal"),
    ("plata", "Plata, estano (paleta)", (192, 192, 184), "metal"),
    ("hielo", "Hielo, glaciar, tempanos", (176, 214, 226), "gema"),
    ("hierro_oxidado", "Hierro oxidado, culto", (128, 72, 46), "piedra"),
    ("turquesa", "Turquesa (paleta)", (68, 198, 196), "gema"),
    ("cornalina", "Cornalina (paleta)", (184, 62, 40), "gema"),
    ("lapislazuli", "Lapislazuli (paleta)", (44, 66, 136), "gema"),
    ("hueso", "Hueso, craneos (paleta)", (242, 228, 192), "hueso"),
    ("cuerno", "Cuerno, astas, pezunas", (116, 96, 72), "cuerno"),
    ("piedra", "Piedra gris, rocas, muros", (138, 134, 124), "piedra"),
    ("piedra_oscura", "Piedra oscura, basalto", (84, 82, 80), "piedra"),
    ("arenisca", "Arenisca, arena, dunas", (200, 170, 120), "piedra"),
    ("adobe", "Adobe y barro", (176, 136, 96), "piedra"),
    ("carne", "Carne fresca", (164, 58, 52), "cuero"),
    ("teja", "Tejas de barro cocido", (150, 70, 48), "teja"),
    ("tela_roja", "Tela roja", (176, 48, 40), "tela"),
    ("tela_azul", "Tela azul, uniformes", (50, 70, 140), "tela"),
    ("tela_ocre", "Tela ocre", (190, 146, 70), "tela"),
    ("tela_lino", "Lino crudo, tunicas", (222, 210, 180), "tela"),
    ("tela_negra", "Tela negra, culto", (34, 30, 34), "tela"),
    ("piel_clara", "Piel humana clara", (232, 190, 160), "piel"),
    ("piel_media", "Piel humana media", (204, 150, 110), "piel"),
    ("piel_morena", "Piel humana morena", (140, 92, 62), "piel"),
    ("cabello", "Cabello y barba", (30, 24, 22), "pelaje"),
    ("pelaje_bayo", "Caballo bayo", (176, 124, 72), "pelaje"),
    ("pelaje_castano", "Caballo castano, ganado", (110, 62, 36), "pelaje"),
    ("pelaje_negro", "Pelaje negro, cuervo, yak", (36, 32, 30), "pelaje"),
    ("pelaje_tordo", "Pelaje gris, lobo, burro", (168, 166, 160), "pelaje"),
    ("pelaje_leonado", "Camello, felinos, antilopes", (192, 150, 96), "pelaje"),
    ("pelaje_tigre", "Tigre (rayas)", (214, 122, 40), "tigre"),
    ("plumas", "Plumas de rapaz", (124, 96, 70), "plumas"),
    ("escamas", "Escamas: reptiles, peces", (92, 110, 64), "escamas"),
    ("hierba", "Hierba verde de la estepa", (112, 150, 62), "hierba"),
    ("hojas", "Hojas y agujas de coniferas", (52, 92, 56), "hierba"),
    ("agua", "Agua", (52, 110, 140), "agua"),
    ("nieve", "Nieve", (240, 244, 248), "nieve"),
]
assert len(CELDAS) == REJILLA * REJILLA
CEL = {c[0]: i for i, c in enumerate(CELDAS)}


def _tonos(rgb, estilo):
    """Cuatro tonos por celda: base, oscuro, mas oscuro, claro (256 colores en total)."""
    def k(f):
        return tuple(max(0, min(255, int(round(v * f)))) for v in rgb)
    if estilo == "fuego":
        return [rgb, (176, 52, 24), (96, 28, 18), (252, 208, 80)]
    if estilo == "tigre":
        return [rgb, k(0.85), (30, 22, 18), (238, 214, 170)]
    if estilo == "abedul":
        return [rgb, k(0.85), (40, 36, 34), (248, 246, 240)]
    if estilo == "metal":
        return [rgb, k(0.82), k(0.66), k(1.22)]
    if estilo == "gema":
        return [rgb, k(0.8), k(0.6), k(1.18)]
    if estilo == "nieve":
        return [rgb, (214, 224, 236), (190, 204, 220), (255, 255, 255)]
    return [rgb, k(0.88), k(0.74), k(1.1)]


def _patron(estilo, rng):
    """Mapa 32x32 de indices de tono (0..3) con ruido de pixel de baja resolucion."""
    n = CELDA
    g = [[0] * n for _ in range(n)]

    def bloque(x, y, t, s=2):
        for yy in range(y, min(n, y + s)):
            for xx in range(x, min(n, x + s)):
                g[yy][xx] = t
    if estilo in ("fieltro", "lana", "piel", "hueso"):
        p_osc, p_cla = {"fieltro": (0.16, 0.08), "lana": (0.25, 0.2), "piel": (0.06, 0.04), "hueso": (0.1, 0.05)}[estilo]
        for y in range(0, n, 2):
            for x in range(0, n, 2):
                r = rng.random()
                bloque(x, y, 1 if r < p_osc else 3 if r < p_osc + p_cla else 0)
        if estilo == "lana":
            for _ in range(10):
                x, y = rng.randrange(n - 3), rng.randrange(n - 3)
                g[y][x] = g[y][x + 2] = g[y + 2][x + 1] = 2
        if estilo == "hueso":
            for _ in range(3):
                x, y = rng.randrange(n), rng.randrange(n)
                for i in range(rng.randrange(4, 10)):
                    g[(y + i // 3) % n][(x + i) % n] = 1
    elif estilo in ("cuero",):
        centros = [(rng.randrange(n), rng.randrange(n), rng.uniform(3, 7)) for _ in range(6)]
        for y in range(n):
            for x in range(n):
                if any((x - cx) ** 2 + (y - cy) ** 2 < r * r for cx, cy, r in centros):
                    g[y][x] = 1
        for _ in range(4):  # grietas
            x, y = rng.randrange(n), rng.randrange(n)
            for i in range(rng.randrange(5, 12)):
                g[y % n][x % n] = 2
                x += rng.choice((1, 1, 0))
                y += rng.choice((1, 0, -1))
        for _ in range(12):
            g[rng.randrange(n)][rng.randrange(n)] = 3
    elif estilo == "pelaje":
        for x in range(n):
            y = 0
            while y < n:
                run = rng.randrange(2, 7)
                r = rng.random()
                t = 1 if r < 0.3 else 3 if r < 0.42 else 2 if r < 0.48 else 0
                for yy in range(y, min(n, y + run)):
                    g[yy][x] = t
                y += run
    elif estilo in ("madera", "cuerno"):
        fase = rng.uniform(0, 6)
        for y in range(n):
            for x in range(n):
                v = math.sin((y + 1.5 * math.sin(x * 0.3 + fase)) * (0.9 if estilo == "madera" else 0.5))
                g[y][x] = 1 if v > 0.75 else 2 if v < -0.92 else 0
        for _ in range(3):
            g[rng.randrange(n)][rng.randrange(n)] = 2
        for _ in range(6):
            g[rng.randrange(n)][rng.randrange(n)] = 3
    elif estilo == "corteza":
        for x in range(0, n, 2):
            y = 0
            while y < n:
                run = rng.randrange(3, 9)
                t = rng.choice((0, 0, 1, 1, 2, 3))
                for yy in range(y, min(n, y + run)):
                    g[yy][x] = t
                    g[yy][min(n - 1, x + 1)] = t
                y += run
    elif estilo == "abedul":
        for _ in range(14):
            x, y = rng.randrange(n), rng.randrange(n)
            for i in range(rng.randrange(3, 8)):
                g[y][(x + i) % n] = 2
        for y in range(0, n, 2):
            for x in range(0, n, 2):
                if rng.random() < 0.08:
                    bloque(x, y, 1)
    elif estilo == "tejido":
        for y in range(n):
            for x in range(n):
                g[y][x] = 1 if ((x // 4 + y // 4) % 2) ^ ((x + y) % 4 == 0) else 0
                if (x - y) % 8 == 0:
                    g[y][x] = 2
    elif estilo == "tela":
        for y in range(n):
            for x in range(n):
                g[y][x] = 1 if (x + y) % 2 == 0 and rng.random() < 0.5 else 0
        for _ in range(8):
            g[rng.randrange(n)][rng.randrange(n)] = 3
    elif estilo == "hierba":
        for _ in range(70):
            x, y = rng.randrange(n), rng.randrange(n)
            t = rng.choice((1, 1, 2, 3))
            for i in range(rng.randrange(2, 6)):
                g[(y - i) % n][x] = t
    elif estilo == "metal":
        for y in range(n):
            for x in range(n):
                d = (x + y) % 16
                g[y][x] = 3 if d in (3, 4) else 1 if d in (11,) else 0
        for _ in range(14):
            g[rng.randrange(n)][rng.randrange(n)] = 2
    elif estilo == "piedra":
        for y in range(0, n, 2):
            for x in range(0, n, 2):
                r = rng.random()
                bloque(x, y, 1 if r < 0.25 else 2 if r < 0.33 else 3 if r < 0.45 else 0)
    elif estilo == "gema":
        for y in range(0, n, 2):
            for x in range(0, n, 2):
                bloque(x, y, 3 if rng.random() < 0.1 else 0)
        for _ in range(5):  # vetas
            x, y = rng.randrange(n), rng.randrange(n)
            for i in range(rng.randrange(6, 14)):
                g[y % n][x % n] = 2
                x += 1
                y += rng.choice((1, 0, 0, -1))
    elif estilo == "teja":
        for y in range(n):
            for x in range(n):
                fila = y // 6
                xx = (x + (3 if fila % 2 else 0)) % 8
                g[y][x] = 2 if y % 6 == 5 else 1 if xx == 0 else 3 if y % 6 == 0 else 0
    elif estilo == "agua":
        for y in range(n):
            for x in range(n):
                v = math.sin(x * 0.45 + y * 1.3 + math.sin(y * 0.7) * 2)
                g[y][x] = 3 if v > 0.93 else 1 if v < -0.6 else 0
    elif estilo == "nieve":
        for y in range(0, n, 2):
            for x in range(0, n, 2):
                r = rng.random()
                bloque(x, y, 1 if r < 0.1 else 3 if r < 0.3 else 0)
    elif estilo == "fuego":
        for y in range(n):
            for x in range(n):
                v = y / n + rng.uniform(-0.18, 0.18) + 0.08 * math.sin(x * 0.8)
                g[y][x] = 3 if v < 0.3 else 0 if v < 0.6 else 1 if v < 0.85 else 2
    elif estilo == "tigre":
        for y in range(n):
            for x in range(n):
                v = math.sin(x * 0.55 + 2.0 * math.sin(y * 0.25))
                g[y][x] = 2 if v > 0.8 else 1 if v > 0.55 else 0
                if y > 26:
                    g[y][x] = 3
    elif estilo == "plumas":
        for y in range(n):
            for x in range(n):
                v = (y + abs((x % 8) - 4)) % 6
                g[y][x] = 2 if v == 0 else 1 if v == 1 else 3 if v == 3 and x % 8 == 4 else 0
    elif estilo == "escamas":
        for y in range(n):
            for x in range(n):
                xx = (x + (3 if (y // 4) % 2 else 0)) % 6
                yy = y % 4
                g[y][x] = 2 if yy == 3 or xx == 0 else 3 if (yy == 0 and xx == 3) else 0
    elif estilo == "cuerda":
        for y in range(n):
            for x in range(n):
                v = (x + y) % 6
                g[y][x] = 2 if v == 0 else 1 if v == 1 else 3 if v == 4 else 0
    return g


def generar_atlas():
    from PIL import Image
    paleta, px = [], []
    img = Image.new("P", (LADO, LADO))
    for i, (nombre, _, rgb, estilo) in enumerate(CELDAS):
        paleta += [c for t in _tonos(rgb, estilo) for c in t]
        rng = random.Random(zlib.crc32(nombre.encode()))
        g = _patron(estilo, rng)
        x0, y0 = (i % REJILLA) * CELDA, (i // REJILLA) * CELDA
        for y in range(CELDA):
            for x in range(CELDA):
                # Texel de 2x2 px: pixel art de baja resolucion (y un PNG mas liviano dentro de cada GLB).
                img.putpixel((x0 + x, y0 + y), i * 4 + g[y & ~1][x & ~1])
    img.putpalette(paleta)
    return img


def cmd_atlas(forzar=False):
    from PIL import Image, ImageDraw
    os.makedirs(DIR, exist_ok=True)
    if forzar or not os.path.isfile(ATLAS):
        generar_atlas().save(ATLAS, optimize=True)
        print(f"Escrito {rel(ATLAS)}")
    else:
        print(f"{rel(ATLAS)} ya existe: no se toca (usa --forzar para regenerarlo y perder lo pintado).")
    atlas = Image.open(ATLAS).convert("RGB")
    if atlas.size != (LADO, LADO):
        print(f"AVISO: el atlas mide {atlas.size}, se espera {LADO}x{LADO}; la guia lo escala.")
        atlas = atlas.resize((LADO, LADO), Image.NEAREST)
    k = 4
    guia = atlas.resize((LADO * k, LADO * k), Image.NEAREST).convert("RGBA")
    capa = Image.new("RGBA", guia.size, (0, 0, 0, 0))
    dr = ImageDraw.Draw(capa)
    fuente = _fuente(13)
    s = CELDA * k
    for i, (nombre, _, _, _) in enumerate(CELDAS):
        x0, y0 = (i % REJILLA) * s, (i // REJILLA) * s
        dr.rectangle([x0, y0 + s - 34, x0 + s - 1, y0 + s - 1], fill=(16, 15, 18, 190))
        dr.text((x0 + 4, y0 + s - 33), f"{i:02d} c{i % REJILLA} f{i // REJILLA}", fill=(248, 218, 132, 255), font=fuente)
        dr.text((x0 + 4, y0 + s - 17), nombre, fill=(242, 228, 192, 255), font=fuente)
        # zona util (margen de 2 px del atlas)
        m = MARGEN_UV * k
        dr.rectangle([x0 + m, y0 + m, x0 + s - 1 - m, y0 + s - 1 - m], outline=(255, 255, 255, 60))
    for j in range(REJILLA + 1):
        p = min(j * s, LADO * k - 1)
        dr.line([(p, 0), (p, LADO * k)], fill=(16, 15, 18, 255), width=2)
        dr.line([(0, p), (LADO * k, p)], fill=(16, 15, 18, 255), width=2)
    Image.alpha_composite(guia, capa).convert("RGB").save(ATLAS_GUIA, optimize=True)
    print(f"Escrito {rel(ATLAS_GUIA)}")
    with open(ATLAS_TSV, "w", encoding="utf-8") as f:
        f.write("celda\tcolumna\tfila\tnombre\tuso\n")
        for i, (nombre, uso, _, _) in enumerate(CELDAS):
            f.write(f"{i}\t{i % REJILLA}\t{i // REJILLA}\t{nombre}\t{uso}\n")
    print(f"Escrito {rel(ATLAS_TSV)}")
    return 0


def _fuente(tam):
    from PIL import ImageFont
    try:
        return ImageFont.load_default(size=tam)
    except TypeError:
        return ImageFont.load_default()


def medias_atlas():
    """Color medio (sRGB 0..255) de la zona util de cada celda del atlas actual."""
    from PIL import Image
    img = Image.open(ATLAS).convert("RGB")
    if img.size != (LADO, LADO):
        img = img.resize((LADO, LADO), Image.NEAREST)
    out = []
    for i in range(len(CELDAS)):
        x0, y0 = (i % REJILLA) * CELDA + MARGEN_UV, (i // REJILLA) * CELDA + MARGEN_UV
        reg = img.crop((x0, y0, x0 + CELDA - 2 * MARGEN_UV, y0 + CELDA - 2 * MARGEN_UV)).resize((1, 1), Image.BOX)
        out.append(reg.getpixel((0, 0)))
    return out


# ---------------------------------------------------------------------------
# Geometria: vectores como tuplas, triangulos como (p0, p1, p2).
# ---------------------------------------------------------------------------
def va(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def vs(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def vm(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cruz(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def vlen(a): return math.sqrt(dot(a, a))


def vn(a):
    l = vlen(a)
    return (a[0] / l, a[1] / l, a[2] / l) if l > 1e-12 else (0.0, 1.0, 0.0)


def lerp(a, b, t): return va(a, vm(vs(b, a), t))
def centro(pts): return vm(tuple(map(sum, zip(*pts))), 1.0 / len(pts))
def normal_tri(t): return cruz(vs(t[1], t[0]), vs(t[2], t[0]))


def orientar(t, fuera):
    return t if dot(normal_tri(t), fuera) >= 0 else (t[0], t[2], t[1])


def perp(d):
    h = (1.0, 0.0, 0.0) if abs(d[0]) < 0.9 else (0.0, 0.0, 1.0)
    u = vn(cruz(h, d))
    return u, vn(cruz(d, u))


def anillo(c, d, ru, rv, n, fase=0.0, u=None, v=None):
    if u is None:
        u, v = perp(vn(d))
    return [va(c, va(vm(u, math.cos(fase + 2 * math.pi * k / n) * ru), vm(v, math.sin(fase + 2 * math.pi * k / n) * rv)))
            for k in range(n)]


def loft(anillos, tapas=True, cerrado=False):
    """Une anillos con el mismo numero de puntos (o un solo punto: polo). Caras hacia fuera."""
    tris = []
    cs = [centro(a) for a in anillos]
    seq = list(range(len(anillos))) + ([0] if cerrado else [])
    for i, j in zip(seq, seq[1:]):
        a, b = anillos[i], anillos[j]
        mid = lerp(cs[i], cs[j], 0.5)
        if len(a) == 1 and len(b) == 1:
            continue
        if len(a) == 1 or len(b) == 1:
            p, r = (a[0], b) if len(a) == 1 else (b[0], a)
            for k in range(len(r)):
                t = (p, r[k], r[(k + 1) % len(r)])
                tris.append(orientar(t, vs(centro(t), mid)))
            continue
        n = len(a)
        for k in range(n):
            k2 = (k + 1) % n
            for t in ((a[k], a[k2], b[k2]), (a[k], b[k2], b[k])):
                tris.append(orientar(t, vs(centro(t), mid)))
    if tapas and not cerrado:
        for idx, otro in ((0, 1), (len(anillos) - 1, len(anillos) - 2)):
            r = anillos[idx]
            if len(r) > 2:
                fuera = vs(cs[idx], cs[otro]) if len(anillos) > 1 else (0, 1, 0)
                for k in range(1, len(r) - 1):
                    tris.append(orientar((r[0], r[k], r[k + 1]), fuera))
    return tris


def tronco(p0, p1, r0, r1, n, fase=0.0, rv0=None, rv1=None, tapas=True, u=None, v=None):
    """Tronco de cono (cilindro, cono si r1 == 0) entre p0 y p1."""
    d = vs(p1, p0)
    if u is None:
        u, v = perp(vn(d))
    a = anillo(p0, d, r0, rv0 if rv0 is not None else r0, n, fase, u, v)
    b = [p1] if r1 <= 0 else anillo(p1, d, r1, rv1 if rv1 is not None else r1, n, fase, u, v)
    return loft([a, b], tapas)


def viga(p0, p1, g, g2=None):
    """Viga de seccion rectangular (g x g2) entre dos puntos."""
    d = vn(vs(p1, p0))
    if abs(d[1]) > 0.9:
        u, v = (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)
    else:
        u = vn(cruz(d, (0.0, 1.0, 0.0)))
        v = vn(cruz(u, d))
    g2 = g if g2 is None else g2
    k = math.sqrt(2) / 2
    return tronco(p0, p1, g * k, g * k, 4, math.pi / 4, g2 * k, g2 * k, u=u, v=v)


def caja(c, s):
    hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
    p = [(c[0] + x * hx, c[1] + y * hy, c[2] + z * hz) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
    caras = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    tris = []
    for a, b, cc, d in caras:
        for t in ((p[a], p[b], p[cc]), (p[a], p[cc], p[d])):
            tris.append(orientar(t, vs(centro(t), c)))
    return tris


def caja_suelo(x, z, sx, sy, sz, y=0.0):
    return caja((x, y + sy / 2, z), (sx, sy, sz))


def elipsoide(c, rx, ry, rz, n=8, m=5):
    anillos = [[va(c, (0, -ry, 0))]]
    for i in range(1, m):
        a = -math.pi / 2 + math.pi * i / m
        y, r = math.sin(a), math.cos(a)
        anillos.append([va(c, (math.cos(2 * math.pi * k / n) * rx * r, y * ry, math.sin(2 * math.pi * k / n) * rz * r))
                        for k in range(n)])
    anillos.append([va(c, (0, ry, 0))])
    return loft(anillos, tapas=False)


def cupula(c, rx, ry, rz, n=8, m=3, base=True):
    """Media elipsoide (base en y = c.y)."""
    anillos = []
    for i in range(m):
        a = math.pi / 2 * i / m
        y, r = math.sin(a), math.cos(a)
        anillos.append([va(c, (math.cos(2 * math.pi * k / n) * rx * r, y * ry, math.sin(2 * math.pi * k / n) * rz * r))
                        for k in range(n)])
    anillos.append([va(c, (0, ry, 0))])
    return loft(anillos, tapas=base)


def roca(c, rx, ry, rz, n, m, rng, j=0.22):
    anillos = [[va(c, (0, -ry * 0.3, 0))]]
    for i in range(1, m):
        a = -math.pi / 2 + math.pi * i / m
        y, r = math.sin(a), math.cos(a)
        ring = []
        for k in range(n):
            f = 1 + rng.uniform(-j, j)
            ring.append(va(c, (math.cos(2 * math.pi * k / n) * rx * r * f, max(y, -0.3) * ry * (1 + rng.uniform(-j, j) * 0.5),
                               math.sin(2 * math.pi * k / n) * rz * r * f)))
        anillos.append(ring)
    anillos.append([va(c, (rng.uniform(-0.2, 0.2) * rx, ry, rng.uniform(-0.2, 0.2) * rz))])
    return loft(anillos, tapas=False)


def toro(c, R, r, n, m, eje="y", rz=None):
    anillos = []
    for i in range(n):
        a = 2 * math.pi * i / n
        if eje == "y":
            radial, d = (math.cos(a), 0.0, math.sin(a)), (0.0, 1.0, 0.0)
            cc = va(c, (math.cos(a) * R, 0, math.sin(a) * (rz or R)))
        elif eje == "x":
            radial, d = (0.0, math.cos(a), math.sin(a)), (1.0, 0.0, 0.0)
            cc = va(c, (0, math.cos(a) * R, math.sin(a) * (rz or R)))
        else:
            radial, d = (math.cos(a), math.sin(a), 0.0), (0.0, 0.0, 1.0)
            cc = va(c, (math.cos(a) * R, math.sin(a) * (rz or R), 0))
        anillos.append([va(cc, va(vm(radial, math.cos(2 * math.pi * k / m) * r), vm(d, math.sin(2 * math.pi * k / m) * r)))
                        for k in range(m)])
    return loft(anillos, tapas=False, cerrado=True)


def cinta(puntos, anchos, grosor, normal=(1.0, 0.0, 0.0), asim=1.0):
    """Hoja/listón a lo largo de una curva. anchos: medio ancho por punto (0 = punta).
    asim < 1 deja un lomo (filo de un solo lado, como un sable)."""
    anillos = []
    gs = grosor if isinstance(grosor, (list, tuple)) else [grosor] * len(puntos)
    for i, p in enumerate(puntos):
        t = vn(vs(puntos[min(i + 1, len(puntos) - 1)], puntos[max(i - 1, 0)]))
        lado = vn(cruz(normal, t))
        w = anchos[i]
        if w <= 1e-6:
            anillos.append([p])
            continue
        g = gs[i]
        anillos.append([va(p, vm(lado, w)), va(p, vm(normal, g)), va(p, vm(lado, -w * asim)), va(p, vm(normal, -g))])
    return loft(anillos)


def placa(puntos, anchos, grosor, normal=(1.0, 0.0, 0.0)):
    """Como cinta, pero con seccion rectangular (tabla, vela, ala)."""
    anillos = []
    for i, p in enumerate(puntos):
        t = vn(vs(puntos[min(i + 1, len(puntos) - 1)], puntos[max(i - 1, 0)]))
        lado = vn(cruz(normal, t))
        w = max(anchos[i], 1e-4)
        anillos.append([va(va(p, vm(lado, sw * w)), vm(normal, sn * grosor)) for sw, sn in ((1, 1), (-1, 1), (-1, -1), (1, -1))])
    return loft(anillos)


def extruir(poli, origen, U, V, D, largo):
    """Extruye un poligono convexo (coordenadas (u, v)) a lo largo de D."""
    a = [va(origen, va(vm(U, u), vm(V, v))) for u, v in poli]
    b = [va(p, vm(D, largo)) for p in a]
    return loft([a, b])


def prisma_tejado(x0, x1, ancho, y0, alto, z=0.0):
    """Tejado a dos aguas: triangulo en el plano ZY extruido a lo largo de X."""
    return extruir([(-ancho / 2, 0), (ancho / 2, 0), (0, alto)], (x0, y0, z), (0, 0, 1), (0, 1, 0), (1, 0, 0), x1 - x0)


def octaedro(c, r):
    p = [va(c, d) for d in ((r, 0, 0), (-r, 0, 0), (0, r, 0), (0, -r, 0), (0, 0, r), (0, 0, -r))]
    tris = []
    for a in (0, 1):
        for b in (2, 3):
            for cc in (4, 5):
                t = (p[a], p[b], p[cc])
                tris.append(orientar(t, vs(centro(t), c)))
    return tris


def tetraedro(c, r):
    p = [va(c, d) for d in ((r, -r * 0.5, 0), (-r * 0.5, -r * 0.5, r * 0.87), (-r * 0.5, -r * 0.5, -r * 0.87), (0, r, 0))]
    return [orientar((p[a], p[b], p[d]), vs(centro((p[a], p[b], p[d])), c)) for a, b, d in ((0, 1, 2), (0, 1, 3), (1, 2, 3), (0, 2, 3))]


def icosaedro(c, r):
    t = (1 + math.sqrt(5)) / 2
    v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
         (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    v = [va(c, vm(vn(p), r)) for p in v]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    return [orientar((v[a], v[b], v[d]), vs(centro((v[a], v[b], v[d])), c)) for a, b, d in f]


def caja_biselada(c, s, n_bisel=0.18):
    """Caja con aristas achaflanadas (forma de respaldo)."""
    sx, sy, sz = s
    b = min(sx, sz) * n_bisel
    by = sy * 0.12

    def oct_(dx, dz):
        hx, hz = sx / 2 - dx, sz / 2 - dz
        bb = max(1e-4, b - max(dx, dz))
        return [(hx, hz - bb), (hx - bb, hz), (-hx + bb, hz), (-hx, hz - bb), (-hx, -hz + bb), (-hx + bb, -hz), (hx - bb, -hz), (hx, -hz + bb)]
    anillos = []
    for y, ins in ((-sy / 2, b * 0.6), (-sy / 2 + by, 0), (sy / 2 - by, 0), (sy / 2, b * 0.6)):
        anillos.append([(c[0] + x, c[1] + y, c[2] + z) for x, z in oct_(ins, ins)])
    return loft(anillos)


# Transformaciones (no cambian el sentido de las caras salvo espejo).
def T(tris, f): return [(f(t[0]), f(t[1]), f(t[2])) for t in tris]
def mover(tris, d): return T(tris, lambda p: va(p, d))


def rotar(tris, eje, ang, piv=(0.0, 0.0, 0.0)):
    c, s = math.cos(ang), math.sin(ang)

    def f(p):
        x, y, z = vs(p, piv)
        if eje == "x":
            q = (x, y * c - z * s, y * s + z * c)
        elif eje == "y":
            q = (x * c + z * s, y, -x * s + z * c)
        else:
            q = (x * c - y * s, x * s + y * c, z)
        return va(q, piv)
    return T(tris, f)


def espejo_z(tris): return [(t[0], t[2], t[1]) for t in T(tris, lambda p: (p[0], p[1], -p[2]))]


def S(d, lo, hi):
    """Segmentos segun el nivel de detalle d (0..3)."""
    return int(round(lo + (hi - lo) * d / 3.0))


class Modelo:
    def __init__(self):
        self.partes = OrderedDict()   # nombre -> [celda, tris]
        self.agarre = None            # punto de agarre en coordenadas de diseno

    def add(self, nombre, celda, tris):
        c = CEL[celda] if isinstance(celda, str) else celda
        key, i = nombre, 2
        while key in self.partes and self.partes[key][0] != c:
            key, i = f"{nombre}_{i}", i + 1
        tris = [t for t in tris if vlen(normal_tri(t)) > 1e-14]
        self.partes.setdefault(key, [c, []])[1].extend(tris)

    def ntris(self):
        return sum(len(p[1]) for p in self.partes.values())

    def bbox(self):
        pts = [p for _, ts in self.partes.values() for t in ts for p in t]
        return tuple(min(p[k] for p in pts) for k in range(3)), tuple(max(p[k] for p in pts) for k in range(3))

    def normalizar(self, L, H, W):
        """Escala cada eje a las medidas exactas (X=largo, Y=alto, Z=ancho) y coloca el origen."""
        mn, mx = self.bbox()
        tgt = (L, H, W)
        esc = [tgt[k] / (mx[k] - mn[k]) if mx[k] - mn[k] > 1e-9 else 1.0 for k in range(3)]

        def f(p):
            return tuple((p[k] - mn[k]) * esc[k] for k in range(3))
        if self.agarre is not None:
            g = f(self.agarre)
            off = (-g[0], -g[1], -g[2])
        else:
            off = (-L / 2, 0.0, -W / 2)
        for key in self.partes:
            self.partes[key][1] = T(self.partes[key][1], lambda p: va(f(p), off))


# ---------------------------------------------------------------------------
# Familias de formas. Cada constructor recibe (m, it, d, rng, W, H, L) y dibuja en
# metros aproximados; luego Modelo.normalizar ajusta a las medidas exactas.
# W = ancho (Z), H = alto (Y), L = largo (X).
# ---------------------------------------------------------------------------
def nombre_de(it): return it["id"].split(".")[-1]


def tag(it, clave):
    for t in it["tags"]:
        k, _, v = t.partition(":")
        if k == clave:
            return v
    return None


def metal_de(it, defecto="hierro"):
    n = nombre_de(it) + " " + (tag(it, "material") or "")
    for k, c in (("padre", "oro"), ("oro", "oro"), ("bronce", "bronce"), ("acero", "acero"), ("damasquinado", "acero"),
                 ("hierro", "hierro"), ("cobre", "cobre"), ("estano", "plata"), ("culto", "hierro_oxidado")):
        if k in n:
            return c
    return defecto


# --- Personajes -------------------------------------------------------------
ROLES = {
    # ropa del torso, mangas, pantalon, botas, sombrero, faldon (deel), capa, barba
    "protagonista": dict(ropa="fieltro_azul", faldon="fieltro_azul", gorro="piel", faja="fieltro_ocre"),
    "padre": dict(ropa="fieltro_rojo", faldon="fieltro_rojo", gorro="casco_oro", barba=True, faja="oro"),
    "madre": dict(ropa="tela_azul", faldon="tela_azul", gorro="alto", faja="tela_roja"),
    "medio_hermano": dict(ropa="fieltro_oscuro", faldon="fieltro_oscuro", gorro="punta", faja="tela_roja"),
    "rey": dict(ropa="tela_roja", faldon="tela_roja", gorro="corona", capa="piel_blanca", barba=True, faja="oro"),
    "pastor": dict(ropa="fieltro_ocre", faldon="fieltro_ocre", gorro="punta"),
    "jinete_arquero": dict(ropa="tela_azul", faldon="tela_azul", gorro="piel", carcaj=True),
    "guerrero_nomada": dict(ropa="cuero", faldon="cuero", gorro="casco", hombreras="cuero"),
    "chaman": dict(ropa="fieltro_oscuro", faldon="fieltro_oscuro", gorro="plumas", flecos=True),
    "curandero": dict(ropa="tela_lino", faldon="tela_lino", gorro="punta", barba=True),
    "herrero": dict(ropa="cuero_oscuro", pantalon="tela_ocre", delantal=True),
    "explorador": dict(ropa="piel_pardo", faldon="piel_pardo", gorro="piel"),
    "campesino": dict(ropa="tela_lino", pantalon="tela_ocre", gorro="paja"),
    "obrero": dict(ropa="tela_ocre", pantalon="cuero"),
    "burocrata": dict(ropa="tela_azul", faldon="tela_azul", gorro="alto"),
    "lancero": dict(ropa="hierro", pantalon="tela_roja", gorro="casco", hombreras="hierro"),
    "ballestero": dict(ropa="tela_roja", pantalon="tela_ocre", gorro="casco"),
    "mosquetero": dict(ropa="tela_azul", pantalon="tela_ocre", gorro="ala"),
    "artillero": dict(ropa="tela_roja", pantalon="tela_azul", gorro="ala"),
    "oficial": dict(ropa="tela_azul", pantalon="tela_azul", gorro="casco", hombreras="oro", capa="tela_roja"),
    "verdugo": dict(ropa="tela_negra", pantalon="tela_negra", gorro="capucha"),
    "sacerdote_culto": dict(ropa="tela_negra", faldon="tela_negra", gorro="capucha", capa="tela_roja"),
    "fanatico": dict(ropa="tela_roja", pantalon="tela_negra", gorro="capucha"),
    "captor_culto": dict(ropa="tela_negra", pantalon="cuero_oscuro", gorro="capucha", hombreras="hierro_oxidado"),
    "prisionero": dict(ropa="tela_lino", pantalon="tela_lino", pies="piel"),
    "refugiado": dict(ropa="tela_ocre", faldon="tela_ocre", gorro="pañuelo"),
    "comerciante": dict(ropa="tela_roja", faldon="tela_roja", gorro="turbante", faja="oro"),
    "nomada_desierto": dict(ropa="tela_lino", faldon="tela_lino", gorro="turbante", piel="piel_morena"),
    "cazador_bosque": dict(ropa="piel_pardo", pantalon="cuero", gorro="piel", piel="piel_clara"),
    "pescador_fiordo": dict(ropa="lana", pantalon="cuero", gorro="piel", piel="piel_clara", barba=True),
    "bandido": dict(ropa="cuero_oscuro", pantalon="cuero", gorro="pañuelo"),
    "mercenario": dict(ropa="hierro", pantalon="cuero", gorro="casco", hombreras="cuero"),
    "nino": dict(ropa="fieltro_rojo", faldon="fieltro_rojo"),
    "anciano": dict(ropa="tela_lino", faldon="tela_lino", gorro="punta", barba=True),
}


def b_humanoide(m, it, d, rng, W, H, L):
    nom, sub = nombre_de(it), it["id"].split(".")[1]
    base = sub == "base"
    rol = ROLES.get(nom, {}) if not base else {}
    pieles = ["piel_clara", "piel_media", "piel_media", "piel_morena"]
    piel = rol.get("piel", pieles[zlib.crc32(nom.encode()) % 4] if not base else "piel_media")
    ropa = rol.get("ropa", piel)
    pant = rol.get("pantalon", rol.get("faldon", piel) if not base else piel)
    if "faldon" in rol and "pantalon" not in rol:
        pant = "cuero_oscuro"
    pies = rol.get("pies", "cuero_oscuro" if not base else piel)
    if pies == "piel":  # descalzo
        pies = piel
    n = S(d, 5, 7)
    nh = S(d, 6, 8)
    ancho = {"cuerpo_robusto": 1.18, "cuerpo_gigante": 1.22, "cuerpo_nino": 0.95}.get(nom, 1.0)
    cab = {"cuerpo_nino": 1.35, "nino": 1.1}.get(nom, 1.0)
    enc = 0.05 if nom in ("cuerpo_anciano", "anciano") else 0.0  # leve encorvado
    # Piernas y pies (izquierda en -Z: el personaje mira a +X, su derecha es +Z).
    for lado, s in (("izq", -1), ("der", 1)):
        z = 0.095 * s * ancho
        m.add(f"pie_{lado}", pies, caja((0.06, 0.04, z), (0.25, 0.08, 0.1)))
        m.add(f"pierna_{lado}", pant, tronco((0, 0.07, z), (0, 0.48, z), 0.045, 0.058, n))
        m.add(f"muslo_{lado}", pant, tronco((0, 0.47, z), (0, 0.92, z * 0.95), 0.06, 0.08 * ancho, n))
    m.add("pelvis", "tela_lino" if base else pant, elipsoide((0, 0.95, 0), 0.11, 0.09, 0.16 * ancho, n, 4))
    # Torso: anillos elipticos (rz = hombros, rx = pecho).
    anillos = []
    for y, rx, rz in ((0.92, 0.1, 0.15), (1.12, 0.1, 0.145), (1.32, 0.115, 0.185), (1.43, 0.095, 0.17)):
        anillos.append(anillo((enc * (y - 0.9) * 2, y, 0), (0, 1, 0), rx, rz * ancho, n, 0, (1, 0, 0), (0, 0, 1)))
    m.add("torso", ropa, loft(anillos))
    m.add("cuello", piel, tronco((enc, 1.42, 0), (enc * 1.4, 1.53, 0), 0.05, 0.045, n))
    hy = 1.62
    hx = enc * 1.8
    m.add("cabeza", piel, elipsoide((hx, hy, 0), 0.1 * cab, 0.12 * cab, 0.09 * cab, nh, S(d, 4, 6)))
    m.add("cabeza", piel, tronco((hx + 0.085 * cab, hy - 0.01, 0), (hx + 0.125 * cab, hy - 0.03, 0), 0.025, 0.0, 3))  # nariz
    if not base or nom == "cuerpo_anciano":
        m.add("cabello", "cabello" if nom != "anciano" else "tela_lino",
              cupula((hx - 0.015, hy + 0.01, 0), 0.1 * cab, 0.125 * cab, 0.098 * cab, nh, 3))
    # Brazos en pose A (unos 20 grados).
    mangas = rol.get("mangas", ropa)
    for lado, s in (("izq", -1), ("der", 1)):
        hom = (enc * 1.2, 1.4, 0.2 * s * ancho)
        codo = (enc * 1.2 + 0.0, 1.14, 0.26 * s * ancho)
        mun = (0.03, 0.9, 0.29 * s * ancho)
        m.add(f"brazo_{lado}", mangas, tronco(hom, codo, 0.055 * ancho, 0.045, n))
        m.add(f"antebrazo_{lado}", mangas, tronco(codo, mun, 0.045, 0.035, n))
        m.add(f"mano_{lado}", piel, elipsoide((0.035, 0.84, 0.295 * s * ancho), 0.035, 0.06, 0.025, S(d, 4, 6), 3))
    # Ropa y accesorios segun el rol.
    if "faldon" in rol:
        m.add("faldon", rol["faldon"], tronco((enc * 0.2, 1.0, 0), (0.0, 0.42, 0), 0.125, 0.17, n, 0, 0.18 * ancho, 0.23 * ancho,
                                              u=(1, 0, 0), v=(0, 0, 1)))
    if "faja" in rol:
        m.add("faja", rol["faja"], tronco((0, 0.93, 0), (0, 1.02, 0), 0.115, 0.115, n, 0, 0.165 * ancho, 0.165 * ancho,
                                          u=(1, 0, 0), v=(0, 0, 1)))
    if rol.get("delantal"):
        m.add("delantal", "cuero", caja((0.11, 0.85, 0), (0.03, 0.6, 0.26)))
    if rol.get("capa"):
        m.add("capa", rol["capa"], placa([(-0.12, 1.45, 0), (-0.17, 0.95, 0), (-0.21, 0.4, 0)], [0.2, 0.24, 0.27], 0.015,
                                         normal=(1, 0, 0)))
    if rol.get("hombreras"):
        for lado, s in (("izq", -1), ("der", 1)):
            m.add(f"hombrera_{lado}", rol["hombreras"], cupula((enc, 1.4, 0.2 * s * ancho), 0.08, 0.06, 0.08, n, 2))
    if rol.get("carcaj"):
        m.add("carcaj", "cuero", rotar(tronco((-0.13, 0.9, 0.0), (-0.13, 1.45, 0.0), 0.05, 0.06, n), "x", 0.35, (-0.13, 1.2, 0)))
    if rol.get("flecos"):
        for k in range(S(d, 4, 8)):
            a = 2 * math.pi * k / S(d, 4, 8)
            p = (0.12 * math.cos(a), 0.9, 0.17 * math.sin(a))
            m.add("flecos", "tela_roja", tronco(p, va(p, (0, -0.25, 0)), 0.012, 0.0, 3))
    if rol.get("barba"):
        m.add("barba", "cabello" if nom not in ("anciano", "curandero") else "tela_lino",
              tronco((hx + 0.07, hy - 0.06, 0), (hx + 0.08, hy - 0.2, 0), 0.05, 0.0, n))
    g = rol.get("gorro")
    top = (hx, hy + 0.1 * cab, 0)
    if g == "punta":
        m.add("gorro", "fieltro_rojo", tronco(va(top, (0, -0.04, 0)), va(top, (0, 0.2, 0)), 0.105, 0.0, n))
        m.add("gorro", "fieltro_rojo", tronco(va(top, (0, -0.06, 0)), va(top, (0, -0.02, 0)), 0.125, 0.125, n))
    elif g == "piel":
        m.add("gorro", "piel_pardo", tronco(va(top, (-0.01, -0.04, 0)), va(top, (-0.01, 0.03, 0)), 0.115, 0.12, n))
        m.add("gorro", "fieltro_rojo", cupula(va(top, (-0.01, 0.01, 0)), 0.1, 0.08, 0.1, n, 2))
    elif g in ("casco", "casco_oro"):
        met = "oro" if g == "casco_oro" else "hierro"
        m.add("casco", met, cupula(va(top, (0, -0.06, 0)), 0.115, 0.13, 0.11, n, 3))
        m.add("casco", met, tronco(va(top, (0, 0.06, 0)), va(top, (0, 0.17, 0)), 0.015, 0.0, 4))
    elif g == "alto":
        m.add("gorro", "tela_roja", tronco(va(top, (0, -0.05, 0)), va(top, (0, 0.2, 0)), 0.11, 0.07, n))
        m.add("gorro", "oro", tronco(va(top, (0, 0.2, 0)), va(top, (0, 0.24, 0)), 0.03, 0.0, 4))
    elif g == "corona":
        m.add("corona", "oro", tronco(va(top, (0, -0.04, 0)), va(top, (0, 0.06, 0)), 0.11, 0.12, n))
        m.add("corona", "turquesa", caja(va(top, (0.11, 0.01, 0)), (0.02, 0.04, 0.04)))
    elif g == "plumas":
        m.add("gorro", "fieltro_oscuro", tronco(va(top, (0, -0.05, 0)), va(top, (0, 0.04, 0)), 0.115, 0.11, n))
        for k in range(5):
            a = -0.8 + 0.4 * k
            m.add("plumas", "plumas", rotar(placa([va(top, (0, 0.03, 0)), va(top, (0, 0.28, 0))], [0.025, 0.035], 0.006,
                                                  normal=(1, 0, 0)), "x", a, va(top, (0, 0.03, 0))))
    elif g == "paja":
        m.add("sombrero", "hierba_seca", tronco(va(top, (0, -0.04, 0)), va(top, (0, 0.12, 0)), 0.26, 0.0, n))
    elif g == "ala":
        m.add("sombrero", "tela_negra", tronco(va(top, (0, -0.05, 0)), va(top, (0, -0.03, 0)), 0.2, 0.2, n))
        m.add("sombrero", "tela_negra", tronco(va(top, (0, -0.03, 0)), va(top, (0, 0.09, 0)), 0.1, 0.09, n))
    elif g == "capucha":
        m.add("capucha", ropa, cupula((hx - 0.02, hy - 0.06, 0), 0.13, 0.22, 0.12, n, 3))
        m.add("capucha", ropa, tronco((hx - 0.02, hy + 0.14, 0), (hx - 0.1, hy + 0.24, 0), 0.04, 0.0, 4))
    elif g == "turbante":
        m.add("turbante", "tela_lino" if ropa != "tela_lino" else "tela_azul", toro(va(top, (0, -0.02, 0)), 0.09, 0.04, n, 4))
        m.add("turbante", "tela_lino" if ropa != "tela_lino" else "tela_azul", cupula(va(top, (0, -0.02, 0)), 0.09, 0.09, 0.09, n, 2))
    elif g == "pañuelo":
        m.add("panuelo", "tela_roja", cupula((hx - 0.03, hy + 0.02, 0), 0.1, 0.12, 0.1, n, 3))


# --- Cuadrupedos ------------------------------------------------------------
# pata: altura de la cruz/longitud; cr: radio del cuerpo; cu: (largo, angulo, radio) del cuello;
# ca: (largo, radio, angulo) de la cabeza; cola: (largo, caida, radio); extras.
ESPECIES = {
    "caballo": dict(pel="pelaje_bayo", pata=0.6, cr=0.19, cu=(0.55, 62, 0.085), ca=(0.42, 0.065, -55), cola=(0.45, 72, 0.035), crin=True, oreja=0.06),
    "mula": dict(pel="pelaje_castano", pata=0.6, cr=0.19, cu=(0.4, 48, 0.085), ca=(0.38, 0.065, -58), cola=(0.4, 75, 0.03), crin=True, oreja=0.11),
    "burro": dict(pel="pelaje_tordo", pata=0.52, cr=0.2, cu=(0.34, 42, 0.085), ca=(0.36, 0.07, -58), cola=(0.38, 80, 0.022), crin=True, oreja=0.14),
    "camello": dict(pel="pelaje_leonado", pata=0.85, cr=0.2, cu=(0.55, 20, 0.08), ca=(0.28, 0.07, -10), cola=(0.3, 80, 0.03), joroba=2, oreja=0.04, cuello_s=True),
    "dromedario": dict(pel="pelaje_leonado", pata=0.85, cr=0.2, cu=(0.55, 20, 0.08), ca=(0.28, 0.07, -10), cola=(0.3, 80, 0.03), joroba=1, oreja=0.04, cuello_s=True),
    "elefante": dict(pel="piedra", pata=0.55, cr=0.32, cu=(0.12, 20, 0.22), ca=(0.32, 0.2, -30), cola=(0.3, 80, 0.025), trompa=True, oreja=0.0, gruesa=2.0),
    "reno": dict(pel="pelaje_tordo", pata=0.6, cr=0.18, cu=(0.32, 45, 0.08), ca=(0.25, 0.06, -45), cola=(0.08, 40, 0.04), astas="reno", oreja=0.06),
    "ciervo": dict(pel="pelaje_castano", pata=0.75, cr=0.16, cu=(0.38, 60, 0.07), ca=(0.25, 0.06, -45), cola=(0.08, 40, 0.035), astas="ciervo", oreja=0.07),
    "ibice": dict(pel="pelaje_leonado", pata=0.6, cr=0.18, cu=(0.25, 45, 0.08), ca=(0.22, 0.06, -45), cola=(0.08, 30, 0.03), cuernos="ibice", oreja=0.05),
    "antilope": dict(pel="pelaje_leonado", pata=0.65, cr=0.15, cu=(0.3, 50, 0.06), ca=(0.22, 0.055, -40), cola=(0.1, 40, 0.025), cuernos="recto", oreja=0.06),
    "gacela": dict(pel="pelaje_leonado", pata=0.7, cr=0.14, cu=(0.3, 55, 0.055), ca=(0.2, 0.05, -40), cola=(0.1, 40, 0.025), cuernos="recto", oreja=0.07),
    "cabra": dict(pel="piel_blanca", pata=0.55, cr=0.18, cu=(0.25, 45, 0.08), ca=(0.22, 0.06, -45), cola=(0.08, 20, 0.03), cuernos="cabra", oreja=0.05, barba=True),
    "oveja": dict(pel="lana", pata=0.45, cr=0.24, cu=(0.2, 35, 0.1), ca=(0.22, 0.07, -45), cola=(0.1, 80, 0.04), cuernos="oveja", oreja=0.05),
    "yak": dict(pel="pelaje_negro", pata=0.45, cr=0.3, cu=(0.12, 20, 0.2), ca=(0.25, 0.11, -50), cola=(0.4, 80, 0.05), cuernos="buey", oreja=0.05, joroba=1, gruesa=1.6),
    "buey": dict(pel="pelaje_castano", pata=0.5, cr=0.28, cu=(0.15, 20, 0.18), ca=(0.28, 0.1, -50), cola=(0.5, 85, 0.03), cuernos="buey", oreja=0.06, gruesa=1.5),
    "becerro": dict(pel="pelaje_castano", pata=0.6, cr=0.2, cu=(0.15, 25, 0.13), ca=(0.24, 0.08, -45), cola=(0.35, 85, 0.025), oreja=0.06, gruesa=1.2),
    "perro": dict(pel="pelaje_castano", pata=0.55, cr=0.17, cu=(0.2, 45, 0.08), ca=(0.25, 0.07, -5), cola=(0.32, 30, 0.04), oreja=0.07),
    "lobo": dict(pel="pelaje_tordo", pata=0.58, cr=0.17, cu=(0.2, 35, 0.09), ca=(0.28, 0.075, -10), cola=(0.38, 55, 0.05), oreja=0.07),
    "zorro": dict(pel="pelaje_tigre", pata=0.45, cr=0.15, cu=(0.18, 35, 0.07), ca=(0.25, 0.06, -5), cola=(0.45, 35, 0.07), oreja=0.07),
    "coyote": dict(pel="pelaje_tordo", pata=0.55, cr=0.16, cu=(0.2, 35, 0.075), ca=(0.26, 0.065, -10), cola=(0.38, 55, 0.05), oreja=0.08),
    "hiena": dict(pel="pelaje_leonado", pata=0.62, cr=0.2, cu=(0.25, 30, 0.1), ca=(0.25, 0.08, -15), cola=(0.25, 70, 0.03), oreja=0.06, trasera_baja=True),
    "tigre": dict(pel="pelaje_tigre", pata=0.45, cr=0.2, cu=(0.15, 20, 0.12), ca=(0.22, 0.1, 0), cola=(0.6, 50, 0.04), oreja=0.04, felino=True),
    "leopardo": dict(pel="piel_blanca", pata=0.4, cr=0.17, cu=(0.15, 20, 0.1), ca=(0.2, 0.08, 0), cola=(0.7, 40, 0.06), oreja=0.04, felino=True),
    "puma": dict(pel="pelaje_leonado", pata=0.45, cr=0.17, cu=(0.15, 20, 0.1), ca=(0.2, 0.08, 0), cola=(0.6, 50, 0.04), oreja=0.04, felino=True),
    "oso": dict(pel="piel_pardo", pata=0.42, cr=0.32, cu=(0.15, 20, 0.2), ca=(0.28, 0.13, -15), cola=(0.06, 40, 0.05), oreja=0.05, gruesa=1.8),
    "jabali": dict(pel="piel_pardo", pata=0.42, cr=0.24, cu=(0.1, 10, 0.18), ca=(0.32, 0.11, -20), cola=(0.12, 80, 0.015), oreja=0.05, colmillos=True, gruesa=1.3),
    "liebre": dict(pel="piel_pardo", pata=0.35, cr=0.2, cu=(0.1, 40, 0.1), ca=(0.2, 0.08, -10), cola=(0.05, 20, 0.05), oreja=0.22, liebre=True),
    "cocodrilo": dict(pel="escamas", pata=0.15, cr=0.12, cu=(0.12, 0, 0.1), ca=(0.42, 0.07, 0), cola=(1.0, 5, 0.08), oreja=0.0, reptil=True, gruesa=1.6),
}


def especie_de(nom):
    for k in ("caballo", "camello", "dromedario", "elefante", "reno", "ciervo", "ibice", "antilope", "gacela", "cabra", "oveja",
              "yak", "buey", "becerro", "lobo", "zorro", "coyote", "hiena", "tigre", "leopardo", "puma", "oso", "jabali", "liebre",
              "cocodrilo", "mula", "burro", "perro"):
        if k in nom:
            return k
    return "perro"


def b_cuadrupedo(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    e = ESPECIES[especie_de(nom)]
    pel = e["pel"]
    if nom == "caballo_guerra":
        pel = "pelaje_negro"
    if nom == "perro_salvaje":
        pel = "pelaje_leonado"
    n = S(d, 4, 8)
    pata, cr = e["pata"], e["cr"]
    rz = cr * 0.85 * (1.15 if e.get("gruesa") else 1.0)
    by = pata + cr * 0.9           # centro del cuerpo
    # Cuerpo: anillos elipticos a lo largo de X (grupa en -X, pecho en +X).
    anillos = []
    perfil = ((-0.5, 0.55, 0.0), (-0.42, 0.95, 0.0), (-0.1, 1.0, -0.05), (0.25, 1.0, 0.0), (0.45, 0.9, 0.05), (0.53, 0.55, 0.08))
    for x, f, dy in perfil:
        yy = by + dy * cr + (0.1 * cr if (e.get("trasera_baja") and x > 0) else 0)
        anillos.append(anillo((x, yy, 0), (1, 0, 0), cr * f, rz * f, n, 0, (0, 1, 0), (0, 0, 1)))
    m.add("cuerpo", pel, loft(anillos))
    # Patas
    rp = 0.04 * (e.get("gruesa", 1.0) ** 0.9)
    pez = "cuerno" if not (e.get("felino") or e.get("reptil")) else pel
    for nombre, x in (("delantera", 0.37), ("trasera", -0.37)):
        for lado, s in (("izq", -1), ("der", 1)):
            z = rz * 0.55 * s
            top = (x, by - cr * 0.2, z)
            if e.get("reptil"):
                codo = (x + 0.05, pata * 0.8, z + 0.12 * s)
                pie = (x + 0.1, 0.0, z + 0.18 * s)
            else:
                codo = (x + (0.03 if x < 0 else -0.02), pata * 0.5, z)
                pie = (x + 0.0, 0.03, z)
            if e.get("liebre") and x < 0:
                codo = (x + 0.12, pata * 0.4, z)
            # Una sola pieza por pata (muslo, rodilla, tobillo): cada pata es un objeto en Nomad.
            npata = max(4, n - 2)
            m.add(f"pata_{nombre}_{lado}", pel, loft([anillo(top, (0, 1, 0), rp * 1.7, rp * 1.5, npata),
                                                      anillo(codo, (0, 1, 0), rp, rp, npata),
                                                      anillo((pie[0], 0.05 if d >= 2 else 0.0, pie[2]), (0, 1, 0), rp * 0.8, rp * 0.8, npata)]))
            if d >= 2:
                m.add(f"pezuna_{nombre}_{lado}", pez, tronco((pie[0], 0.0, pie[2]), (pie[0], 0.05, pie[2]), rp * 1.05, rp * 0.85, npata))
    # Cuello y cabeza
    cl, ca_, cr2 = e["cu"]
    a = math.radians(ca_)
    c0 = (0.45, by + cr * 0.35, 0)
    if e.get("cuello_s"):  # camello: cuello en S (baja y sube)
        c_mid = va(c0, (cl * 0.5, -cl * 0.1, 0))
        c1 = va(c_mid, (cl * 0.35, cl * 0.6, 0))
        m.add("cuello", pel, tronco(c0, c_mid, cr2 * 1.3, cr2, n))
        m.add("cuello", pel, tronco(c_mid, c1, cr2, cr2 * 0.9, n))
    else:
        c1 = va(c0, (cl * math.cos(a), cl * math.sin(a), 0))
        m.add("cuello", pel, tronco(c0, c1, cr2 * 1.25, cr2, n))
    hl, hr, ha = e["ca"]
    ah = math.radians(ha)
    hd = (math.cos(ah), math.sin(ah), 0)
    h0 = va(c1, vm(hd, -hr * 0.6))
    h1 = va(h0, vm(hd, hl * 0.35))
    h2 = va(h0, vm(hd, hl))
    up = vn(cruz((0, 0, 1), hd))
    # Craneo y quijada anchos, hocico mas estrecho; el polo trasero cierra la nuca.
    rings = [[va(h0, vm(hd, -hr * 0.5))],
             anillo(h0, hd, hr * 1.05, hr * 0.95, n, 0, up, (0, 0, 1)),
             anillo(va(h1, vm(up, -hr * 0.15)), hd, hr * 1.0, hr * 0.8, n, 0, up, (0, 0, 1)),
             anillo(h2, hd, hr * 0.62, hr * 0.55, n, 0, up, (0, 0, 1))]
    m.add("cabeza", pel, loft(rings))
    if e.get("crin"):
        pts = [lerp(c0, c1, t) for t in (0.0, 0.5, 1.0)]
        pts = [va(p, vm(vn(cruz((0, 0, 1), vs(c1, c0))), cr2 * 0.9)) for p in pts]
        m.add("crin", "cabello", placa(pts, [0.03, 0.035, 0.025], 0.012, normal=(0, 0, 1)))
    ore = e.get("oreja", 0.05)
    if ore > 0:
        for s in (-1, 1):
            base = va(h0, va(vm(up, hr * 0.8), (0, 0, hr * 0.5 * s)))
            punta = va(base, va(vm(up, ore), (-ore * 0.3 if not e.get("liebre") else -ore * 0.4, 0, ore * 0.2 * s)))
            m.add("orejas", pel, tronco(base, punta, max(0.018, ore * 0.3), 0.0, 4))
    if e.get("trompa"):
        t0 = h2
        t1 = va(t0, (0.12, -0.3, 0))
        t2 = va(t1, (0.03, -0.35, 0))
        m.add("trompa", pel, tronco(t0, t1, hr * 0.45, hr * 0.3, n))
        m.add("trompa", pel, tronco(t1, t2, hr * 0.3, hr * 0.2, n))
        for s in (-1, 1):
            b = va(h1, (0.0, -hr * 0.6, hr * 0.5 * s))
            m.add("colmillos", "hueso", tronco(b, va(b, (0.3, 0.05, 0.03 * s)), 0.03, 0.0, 5))
            m.add("orejas", pel, mover(rotar(elipsoide((0, 0, 0), 0.03, 0.22, 0.17, max(4, n - 2), 3), "y", 0.5 * s), va(h0, (-0.05, -0.02, (hr + 0.1) * s))))
    if e.get("colmillos"):
        for s in (-1, 1):
            b = va(h2, (-0.05, -0.02, hr * 0.4 * s))
            m.add("colmillos", "hueso", tronco(b, va(b, (0.02, 0.08, 0.02 * s)), 0.015, 0.0, 4))
    if e.get("barba"):
        m.add("barba", pel, tronco(va(h2, (-0.03, -hr * 0.4, 0)), va(h2, (-0.06, -hr * 0.4 - 0.12, 0)), 0.03, 0.0, 4))
    cuernos = e.get("cuernos")
    if cuernos:
        for s in (-1, 1):
            b = va(h0, va(vm(up, hr * 0.85), (0.02, 0, hr * 0.35 * s)))
            if cuernos == "ibice":
                pts = [b] + [va(b, (-0.3 * math.sin(t) * 1.0, 0.32 * math.sin(t * 1.3) + 0.05, 0.04 * s)) for t in (0.6, 1.2, 1.8)]
            elif cuernos == "cabra":
                pts = [b, va(b, (-0.06, 0.12, 0.02 * s)), va(b, (-0.16, 0.18, 0.04 * s))]
            elif cuernos == "oveja":
                pts = [b, va(b, (-0.06, 0.06, 0.06 * s)), va(b, (-0.02, -0.02, 0.1 * s)), va(b, (0.04, -0.08, 0.09 * s))]
            elif cuernos == "buey":
                pts = [b, va(b, (0.0, 0.03, 0.16 * s)), va(b, (0.04, 0.16, 0.24 * s))]
            else:  # recto
                pts = [b, va(b, (-0.08, 0.2, 0.02 * s)), va(b, (-0.14, 0.32, 0.03 * s))]
            r0 = 0.03 if cuernos != "recto" else 0.018
            for i in range(len(pts) - 1):
                f0, f1 = 1 - i / (len(pts) - 1), 1 - (i + 1) / (len(pts) - 1)
                m.add("cuernos", "cuerno", tronco(pts[i], pts[i + 1], r0 * max(f0, 0.25), r0 * f1 * 0.9, 5))
    astas = e.get("astas")
    if astas:
        alto = 0.45 if astas == "ciervo" else 0.4
        for s in (-1, 1):
            b = va(h0, va(vm(up, hr * 0.8), (0, 0, hr * 0.35 * s)))
            p1 = va(b, (-0.08, alto * 0.5, 0.12 * s))
            p2 = va(b, (-0.12, alto, 0.2 * s))
            m.add("astas", "cuerno", tronco(b, p1, 0.018, 0.014, 4))
            m.add("astas", "cuerno", tronco(p1, p2, 0.014, 0.0, 4))
            for t, ext in ((0.3, (0.12, 0.12, 0.02 * s)), (0.7, (0.1, 0.12, 0.0))):
                q = lerp(b, p2, t)
                m.add("astas", "cuerno", tronco(q, va(q, ext), 0.01, 0.0, 4))
            if astas == "reno":
                q = lerp(b, p1, 0.3)
                m.add("astas", "cuerno", placa([q, va(q, (0.14, -0.04, 0))], [0.03, 0.05], 0.006, normal=(0, 0, 1)))
    joroba = e.get("joroba", 0)
    if joroba:
        xs = (0.0,) if joroba == 1 else (-0.18, 0.18)
        if nom.startswith("yak"):
            xs = (0.3,)
        for i, x in enumerate(xs):
            m.add(f"joroba{'_' + str(i + 1) if len(xs) > 1 else ''}", pel, cupula((x, by + cr * 0.6, 0), 0.16, cr * 0.9, rz * 0.6, n, 3))
    # Cola
    tl, td, tr = e["cola"]
    t0 = (-0.5, by + cr * 0.25, 0)
    at = math.radians(td)
    t1 = va(t0, (-tl * math.cos(at), -tl * math.sin(at), 0))
    if e.get("reptil"):
        m.add("cola", pel, tronco((-0.45, by, 0), va(t1, (0, 0.03, 0)), cr * 0.8, 0.0, n, 0, cr * 0.8 * 0.6, 0, u=(0, 1, 0), v=(0, 0, 1)))
    elif e.get("felino"):
        mid = va(t0, (-tl * 0.5, -tl * 0.45, 0))
        m.add("cola", pel, tronco(t0, mid, tr, tr * 0.9, n))
        m.add("cola", pel, tronco(mid, va(mid, (-tl * 0.5, tl * 0.05, 0)), tr * 0.9, tr * 0.6, n))
    else:
        m.add("cola", pel if not e.get("crin") else "cabello", tronco(t0, t1, tr, tr * 0.5 if not e.get("crin") else tr * 1.3, max(4, n - 2)))


def b_ave(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    pel = "pelaje_negro" if nom == "cuervo" else "plumas"
    n = S(d, 5, 8)
    by = 0.24
    m.add("cuerpo", pel, rotar(elipsoide((0, by, 0), 0.22, 0.12, 0.12, n, S(d, 3, 5)), "z", 0.25, (0, by, 0)))
    cab = "hueso" if nom in ("aguila",) else pel
    if nom == "buitre":
        m.add("cuello", "piel_media", tronco((0.15, by + 0.06, 0), (0.24, by + 0.2, 0), 0.04, 0.035, n))
        hc = (0.27, by + 0.22, 0)
    else:
        hc = (0.22, by + 0.13, 0)
    m.add("cabeza", cab, elipsoide(hc, 0.07, 0.065, 0.06, n, S(d, 3, 4)))
    m.add("pico", "oro" if nom != "cuervo" else "pelaje_negro", tronco(va(hc, (0.05, 0, 0)), va(hc, (0.13, -0.03, 0)), 0.025, 0.0, 4))
    # Alas desplegadas a lo largo de Z (ancho = envergadura), con un poco de diedro.
    for lado, s in (("izq", -1), ("der", 1)):
        pts = [(0.02, by + 0.05, 0.06 * s), (0.0, by + 0.12, 0.45 * s), (-0.08, by + 0.18, 0.9 * s)]
        m.add(f"ala_{lado}", pel, placa(pts, [0.13, 0.12, 0.06], 0.012, normal=(0, 1, 0)))
    m.add("cola", pel, placa([(-0.18, by - 0.02, 0), (-0.38, by - 0.08, 0)], [0.05, 0.1], 0.01, normal=(0, 1, 0)))
    for s in (-1, 1):
        m.add("patas", "oro" if nom != "cuervo" else "pelaje_negro", tronco((0.0, by - 0.08, 0.04 * s), (0.02, 0.0, 0.05 * s), 0.015, 0.012, 4))


def b_pez(m, it, d, rng, W, H, L):
    n = S(d, 4, 6)
    m.add("cuerpo", "escamas", loft([[(-0.15, 0.05, 0)], anillo((-0.05, 0.05, 0), (1, 0, 0), 0.04, 0.025, n, 0, (0, 1, 0), (0, 0, 1)),
                                     anillo((0.08, 0.05, 0), (1, 0, 0), 0.035, 0.022, n, 0, (0, 1, 0), (0, 0, 1)), [(0.17, 0.045, 0)]]))
    m.add("aleta", "escamas", placa([(-0.14, 0.05, 0), (-0.2, 0.05, 0)], [0.01, 0.05], 0.003, normal=(0, 0, 1)))


def b_serpiente(m, it, d, rng, W, H, L):
    k = S(d, 5, 9)
    n = S(d, 4, 6)
    anillos = []
    for i in range(k + 1):
        t = i / k
        x = -0.5 + t
        z = 0.03 * math.sin(t * 2 * math.pi * 1.5)
        r = 0.02 * (0.4 + 0.6 * math.sin(math.pi * min(t * 1.2, 1.0))) if i < k else 0
        if i == k:
            anillos.append([(x + 0.02, 0.02, z)])
        else:
            anillos.append(anillo((x, 0.02, z), (1, 0, 0), r, r * 1.2, n, 0, (0, 1, 0), (0, 0, 1)))
    anillos[0] = [(-0.52, 0.015, 0.0)]
    m.add("cuerpo", "escamas", loft(anillos))


def b_artropodo(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 4, 6)
    col = "pelaje_negro" if nom == "arana" else "pelaje_leonado"
    m.add("cuerpo", col, elipsoide((0, 0.025, 0), 0.04, 0.02, 0.03, n, 3))
    m.add("cabeza", col, elipsoide((0.05, 0.022, 0), 0.02, 0.015, 0.02, n, 3) if d >= 2 else octaedro((0.05, 0.022, 0), 0.018))
    patas = 4 if nom == "arana" else 3
    for i in range(patas):
        x = -0.02 + 0.02 * i
        for s in (-1, 1):
            m.add("patas", col, tronco((x, 0.025, 0.02 * s), (x + 0.01 * (i - 1), 0.0, 0.07 * s), 0.004, 0.002, 3, tapas=False))
    if nom == "escorpion":
        pts = [(-0.04, 0.025, 0), (-0.08, 0.04, 0), (-0.09, 0.07, 0), (-0.06, 0.09, 0)]
        if d < 2:
            pts = [pts[0], pts[2], pts[3]]
        m.add("cola", col, cinta(pts, [0.01, 0.009, 0.007, 0.0][-len(pts):], 0.006, normal=(0, 0, 1)))
        for s in (-1, 1):
            m.add("pinzas", col, tronco((0.06, 0.02, 0.015 * s), (0.11, 0.02, 0.04 * s), 0.008, 0.004, 3 if d < 2 else 4))


def b_insecto(m, it, d, rng, W, H, L):
    if d >= 2:
        m.add("cuerpo", "pelaje_negro" if "mosc" in nombre_de(it) or "mosq" in nombre_de(it) else "pelaje_tigre",
              elipsoide((0, 0.5, 0), 0.5, 0.3, 0.3, 4, 3))
    else:
        m.add("cuerpo", "pelaje_negro" if "mosc" in nombre_de(it) or "mosq" in nombre_de(it) else "pelaje_tigre",
              tetraedro((0, 0.5, 0), 0.5))
    for s in (-1, 1):
        m.add("alas", "hielo", [orientar(((0, 0.75, 0), (-0.3, 0.8, 0.9 * s), (0.2, 0.8, 0.8 * s)), (0, 1, 0))])


def b_tortuga(m, it, d, rng, W, H, L):
    n = S(d, 6, 10)
    m.add("caparazon", "escamas", cupula((0, 0.08, 0), 0.45, 0.32, 0.4, n, S(d, 2, 3)))
    m.add("vientre", "hueso", tronco((0, 0.02, 0), (0, 0.08, 0), 0.42, 0.45, n, 0, 0.37, 0.4, u=(1, 0, 0), v=(0, 0, 1)))
    m.add("cabeza", "piel_pardo", elipsoide((0.52, 0.1, 0), 0.1, 0.06, 0.06, S(d, 4, 6), 3))
    for x, s, lado in ((0.25, 1, "der"), (0.25, -1, "izq"), (-0.28, 1, "der"), (-0.28, -1, "izq")):
        largo = 0.35 if x > 0 else 0.2
        m.add(f"aleta_{lado}", "piel_pardo", placa([(x, 0.05, 0.35 * s), (x - 0.08, 0.04, (0.35 + largo) * s)], [0.08, 0.04], 0.015,
                                                   normal=(0, 1, 0)))


def b_colmena(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 6, 10)
    if nom == "colmena":  # colmena de paja (skep)
        anillos = [anillo((0, y, 0), (0, 1, 0), r, r, n) for y, r in ((0, 0.2), (0.15, 0.2), (0.32, 0.16), (0.44, 0.08))] + [[(0, 0.5, 0)]]
        m.add("colmena", "hierba_seca", loft(anillos))
        m.add("entrada", "carbon", caja((0.19, 0.04, 0), (0.04, 0.05, 0.08)))
    else:  # avispero colgante
        m.add("nido", "fieltro_gris", elipsoide((0, 0.18, 0), 0.15, 0.18, 0.15, n, S(d, 3, 5)))
        m.add("rama", "corteza", tronco((0, 0.34, -0.15), (0, 0.4, 0.15), 0.02, 0.02, 4))


# --- Armas ------------------------------------------------------------------
def _empunadura(m, y0, y1, r, n, guarda_w, guarda_cell="bronce", pomo=True, pomo_cell=None):
    m.add("empunadura", "cuero", tronco((0, y0, 0), (0, y1, 0), r, r * 1.1, n))
    m.add("guarda", guarda_cell, caja((0, y1 + 0.012, 0), (r * 3, 0.024, guarda_w)))
    if pomo:
        m.add("pomo", pomo_cell or guarda_cell, elipsoide((0, y0 - 0.01, 0), r * 1.5, r * 1.4, r * 1.5, max(4, n), 3))
    m.agarre = (0, (y0 + y1) / 2, 0)


def b_espada(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 4, 6)
    metal = metal_de(it)
    hoja = "acero" if metal in ("oro",) else metal
    curva = 0.09 if "sable" in nom else 0.0
    grip = 0.22 if ("larga" in nom or "padre" in nom) else (0.1 if nom in ("cuchillo", "daga") else 0.13)
    total = H
    lh = total - grip - 0.05
    ancho_h = 0.018 if nom == "cuchillo" else 0.02 if nom == "daga" else 0.024 if "sable" in nom else 0.03
    k = S(d, 3, 6) if curva else 2
    pts, anchos = [], []
    for i in range(k + 1):
        t = i / k
        y = grip + 0.024 + lh * t
        z = curva * t * t * (lh / 0.75)
        pts.append((0, y, z))
        anchos.append(ancho_h * (1 - 0.35 * t) if i < k else 0.0)
    m.add("hoja", hoja, cinta(pts, anchos, 0.004, normal=(1, 0, 0), asim=0.35 if curva else 1.0))
    guarda = {"oro": "oro", "bronce": "bronce"}.get(metal, "hierro" if curva == 0 else "bronce")
    gw = W * (0.7 if curva else 0.95)
    _empunadura(m, 0.04, grip, 0.014, n, gw, guarda, pomo_cell="turquesa" if "padre" in nom else None)
    if "padre" in nom or "damasquinado" in nom:
        m.add("incrustaciones", "turquesa", caja((0, grip + 0.012, 0), (0.05, 0.02, 0.03)))


def b_hacha(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 4, 6)
    metal = "oro" if "ceremonial" in nom else metal_de(it)
    m.add("mango", "madera_clara", tronco((0, 0, 0), (0, H, 0), 0.017, 0.015, n))
    yh = H * 0.86
    alto = 0.12 if "ceremonial" not in nom else 0.3
    anillos = []
    for z, h, t in ((-0.02, 0.06, 0.03), (0.04, 0.05, 0.022), (0.1, alto * 0.7, 0.012), (W * 0.85, alto, 0.004)):
        anillos.append([(x, yh + y, z) for x, y in ((t, h / 2), (-t, h / 2), (-t, -h / 2), (t, -h / 2))])
    m.add("cabeza", metal, loft(anillos))
    if "ceremonial" in nom:
        m.add("adorno", "turquesa", caja((0, yh, 0.06), (0.03, 0.03, 0.03)))
        m.add("adorno", "oro", tronco((0, H, 0), (0, H + 0.08, 0), 0.02, 0.0, n))
    m.agarre = (0, H * 0.18, 0)


def b_maza(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 4, 6)
    m.add("mango", "madera" if "tigre" not in nom else "madera_oscura", tronco((0, 0, 0), (0, H * 0.8, 0), 0.016, 0.018, n))
    c = (0, H * 0.85, 0)
    if "tigre" in nom:
        m.add("cabeza", "oro", elipsoide(c, 0.07, 0.065, 0.06, S(d, 5, 8), S(d, 3, 5)))
        m.add("cabeza", "oro", tronco(va(c, (0.04, -0.01, 0)), va(c, (0.11, -0.02, 0)), 0.04, 0.025, S(d, 5, 8)))
        for s in (-1, 1):
            m.add("orejas", "oro", tronco(va(c, (-0.01, 0.05, 0.035 * s)), va(c, (-0.02, 0.09, 0.04 * s)), 0.018, 0.0, 4))
            m.add("ojos", "turquesa", caja(va(c, (0.06, 0.02, 0.025 * s)), (0.015, 0.012, 0.012)))
    else:
        m.add("cabeza", "hierro", icosaedro(c, 0.045) if d >= 1 else octaedro(c, 0.05))
        for k in range(S(d, 4, 6)):
            a = 2 * math.pi * k / S(d, 4, 6)
            m.add("aletas", "hierro", caja(va(c, (math.cos(a) * 0.04, 0, math.sin(a) * 0.04)), (0.02, 0.1, 0.02)))
    m.agarre = (0, H * 0.15, 0)


def b_asta(m, it, d, rng, W, H, L):
    """Lanzas, picas, gujas y alabardas: asta vertical con la punta arriba."""
    nom = nombre_de(it)
    n = S(d, 4, 6)
    metal = metal_de(it)
    if "hoja_ancha" in nom:
        metal = "acero"
    r = 0.016
    m.add("asta", "madera_clara" if "doble" not in nom else "madera_oscura", tronco((0, 0, 0), (0, H, 0), r, r * 0.9, n))
    if "guja" in nom:
        ancho = 0.06 if "hoja_ancha" not in nom else 0.1
        y0 = H * 0.78
        k = S(d, 3, 5)
        pts = [(0, y0 + (H - y0) * i / k + 0.02, 0.025 + 0.035 * (i / k) ** 2) for i in range(k + 1)]
        m.add("hoja", metal, cinta(pts, [ancho * (1 - 0.6 * i / k) if i < k else 0 for i in range(k + 1)], 0.005,
                                  normal=(1, 0, 0), asim=0.4))
        m.add("virola", "bronce", tronco((0, y0 - 0.02, 0), (0, y0 + 0.05, 0), r * 1.6, r * 1.6, n))
        if "hoja_ancha" in nom:
            m.add("borla", "tela_roja", tronco((0, y0 - 0.02, 0), (0, y0 - 0.2, 0), 0.02, 0.05, n))
    elif "alabarda" in nom:
        y0 = H * 0.8
        m.add("punta", metal, cinta([(0, y0, 0), (0, H * 0.92, 0), (0, H, 0)], [0.03, 0.02, 0.0], 0.006))
        anillos = []
        for z, h, t in ((0.0, 0.08, 0.012), (0.08, 0.1, 0.008), (W * 0.65, 0.22, 0.003)):
            anillos.append([(x, y0 + y, z) for x, y in ((t, h / 2), (-t, h / 2), (-t, -h / 2), (t, -h / 2))])
        m.add("hacha", metal, loft(anillos))
        m.add("pico", metal, tronco((0, y0, 0), (0, y0 + 0.02, -W * 0.33), 0.02, 0.0, 4))
    else:
        punta_l = 0.3 if "pica" not in nom else 0.22
        y0 = H - punta_l
        m.add("punta", metal, cinta([(0, y0, 0), (0, y0 + punta_l * 0.35, 0), (0, H, 0)], [0.025, 0.032, 0.0], 0.008, normal=(1, 0, 0)))
        m.add("virola", "bronce" if metal != "bronce" else "cuero", tronco((0, y0 - 0.06, 0), (0, y0 + 0.01, 0), r * 1.5, r * 1.3, n))
        if "doble" in nom:
            m.add("punta_inferior", metal, cinta([(0, punta_l, 0), (0, punta_l * 0.65, 0), (0, 0, 0)], [0.025, 0.032, 0.0], 0.008))
            m.add("borla", "cabello", tronco((0, y0 - 0.05, 0), (0, y0 - 0.25, 0), 0.02, 0.045, n))
        else:
            m.add("regaton", "hierro", tronco((0, 0, 0), (0, 0.06, 0), r * 0.9, r * 1.1, n))
    m.agarre = (0, H * 0.4, 0)


def b_arco(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    k = S(d, 3, 6)
    largo = "largo" in nom
    pts, anchos = [], []
    for i in range(2 * k + 1):
        t = -1 + i / k               # -1..1
        y = H / 2 * t
        if largo:
            z = -0.08 * (1 - t * t)
        else:  # recurvo: el arco se tensa hacia atras y las puntas se vuelven hacia delante
            z = -0.09 * (1 - t * t) + 0.05 * max(0.0, abs(t) - 0.65) / 0.35
        pts.append((0, y, z))
        anchos.append(0.018 * (1 - 0.5 * abs(t)) + 0.006)
    cel = "cuerno" if "cuerno" in nom else ("madera" if largo else "madera_oscura")
    m.add("palas", cel, placa(pts, anchos, 0.012, normal=(1, 0, 0)))
    m.add("empunadura", "cuero", tronco((0, -0.06, pts[k][2]), (0, 0.06, pts[k][2]), 0.02, 0.02, S(d, 4, 6)))
    m.add("cuerda", "cuerda", viga(va(pts[0], (0, 0.02, 0)), va(pts[-1], (0, -0.02, 0)), 0.004))
    if "cuerno" in nom:
        m.add("adornos", "oro", caja((0, H * 0.25, pts[k // 2 * 3][2]), (0.02, 0.04, 0.03)))
    m.agarre = (0, 0, pts[k][2])


def b_ballesta(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 4, 6)
    m.add("cureña", "madera", viga((-0.45, 0.1, 0), (0.45, 0.12, 0), 0.06, 0.05))
    m.add("culata", "madera", viga((-0.45, 0.1, 0), (-0.3, 0.02, 0), 0.07, 0.05))
    k = S(d, 2, 4)
    pts = [(0.38 - 0.08 * (1 - (2 * i / (2 * k) - 1) ** 2) * 0, 0.13, -W / 2 + W * i / (2 * k)) for i in range(2 * k + 1)]
    pts = [(p[0] - 0.08 * (1 - (2 * i / (2 * k) - 1) ** 2), p[1], p[2]) for i, p in enumerate(pts)]
    m.add("arco", metal_de(it, "madera_oscura") if "repeticion" not in nom else "madera_oscura",
          placa(pts, [0.022] * len(pts), 0.012, normal=(0, 1, 0)))
    m.add("cuerda", "cuerda", viga(pts[0], (0.0, 0.14, 0), 0.005))
    m.add("cuerda", "cuerda", viga((0.0, 0.14, 0), pts[-1], 0.005))
    m.add("estribo", "hierro", toro((0.5, 0.13, 0), 0.04, 0.008, 5, 3, eje="z"))
    m.add("gatillo", "hierro", viga((-0.18, 0.08, 0), (-0.2, 0.0, 0), 0.012))
    if "repeticion" in nom:
        m.add("cargador", "madera_clara", caja((0.05, 0.24, 0), (0.4, 0.14, 0.05)))
        m.add("palanca", "madera", viga((0.0, 0.18, 0.04), (-0.25, 0.32, 0.04), 0.02))
    m.agarre = (-0.2, 0.06, 0)


def b_mosquete(m, it, d, rng, W, H, L):
    n = S(d, 4, 6)
    m.add("canon", "hierro", tronco((-0.25, 0.2, 0), (0.75, 0.2, 0), 0.016, 0.013, n))
    m.add("caja", "madera", viga((-0.3, 0.19, 0), (0.55, 0.19, 0), 0.035, 0.04))
    m.add("culata", "madera", loft([[(x, y + 0.19, z) for x, y, z in ((-0.3, 0.02, 0.02), (-0.3, 0.02, -0.02), (-0.3, -0.03, -0.02), (-0.3, -0.03, 0.02))],
                                     [(x, y, z) for x, y, z in ((-0.75, 0.15, 0.03), (-0.75, 0.15, -0.03), (-0.75, 0.0, -0.03), (-0.75, 0.0, 0.03))]]))
    m.add("llave", "hierro", caja((-0.2, 0.2, 0.025), (0.08, 0.04, 0.012)))
    m.add("gatillo", "hierro", viga((-0.25, 0.17, 0), (-0.26, 0.13, 0), 0.008))
    m.agarre = (-0.3, 0.16, 0)


def b_canon(m, it, d, rng, W, H, L):
    n = S(d, 6, 10)
    yb = 0.75
    m.add("canon", "bronce", tronco((-1.0, yb, 0), (1.4, yb + 0.1, 0), 0.24, 0.17, n))
    m.add("canon", "bronce", tronco((1.3, yb + 0.1, 0), (1.5, yb + 0.1, 0), 0.21, 0.21, n))
    m.add("canon", "bronce", elipsoide((-1.0, yb, 0), 0.08, 0.2, 0.2, n, 3))
    for s in (-1, 1):
        m.add(f"rueda_{'izq' if s < 0 else 'der'}", "madera", tronco((0.1, 0.45, 0.55 * s), (0.1, 0.45, 0.72 * s), 0.45, 0.45, n))
        m.add("cureña", "madera_oscura", caja((-0.3, 0.55, 0.32 * s), (1.6, 0.35, 0.1)))
    m.add("eje", "hierro", tronco((0.1, 0.45, -0.75), (0.1, 0.45, 0.75), 0.04, 0.04, 4))
    m.add("contera", "madera_oscura", viga((-0.8, 0.45, 0), (-1.5, 0.05, 0), 0.18, 0.5))


def b_honda(m, it, d, rng, W, H, L):
    for s in (-1, 1):
        m.add("cuerdas", "cuerda", tronco((0, H, 0), (0, 0.06, 0.012 * s), 0.003, 0.003, 3))
    m.add("bolsa", "cuero", cupula((0, 0.02, 0), 0.015, 0.05, 0.022, 4, 2))
    if d >= 2:
        m.add("lazo", "cuerda", toro((0, H, 0), 0.015, 0.004, 4, 3, eje="z"))
    m.agarre = (0, H, 0)


def b_lazo(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 6, 10)
    if "boleadoras" in nom:
        for k in range(3):
            a = 2 * math.pi * k / 3
            p = (math.cos(a) * 0.17, 0.05, math.sin(a) * 0.17)
            m.add("bolas", "piedra", icosaedro(p, 0.045) if d >= 2 else octaedro(p, 0.05))
            m.add("cuerdas", "cuerda", viga((0, 0.04, 0), p, 0.008))
        m.agarre = (0, 0.04, 0)
        return
    for k, r in enumerate((0.22, 0.19, 0.16)[:S(d, 1, 3)]):
        m.add("rollo", "cuerda", toro((0, 0.03 + 0.012 * k, 0), r, 0.012, max(8, n), 3))
    if "plomadas" in nom:
        for k in range(4):
            a = 2 * math.pi * k / 4
            m.add("plomadas", "plata", octaedro((math.cos(a) * 0.25, 0.04, math.sin(a) * 0.25), 0.03))
    m.agarre = (0.22, 0.04, 0)


# --- Proyectiles --------------------------------------------------------------
def b_proyectil(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    sub = it["id"].split(".")[1]
    if sub in ("bala", "piedra"):
        cel = "piedra" if sub == "piedra" else ("hierro" if "canon" in nom else "plata")
        m.add("proyectil", cel, icosaedro((0, 0.5, 0), 0.5) if int(it["tris_max"]) >= 20 else octaedro((0, 0.5, 0), 0.5))
        return
    m.add("astil", "madera_clara", tronco((0, 0.04, 0), (0, H * 0.88, 0), 0.004, 0.004, 3, tapas=False))
    m.add("punta", "hierro", tronco((0, H * 0.88, 0), (0, H, 0), 0.007, 0.0, 3))
    if "silbadora" in nom:
        m.add("silbato", "hueso", octaedro((0, H * 0.84, 0), 0.014))
    m.add("plumas", "plumas", tronco((0, 0.0, 0), (0, H * 0.16, 0), 0.01, 0.006, 3))


# --- Escudos ------------------------------------------------------------------
def b_escudo(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 8, 14)
    t = 0.04
    if "paves" in nom:
        m.add("tablero", "madera", caja((0, H / 2, 0), (0.08, H, W)))
        m.add("nervio", "madera_oscura", extruir([(-0.06, 0.0), (0.06, 0.0), (0.0, 0.06)], (0.04, 0.0, 0), (0, 0, 1), (1, 0, 0), (0, 1, 0), H))
        m.add("blason", "tela_azul", caja((0.045, H * 0.6, 0), (0.02, H * 0.3, W * 0.6)))
        m.add("asa", "cuero", caja((-0.06, H * 0.55, 0), (0.04, 0.12, 0.05)))
        m.agarre = (-0.06, H * 0.55, 0)
        return
    cy = H / 2
    if "lamina" in nom:
        m.add("tablero", "hierro", tronco((-t, cy, 0), (t, cy, 0), H / 2, H / 2 * 0.95, n, 0, W / 2, W / 2 * 0.95, u=(0, 1, 0), v=(0, 0, 1)))
        for k in range(3):
            y = cy - H * 0.3 + k * H * 0.3
            m.add("laminas", "acero", caja((t + 0.01, y, 0), (0.02, H * 0.06, W * 0.85 * math.sqrt(max(0.1, 1 - ((y - cy) / (H / 2)) ** 2)))))
    elif "culto" in nom:
        poli = [(0, H / 2), (W / 2, H * 0.25), (W * 0.42, -H * 0.15), (0, -H / 2), (-W * 0.42, -H * 0.15), (-W / 2, H * 0.25)]
        m.add("tablero", "tela_negra", extruir(poli, (-t, cy, 0), (0, 0, 1), (0, 1, 0), (1, 0, 0), 2 * t))
        m.add("emblema", "cornalina", extruir([(0, 0.14), (0.1, -0.08), (-0.1, -0.08)], (t, cy, 0), (0, 0, 1), (0, 1, 0), (1, 0, 0), 0.02))
        m.add("borde", "bronce", toro((0, cy, 0), H * 0.38, 0.015, n, 3, eje="x", rz=W * 0.3))
    else:
        cel = {"mimbre": "mimbre", "cuero": "cuero"}.get(nom, "madera")
        m.add("tablero", cel, loft([anillo((-t, cy, 0), (1, 0, 0), H / 2, W / 2, n, 0, (0, 1, 0), (0, 0, 1)),
                                    anillo((t * 0.6, cy, 0), (1, 0, 0), H / 2 * 0.97, W / 2 * 0.97, n, 0, (0, 1, 0), (0, 0, 1)),
                                    anillo((t, cy, 0), (1, 0, 0), H / 2 * 0.6, W / 2 * 0.6, n, 0, (0, 1, 0), (0, 0, 1))]))
        if nom == "umbo":
            m.add("umbo", "hierro", mover(rotar(cupula((0, 0, 0), 0.12, 0.08, 0.12, S(d, 5, 8), 2), "z", -math.pi / 2), (t, cy, 0)))
            if d >= 2:
                m.add("borde", "hierro", toro((0, cy, 0), H / 2 * 0.97, 0.02, n, 3, eje="x", rz=W / 2 * 0.97))
        elif nom == "cuero":
            m.add("adorno", "oro", mover(rotar(cupula((0, 0, 0), 0.06, 0.03, 0.06, 6, 2), "z", -math.pi / 2), (t, cy, 0)))
        elif d >= 2:
            m.add("borde", "madera_oscura", toro((0, cy, 0), H / 2 * 0.97, 0.018, n, 3, eje="x", rz=W / 2 * 0.97))
    m.add("asa", "cuero", caja((-t - 0.02, cy, 0), (0.03, 0.03, 0.12)))
    m.agarre = (-t - 0.02, cy, 0)


# --- Totems -------------------------------------------------------------------
def b_totem(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 6, 8)
    r = min(W, L) * 0.18
    if nom == "idolo_culto":
        m.add("base", "piedra_oscura", caja_suelo(0, 0, L, H * 0.12, W))
        m.add("cuerpo", "piedra_oscura", tronco((0, H * 0.12, 0), (0, H * 0.7, 0), 0.38, 0.3, n))
        m.add("cabeza", "piedra_oscura", caja((0, H * 0.82, 0), (0.45, 0.5, 0.45)))
        for s in (-1, 1):
            m.add("ojos", "cornalina", caja((0.23, H * 0.86, 0.1 * s), (0.03, 0.08, 0.1)))
            m.add("brazos", "piedra_oscura", caja((0.2, H * 0.45, 0.32 * s), (0.25, 0.6, 0.14)))
        m.add("boca", "tela_negra", caja((0.23, H * 0.75, 0), (0.03, 0.06, 0.25)))
        return
    if nom == "craneos_culto":
        m.add("monton", "piedra_oscura", cupula((0, 0, 0), 0.45, 0.35, 0.45, n, 2))
        m.add("poste", "madera_oscura", tronco((0, 0, 0), (0, H, 0), 0.05, 0.04, n))
        for k, (x, y, z) in enumerate(((0.25, 0.35, 0.1), (-0.1, 0.4, 0.25), (-0.15, 0.4, -0.22), (0.05, H - 0.12, 0), (0.15, 0.55, -0.15))):
            m.add("craneos", "hueso", elipsoide((x, y, z), 0.1, 0.09, 0.08, S(d, 5, 6), 3))
        return
    if nom == "estandarte_tamga":
        m.add("asta", "madera", tronco((0, 0, -W / 2 + 0.05), (0, H, -W / 2 + 0.05), 0.03, 0.025, n))
        m.add("bandera", "tela_roja", caja((0, H * 0.8, 0.02), (0.02, H * 0.3, W * 0.9)))
        m.add("tamga", "oro", caja((0.015, H * 0.8, 0.05), (0.02, H * 0.12, W * 0.25)))
        m.add("remate", "oro", tronco((0, H - 0.02, -W / 2 + 0.05), (0, H + 0.12, -W / 2 + 0.05), 0.04, 0.0, 4))
        return
    m.add("poste", "madera", tronco((0, 0, 0), (0, H * 0.82, 0), r, r * 0.85, n))
    top = H * 0.82
    if nom == "sulde":
        m.add("punta", "hierro", cinta([(0, top, 0), (0, top + 0.12, 0), (0, H, 0)], [0.05, 0.06, 0], 0.012))
        m.add("disco", "hierro", tronco((0, top - 0.02, 0), (0, top + 0.01, 0), 0.12, 0.12, n))
        m.add("crin", "cabello", tronco((0, top - 0.02, 0), (0, top - 0.55, 0), 0.08, 0.2, n))
    elif nom == "poste_clan":
        for k, cel in enumerate(("madera_pintada", "fieltro_azul", "madera_pintada")):
            y = H * 0.25 + k * H * 0.18
            m.add(f"tallas", cel, caja((0, y, 0), (r * 2.6, H * 0.1, r * 2.6)))
        m.add("travesano", "madera", caja((0, top - 0.1, 0), (0.08, 0.08, W)))
        m.add("craneo", "hueso", elipsoide((0.05, top + 0.12, 0), 0.15, 0.1, 0.08, n, 3))
        m.add("cintas", "fieltro_azul", caja((0.02, top - 0.4, W * 0.4), (0.01, 0.5, 0.06)))
    elif nom == "guardian":
        m.add("cara", "madera_oscura", caja((0, top - 0.3, 0), (r * 2.4, 0.7, r * 2.4)))
        for s in (-1, 1):
            m.add("ojos", "turquesa", caja((r * 1.2, top - 0.15, 0.08 * s), (0.03, 0.06, 0.08)))
            m.add("alas", "madera", placa([(0, top - 0.5, 0.1 * s), (0, top - 0.2, W / 2 * s)], [0.2, 0.08], 0.02, normal=(1, 0, 0)))
        m.add("cara", "madera_oscura", caja((0, top + 0.15, 0), (r * 2.2, 0.25, r * 2.2)))
    elif nom == "aguila":
        m.add("cuerpo", "madera_pintada", elipsoide((0, top + 0.18, 0), 0.18, 0.22, 0.14, n, 4))
        m.add("cabeza", "madera_pintada", elipsoide((0.12, top + 0.42, 0), 0.1, 0.09, 0.08, n, 3))
        m.add("pico", "oro", tronco((0.2, top + 0.42, 0), (0.3, top + 0.38, 0), 0.03, 0, 4))
        for s in (-1, 1):
            m.add("alas", "madera", placa([(0, top + 0.25, 0.1 * s), (-0.05, top + 0.45, W / 2 * s)], [0.15, 0.08], 0.02, normal=(1, 0, 0)))
    else:  # cabezas de animal: lobo, ciervo, tigre
        cel = {"lobo": "pelaje_tordo", "tigre": "pelaje_tigre", "ciervo": "madera_pintada"}.get(nom, "madera")
        hc = (0.0, top + 0.18, 0)
        m.add("cabeza", cel, elipsoide(hc, 0.2, 0.18, 0.17, n, 4))
        hoc = 0.22 if nom == "lobo" else 0.14
        m.add("hocico", cel, tronco(va(hc, (0.12, -0.03, 0)), va(hc, (0.12 + hoc, -0.06, 0)), 0.1, 0.05, n))
        for s in (-1, 1):
            m.add("ojos", "turquesa", caja(va(hc, (0.17, 0.05, 0.08 * s)), (0.03, 0.04, 0.04)))
            if nom == "ciervo":
                b = va(hc, (-0.02, 0.15, 0.08 * s))
                p1 = va(b, (-0.05, 0.25, W * 0.25 * s))
                p2 = va(p1, (-0.05, 0.2, W * 0.2 * s))
                m.add("astas", "cuerno", tronco(b, p1, 0.03, 0.02, 4))
                m.add("astas", "cuerno", tronco(p1, p2, 0.02, 0.0, 4))
                m.add("astas", "cuerno", tronco(p1, va(p1, (0.12, 0.15, 0)), 0.015, 0.0, 4))
            else:
                b = va(hc, (-0.04, 0.14, 0.09 * s))
                m.add("orejas", cel, tronco(b, va(b, (-0.02, 0.14 if nom == "lobo" else 0.07, 0.02 * s)), 0.05, 0.0, 4))


# --- Armadura y vestimenta ------------------------------------------------------
def material_armadura(it):
    nom = nombre_de(it)
    for k, c, acento in (("laminar_cuero", "cuero", "cuero_oscuro"), ("fieltro", "fieltro_gris", "fieltro_rojo"),
                         ("escamas_hierro", "hierro", "cuero"), ("malla", "acero", "cuero"), ("oro_padre", "oro", "turquesa"),
                         ("mascara_culto", "hueso", "cornalina"), ("culto", "tela_negra", "hierro_oxidado"),
                         ("bronce", "bronce", "cuero"), ("barda_cuero", "cuero", "fieltro_rojo"), ("barda_hierro", "hierro", "cuero")):
        if k in nom:
            return c, acento
    return "cuero", "cuero_oscuro"


def tubo_hueco(y0, y1, rx0, rz0, rx1, rz1, grosor, n, x=0.0, abierto_frente=False):
    """Tubo de pared gruesa (vertical) para piezas que envuelven el cuerpo."""
    def rings(f):
        a = anillo((x, y0, 0), (0, 1, 0), rx0 * f, rz0 * f, n, 0, (1, 0, 0), (0, 0, 1))
        b = anillo((x, y1, 0), (0, 1, 0), rx1 * f, rz1 * f, n, 0, (1, 0, 0), (0, 0, 1))
        return a, b
    oa, ob = rings(1.0)
    k = 1.0 - grosor / max(min(rx0, rz0, rx1, rz1), 1e-6)
    ia, ib = rings(max(0.3, k))
    tris = loft([oa, ob], tapas=False)
    inner = loft([ia, ib], tapas=False)
    tris += [(t[0], t[2], t[1]) for t in inner]
    for r_o, r_i, up in ((oa, ia, -1), (ob, ib, 1)):
        for j in range(n):
            j2 = (j + 1) % n
            for t in ((r_o[j], r_o[j2], r_i[j2]), (r_o[j], r_i[j2], r_i[j])):
                tris.append(orientar(t, (0, up, 0)))
    return tris


def b_armadura(m, it, d, rng, W, H, L):
    slot = it["id"].split(".")[1]
    nom = nombre_de(it)
    mat, ac = material_armadura(it)
    n = S(d, 4, 10)
    if slot == "casco":
        if "mascara" in nom:
            k = S(d, 3, 5)
            pts = [(0.03 * math.cos(math.pi * (i / k - 0.5)), H / 2, (W / 2) * math.sin(math.pi * (i / k - 0.5))) for i in range(k + 1)]
            m.add("mascara", mat, loft([[va(p, (0, y, 0)) for p in pts] + [va(p, (-0.02, y, 0)) for p in reversed(pts)] for y in (-H / 2 + 0.02, H / 2 - 0.02)]))
            for s in (-1, 1):
                m.add("ojos", "tela_negra", caja((0.035, H * 0.6, 0.05 * s), (0.02, 0.03, 0.05)))
            m.add("adorno", ac, caja((0.035, H * 0.3, 0), (0.02, 0.04, 0.08)))
            m.add("cuernos", "hueso", tronco((0, H * 0.85, -0.06), (-0.02, H, -0.11), 0.015, 0, 4))
            m.add("cuernos", "hueso", tronco((0, H * 0.85, 0.06), (-0.02, H, 0.11), 0.015, 0, 4))
            return
        m.add("cupula", mat, cupula((0, H * 0.25, 0), W / 2, H * 0.6, W / 2, n, S(d, 2, 4)))
        m.add("aro", ac if mat not in ("oro",) else "oro", tubo_hueco(H * 0.2, H * 0.3, W / 2 * 1.04, W / 2 * 1.04, W / 2 * 1.04, W / 2 * 1.04, 0.012, n))
        m.add("cimera", "oro" if mat in ("oro", "bronce") else mat, tronco((0, H * 0.82, 0), (0, H, 0), 0.02, 0.0, 4))
        if mat == "oro":
            m.add("incrustaciones", "turquesa", caja((W / 2 * 0.95, H * 0.45, 0), (0.02, 0.05, 0.05)))
        # Cogotera (protege la nuca, hacia -X) y laterales.
        m.add("cogotera", ac if mat != "acero" else "acero", placa([(-W / 2 * 0.9, H * 0.22, 0), (-W / 2 * 1.0, 0.0, 0)], [W * 0.38, W * 0.42], 0.01, normal=(1, 0, 0)))
        return
    if slot == "cuello":
        m.add("gola", mat, tubo_hueco(0, H, L / 2, W / 2, L / 2 * 0.75, W / 2 * 0.75, 0.03, n))
        return
    if slot == "torso":
        m.add("coraza", mat, tubo_hueco(0, H * 0.85, L / 2 * 0.9, W / 2 * 0.85, L / 2, W / 2, 0.03, n))
        m.add("cuello", ac, tubo_hueco(H * 0.85, H, L / 2, W / 2, L / 2 * 0.5, W / 2 * 0.4, 0.03, n))
        if mat in ("cuero", "hierro", "bronce"):
            for k in range(S(d, 2, 4)):
                y = H * 0.12 + k * H * 0.18
                m.add("hileras", ac, tubo_hueco(y, y + 0.025, L / 2 * 0.93, W / 2 * 0.88, L / 2 * 0.93, W / 2 * 0.88, 0.01, n))
        return
    if slot == "hombreras":
        rz = W * 0.25
        for lado, s in (("izq", -1), ("der", 1)):
            c = (0, 0.0, (W / 2 - rz) * s)
            m.add(f"hombrera_{lado}", mat, rotar(cupula(c, L / 2, H, rz, n, 2), "x", -0.35 * s, c))
            m.add(f"borde_{lado}", ac, tronco(c, va(c, (0, 0.02, 0)), L / 2, L / 2, n, 0, rz, rz, u=(1, 0, 0), v=(0, 0, 1)))
        m.add("correa", "cuero", caja((0, H * 0.85, 0), (0.04, 0.02, W - 2 * rz)))
        return
    if slot in ("brazales", "grebas"):
        m.add(slot[:-1] if slot == "brazales" else "greba", mat, tubo_hueco(0, H, L / 2 * 0.8, W / 2 * 0.8, L / 2, W / 2, 0.012, n))
        if d >= 2:
            m.add("correas", ac, tubo_hueco(H * 0.3, H * 0.36, L / 2 * 0.88, W / 2 * 0.88, L / 2 * 0.9, W / 2 * 0.9, 0.008, n))
        if slot == "grebas":
            m.add("rodillera", mat, cupula((L / 2 * 0.6, H * 0.88, 0), 0.04, 0.07, W / 2 * 0.8, 6, 2))
        return
    if slot == "guantes":
        m.add("puno", mat, tubo_hueco(H * 0.55, H, L / 2 * 0.9, W / 2 * 0.9, L / 2, W / 2, 0.01, n))
        m.add("palma", mat, caja((0, H * 0.4, 0), (L * 0.5, H * 0.3, W * 0.8)))
        m.add("dedos", mat, caja((0.0, H * 0.13, 0), (L * 0.4, H * 0.26, W * 0.75)))
        m.add("pulgar", mat, rotar(caja((0.0, H * 0.35, W * 0.45), (0.025, H * 0.25, 0.025)), "x", -0.5, (0.0, H * 0.45, W * 0.4)))
        return
    if slot == "faldar":
        m.add("faldar", mat, tubo_hueco(0, H, L / 2, W / 2, L / 2 * 0.75, W / 2 * 0.75, 0.02, n))
        m.add("cinturon", ac, tubo_hueco(H * 0.88, H, L / 2 * 0.78, W / 2 * 0.78, L / 2 * 0.76, W / 2 * 0.76, 0.015, n))
        return
    if slot == "botas":
        m.add("cana", mat, tubo_hueco(H * 0.25, H, L / 4 * 0.9, W / 2 * 0.85, L / 4, W / 2, 0.012, n, x=-L / 4))
        m.add("pie", mat, loft([[(-L / 2, 0.0, W / 2 * 0.9), (-L / 2, 0.0, -W / 2 * 0.9), (-L / 2, H * 0.35, -W / 2 * 0.9), (-L / 2, H * 0.35, W / 2 * 0.9)],
                                [(L / 4, 0.0, W / 2), (L / 4, 0.0, -W / 2), (L / 4, H * 0.22, -W / 2), (L / 4, H * 0.22, W / 2)],
                                [(L / 2, 0.0, W / 2 * 0.6), (L / 2, 0.0, -W / 2 * 0.6), (L / 2, H * 0.1, -W / 2 * 0.6), (L / 2, H * 0.1, W / 2 * 0.6)]]))
        m.add("suela", "cuero_oscuro", caja((0, 0.01, 0), (L, 0.02, W * 0.95)))
        return
    if slot == "montura":  # barda: manto que cubre el cuerpo y el cuello del caballo
        k = S(d, 4, 7)
        anillos = []
        for i in range(k + 1):
            x = -L / 2 + L * 0.8 * i / k
            ring = []
            for j in range(9):
                a = math.pi * j / 8           # semicirculo abierto por abajo
                ring.append((x, H * 0.62 + math.sin(a) * H * 0.22, math.cos(a) * W / 2))
            ring += [(x, H * 0.02, -W / 2), (x, H * 0.02, W / 2)]
            anillos.append(ring)
        m.add("manto", mat, _lamina_seccion(anillos))
        m.add("cuello", mat, mover(rotar(tubo_hueco(0, H * 0.45, 0.15, W / 2 * 0.45, 0.12, W / 2 * 0.35, 0.02, n, x=0), "z", -0.7, (0, 0, 0)), (L * 0.32, H * 0.6, 0)))
        m.add("borde", ac, caja((0, H * 0.06, W / 2), (L * 0.8, H * 0.08, 0.02)))
        m.add("borde", ac, caja((0, H * 0.06, -W / 2), (L * 0.8, H * 0.08, 0.02)))
        return
    b_caja(m, it, d, rng, W, H, L)


def _lamina_seccion(anillos):
    """Lamina de doble cara a partir de secciones abiertas (una polilinea por anillo)."""
    tris = []
    for a, b in zip(anillos, anillos[1:]):
        for j in range(len(a) - 1):
            for t in ((a[j], a[j + 1], b[j + 1]), (a[j], b[j + 1], b[j])):
                tris += [t, (t[0], t[2], t[1])]
    return tris


def b_vestimenta(m, it, d, rng, W, H, L):
    slot = it["id"].split(".")[1]
    nom = nombre_de(it)
    n = S(d, 4, 10)
    col = {"deel": "fieltro_azul", "deel_invierno": "fieltro_rojo", "tunica_campesina": "tela_lino", "uniforme_imperial": "tela_azul",
           "tunica_culto": "tela_negra", "harapos": "tela_ocre", "capa": "tela_roja", "capa_piel": "piel_pardo",
           "gorro_piel": "piel_pardo", "gorro_punta": "fieltro_rojo", "pantalon": "tela_ocre", "botas_fieltro": "fieltro_blanco",
           "cinturon": "cuero", "bufanda": "fieltro_azul"}.get(nom, "tela_lino")
    if slot == "torso":
        largo = H
        m.add("cuerpo", col, tubo_hueco(0, largo * 0.55, L / 2, W * 0.36, L / 2 * 0.8, W * 0.3, 0.02, n))
        m.add("pecho", col, tubo_hueco(largo * 0.55, largo * 0.95, L / 2 * 0.8, W * 0.3, L / 2 * 0.65, W * 0.3, 0.02, n))
        if d >= 1:
            m.add("cuello", "fieltro_ocre" if "deel" in nom else col, tubo_hueco(largo * 0.93, largo, L / 2 * 0.4, W * 0.15, L / 2 * 0.35, W * 0.12, 0.015, n))
        for s, lado in ((-1, "izq"), (1, "der")):
            top = (0, largo * 0.9, W * 0.3 * s)
            bot = (0.02, largo * 0.48, W / 2 * s * 0.92)
            m.add(f"manga_{lado}", col, tronco(top, bot, 0.06, 0.07, n))
        if "deel" in nom:
            m.add("faja", "fieltro_ocre" if nom == "deel" else "tela_azul",
                  tubo_hueco(largo * 0.5, largo * 0.57, L / 2 * 0.84, W * 0.32, L / 2 * 0.84, W * 0.32, 0.015, n))
            m.add("solapa", "oro", caja((L / 2 * 0.72, largo * 0.8, W * 0.08), (0.02, largo * 0.25, 0.04)))
        if nom == "deel_invierno":
            m.add("ribete", "piel_blanca", tubo_hueco(0, 0.06, L / 2 * 1.04, W * 0.38, L / 2 * 1.04, W * 0.38, 0.02, n))
        if nom == "uniforme_imperial":
            for k in range(4):
                m.add("botones", "oro", caja((L / 2 * 0.78, largo * (0.55 + 0.1 * k), 0), (0.015, 0.02, 0.02)))
            m.add("cinturon", "cuero_oscuro", tubo_hueco(largo * 0.5, largo * 0.55, L / 2 * 0.84, W * 0.32, L / 2 * 0.84, W * 0.32, 0.015, n))
        if nom == "tunica_culto":
            m.add("capucha", col, cupula((-0.05, largo * 0.88, 0), 0.14, 0.2, 0.13, n, 3))
        if nom == "harapos":
            for k in range(S(d, 2, 5)):
                a = 2 * math.pi * k / S(d, 2, 5)
                m.add("jirones", col, tronco((L / 2 * 0.9 * math.cos(a), 0.05, W * 0.33 * math.sin(a)),
                                             (L / 2 * 0.9 * math.cos(a), -0.12, W * 0.33 * math.sin(a)), 0.04, 0.0, 3))
        return
    if slot == "cabeza":
        if "punta" in nom:
            m.add("copa", col, tronco((0, 0.05, 0), (-0.03, H, 0), W / 2 * 0.85, 0.0, n))
            m.add("ala", "fieltro_ocre", tubo_hueco(0, 0.06, L / 2, W / 2, L / 2, W / 2, 0.03, n))
        else:
            m.add("copa", "fieltro_rojo", cupula((0, H * 0.3, 0), W / 2 * 0.85, H * 0.7, W / 2 * 0.85, n, 3))
            m.add("ala", col, tubo_hueco(0, H * 0.45, L / 2, W / 2, L / 2 * 0.95, W / 2 * 0.95, 0.04, n))
        return
    if slot == "piernas":
        for s, lado in ((-1, "izq"), (1, "der")):
            m.add(f"pernera_{lado}", col, tronco((0, 0.0, W * 0.24 * s), (0, H * 0.78, W * 0.2 * s), 0.065, 0.09, n))
        m.add("cintura", col, tubo_hueco(H * 0.75, H, L / 2, W / 2, L / 2 * 0.95, W / 2 * 0.95, 0.02, n))
        return
    if slot == "pies":
        m.add("cana", col, tubo_hueco(H * 0.2, H, L / 4, W / 2 * 0.9, L / 4, W / 2, 0.012, n, x=-L / 4))
        m.add("pie", col, caja((0, H * 0.12, 0), (L, H * 0.24, W)))
        m.add("puntera", "fieltro_rojo", tronco((L / 2 - 0.02, H * 0.12, 0), (L / 2 + 0.02, H * 0.22, 0), 0.04, 0.0, 4))
        m.add("suela", "cuero_oscuro", caja((0, 0.01, 0), (L * 1.02, 0.02, W)))
        return
    if slot == "espalda":  # capa: lamina curva detras (-X), de los hombros al suelo
        k = S(d, 3, 6)
        anillos = []
        for i in range(k + 1):
            y = H - H * i / k
            ancho = W / 2 * (0.6 + 0.4 * i / k)
            ring = [(-L / 2 * 0.5 - 0.15 * math.sin(math.pi * j / 6) * (i / k + 0.3), y, ancho * math.cos(math.pi * j / 6)) for j in range(7)]
            anillos.append(ring)
        m.add("capa", col, _lamina_seccion(anillos))
        m.add("cuello", "piel_blanca" if "piel" in nom else "oro", tubo_hueco(H * 0.94, H, L / 2 * 0.5, W * 0.18, L / 2 * 0.5, W * 0.18, 0.02, n))
        return
    if slot == "cintura":
        m.add("cinto", col, tubo_hueco(0, H, L / 2, W / 2, L / 2, W / 2, 0.012, n))
        m.add("hebilla", "oro", caja((L / 2, H / 2, 0), (0.015, H * 1.0, 0.08)))
        m.add("placas", "oro", caja((0, H / 2, W / 2), (0.06, H * 0.8, 0.012)))
        return
    if slot == "cuello":
        m.add("vuelta", col, toro((0, H * 0.75, 0), W / 2 * 0.7, W * 0.12, n, 4))
        m.add("cola", col, placa([(L * 0.25, H * 0.7, W * 0.2), (L * 0.35, 0.0, W * 0.3)], [0.05, 0.06], 0.012, normal=(1, 0, 0)))
        return
    b_caja(m, it, d, rng, W, H, L)


# --- Accesorios -----------------------------------------------------------------
def b_accesorio(m, it, d, rng, W, H, L):
    sub, nom = it["id"].split(".")[1], nombre_de(it)
    n = S(d, 4, 10)
    if sub == "amuleto" or nom == "placa_arreo_padre":
        m.add("placa", "oro", loft([anillo((-L / 2, H / 2, 0), (1, 0, 0), H / 2, W / 2, n, 0, (0, 1, 0), (0, 0, 1)),
                                    anillo((L / 2 * 0.4, H / 2, 0), (1, 0, 0), H / 2 * 0.95, W / 2 * 0.95, n, 0, (0, 1, 0), (0, 0, 1))]))
        # Relieve del animal: un bulto y la cabeza, y una turquesa engastada.
        m.add("relieve", "oro", mover(rotar(cupula((0, 0, 0), H * 0.28, L * 0.5, W * 0.32, S(d, 4, 6), 2), "z", -math.pi / 2), (L / 2 * 0.4, H * 0.45, -W * 0.08)))
        m.add("turquesa", "turquesa", caja((L / 2 * 0.6, H * 0.6, W * 0.25), (L * 0.4, H * 0.18, H * 0.18)))
        if sub == "amuleto":
            m.add("anilla", "oro", toro((0, H * 0.95, 0), H * 0.12, H * 0.035, 5, 3, eje="x"))
        return
    if nom == "silla_montar":
        k = S(d, 3, 5)
        anillos = []
        for i in range(k + 1):
            x = -L / 2 + L * i / k
            t = (i / k) * 2 - 1
            y = H * 0.45 + H * 0.4 * t * t
            anillos.append(anillo((x, y, 0), (1, 0, 0), 0.04, W * 0.28, n, 0, (0, 1, 0), (0, 0, 1)))
        m.add("asiento", "madera_pintada", loft(anillos))
        m.add("arzon", "madera_pintada", caja((L / 2 - 0.04, H * 0.8, 0), (0.06, H * 0.4, W * 0.4)))
        m.add("arzon", "madera_pintada", caja((-L / 2 + 0.04, H * 0.75, 0), (0.06, H * 0.35, W * 0.45)))
        for s in (-1, 1):
            m.add("faldones", "cuero", caja((0, H * 0.3, W * 0.4 * s), (L * 0.7, H * 0.55, 0.03)))
        m.add("adornos", "plata", caja((L / 2 - 0.005, H * 0.85, 0), (0.02, 0.06, 0.08)))
        return
    if nom == "brida":
        m.add("muserola", "cuero", tubo_hueco(H * 0.1, H * 0.25, 0.12, W / 2 * 0.6, 0.12, W / 2 * 0.6, 0.012, n, x=L * 0.25))
        m.add("testera", "cuero", tubo_hueco(H * 0.6, H * 0.72, 0.1, W / 2 * 0.7, 0.1, W / 2 * 0.7, 0.012, n, x=-L * 0.1))
        for s in (-1, 1):
            m.add("carrilleras", "cuero", viga((L * 0.25, H * 0.2, W / 2 * 0.6 * s), (-L * 0.1, H * 0.66, W / 2 * 0.7 * s), 0.02, 0.01))
            if d >= 1:
                m.add("riendas", "cuero", viga((L * 0.32, H * 0.15, W / 2 * 0.6 * s), (-L / 2, H * 0.0, W / 2 * s), 0.015, 0.008))
            if d >= 2:
                m.add("bocado", "hierro", toro((L * 0.34, H * 0.15, W / 2 * 0.62 * s), 0.025, 0.006, 5, 3, eje="z"))
        m.add("adorno", "oro", caja((-L * 0.1 + 0.1, H * 0.95, 0), (0.02, 0.08, 0.06)))
        return
    if nom == "estribos":
        k = S(d, 3, 6)
        pts = [(0, H * 0.15 + H * 0.8 * math.sin(math.pi * i / k), W / 2 * 0.8 * math.cos(math.pi * i / k)) for i in range(k + 1)]
        m.add("arco", "hierro", placa(pts, [L / 2 * 0.3] * len(pts), 0.008, normal=(1, 0, 0)))
        m.add("pisa", "hierro", caja((0, H * 0.1, 0), (L, H * 0.06, W * 0.85)))
        m.add("ojal", "cuero", caja((0, H * 0.96, 0), (0.02, 0.04, 0.04)))
        return
    if nom == "alforjas":
        for s, lado in ((-1, "izq"), (1, "der")):
            m.add(f"bolsa_{lado}", "cuero_claro", (caja_biselada if d >= 2 else caja)((0, H * 0.35, (W / 2 - 0.08) * s), (L, H * 0.7, 0.16)))
            m.add(f"tapa_{lado}", "cuero", caja((0, H * 0.66, (W / 2 - 0.07) * s), (L * 1.02, H * 0.1, 0.17)))
        m.add("puente", "cuero", caja((0, H * 0.95, 0), (L * 0.6, H * 0.08, W - 0.3)))
        return
    if nom == "piedra_runica":
        m.add("estela", "piedra", loft([[(x, 0.0, z) for x, z in ((L / 2, W / 2), (-L / 2, W / 2), (-L / 2, -W / 2), (L / 2, -W / 2))],
                                        [(x, H * 0.75, z) for x, z in ((L / 2 * 0.9, W / 2 * 0.95), (-L / 2 * 0.9, W / 2 * 0.95), (-L / 2 * 0.9, -W / 2 * 0.95), (L / 2 * 0.9, -W / 2 * 0.95))],
                                        [(x, H, z) for x, z in ((L / 2 * 0.7, W / 2 * 0.4), (-L / 2 * 0.7, W / 2 * 0.4), (-L / 2 * 0.7, -W / 2 * 0.4), (L / 2 * 0.7, -W / 2 * 0.4))]]))
        for k in range(S(d, 2, 4)):
            m.add("runas", "piedra_oscura", caja((L / 2 * 0.95, H * (0.2 + 0.13 * k), 0), (0.02, 0.04, W * 0.6)))
        return
    if nom == "tablilla":
        m.add("tablilla", "madera_clara", caja((0, H / 2, 0), (L, H, W)))
        for k in range(S(d, 2, 4)):
            m.add("signos", "madera_oscura", caja((L / 2, H * (0.25 + 0.15 * k), 0), (0.005, 0.015, W * 0.7)))
        m.add("cordel", "cuerda", toro((0, H * 0.95, 0), 0.02, 0.004, S(d, 4, 5), 3, eje="x"))
        return
    b_caja(m, it, d, rng, W, H, L)


# --- Estructuras ----------------------------------------------------------------
def b_yurta(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 8, 16)
    R = min(W, L) / 2
    hw = H * 0.6
    if "quemada" in nom:
        for k in range(n):
            a = 2 * math.pi * k / n
            p = (math.cos(a) * R, 0, math.sin(a) * R)
            alto = hw * rng.uniform(0.4, 1.0) * 1.3
            m.add("enrejado", "carbon", viga(p, va(p, (rng.uniform(-0.2, 0.2), alto, rng.uniform(-0.2, 0.2))), 0.07))
        for k in range(S(d, 3, 6)):
            a = rng.uniform(0, 2 * math.pi)
            m.add("varas", "carbon", viga((math.cos(a) * R * 0.9, 0.05, math.sin(a) * R * 0.9),
                                          (math.cos(a + 2.5) * R * 0.4, rng.uniform(0.2, H), math.sin(a + 2.5) * R * 0.4), 0.06))
        m.add("cenizas", "carbon", cupula((0, 0, 0), R * 0.85, 0.15, R * 0.85, n, 2))
        m.add("restos", "fieltro_oscuro", placa([(-R * 0.5, 0.05, -R * 0.3), (R * 0.2, 0.08, R * 0.4)], [R * 0.3, R * 0.25], 0.02, normal=(0, 1, 0)))
        m.add("puerta", "carbon", caja((R, hw * 0.35, 0), (0.1, hw * 0.7, 0.8)))
        return
    jefe = "jefe" in nom
    fase = math.pi / n  # una cara plana (no una arista) mira a +X
    m.add("plataforma", "madera", tronco((0, 0, 0), (0, 0.08, 0), R * 1.05, R * 1.05, n, fase))
    m.add("pared", "fieltro_blanco", tronco((0, 0.08, 0), (0, hw, 0), R, R, n, fase))
    m.add("techo", "fieltro_crema", tronco((0, hw, 0), (0, H * 0.92, 0), R * 1.06, R * 0.2, n, fase))
    m.add("corona", "madera", tronco((0, H * 0.9, 0), (0, H * 0.97, 0), R * 0.22, R * 0.2, n, fase))
    m.add("chimenea", "carbon", tronco((R * 0.08, H * 0.9, 0), (R * 0.08, H, 0), 0.06, 0.06, 6))
    for y in (hw * 0.35, hw * 0.75):
        m.add("cuerdas", "cuerda", tronco((0, y, 0), (0, y + 0.06, 0), R * 1.015, R * 1.015, n, fase))
    ap = R * math.cos(math.pi / n)
    m.add("puerta", "madera_pintada", caja((ap + 0.02, 0.08 + hw * 0.4, 0), (0.08, hw * 0.78, min(0.9, R * 0.4))))
    m.add("marco", "madera_oscura", caja((ap + 0.005, 0.08 + hw * 0.42, 0), (0.06, hw * 0.86, min(1.05, R * 0.46))))
    if jefe or "enfermeria" in nom:
        cel = "fieltro_azul" if jefe else "fieltro_rojo"
        m.add("banda", cel, tronco((0, hw * 0.88, 0), (0, hw * 0.98, 0), R * 1.02, R * 1.02, n, fase))
        m.add("banda", cel, tronco((0, hw + (H * 0.92 - hw) * 0.25, 0), (0, hw + (H * 0.92 - hw) * 0.35, 0), R * 0.86, R * 0.76, n, fase))
    if jefe:
        m.add("remate", "oro", tronco((0, H * 0.97, 0), (0, H, 0), 0.12, 0.0, 6))


def _casa(m, W, H, L, d, pared, tejado, alto_pared=0.6, puerta="madera", tejado_tipo="dos_aguas", z=0.0):
    hp = H * alto_pared
    m.add("paredes", pared, caja((0, hp / 2, z), (L, hp, W)))
    if tejado_tipo == "plano":
        m.add("tejado", tejado, caja((0, hp + (H - hp) / 2, z), (L * 1.04, H - hp, W * 1.04)))
    else:
        m.add("tejado", tejado, prisma_tejado(-L / 2 - 0.15, L / 2 + 0.15, W * 1.1, hp, H - hp, z))
    m.add("puerta", puerta, caja((L / 2 + 0.03, min(1.0, hp * 0.4), z), (0.06, min(2.0, hp * 0.8), min(1.0, W * 0.25))))
    for s in (-1, 1):
        m.add("ventanas", "madera_oscura", caja((L / 4 * s, hp * 0.6, z + W / 2 + 0.02), (min(0.6, L * 0.1), min(0.6, hp * 0.2), 0.04)))


def b_estructura(m, it, d, rng, W, H, L):
    sub, nom = it["id"].split(".")[1], nombre_de(it)
    n = S(d, 6, 12)
    if nom in ("yurta_comun", "yurta_jefe", "yurta_quemada", "enfermeria"):
        return b_yurta(m, it, d, rng, W, H, L)
    if nom == "tienda_ligera":
        m.add("lona", "fieltro_crema", prisma_tejado(-L / 2, L / 2, W, 0, H))
        m.add("palos", "madera", viga((L / 2 + 0.05, 0, 0), (L / 2 + 0.05, H * 1.05, 0), 0.05))
        m.add("palos", "madera", viga((-L / 2 - 0.05, 0, 0), (-L / 2 - 0.05, H * 1.05, 0), 0.05))
        m.add("entrada", "fieltro_oscuro", extruir([(-W * 0.2, 0), (W * 0.2, 0), (0, H * 0.6)], (L / 2 + 0.005, 0, 0), (0, 0, 1), (0, 1, 0), (1, 0, 0), 0.02))
        return
    if nom in ("choza_fieltro", "refugio_nieve", "horno_cocina"):
        cel = {"choza_fieltro": "fieltro_gris", "refugio_nieve": "nieve", "horno_cocina": "adobe"}[nom]
        m.add("cupula", cel, cupula((0, 0, 0), L / 2, H, W / 2, n, S(d, 3, 5)))
        m.add("entrada", "carbon" if nom != "refugio_nieve" else "hielo", caja((L / 2 * 0.9, H * 0.22, 0), (L * 0.25, H * 0.44, W * 0.3)))
        if nom == "horno_cocina":
            m.add("chimenea", "adobe", tronco((-L * 0.15, H * 0.7, 0), (-L * 0.15, H, 0), 0.12, 0.1, 6))
        return
    if nom == "casa_adobe":
        _casa(m, W, H, L, d, "adobe", "adobe", 0.85, tejado_tipo="plano")
        return
    if nom == "cabana_troncos":
        hp = H * 0.6
        k = S(d, 3, 6)
        for i in range(k):
            y = hp * (i + 0.5) / k
            r = hp / k / 2
            for s in (-1, 1):
                m.add("troncos", "corteza", tronco((-L / 2 - 0.15, y, W / 2 * s), (L / 2 + 0.15, y, W / 2 * s), r, r, 5))
                m.add("troncos", "corteza", tronco((L / 2 * s, y + r, -W / 2 - 0.15), (L / 2 * s, y + r, W / 2 + 0.15), r, r, 5))
        m.add("tejado", "madera_oscura", prisma_tejado(-L / 2 - 0.3, L / 2 + 0.3, W * 1.15, hp, H - hp))
        m.add("puerta", "madera", caja((L / 2 + 0.2, 1.0, 0), (0.06, 2.0, 1.0)))
        return
    if nom == "casa_larga":
        hp = H * 0.4
        m.add("paredes", "madera", caja((0, hp / 2, 0), (L, hp, W)))
        k = S(d, 3, 6)
        anillos = []
        for i in range(k + 1):
            x = -L / 2 + L * i / k
            hh = (H - hp) * (1 - 0.15 * (2 * i / k - 1) ** 2)
            anillos.append([(x, hp, W * 0.58), (x, hp + hh, 0), (x, hp, -W * 0.58)])
        m.add("tejado", "hierba_seca", loft(anillos))
        m.add("puerta", "madera_oscura", caja((L / 2 + 0.03, 1.0, 0), (0.06, 2.0, 1.2)))
        return
    if nom in ("casa_campesina", "casa_urbana", "cuartel", "granero"):
        tej = {"casa_campesina": "hierba_seca"}.get(nom, "teja")
        pared = {"casa_urbana": "adobe", "granero": "madera", "cuartel": "piedra"}.get(nom, "adobe")
        if nom == "granero":
            for x in (-1, 1):
                for z in (-1, 1):
                    m.add("pilotes", "piedra", caja_suelo(x * L * 0.4, z * W * 0.4, 0.5, 1.0, 0.5))
            _casa(m, W, H - 1.0, L, d, pared, tej, 0.6)
            m.partes["paredes"][1] = mover(m.partes["paredes"][1], (0, 1.0, 0))
            m.partes["tejado"][1] = mover(m.partes["tejado"][1], (0, 1.0, 0))
            m.partes["puerta"][1] = mover(m.partes["puerta"][1], (0, 1.0, 0))
            m.partes["ventanas"][1] = mover(m.partes["ventanas"][1], (0, 1.0, 0))
            return
        _casa(m, W, H, L, d, pared, tej, 0.62 if nom != "casa_urbana" else 0.72)
        if nom == "casa_urbana":
            m.add("balcon", "madera", caja((L / 2 + 0.4, H * 0.45, 0), (0.8, 0.15, W * 0.5)))
        return
    if nom == "palacio":
        m.add("cuerpo", "adobe", caja((0, H * 0.25, 0), (L * 0.7, H * 0.5, W * 0.7)))
        m.add("tejado", "teja", prisma_tejado(-L * 0.37, L * 0.37, W * 0.75, H * 0.5, H * 0.2))
        m.add("cupula", "oro", cupula((0, H * 0.62, 0), L * 0.12, H * 0.3, L * 0.12, n, 3))
        m.add("cupula", "oro", tronco((0, H * 0.9, 0), (0, H, 0), 0.4, 0.0, 6))
        for x in (-1, 1):
            for z in (-1, 1):
                m.add("torres", "adobe", tronco((x * L * 0.42, 0, z * W * 0.42), (x * L * 0.42, H * 0.65, z * W * 0.42), 2.2, 2.0, S(d, 6, 8)))
                m.add("chapiteles", "teja", tronco((x * L * 0.42, H * 0.65, z * W * 0.42), (x * L * 0.42, H * 0.85, z * W * 0.42), 2.6, 0.0, S(d, 6, 8)))
        m.add("escalinata", "piedra", caja((L * 0.4, 0.6, 0), (L * 0.15, 1.2, W * 0.3)))
        m.add("puerta", "madera_pintada", caja((L * 0.35 + 0.05, 2.5, 0), (0.1, 3.5, 3.0)))
        return
    if nom in ("fogata", "hoguera"):
        k = S(d, 4, 8)
        R = min(W, L) / 2 * 0.8
        for i in range(k):
            a = 2 * math.pi * i / k
            c = (math.cos(a) * R, 0.06, math.sin(a) * R)
            m.add("piedras", "piedra", roca(c, 0.12, 0.1, 0.1, 4, 3, rng) if d >= 2 else octaedro(c, 0.1))
        nl = S(d, 2, 3) if nom == "fogata" else S(d, 3, 5)
        for i in range(nl):
            a = 2 * math.pi * i / nl + 0.5
            m.add("lena", "madera_oscura", viga((math.cos(a) * R * 0.8, 0.02, math.sin(a) * R * 0.8), (0, H * 0.7, 0), 0.08))
        m.add("fuego", "fuego", tronco((0, 0.0, 0), (0, H, 0), R * 0.32, 0.0, S(d, 3, 6)))
        return
    if nom == "corral":
        k = S(d, 6, 8)
        R = min(W, L) / 2
        pts = [(math.cos(2 * math.pi * i / k + math.pi / k) * R, math.sin(2 * math.pi * i / k + math.pi / k) * R) for i in range(k)]
        for i, (x, z) in enumerate(pts):
            m.add("postes", "madera", tronco((x, 0, z), (x, H, z), 0.1, 0.09, 3 if d < 2 else 4))
            x2, z2 = pts[(i + 1) % k]
            if i != 0:  # el tramo entre el primer y el ultimo poste queda abierto: la entrada (+X)
                for y in ((H * 0.45, H * 0.85) if d >= 2 else (H * 0.75,)):
                    m.add("travesanos", "madera_clara", viga((x, y, z), (x2, y, z2), 0.09))
        return
    if nom == "secadero":
        for s in (-1, 1):
            m.add("postes", "madera", viga((0, 0, W / 2 * s * 0.95), (0, H, W / 2 * s * 0.95), 0.08))
        m.add("travesano", "madera", viga((0, H * 0.95, -W / 2), (0, H * 0.95, W / 2), 0.07))
        for k in range(3):
            z = -W / 3 + k * W / 3
            m.add("pieles", ["piel_pardo", "cuero_claro", "carne"][k], caja((0, H * 0.65, z), (0.03, H * 0.55, W / 3 * 0.8)))
        return
    if nom == "forja":
        m.add("hogar", "piedra", caja((-L * 0.2, H * 0.3, 0), (L * 0.55, H * 0.6, W * 0.8)))
        m.add("brasas", "fuego", caja((-L * 0.2, H * 0.62, 0), (L * 0.4, 0.06, W * 0.5)))
        m.add("fuelle", "cuero", elipsoide((-L * 0.48, H * 0.55, 0), 0.15, 0.1, 0.25, 6, 3))
        m.add("tocon", "corteza", tronco((L * 0.3, 0, 0), (L * 0.3, H * 0.4, 0), 0.25, 0.25, n))
        m.add("yunque", "hierro", caja((L * 0.3, H * 0.5, 0), (0.45, 0.2, 0.2)))
        m.add("yunque", "hierro", tronco((L * 0.52, H * 0.55, 0), (L * 0.5 + 0.2, H * 0.55, 0), 0.07, 0.0, 4))
        m.add("chimenea", "piedra", tronco((-L * 0.3, H * 0.6, 0), (-L * 0.3, H, 0), 0.25, 0.15, 6))
        return
    if nom in ("jaula_prisioneros", "jaula_sacrificio"):
        cel = "madera" if nom == "jaula_prisioneros" else "hierro_oxidado"
        m.add("base", "madera_oscura", caja_suelo(0, 0, L, 0.15, W))
        m.add("techo", "madera_oscura", caja((0, H - 0.07, 0), (L, 0.14, W)))
        k = S(d, 3, 5)
        perimetro = []
        for i in range(k):
            t = -0.5 + i / k
            perimetro += [(t * L, -W / 2), (L / 2, t * W), (-t * L, W / 2), (-L / 2, -t * W)]
        for x, z in perimetro:
            m.add("barrotes", cel, tronco((x, 0.15, z), (x, H - 0.14, z), 0.035, 0.035, 3 if d < 2 else 4))
        if nom == "jaula_sacrificio":
            m.add("craneos", "hueso", elipsoide((0, H + 0.0, 0), 0.15, 0.12, 0.12, 5, 3))
        return
    if nom in ("poste_ejecucion",):
        m.add("poste", "madera_oscura", viga((0, 0, 0), (0, H, 0), 0.22))
        m.add("travesano", "madera_oscura", viga((0, H * 0.85, -W / 2), (0, H * 0.85, W / 2), 0.12))
        m.add("cuerdas", "cuerda", toro((0.11, H * 0.6, 0), 0.08, 0.02, S(d, 4, 6), 3, eje="x"))
        return
    if nom in ("empalizada",):
        k = S(d, 4, 9)
        r = W / k / 2
        for i in range(k):
            z = -W / 2 + r + i * 2 * r
            h = H * rng.uniform(0.85, 1.0)
            nn = S(d, 4, 6)
            m.add("troncos", "corteza", loft([anillo((0, 0, z), (0, 1, 0), r, r, nn), anillo((0, h * 0.85, z), (0, 1, 0), r, r, nn), [(0, h, z)]]))
        m.add("travesano", "madera", viga((-r - 0.04, H * 0.6, -W / 2), (-r - 0.04, H * 0.6, W / 2), 0.08))
        return
    if nom == "atalaya":
        for x in (-1, 1):
            for z in (-1, 1):
                m.add("patas", "madera", viga((x * L * 0.45, 0, z * W * 0.45), (x * L * 0.3, H * 0.75, z * W * 0.3), 0.15))
        m.add("plataforma", "madera_clara", caja((0, H * 0.75, 0), (L * 0.8, 0.12, W * 0.8)))
        m.add("baranda", "madera", caja((L * 0.38, H * 0.8, 0), (0.06, 0.5, W * 0.8)))
        m.add("techo", "fieltro_crema", tronco((0, H * 0.88, 0), (0, H, 0), L * 0.5, 0.0, 4, math.pi / 4))
        m.add("escalera", "madera_clara", viga((L * 0.6, 0, 0), (L * 0.35, H * 0.75, 0), 0.08, 0.4))
        return
    if nom == "muralla" or nom == "muro_piedra" or nom == "muro":
        cel = "piedra" if nom != "muralla" else "piedra"
        if nom == "muro":
            k = S(d, 3, 6)
            for i in range(k):
                z = -W / 2 + W * (i + 0.5) / k
                h = H * rng.uniform(0.35, 1.0)
                m.add("muro", "piedra", caja((rng.uniform(-0.1, 0.1), h / 2, z), (L, h, W / k * 1.02)))
            m.add("escombros", "piedra_oscura", roca((L * 0.6, 0.1, 0), 0.4, 0.3, 0.6, 5, 3, rng))
            return
        m.add("muro", cel, caja((0, H * 0.43, 0), (L, H * 0.86, W)))
        k = S(d, 3, 6)
        for i in range(k):
            z = -W / 2 + W * (i + 0.25) / k
            m.add("almenas", cel, caja((L * 0.25, H * 0.93, z + W / k * 0.25), (L * 0.5, H * 0.14, W / k * 0.5)))
        return
    if nom == "puerta":
        for s in (-1, 1):
            m.add("torres", "piedra", caja((0, H * 0.45, W * 0.38 * s), (L, H * 0.9, W * 0.24)))
            m.add("almenas", "piedra", caja((0, H * 0.95, W * 0.38 * s), (L * 0.9, H * 0.1, W * 0.2)))
        m.add("dintel", "piedra", caja((0, H * 0.7, 0), (L * 0.8, H * 0.3, W * 0.52)))
        m.add("porton", "madera_oscura", caja((L * 0.3, H * 0.28, 0), (0.2, H * 0.56, W * 0.52)))
        m.add("bandera", "tela_roja", caja((L * 0.5, H * 0.7, 0), (0.05, H * 0.25, W * 0.15)))
        return
    if nom == "torre":
        m.add("torre", "piedra", caja((0, H * 0.43, 0), (L * 0.85, H * 0.86, W * 0.85)))
        m.add("parapeto", "piedra", caja((0, H * 0.9, 0), (L, H * 0.06, W)))
        for x in (-1, 1):
            for z in (-1, 1):
                m.add("almenas", "piedra", caja((x * L * 0.4, H * 0.96, z * W * 0.4), (L * 0.2, H * 0.08, W * 0.2)))
        m.add("puerta", "madera_oscura", caja((L * 0.43, 1.1, 0), (0.06, 2.2, 1.2)))
        m.add("troneras", "carbon", caja((L * 0.43, H * 0.6, 0), (0.06, 1.0, 0.3)))
        return
    if nom == "molino":
        m.add("torre", "adobe", tronco((0, 0, 0), (0, H * 0.7, 0), min(W, L) * 0.35, min(W, L) * 0.25, n))
        m.add("tejado", "teja", tronco((0, H * 0.7, 0), (0, H * 0.85, 0), min(W, L) * 0.28, 0.0, n))
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            p = (min(W, L) * 0.3, H * 0.65, 0)
            m.add("aspas", "tela_lino", placa([p, va(p, (0, math.sin(a) * H * 0.33, math.cos(a) * W * 0.48))], [0.1, 0.4], 0.03, normal=(1, 0, 0)))
        m.add("puerta", "madera", caja((min(W, L) * 0.35, 1.0, 0), (0.1, 2.0, 1.0)))
        return
    if nom == "fundicion":
        _casa(m, W, H * 0.6, L, d, "piedra", "teja", 0.6)
        for i, x in enumerate((-L * 0.25, L * 0.15)):
            m.add("chimeneas", "piedra_oscura", tronco((x, H * 0.3, W * 0.2), (x, H, W * 0.2), 1.0, 0.8, S(d, 5, 8)))
        m.add("hornos", "fuego", caja((L / 2 + 0.05, 1.0, W * 0.25), (0.1, 1.6, 2.0)))
        return
    if nom == "cadalso":
        m.add("plataforma", "madera", caja((0, H * 0.4, 0), (L, 0.2, W)))
        for x in (-1, 1):
            for z in (-1, 1):
                m.add("pilares", "madera_oscura", viga((x * L * 0.45, 0, z * W * 0.45), (x * L * 0.45, H * 0.4, z * W * 0.45), 0.2))
        m.add("horca", "madera_oscura", viga((0, H * 0.4, -W * 0.3), (0, H, -W * 0.3), 0.2))
        m.add("horca", "madera_oscura", viga((0, H * 0.95, -W * 0.3), (0, H * 0.95, W * 0.15), 0.15))
        m.add("escalera", "madera_clara", viga((L * 0.75, 0, 0), (L * 0.48, H * 0.4, 0), 0.1, 1.0))
        return
    if nom in ("altar", "templo"):
        k = 3
        for i in range(k):
            f = 1 - i * 0.22
            m.add(f"grada_{i + 1}", "piedra" if nom == "altar" else "arenisca", caja((0, H * 0.08 * (2 * i + 1) / 2 + H * 0.08 * i / 2 * 0, 0),
                                                                                      (L * f, H * 0.08 if nom == "altar" else H * 0.12, W * f)))
            m.partes[f"grada_{i + 1}"][1] = mover(m.partes[f"grada_{i + 1}"][1], (0, (H * (0.08 if nom == "altar" else 0.12)) * i - H * 0.04 * (2 * i + 1) / 2 + H * (0.04 if nom == "altar" else 0.06), 0))
        base = H * (0.24 if nom == "altar" else 0.36)
        if nom == "altar":
            m.add("ara", "piedra_oscura", caja((0, base + 0.5, 0), (2.5, 1.0, 1.4)))
            m.add("ofrendas", "cornalina", caja((0, base + 1.05, 0), (1.5, 0.1, 0.8)))
            for k2 in range(4):
                a = math.pi / 4 + k2 * math.pi / 2
                m.add("pilares", "piedra", tronco((math.cos(a) * L * 0.3, base, math.sin(a) * W * 0.3), (math.cos(a) * L * 0.3, H, math.sin(a) * W * 0.3), 0.4, 0.3, 6))
        else:
            m.add("cella", "arenisca", caja((0, base + (H - base) * 0.35, 0), (L * 0.45, (H - base) * 0.7, W * 0.45)))
            m.add("tejado", "teja", prisma_tejado(-L * 0.28, L * 0.28, W * 0.55, base + (H - base) * 0.7, (H - base) * 0.3))
            for i in range(S(d, 3, 5)):
                z = -W * 0.25 + W * 0.5 * i / (S(d, 3, 5) - 1)
                m.add("columnas", "arenisca", tronco((L * 0.27, base, z), (L * 0.27, base + (H - base) * 0.7, z), 0.5, 0.45, 6))
            m.add("puerta", "tela_negra", caja((L * 0.23, base + 2.0, 0), (0.1, 4.0, 3.0)))
        return
    if nom in ("escombros", "cenizas"):
        k = S(d, 4, 8) if nom == "escombros" else S(d, 2, 6)
        for i in range(k):
            x, z = rng.uniform(-L * 0.35, L * 0.35), rng.uniform(-W * 0.35, W * 0.35)
            if nom == "escombros":
                m.add("piedras", rng.choice(("piedra", "piedra_oscura", "adobe")), roca((x, 0.0, z), rng.uniform(0.2, 0.45), H * rng.uniform(0.4, 0.9),
                                                                                       rng.uniform(0.2, 0.45), 5, 3, rng))
            else:
                m.add("tizones", "carbon", viga((x, 0.05, z), (x + rng.uniform(-0.5, 0.5), 0.08, z + rng.uniform(-0.5, 0.5)), 0.1))
        if nom == "cenizas":
            m.add("cenizas", "fieltro_gris", cupula((0, 0, 0), L / 2, H, W / 2, n, 2))
            m.add("brasas", "fuego", cupula((0, 0, 0), L * 0.12, H * 0.6, W * 0.12, 5, 2))
        return
    if nom == "refugio":
        m.add("lona", "piel_pardo", extruir([(L / 2, 0), (-L / 2, 0), (-L / 2, H), (-L / 2 + 0.1, H)], (0, 0, -W / 2), (1, 0, 0), (0, 1, 0), (0, 0, 1), W))
        m.add("palos", "madera", viga((-L / 2 + 0.05, 0, -W / 2), (-L / 2 + 0.05, H, -W / 2), 0.08))
        m.add("palos", "madera", viga((-L / 2 + 0.05, 0, W / 2), (-L / 2 + 0.05, H, W / 2), 0.08))
        return
    if nom in ("horno_bronce", "horno_acero"):
        R = min(W, L) / 2
        hh = H * (0.7 if nom == "horno_acero" else 0.85)
        m.add("horno", "adobe" if nom == "horno_bronce" else "piedra", tronco((0, 0, 0), (0, hh, 0), R, R * 0.55, n))
        m.add("boca", "fuego", caja((R * 0.8, hh * 0.18, 0), (R * 0.4, hh * 0.25, R * 0.5)))
        m.add("fuelle", "cuero", elipsoide((-R * 0.95, hh * 0.2, 0), 0.2, 0.15, 0.3, 6, 3))
        if nom == "horno_acero":
            m.add("chimenea", "piedra_oscura", tronco((0, hh, 0), (0, H, 0), R * 0.35, R * 0.3, n))
        else:
            m.add("crisol", "carbon", tronco((0, hh, 0), (0, H, 0), R * 0.5, R * 0.45, n))
        return
    if nom == "trinchera":
        for s in (-1, 1):
            m.add("parapetos", "adobe", extruir([(-W * 0.18, 0), (W * 0.18, 0), (W * 0.05, H), (-W * 0.05, H)], (-L / 2, 0, W * 0.32 * s), (0, 0, 1), (0, 1, 0), (1, 0, 0), L))
        m.add("fondo", "adobe", caja((0, 0.02, 0), (L, 0.04, W * 0.3)))
        m.add("estacas", "madera", viga((0, 0, W * 0.5), (L * 0.1, H, W * 0.5), 0.06))
        return
    if nom == "campamento_abandonado":
        b_campamento(m, it, d, rng, W, H, L)
        return
    b_caja(m, it, d, rng, W, H, L)


def b_campamento(m, it, d, rng, W, H, L):
    """Campamento abandonado: armazon de yurta caido, fogon apagado, un carro roto."""
    R = L * 0.25
    xc = -L * 0.15
    for k in range(S(d, 5, 8)):
        a = 2 * math.pi * k / S(d, 5, 8)
        p = (xc + math.cos(a) * R, 0, math.sin(a) * R)
        m.add("enrejado", "madera_oscura", viga(p, va(p, (rng.uniform(-0.3, 0.3), H * rng.uniform(0.35, 0.6), rng.uniform(-0.3, 0.3))), 0.16))
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.4
        m.add("varas", "madera_oscura", viga((xc + math.cos(a) * R, H * 0.45, math.sin(a) * R), (xc, H, 0), 0.14))
    m.add("fieltro", "fieltro_gris", placa([(xc - R * 0.8, 0.06, -R * 0.2), (xc + R * 0.7, 0.06, R * 0.5)], [R * 0.5, R * 0.35], 0.04, normal=(0, 1, 0)))
    for k in range(6):
        a = 2 * math.pi * k / 6
        m.add("fogon", "piedra", roca((L * 0.3 + math.cos(a) * 0.6, 0.05, math.sin(a) * 0.6), 0.15, 0.12, 0.15, 4, 3, rng))
    m.add("cenizas", "carbon", cupula((L * 0.3, 0, 0), 0.45, 0.1, 0.45, 6, 2))
    m.add("carro", "madera", caja((0, 0.4, -L * 0.35), (2.5, 0.25, 1.4)))
    m.add("rueda", "madera_oscura", tronco((0.6, 0.45, -L * 0.35 - 0.75), (0.6, 0.45, -L * 0.35 - 0.65), 0.45, 0.45, 8))
    m.add("caldero", "bronce", cupula((L * 0.3, 0.0, 0), 0.25, 0.3, 0.25, 6, 2))
    m.add("lindes", "piedra", caja_suelo(-L / 2 + 0.2, W / 2 - 0.2, 0.4, 0.2, 0.4))
    m.add("lindes", "piedra", caja_suelo(L / 2 - 0.2, -W / 2 + 0.2, 0.4, 0.2, 0.4))


# --- Vehiculos ------------------------------------------------------------------
def _casco(L, W, Hc, k, n_sec=6, y0=0.0, popa=0.75):
    """Casco de barco: secciones en U a lo largo de X, proa en +X."""
    anillos = []
    for i in range(k + 1):
        t = i / k
        x = -L / 2 + L * t
        f = math.sin(math.pi * (0.15 + 0.85 * t)) ** 0.6 if t < 1 else 0
        f = max(f, popa * (1 - t) ** 3) if t < 0.5 else f
        if i == k:
            anillos.append([(x, y0 + Hc * 1.15, 0)])
            continue
        w = W / 2 * f
        h = Hc * (1 + 0.15 * (2 * t - 1) ** 4)
        ring = [(x, y0 + h, w), (x, y0 + h * 0.5, w * 0.92), (x, y0 + h * 0.12, w * 0.5),
                (x, y0 + h * 0.12, -w * 0.5), (x, y0 + h * 0.5, -w * 0.92), (x, y0 + h, -w)]
        anillos.append(ring)
    return loft(anillos)


def b_vehiculo(m, it, d, rng, W, H, L):
    sub, nom = it["id"].split(".")[1], nombre_de(it)
    n = S(d, 6, 12)
    k = S(d, 4, 8)
    if sub == "agua":
        if nom == "balsa":
            q = S(d, 4, 7)
            r = W / q / 2
            for i in range(q):
                z = -W / 2 + r + 2 * r * i
                m.add("troncos", "corteza", tronco((-L / 2, r, z), (L / 2, r, z), r, r, 5))
            for x in (-L * 0.35, L * 0.35):
                m.add("amarres", "cuerda", viga((x, 2 * r, -W / 2), (x, 2 * r, W / 2), 0.08))
            m.add("palo", "madera_clara", viga((0, 2 * r, 0), (-L * 0.4, H, W * 0.2), 0.05))
            return
        if nom in ("bote_remos", "canoa_pieles"):
            Hc = H * 0.7
            m.add("casco", "madera" if nom == "bote_remos" else "cuero_claro", _casco(L, W, Hc, k))
            if nom == "bote_remos":
                m.add("bancos", "madera_clara", caja((0, Hc * 0.75, 0), (0.25, 0.05, W * 0.85)))
                for s in (-1, 1):
                    m.add("remos", "madera_clara", viga((0.0, Hc, W * 0.42 * s), (-L * 0.25, H * 0.05, W * 0.7 * s), 0.05))
            else:
                m.add("remo", "madera_clara", viga((-L * 0.1, Hc * 1.2, -W * 0.6), (L * 0.15, H, W * 0.5), 0.04))
                m.add("cuadernas", "madera_oscura", caja((0, Hc * 0.95, 0), (0.06, 0.05, W * 0.8)))
            return
        # velero, barcos: casco + mastiles + velas
        Hc = H * (0.18 if nom == "velero" else 0.22)
        m.add("casco", "madera", _casco(L, W, Hc, k))
        m.add("cubierta", "madera_clara", caja((-L * 0.05, Hc * 0.98, 0), (L * 0.75, 0.06, W * 0.82)))
        mastiles = 1 if nom == "velero" else (2 if nom == "barco_carga" else 2)
        for i in range(mastiles):
            x = 0.0 if mastiles == 1 else (-L * 0.15 + i * L * 0.3)
            m.add("mastiles", "madera", tronco((x, Hc, 0), (x, H, 0), 0.15 if nom != "velero" else 0.1, 0.08, 6))
            hv = (H - Hc) * 0.65
            m.add("velas", "tela_lino" if nom != "barco_guerra" else "tela_roja",
                  placa([(x - 0.15, Hc + (H - Hc) * 0.25, 0), (x - 0.25, Hc + (H - Hc) * 0.9, 0)], [W * 0.5, W * 0.42], 0.03, normal=(1, 0, 0)))
            m.add("vergas", "madera", viga((x, Hc + (H - Hc) * 0.9, -W * 0.5), (x, Hc + (H - Hc) * 0.9, W * 0.5), 0.12))
        if nom != "velero":
            m.add("castillo", "madera_oscura", caja((-L * 0.36, Hc + 1.0, 0), (L * 0.18, 2.0, W * 0.75)))
            m.add("timon", "madera_oscura", caja((-L * 0.5, Hc * 0.6, 0), (0.6, Hc * 1.0, 0.15)))
        if nom == "barco_guerra":
            for i in range(S(d, 3, 6)):
                x = -L * 0.25 + i * L * 0.5 / max(1, S(d, 3, 6) - 1)
                for s in (-1, 1):
                    m.add("escudos", "fieltro_rojo" if i % 2 else "lapislazuli",
                          mover(rotar(cupula((0, 0, 0), 0.4, 0.12, 0.4, 6, 2), "x", s * math.pi / 2), (x, Hc * 0.85, W / 2 * 0.95 * s)))
        return
    # Tierra
    if nom == "trineo":
        for s in (-1, 1):
            pts = [(-L / 2, 0.05, W * 0.4 * s), (L * 0.35, 0.05, W * 0.4 * s), (L / 2, 0.3, W * 0.4 * s)]
            m.add("patines", "madera_oscura", placa(pts, [0.05, 0.05, 0.05], 0.04, normal=(0, 0, 1)))
            for x in (-L * 0.3, 0.0, L * 0.25):
                m.add("puntales", "madera", viga((x, 0.08, W * 0.4 * s), (x, H * 0.55, W * 0.4 * s), 0.05))
        m.add("plataforma", "madera_clara", caja((-L * 0.05, H * 0.55, 0), (L * 0.75, 0.06, W * 0.9)))
        m.add("carga", "piel_pardo", caja_biselada((-L * 0.1, H * 0.8, 0), (L * 0.5, H * 0.45, W * 0.75)))
        m.add("tiro", "cuero", viga((L / 2, 0.3, 0), (L / 2 + 0.1, 0.4, 0), 0.04))
        return
    ruedas4 = nom in ("carruaje", "kibitka", "carreta_mercancias")
    rr = H * (0.25 if nom != "carreta_bueyes" else 0.32)
    xs = (-L * 0.3, L * 0.12) if ruedas4 else (-L * 0.2,)
    lc = L * (0.62 if nom != "carreta_mercancias" else 0.7)
    xc = -L * 0.1 if nom != "carreta_mercancias" else -L * 0.08
    yb = rr * 1.05
    m.add("caja", "madera", caja((xc, yb + 0.15, 0), (lc, 0.3, W * 0.75)))
    for s in (-1, 1):
        m.add("barandas", "madera_clara", caja((xc, yb + 0.45, W * 0.37 * s), (lc, 0.3, 0.06)))
    for x in xs:
        for s, lado in ((-1, "izq"), (1, "der")):
            m.add(f"rueda_{lado}", "madera_oscura", tronco((x, rr, (W / 2 - 0.12) * s), (x, rr, W / 2 * s), rr, rr, n))
            m.add("bujes", "hierro", tronco((x, rr, W / 2 * s), (x, rr, (W / 2 + 0.05) * s), rr * 0.18, rr * 0.12, S(d, 4, 6)))
        m.add("ejes", "madera", tronco((x, rr, -W / 2), (x, rr, W / 2), 0.05, 0.05, 5))
    m.add("lanza", "madera", viga((xc + lc / 2, yb, 0), (L / 2, yb * 0.7, 0), 0.1))
    m.add("yugo", "madera_oscura", viga((L / 2 - 0.1, yb * 0.7, -W * 0.45), (L / 2 - 0.1, yb * 0.7, W * 0.45), 0.08))
    if nom == "kibitka":  # yurta sobre el carro
        R = min(W * 0.45, lc / 2)
        y0 = yb + 0.3
        m.add("pared", "fieltro_blanco", tronco((xc, y0, 0), (xc, y0 + (H - y0) * 0.5, 0), R, R, n))
        m.add("techo", "fieltro_crema", tronco((xc, y0 + (H - y0) * 0.5, 0), (xc, H, 0), R * 1.05, R * 0.2, n))
        m.add("puerta", "madera_pintada", caja((xc + R, y0 + (H - y0) * 0.25, 0), (0.08, (H - y0) * 0.45, 0.7)))
    elif nom == "carruaje":
        m.add("cabina", "madera_pintada", caja((xc, yb + 0.3 + (H - yb - 0.3) * 0.4, 0), (lc * 0.75, (H - yb - 0.3) * 0.8, W * 0.72)))
        m.add("techo", "tela_negra", cupula((xc, yb + 0.3 + (H - yb - 0.3) * 0.8, 0), lc * 0.4, (H - yb - 0.3) * 0.2, W * 0.38, n, 2))
        m.add("pescante", "madera", caja((xc + lc * 0.45, yb + 0.6, 0), (0.4, 0.1, W * 0.6)))
        m.add("adornos", "oro", caja((xc + lc * 0.38, yb + 1.0, 0), (0.04, 0.3, 0.4)))
    elif nom == "carreta_mercancias":
        for i, (x, z, cel) in enumerate(((-0.3, -0.25, "madera_clara"), (0.35, 0.2, "fieltro_crema"), (-0.25, 0.25, "cuero_claro"))):
            m.add("carga", cel, (caja_biselada if d >= 2 else caja)((xc + x * lc, yb + 0.3 + 0.3, z * W), (lc * 0.3, 0.6, W * 0.3)))
    else:  # carreta de bueyes
        m.add("carga", "hierba_seca", cupula((xc, yb + 0.3, 0), lc * 0.45, H - yb - 0.3, W * 0.38, n, 3))


# --- Asedio ---------------------------------------------------------------------
def _ruedas(m, L, W, r, xs, n, cel="madera_oscura"):
    for x in xs:
        for s, lado in ((-1, "izq"), (1, "der")):
            m.add(f"ruedas_{lado}", cel, tronco((x, r, (W / 2 - 0.15) * s), (x, r, W / 2 * s), r, r, n))


def b_asedio(m, it, d, rng, W, H, L):
    nom = nombre_de(it)
    n = S(d, 6, 10)
    if nom == "mantelete":
        m.add("tablero", "madera", caja((0, H * 0.5, 0), (0.12, H, W)))
        for i in range(S(d, 3, 5)):
            z = -W / 2 + W * (i + 0.5) / S(d, 3, 5)
            m.add("tablones", "madera_oscura", caja((0.07, H * 0.5, z), (0.03, H, 0.06)))
        m.add("tronera", "carbon", caja((0.07, H * 0.75, 0), (0.04, 0.15, 0.4)))
        for s in (-1, 1):
            m.add("puntales", "madera", viga((0, H * 0.7, W * 0.35 * s), (-L * 2, 0, W * 0.35 * s), 0.08))
        return
    r = 0.4 if nom != "torre_asedio" else 0.6
    if nom == "ariete":
        _ruedas(m, L, W, r, (-L * 0.3, L * 0.3), n)
        for x in (-L * 0.4, L * 0.4):
            for s in (-1, 1):
                m.add("bastidor", "madera", viga((x, r, W * 0.42 * s), (x, H * 0.75, W * 0.25 * s), 0.18))
        m.add("bastidor", "madera", viga((-L * 0.45, r, -W * 0.42), (L * 0.45, r, -W * 0.42), 0.18))
        m.add("bastidor", "madera", viga((-L * 0.45, r, W * 0.42), (L * 0.45, r, W * 0.42), 0.18))
        m.add("techo", "piel_pardo", prisma_tejado(-L * 0.45, L * 0.45, W * 0.9, H * 0.7, H * 0.3))
        m.add("ariete", "corteza", tronco((-L * 0.45, H * 0.45, 0), (L * 0.42, H * 0.45, 0), 0.25, 0.25, n))
        m.add("cabeza", "hierro", tronco((L * 0.42, H * 0.45, 0), (L * 0.5, H * 0.45, 0), 0.3, 0.2, n))
        for x in (-L * 0.2, L * 0.2):
            m.add("cadenas", "hierro", viga((x, H * 0.45, 0), (x, H * 0.7, 0), 0.05))
        return
    if nom == "torre_asedio":
        _ruedas(m, L, W, r, (-L * 0.3, L * 0.3), n)
        m.add("torre", "madera", loft([[(x * L / 2, r, z * W / 2) for x, z in ((1, 1), (-1, 1), (-1, -1), (1, -1))],
                                       [(x * L / 2 * 0.75, H * 0.92, z * W / 2 * 0.75) for x, z in ((1, 1), (-1, 1), (-1, -1), (1, -1))]]))
        m.add("pieles", "piel_pardo", caja((L / 2 * 0.88, H * 0.5, 0), (0.06, H * 0.6, W * 0.7)))
        m.add("almenas", "madera_oscura", caja((0, H * 0.96, 0), (L * 0.8, H * 0.08, W * 0.8)))
        m.add("puente", "madera_clara", mover(rotar(caja((0, 0, 0), (0.15, H * 0.2, W * 0.6)), "z", 0.25), (L / 2 * 0.85, H * 0.78, 0)))
        return
    if nom == "trebuchet":
        m.add("base", "madera", caja((0, 0.3, W * 0.4), (L * 0.9, 0.3, 0.3)))
        m.add("base", "madera", caja((0, 0.3, -W * 0.4), (L * 0.9, 0.3, 0.3)))
        piv = (0, H * 0.55, 0)
        for s in (-1, 1):
            for x in (-L * 0.3, L * 0.3):
                m.add("caballetes", "madera", viga((x, 0.3, W * 0.4 * s), (0, H * 0.55, W * 0.35 * s), 0.25))
        m.add("eje", "hierro", tronco((0, H * 0.55, -W * 0.4), (0, H * 0.55, W * 0.4), 0.1, 0.1, 6))
        brazo_fin = (-L * 0.45, H, 0)
        m.add("brazo", "madera_oscura", viga(va(piv, (L * 0.2, -H * 0.15, 0)), brazo_fin, 0.25))
        m.add("contrapeso", "madera_oscura", caja(va(piv, (L * 0.22, -H * 0.3, 0)), (1.5, 1.5, 1.8)))
        m.add("contrapeso", "piedra", cupula(va(piv, (L * 0.22, -H * 0.22, 0)), 0.65, 0.4, 0.8, 6, 2))
        m.add("honda", "cuerda", viga(brazo_fin, (-L * 0.45, H * 0.6, 0), 0.05))
        return
    if nom == "catapulta":
        _ruedas(m, L, W, r, (-L * 0.3, L * 0.3), n)
        m.add("bastidor", "madera", caja((0, r + 0.1, W * 0.35), (L * 0.95, 0.2, 0.2)))
        m.add("bastidor", "madera", caja((0, r + 0.1, -W * 0.35), (L * 0.95, 0.2, 0.2)))
        m.add("bastidor", "madera", caja((L * 0.2, H * 0.55, 0), (0.25, 0.25, W * 0.8)))
        for s in (-1, 1):
            m.add("montantes", "madera", viga((L * 0.2, r, W * 0.35 * s), (L * 0.2, H * 0.6, W * 0.35 * s), 0.2))
        m.add("torsion", "cuerda", tronco((-L * 0.25, r + 0.2, -W * 0.3), (-L * 0.25, r + 0.2, W * 0.3), 0.18, 0.18, 6))
        m.add("brazo", "madera_oscura", viga((-L * 0.25, r + 0.2, 0), (L * 0.05, H * 0.95, 0), 0.15))
        m.add("cuchara", "madera_oscura", cupula((L * 0.07, H * 0.92, 0), 0.25, 0.12, 0.25, 6, 2))
        return
    b_caja(m, it, d, rng, W, H, L)


# --- Mapa -----------------------------------------------------------------------
def _arbol_conifera(m, W, H, d, alerce=False):
    n = S(d, 5, 8)
    m.add("tronco", "corteza", tronco((0, 0, 0), (0, H * 0.35, 0), W * 0.07, W * 0.05, n))
    k = S(d, 2, 4)
    for i in range(k):
        y0 = H * (0.2 + 0.7 * i / k)
        r = W / 2 * (1 - i / k * 0.65) * (0.75 if alerce else 1.0)
        m.add("copa", "hojas" if not alerce else "hierba", tronco((0, y0, 0), (0, y0 + H * (0.8 / k + 0.12), 0), r, 0.0, n))


def b_mapa(m, it, d, rng, W, H, L):
    sub, nom = it["id"].split(".")[1], nombre_de(it)
    n = S(d, 5, 10)
    if sub == "roca":
        if nom == "grande":
            m.add("roca", "piedra", roca((0, 0, 0), L * 0.4, H, W * 0.4, n, S(d, 3, 5), rng))
            m.add("roca", "piedra", roca((-L * 0.25, 0, W * 0.25), L * 0.25, H * 0.55, W * 0.25, S(d, 4, 6), 3, rng))
        else:
            m.add("roca", "piedra" if nom != "mediana" else "piedra_oscura", roca((0, 0, 0), L / 2, H, W / 2, n, S(d, 3, 4), rng))
        return
    if sub == "relieve":
        if nom == "acantilado":
            k = S(d, 3, 6)
            for i in range(k):
                z = -W / 2 + W * (i + 0.5) / k
                h = H * rng.uniform(0.7, 1.0)
                m.add("roca", rng.choice(("piedra", "piedra_oscura")), loft([
                    [(x, 0, zz) for x, zz in ((L / 2, z + W / k * 0.6), (-L / 2, z + W / k * 0.6), (-L / 2, z - W / k * 0.6), (L / 2, z - W / k * 0.6))],
                    [(x, h, zz) for x, zz in ((L / 2 * rng.uniform(0.2, 0.7), z + W / k * 0.45), (-L / 2, z + W / k * 0.5),
                                              (-L / 2, z - W / k * 0.5), (L / 2 * rng.uniform(0.2, 0.7), z - W / k * 0.45))]]))
            m.add("nieve", "nieve", caja((-L * 0.25, H * 0.98, 0), (L * 0.5, H * 0.04, W * 0.8)))
            return
        if nom == "duna":
            k = S(d, 3, 6)
            anillos = []
            for i in range(k + 1):
                z = -W / 2 + W * i / k
                f = math.sin(math.pi * i / k) * 0.85 + 0.15
                anillos.append([(L / 2, 0, z), (L * 0.1, H * f, z), (-L / 2, 0, z)])
            m.add("arena", "arenisca", loft(anillos))
            return
        if nom == "grieta_glaciar":
            for s in (-1, 1):
                m.add("hielo", "hielo", extruir([(-L / 2, 0), (L / 2, 0), (L / 2 * 0.7, H), (-L / 2 * 0.9, H * 0.95)], (0, 0, s * W * 0.12), (1, 0, 0), (0, 1, 0), (0, 0, s), W * 0.38))
            m.add("nieve", "nieve", caja((0, H, W * 0.31), (L, 0.15, W * 0.38)))
            m.add("nieve", "nieve", caja((0, H, -W * 0.31), (L, 0.15, W * 0.38)))
            return
    if sub == "agua":
        if nom == "oasis":
            m.add("orilla", "arenisca", tronco((0, 0, 0), (0, H * 0.3, 0), L / 2, L / 2 * 0.85, n, 0, W / 2, W / 2 * 0.85, u=(1, 0, 0), v=(0, 0, 1)))
            m.add("agua", "agua", tronco((0, H * 0.3, 0), (0, H * 0.32, 0), L / 2 * 0.6, L / 2 * 0.6, n, 0, W / 2 * 0.6, W / 2 * 0.6, u=(1, 0, 0), v=(0, 0, 1)))
            for i, (x, z) in enumerate(((-L * 0.35, W * 0.2), (L * 0.3, -W * 0.3), (L * 0.1, W * 0.38))):
                m.add("palmeras", "corteza", tronco((x, 0, z), (x + 0.4, H, z), 0.15, 0.1, 5))
                m.add("hojas", "hierba", tronco((x + 0.4, H * 0.8, z), (x + 0.4, H, z), 1.2, 0.0, 5))
            m.add("hierba", "hierba", tronco((-L * 0.15, H * 0.3, -W * 0.2), (-L * 0.15, H * 0.6, -W * 0.2), 0.6, 0.0, 5))
            return
        if nom == "rio":
            m.add("agua", "agua", caja((0, H * 0.4, 0), (L, H * 0.1, W * 0.6)))
            for s in (-1, 1):
                m.add("orillas", "hierba", extruir([(-W * 0.12, 0), (W * 0.12, 0), (W * 0.08, H), (-W * 0.02, H)], (-L / 2, 0, W * 0.38 * s), (0, 0, s), (0, 1, 0), (1, 0, 0), L))
            m.add("lecho", "arenisca", caja((0, H * 0.15, 0), (L, H * 0.3, W * 0.6)))
            return
        if nom == "tempano":
            m.add("hielo", "hielo", roca((0, 0, 0), L / 2, H, W / 2, n, 3, rng, j=0.3))
            m.add("nieve", "nieve", cupula((0, H * 0.6, 0), L * 0.3, H * 0.4, W * 0.3, n, 2))
            return
    if sub == "vegetacion":
        if nom in ("pino", "alerce"):
            _arbol_conifera(m, min(W, L), H, d, alerce=(nom == "alerce"))
            return
        if nom == "abedul":
            m.add("tronco", "abedul", tronco((0, 0, 0), (0, H * 0.6, 0), 0.14, 0.09, S(d, 5, 7)))
            m.add("ramas", "abedul", viga((0, H * 0.45, 0), (0.6, H * 0.65, 0.3), 0.07))
            m.add("copa", "hierba", roca((0, H * 0.45, 0), W * 0.4, H * 0.55, W * 0.38, n, S(d, 3, 5), rng, 0.15))
            return
        if nom == "palmera":
            k = S(d, 2, 5)
            pts = [(0.5 * (i / k) ** 2, H * 0.92 * i / k, 0) for i in range(k + 1)]
            m.add("tronco", "corteza", loft([anillo(p, (0, 1, 0), 0.2 - 0.06 * i / k, 0.2 - 0.06 * i / k, 5, 0, (1, 0, 0), (0, 0, 1))
                                             for i, p in enumerate(pts)]))
            top = pts[-1]
            for j in range(S(d, 4, 6)):
                ang = 2 * math.pi * j / S(d, 4, 6)
                dz, dx = math.sin(ang), math.cos(ang)
                hoja = [top, va(top, (dx * W * 0.25, H * 0.07, dz * W * 0.25)), va(top, (dx * W * 0.48, -H * 0.12, dz * W * 0.48))]
                if d < 2:
                    hoja = [hoja[0], hoja[2]]
                m.add("hojas", "hierba", placa(hoja, [0.15, 0.3, 0.05][:len(hoja)] if d >= 2 else [0.25, 0.08], 0.02, normal=(0, 1, 0)))
            m.add("datiles", "madera_pintada", elipsoide(va(top, (0, -0.25, 0)), 0.2, 0.2, 0.2, 4, 3))
            return
        if nom == "saxaul":
            for j in range(S(d, 2, 5)):
                a = 2 * math.pi * j / S(d, 2, 5)
                p = (math.cos(a) * W * 0.35, H * 0.7, math.sin(a) * W * 0.35)
                m.add("ramas", "corteza", viga((0, 0, 0), p, 0.1))
                m.add("follaje", "hierba_seca", roca(p, 0.25, 0.18, 0.25, 4, 3, rng))
            return
        if nom in ("hierba_alta", "mata_hierbas", "arbusto", "liquen"):
            k = {"hierba_alta": S(d, 4, 10), "mata_hierbas": S(d, 4, 8), "arbusto": S(d, 2, 3), "liquen": S(d, 2, 3)}[nom]
            for j in range(k):
                x, z = rng.uniform(-L * 0.35, L * 0.35), rng.uniform(-W * 0.35, W * 0.35)
                if nom == "hierba_alta":
                    m.add("hierba", rng.choice(("hierba", "hierba_seca")), tronco((x, 0, z), (x + rng.uniform(-0.2, 0.2), H * rng.uniform(0.7, 1.0), z), 0.12, 0.0, 3, tapas=False))
                elif nom == "mata_hierbas":
                    m.add("hojas", "hierba", tronco((x * 0.5, 0, z * 0.5), (x, H * rng.uniform(0.6, 1.0), z), 0.06, 0.0, 3, tapas=False))
                    if j % 3 == 0:
                        m.add("flores", "fieltro_blanco", octaedro((x, H * 0.9, z), 0.06))
                elif nom == "arbusto":
                    m.add("follaje", "hierba", roca((x * 0.5, 0, z * 0.5), L * 0.35, H, W * 0.35, S(d, 4, 5), 3, rng))
                else:
                    m.add("liquen", rng.choice(("hierba", "hierba_seca")), cupula((x, 0, z), L * 0.3, H, W * 0.3, S(d, 4, 5), 1))
            return
    if sub == "hito":
        if nom == "ovoo":
            m.add("piedras", "piedra", roca((0, 0, 0), L / 2, H * 0.7, W / 2, n, 3, rng, 0.15))
            m.add("poste", "madera", tronco((0, H * 0.5, 0), (0, H, 0), 0.06, 0.04, 5))
            m.add("jadag", "fieltro_azul", caja((0.1, H * 0.8, 0.3), (0.02, 0.4, 0.6)))
            m.add("jadag", "fieltro_azul", caja((0.1, H * 0.85, -0.3), (0.02, 0.3, 0.5)))
            return
        if nom in ("kurgan", "tumba_helada"):
            m.add("tumulo", "hierba" if nom == "kurgan" else "nieve", cupula((0, 0, 0), L / 2, H, W / 2, n, S(d, 2, 4)))
            m.add("anillo", "piedra", toro((0, 0.1, 0), L / 2 * 0.98, 0.25, n, 3, rz=W / 2 * 0.98))
            if nom == "tumba_helada":
                m.add("hielo", "hielo", roca((L * 0.2, H * 0.5, 0), L * 0.18, H * 0.5, W * 0.15, 5, 3, rng))
                m.add("camara", "madera_oscura", caja((L * 0.38, 0.6, 0), (1.0, 1.2, 1.4)))
                m.add("piedra_ciervo", "piedra", caja((L * 0.45, H * 0.4, W * 0.25), (0.2, H * 0.8, 0.5)))
            return
        if nom == "campamento_abandonado":
            b_campamento(m, it, d, rng, W, H, L)
            return
        if nom in ("piedra_ciervo", "balbal"):
            if nom == "balbal":
                m.add("cuerpo", "piedra", caja((0, H * 0.38, 0), (L * 0.9, H * 0.76, W * 0.9)))
                m.add("cabeza", "piedra", elipsoide((0, H * 0.85, 0), L * 0.42, H * 0.15, W * 0.42, S(d, 5, 7), 3))
                m.add("rasgos", "piedra_oscura", caja((L * 0.4, H * 0.86, 0), (0.04, 0.05, W * 0.4)))
                m.add("rasgos", "piedra_oscura", caja((L * 0.44, H * 0.5, 0.1), (0.04, 0.3, 0.08)))
            else:
                m.add("estela", "piedra", loft([[(x, 0, z) for x, z in ((L / 2, W / 2), (-L / 2, W / 2), (-L / 2, -W / 2), (L / 2, -W / 2))],
                                                [(x, H * 0.92, z) for x, z in ((L / 2 * 0.85, W / 2 * 0.9), (-L / 2 * 0.85, W / 2 * 0.9), (-L / 2 * 0.85, -W / 2 * 0.9), (L / 2 * 0.85, -W / 2 * 0.9))],
                                                [(x, H, z) for x, z in ((L / 2 * 0.6, W / 2 * 0.5), (-L / 2 * 0.6, W / 2 * 0.5), (-L / 2 * 0.6, -W / 2 * 0.5), (L / 2 * 0.6, -W / 2 * 0.5))]]))
                for k in range(S(d, 2, 4)):
                    m.add("ciervos", "piedra_oscura", rotar(caja((L / 2, H * (0.3 + 0.15 * k), 0), (0.03, 0.1, W * 0.7)), "x", 0.35, (L / 2, H * (0.3 + 0.15 * k), 0)))
            return
    b_caja(m, it, d, rng, W, H, L)


# --- Utileria -------------------------------------------------------------------
def b_utileria(m, it, d, rng, W, H, L):
    sub, nom = it["id"].split(".")[1], nombre_de(it)
    n = S(d, 4, 10)
    R = min(W, L) / 2
    if nom == "caldero":
        m.add("olla", "bronce", loft([anillo((0, y, 0), (0, 1, 0), r, r, n) for y, r in ((0.1, R * 0.5), (0.2, R * 0.9), (H * 0.8, R), (H, R * 0.9))]))
        for k in range(3):
            a = 2 * math.pi * k / 3
            m.add("patas", "bronce", tronco((math.cos(a) * R * 0.5, 0.12, math.sin(a) * R * 0.5), (math.cos(a) * R * 0.6, 0, math.sin(a) * R * 0.6), 0.03, 0.02, 3))
        if d >= 2:
            for s in (-1, 1):
                m.add("asas", "bronce", toro((0, H, R * 0.9 * s), 0.06, 0.015, 4, 3, eje="z"))
        return
    if nom in ("odre", "airag", "leche", "agua"):
        cel = "cuero_claro" if nom != "agua" else "cuero"
        m.add("bolsa", cel, elipsoide((0, H * 0.4, 0), L / 2, H * 0.4, W / 2, n, 3 if d < 3 else 4))
        m.add("cuello", "cuero_oscuro", tronco((0, H * 0.75, 0), (0, H, 0), min(L, W) * 0.15, min(L, W) * 0.12, 4))
        if d >= 2:
            m.add("tapon", "madera", tronco((0, H * 0.96, 0), (0, H * 1.02, 0), min(L, W) * 0.13, min(L, W) * 0.13, 4))
        return
    if nom == "cofre":
        m.add("caja", "madera", caja((0, H * 0.35, 0), (L, H * 0.7, W)))
        m.add("tapa", "madera", extruir([(-L / 2, 0)] + [(-L / 2 + L * (1 - math.cos(math.pi * i / 4)) / 2, H * 0.3 * math.sin(math.pi * i / 4)) for i in range(1, 4)] + [(L / 2, 0)],
                                         (0, H * 0.7, -W / 2), (1, 0, 0), (0, 1, 0), (0, 0, 1), W))
        for x in (-L * 0.3, L * 0.3):
            m.add("herrajes", "hierro", caja((x, H * 0.5, 0), (0.05, H * 1.0, W * 1.02)))
        m.add("cerradura", "oro", caja((L / 2, H * 0.62, 0), (0.03, 0.08, 0.08)))
        return
    if nom == "barril_polvora":
        m.add("barril", "madera", loft([anillo((0, y, 0), (0, 1, 0), r, r, n) for y, r in ((0, R * 0.82), (H * 0.5, R), (H, R * 0.82))]))
        for y in (H * 0.15, H * 0.85):
            m.add("aros", "hierro", tronco((0, y - 0.02, 0), (0, y + 0.02, 0), R * 0.9, R * 0.9, n, tapas=False))
        m.add("marca", "pelaje_negro", caja((R * 0.95, H * 0.5, 0), (0.02, 0.12, 0.12)))
        return
    if nom in ("saco_grano",):
        m.add("saco", "fieltro_crema", loft([anillo((0, y, 0), (0, 1, 0), L / 2 * f, W / 2 * f, max(5, n), 0, (1, 0, 0), (0, 0, 1)) for y, f in ((0, 0.8), (H * 0.35, 1.0), (H * 0.7, 0.75), (H * 0.82, 0.25))] + [[(0, H, 0)]]))
        m.add("atadura", "cuerda", tronco((0, H * 0.78, 0), (0, H * 0.84, 0), L / 2 * 0.32, L / 2 * 0.3, 4))
        return
    if nom == "antorcha":
        m.add("mango", "madera", tronco((0, 0, 0), (0, H * 0.7, 0), 0.02, 0.025, S(d, 4, 6)))
        m.add("cabeza", "tela_lino", tronco((0, H * 0.6, 0), (0, H * 0.78, 0), 0.04, 0.045, S(d, 4, 6)))
        m.add("llama", "fuego", tronco((0, H * 0.78, 0), (0, H, 0), 0.045, 0.0, S(d, 4, 6)))
        m.agarre = (0, H * 0.3, 0)
        return
    if nom == "cadenas":
        # Cadena enrollada: eslabones en corro, alternando planos y de canto.
        k = S(d, 6, 9)
        for i in range(k):
            a = 2 * math.pi * i / k
            c = (math.cos(a) * R * 0.75, 0.03 if i % 2 else 0.05, math.sin(a) * R * 0.75)
            largo = 2 * math.pi * R * 0.75 / k * 1.1
            tam = (largo, 0.02, 0.05) if i % 2 else (largo, 0.05, 0.015)
            m.add("eslabones", "hierro", rotar(caja(c, tam), "y", -a - math.pi / 2, c))
        return
    if nom == "yunque":
        m.add("pie", "hierro", loft([[(x * L * 0.3, 0, z * W * 0.4) for x, z in ((1, 1), (-1, 1), (-1, -1), (1, -1))],
                                     [(x * L * 0.18, H * 0.6, z * W * 0.25) for x, z in ((1, 1), (-1, 1), (-1, -1), (1, -1))]]))
        m.add("mesa", "hierro", caja((-L * 0.05, H * 0.8, 0), (L * 0.7, H * 0.4, W * 0.8)))
        m.add("cuerno", "hierro", tronco((L * 0.3, H * 0.85, 0), (L / 2, H * 0.85, 0), H * 0.18, 0.0, S(d, 4, 6)))
        return
    if nom == "telar":
        for s in (-1, 1):
            m.add("bastidor", "madera", viga((L * 0.4, 0, W * 0.45 * s), (L * 0.2, H, W * 0.45 * s), 0.06))
            m.add("bastidor", "madera", viga((-L * 0.4, 0, W * 0.45 * s), (L * 0.2, H, W * 0.45 * s), 0.06))
        m.add("enjulio", "madera_clara", tronco((L * 0.2, H * 0.95, -W / 2), (L * 0.2, H * 0.95, W / 2), 0.04, 0.04, 5))
        m.add("tejido", "fieltro_rojo", caja((L * 0.3, H * 0.55, 0), (0.02, H * 0.7, W * 0.8)))
        return
    if nom == "tamboril_chaman":
        m.add("parche", "cuero_claro", tronco((0, 0, 0), (0, H, 0), R, R, n))
        m.add("aro", "madera", tronco((0, 0, 0), (0, H * 0.4, 0), R * 1.03, R * 1.03, n))
        m.add("dibujo", "cornalina", caja((0, H + 0.002, 0), (R * 0.6, 0.004, R * 0.15)))
        return
    if sub == "consumible":
        if nom in ("carne_seca", "carne_fresca"):
            cel = "carne" if nom == "carne_fresca" else "cuero"
            m.add("carne", cel, roca((0, 0, 0), L / 2, H, W / 2, S(d, 4, 6), 3, rng, 0.15))
            if nom == "carne_fresca":
                m.add("hueso", "hueso", tronco((-L / 2, H * 0.3, 0), (L / 2, H * 0.4, 0), 0.015, 0.015, 4))
            return
        if nom == "queso_seco":
            for i in range(3):
                m.add("quesos", "hueso", caja((-L * 0.25 + i * L * 0.25, H * 0.3, rng.uniform(-0.03, 0.03)), (L * 0.22, H * 0.6, W * 0.5)))
            if d >= 1:
                m.add("quesos", "hueso", caja((0, H * 0.8, 0), (L * 0.22, H * 0.4, W * 0.5)))
            return
        if nom == "hierbas":
            for i in range(S(d, 3, 6)):
                z = -W * 0.3 + W * 0.6 * i / max(1, S(d, 3, 6) - 1)
                m.add("tallos", "hierba", viga((-L / 2, H * 0.3, z * 0.3), (L / 2, H * 0.5, z), 0.02))
            if d >= 1:
                m.add("atadura", "cuerda", tronco((-L * 0.3, H * 0.35, -W * 0.15), (-L * 0.3, H * 0.35, W * 0.15), 0.04, 0.04, 4))
            return
        if nom == "pan":
            m.add("pan", "arenisca", cupula((0, 0, 0), L / 2, H, W / 2, S(d, 5, 8), 2))
            return
        if nom in ("miel", "unguento"):
            m.add("tarro", "adobe" if nom == "miel" else "hueso", loft([anillo((0, y, 0), (0, 1, 0), r, r, S(d, 4, 8)) for y, r in ((0, R * 0.7), (H * 0.5, R), (H * 0.8, R * 0.7))]))
            if d >= 1:
                m.add("tapa", "tela_lino" if nom == "miel" else "madera", tronco((0, H * 0.8, 0), (0, H, 0), R * 0.75, R * 0.6, S(d, 4, 8)))
            return
    if sub == "herramienta":
        if nom == "pala":
            m.add("mango", "madera_clara", tronco((0, H * 0.3, 0), (0, H, 0), 0.018, 0.018, S(d, 4, 6)))
            m.add("puño", "madera", caja((0, H - 0.02, 0), (0.03, 0.04, W * 0.5)))
            m.add("hoja", "hierro", extruir([(-W / 2, H * 0.3), (-W / 2, H * 0.08), (0, 0), (W / 2, H * 0.08), (W / 2, H * 0.3)],
                                             (-0.008, 0, 0), (0, 0, 1), (0, 1, 0), (1, 0, 0), 0.016))
            m.agarre = (0, H * 0.75, 0)
            return
        if nom == "gancho_trepa":
            m.add("vastago", "hierro", tronco((0, H * 0.3, 0), (0, H, 0), 0.015, 0.015, 4))
            for k in range(3):
                a = 2 * math.pi * k / 3
                dx, dz = math.cos(a), math.sin(a)
                gancho = [(0, H * 0.35, 0), (dx * W * 0.35, H * 0.12, dz * W * 0.35), (dx * W * 0.5, H * 0.4, dz * W * 0.5)]
                m.add("garfios", "hierro", cinta(gancho, [0.015, 0.015, 0.0], 0.012, normal=vn((-dz, 0, dx))))
            m.add("anilla", "hierro", toro((0, H, 0), 0.025, 0.008, 4, 3, eje="z"))
            if d >= 2:
                m.add("cuerda", "cuerda", toro((0, 0.03, 0), W * 0.3, 0.015, S(d, 5, 8), 3))
            m.agarre = (0, H * 0.7, 0)
            return
    if nom in ("lena", "troncos"):
        k = 3 if nom == "troncos" else S(d, 2, 5)
        r = min(W / (k + 1), H / 2) * 0.55 if nom == "lena" else min(W, H) / 4
        pos = [(0, r, -r * 1.05), (0, r, r * 1.05), (0, r * 2.75, 0)] if nom == "troncos" else \
            [(0, r, -W / 2 + r + i * 2 * r * 1.02) for i in range(k)] + [(0, r * 2.7, 0)]
        for p in pos:
            m.add("troncos", "corteza", tronco(va(p, (-L / 2, 0, 0)), va(p, (L / 2, 0, 0)), r, r, S(d, 4, 6)))
        if nom == "lena" and d >= 2:
            m.add("atadura", "cuerda", tronco((0, H * 0.5, 0), (0.05, H * 0.5, 0), W * 0.55, W * 0.55, 6, u=(0, 1, 0), v=(0, 0, 1)))
        return
    if sub == "material":
        cel = {"piedra": "piedra", "barro": "adobe", "pieles": "piel_pardo", "cuerda": "cuerda", "carbon": "carbon", "cobre": "cobre",
               "estano": "plata", "hierro": "hierro_oxidado", "plumas": "plumas", "hueso": "hueso", "tendones": "cuero_claro",
               "pedernal": "piedra_oscura"}.get(nom, "piedra")
        if nom == "pieles":
            for i in range(3):
                m.add("pieles", ("piel_pardo", "piel_blanca", "cuero_claro")[i], (caja_biselada if d >= 3 else caja)((rng.uniform(-0.03, 0.03), H * (0.17 + 0.33 * i), 0), (L * (1 - 0.1 * i), H * 0.3, W * (1 - 0.1 * i))))
            return
        if nom == "cuerda":
            for i in range(S(d, 1, 3)):
                m.add("rollo", "cuerda", toro((0, 0.03 + i * 0.04, 0), R * (0.75 - i * 0.05), 0.02, max(5, n), 3))
            return
        if nom in ("plumas", "tendones"):
            for i in range(S(d, 1, 5)):
                z = -W * 0.35 + W * 0.7 * i / max(1, S(d, 1, 5) - 1) if S(d, 1, 5) > 1 else 0.0
                m.add("haz", cel, placa([(-L / 2, H * 0.3, z * 0.3), (L / 2, H * 0.5, z)], [0.02, 0.04 if nom == "plumas" else 0.01], 0.004, normal=(0, 1, 0)))
            return
        if nom == "hueso":
            for i in range(S(d, 1, 3)):
                a = i * 1.1
                m.add("huesos", "hueso", rotar(tronco((-L * 0.4, 0.02 + 0.03 * i, 0), (L * 0.4, 0.02 + 0.03 * i, 0), 0.02, 0.02, 4), "y", a))
                for xe in (-L * 0.4, L * 0.4):
                    m.add("huesos", "hueso", rotar(octaedro((xe, 0.03 + 0.03 * i, 0), 0.035), "y", a))
            return
        if nom in ("cobre", "estano", "hierro"):
            for i, (x, z) in enumerate(((-L * 0.2, -W * 0.15), (L * 0.2, W * 0.1), (0, 0))[:S(d, 2, 3)]):
                m.add("lingotes", cel, loft([[(x + a * 0.08, H * 0.5 * (i // 2), z + b * 0.04) for a, b in ((1, 1), (-1, 1), (-1, -1), (1, -1))],
                                             [(x + a * 0.06, H * 0.5 * (i // 2) + H * 0.45, z + b * 0.03) for a, b in ((1, 1), (-1, 1), (-1, -1), (1, -1))]]))
            return
        k = S(d, 1, 4)
        for i in range(k):
            x, z = (rng.uniform(-L * 0.2, L * 0.2), rng.uniform(-W * 0.2, W * 0.2)) if k > 1 else (0.0, 0.0)
            m.add("monton", cel, roca((x, 0, z), L * 0.3, H * rng.uniform(0.6, 1.0), W * 0.3, S(d, 4, 6), 3, rng))
        return
    b_caja(m, it, d, rng, W, H, L)


def b_caja(m, it, d, rng, W, H, L):
    m.add("bloque", "madera", caja_biselada((0, H / 2, 0), (L, H, W)) if d >= 1 else caja((0, H / 2, 0), (L, H, W)))


def elegir(it):
    cat, sub, nom = it["id"].split(".")
    if cat == "personaje":
        return "humanoide", b_humanoide
    if cat == "animal":
        if sub == "ave":
            return "ave", b_ave
        if sub == "insecto":
            return ("colmena", b_colmena) if nom in ("colmena", "avispero") else ("insecto", b_insecto)
        if nom == "peces":
            return "pez", b_pez
        if nom == "vibora":
            return "serpiente", b_serpiente
        if nom in ("escorpion", "arana"):
            return "artropodo", b_artropodo
        if nom == "tortuga_marina":
            return "tortuga", b_tortuga
        return "cuadrupedo", b_cuadrupedo
    if cat == "arma":
        if "lazo" in nom or "boleadoras" in nom:
            return "lazo", b_lazo
        if "honda" in nom:
            return "honda", b_honda
        if "arco" in nom:
            return "arco", b_arco
        if "ballesta" in nom:
            return "ballesta", b_ballesta
        if "mosquete" in nom:
            return "mosquete", b_mosquete
        if nom == "canon":
            return "canon", b_canon
        if "hacha" in nom:
            return "hacha", b_hacha
        if "maza" in nom:
            return "maza", b_maza
        if any(k in nom for k in ("lanza", "pica", "guja", "alabarda")):
            return "asta", b_asta
        return "espada", b_espada
    if cat == "proyectil":
        return "proyectil", b_proyectil
    if cat == "escudo":
        return "escudo", b_escudo
    if cat == "totem":
        return "totem", b_totem
    if cat == "armadura":
        return "armadura", b_armadura
    if cat == "vestimenta":
        return "vestimenta", b_vestimenta
    if cat == "accesorio":
        return "accesorio", b_accesorio
    if cat == "estructura":
        return ("yurta", b_estructura) if "yurta" in nom or nom == "enfermeria" else ("estructura", b_estructura)
    if cat == "vehiculo":
        return ("barco" if sub == "agua" else "carro"), b_vehiculo
    if cat == "asedio":
        return "asedio", b_asedio
    if cat == "mapa":
        return ("vegetacion" if sub == "vegetacion" else "mapa"), b_mapa
    if cat == "utileria":
        return "utileria", b_utileria
    return "caja", b_caja


def medidas(it):
    w, h, l = (float(v) for v in it["medidas_m"].split("x"))
    return w, h, l


def construir(it):
    """Devuelve (Modelo normalizado, familia). Baja el detalle hasta caber en tris_max."""
    W, H, L = medidas(it)
    tmax = int(it["tris_max"])
    familia, fn = elegir(it)
    rng0 = zlib.crc32(it["id"].encode())
    for d in (3, 2, 1, 0):
        m = Modelo()
        fn(m, it, d, random.Random(rng0), W, H, L)
        if m.partes and m.ntris() <= tmax:
            break
    else:
        # Respaldo: la forma no cabe en el presupuesto.
        for nombre, f in (("caja_biselada", lambda: caja_biselada((0, 0.5, 0), (1, 1, 1))), ("caja", lambda: caja((0, 0.5, 0), (1, 1, 1))),
                          ("octaedro", lambda: octaedro((0, 0.5, 0), 0.5)), ("tetraedro", lambda: tetraedro((0, 0.5, 0), 0.5))):
            tris = f()
            if len(tris) <= tmax:
                break
        cel = next(iter(m.partes.values()))[0] if m.partes else CEL["madera"]
        m = Modelo()
        m.add("bloque", cel, tris)
        familia = familia + f" ({nombre})"
    if any(t.startswith("manos:") for t in it["tags"]) and m.agarre is None:
        # Sin punto de agarre definido: centro de la base del objeto.
        mn, mx = m.bbox()
        m.agarre = ((mn[0] + mx[0]) / 2, mn[1], (mn[2] + mx[2]) / 2)
    m.normalizar(L, H, W)
    return m, familia


# ---------------------------------------------------------------------------
# Escritura GLB (glTF 2.0 binario)
# ---------------------------------------------------------------------------
def f32_corto(v):
    """Texto decimal mas corto que vuelve exactamente al mismo float32 (JSON mas liviano)."""
    exacto = struct.unpack("<f", struct.pack("<f", v))[0]
    for prec in range(4, 10):
        x = float(f"{exacto:.{prec}g}")
        if struct.unpack("<f", struct.pack("<f", x))[0] == exacto:
            return x
    return exacto


def srgb_a_lineal(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lineal_a_srgb(c):
    c = max(0.0, min(1.0, c))
    return int(round(255 * (c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055)))


def uv_de_tri(t, mn, mx, celda):
    nrm = normal_tri(t)
    k = max(range(3), key=lambda i: abs(nrm[i]))
    a, b = [i for i in range(3) if i != k]
    cx, cy = (celda % REJILLA) * CELDA, (celda // REJILLA) * CELDA
    util = CELDA - 2 * MARGEN_UV
    out = []
    for p in t:
        ra = (p[a] - mn[a]) / (mx[a] - mn[a]) if mx[a] - mn[a] > 1e-9 else 0.5
        rb = (p[b] - mn[b]) / (mx[b] - mn[b]) if mx[b] - mn[b] > 1e-9 else 0.5
        if b == 1:  # la altura (Y) hacia abajo en la textura
            rb = 1 - rb
        out.append(((cx + MARGEN_UV + ra * util) / LADO, (cy + MARGEN_UV + rb * util) / LADO))
    return out


ZANCADA = {"POSITION": 12, "NORMAL": 12, "TEXCOORD_0": 8, "COLOR_0": 8}


def escribir_glb(ruta, it, m, familia, atlas_png, medias, con_colores=True):
    # Un bufferView por atributo (compartido por todas las partes) para que el JSON sea corto.
    datos = OrderedDict((k, bytearray()) for k in ("POSITION", "NORMAL", "TEXCOORD_0", "COLOR_0", "indices"))
    if not con_colores:
        del datos["COLOR_0"]
    vista_de = {k: i for i, k in enumerate(datos)}
    accs, meshes, nodos = [], [], []

    def acc(clave, ctype, count, tipo, mn=None, mx=None, normalizado=False):
        a = {"bufferView": vista_de[clave], "byteOffset": len(datos[clave]), "componentType": ctype, "count": count, "type": tipo}
        if normalizado:
            a["normalized"] = True
        if mn is not None:
            a["min"], a["max"] = mn, mx
        accs.append(a)
        return len(accs) - 1

    partes = [(nombre, celda, tris) for nombre, (celda, tris) in m.partes.items() if tris]
    celdas = []
    preparadas = []
    for nombre, celda, tris in partes:
        celdas.append(celda)
        pts = [p for t in tris for p in t]
        mn = [min(p[k] for p in pts) for k in range(3)]
        mx = [max(p[k] for p in pts) for k in range(3)]
        # Vertices sin repetir: los triangulos coplanares de una misma cara (quads, tapas)
        # comparten vertices; las aristas vivas se duplican (normales planas por cara).
        verts, idx_de, indices = [], {}, []
        for t in tris:
            nn = vn(normal_tri(t))
            for p, (u, v) in zip(t, uv_de_tri(t, mn, mx, celda)):
                clave = struct.pack("<3f3f2f", *p, *nn, u, v)
                if clave not in idx_de:
                    idx_de[clave] = len(verts)
                    verts.append(clave)
                indices.append(idx_de[clave])
        preparadas.append((nombre, celda, verts, indices))
    idx16 = all(len(v) < 65536 for _, _, v, _ in preparadas)
    for nombre, celda, verts, indices in preparadas:
        nv = len(verts)
        fpos = [struct.unpack_from("<3f", v, 0) for v in verts]  # min/max exactos en float32
        mnf = [f32_corto(min(p[k] for p in fpos)) for k in range(3)]
        mxf = [f32_corto(max(p[k] for p in fpos)) for k in range(3)]
        attrs = {"POSITION": acc("POSITION", 5126, nv, "VEC3", mnf, mxf)}
        datos["POSITION"].extend(b"".join(v[0:12] for v in verts))
        attrs["NORMAL"] = acc("NORMAL", 5126, nv, "VEC3")
        datos["NORMAL"].extend(b"".join(v[12:24] for v in verts))
        attrs["TEXCOORD_0"] = acc("TEXCOORD_0", 5126, nv, "VEC2")
        datos["TEXCOORD_0"].extend(b"".join(v[24:32] for v in verts))
        if con_colores:  # como el exportador de Blender: unsigned short normalizado, lineal
            r, g, b = (int(round(srgb_a_lineal(c) * 65535)) for c in medias[celda])
            attrs["COLOR_0"] = acc("COLOR_0", 5123, nv, "VEC4", normalizado=True)
            datos["COLOR_0"].extend(struct.pack("<4H", r, g, b, 65535) * nv)
        ni = len(indices)
        ia = acc("indices", 5123 if idx16 else 5125, ni, "SCALAR")
        datos["indices"].extend(struct.pack(f"<{ni}{'H' if idx16 else 'I'}", *indices))
        if len(datos["indices"]) % 4:  # el siguiente accessor de indices queda alineado
            datos["indices"].extend(b"\0\0")
        meshes.append({"name": f"{it['id']}:{nombre}", "primitives": [{"attributes": attrs, "indices": ia, "material": 0}]})
        nodos.append({"name": nombre, "mesh": len(meshes) - 1})
    bin_ = bytearray()
    views = []
    for clave, data in list(datos.items()) + [("imagen", atlas_png)]:
        while len(bin_) % 4:
            bin_.append(0)
        v = {"buffer": 0, "byteOffset": len(bin_), "byteLength": len(data)}
        if clave != "imagen":
            v["target"] = 34963 if clave == "indices" else 34962
        if clave in ZANCADA:  # varias partes comparten la vista: byteStride obligatorio
            v["byteStride"] = ZANCADA[clave]
        views.append(v)
        bin_.extend(data)
    img_view = len(views) - 1
    W, H, L = medidas(it)
    raiz = {"name": it["id"], "children": list(range(1, len(nodos) + 1)),
            "extras": {"inventario": it["id"], "nombre": it["nombre"], "medidas_m": it["medidas_m"], "tris_max": int(it["tris_max"]),
                       "familia": familia, "celdas": sorted(set(celdas)),
                       "origen": "agarre" if m.agarre is not None else "suelo",
                       "ejes": "+X frente (largo), +Y arriba (alto), Z ancho"}}
    doc = {
        "asset": {"version": "2.0", "generator": GENERADOR},
        "scene": 0,
        "scenes": [{"name": it["id"], "nodes": [0]}],
        "nodes": [raiz] + nodos,
        "meshes": meshes,
        "materials": [{"name": "atlas_materiales", "pbrMetallicRoughness": {
            "baseColorTexture": {"index": 0}, "baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0.0, "roughnessFactor": 1.0}}],
        "textures": [{"sampler": 0, "source": 0}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "images": [{"name": "atlas_materiales", "bufferView": img_view, "mimeType": "image/png"}],
        "accessors": accs,
        "bufferViews": views,
        "buffers": [{"byteLength": 0}],
    }
    while len(bin_) % 4:
        bin_.append(0)
    doc["buffers"][0]["byteLength"] = len(bin_)
    js = json.dumps(doc, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(bin_)
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with open(ruta, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A))
        f.write(js)
        f.write(struct.pack("<II", len(bin_), 0x004E4942))
        f.write(bin_)


def ruta_nomad(it):
    return os.path.join(DIR_MODELOS, *it["id"].split(".")) + ".glb"


def items_inventario():
    items, errores = inv.load()
    if errores:
        for e in errores:
            print("ERROR inventario:", e)
        sys.exit(1)
    return items


def es_textura(it): return "formato:textura" in it["tags"]


def cmd_modelos(forzar=False, prefijo=None, con_colores=True):
    if not os.path.isfile(ATLAS):
        cmd_atlas()
    atlas_png = open(ATLAS, "rb").read()
    medias = medias_atlas()
    escritos = saltados = 0
    for it in items_inventario():
        if es_textura(it) or (prefijo and not it["id"].startswith(prefijo)):
            continue
        ruta = ruta_nomad(it)
        if os.path.exists(ruta) and not forzar:
            saltados += 1
            continue
        m, familia = construir(it)
        escribir_glb(ruta, it, m, familia, atlas_png, medias, con_colores)
        escritos += 1
    print(f"Modelos: {escritos} escritos, {saltados} ya existian (no se tocan sin --forzar).")
    return 0


# ---------------------------------------------------------------------------
# Lectura y verificacion
# ---------------------------------------------------------------------------
TAM_COMP = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
FMT_COMP = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
N_TIPO = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


class ErrorGLB(Exception):
    pass


def leer_glb(ruta):
    data = open(ruta, "rb").read()
    if len(data) < 20:
        raise ErrorGLB("archivo demasiado corto")
    magic, ver, total = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:
        raise ErrorGLB("no empieza por 'glTF'")
    if ver != 2:
        raise ErrorGLB(f"version {ver} (se espera 2)")
    if total != len(data):
        raise ErrorGLB(f"cabecera dice {total} bytes, el archivo tiene {len(data)}")
    jl, jt = struct.unpack_from("<II", data, 12)
    if jt != 0x4E4F534A or jl % 4:
        raise ErrorGLB("primer bloque no es JSON alineado a 4")
    doc = json.loads(data[20:20 + jl].decode("utf-8"))
    off = 20 + jl
    binario = b""
    if off < len(data):
        bl, bt = struct.unpack_from("<II", data, off)
        if bt != 0x004E4942 or bl % 4:
            raise ErrorGLB("segundo bloque no es BIN alineado a 4")
        binario = data[off + 8: off + 8 + bl]
        if len(binario) != bl:
            raise ErrorGLB("bloque BIN truncado")
    bufs = doc.get("buffers", [])
    if bufs and bufs[0]["byteLength"] > len(binario):
        raise ErrorGLB("buffers[0].byteLength mayor que el bloque BIN")
    return doc, binario


def leer_accessor(doc, binario, i):
    a = doc["accessors"][i]
    v = doc["bufferViews"][a["bufferView"]]
    nc, ct = N_TIPO[a["type"]], a["componentType"]
    tam = TAM_COMP[ct] * nc
    stride = v.get("byteStride", tam)
    ini = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    if a.get("byteOffset", 0) + stride * (a["count"] - 1) + tam > v["byteLength"]:
        raise ErrorGLB(f"accessor {i} se sale de su bufferView")
    if v.get("byteOffset", 0) + v["byteLength"] > doc["buffers"][v["buffer"]]["byteLength"]:
        raise ErrorGLB(f"bufferView {a['bufferView']} se sale del buffer")
    if ini % TAM_COMP[ct]:
        raise ErrorGLB(f"accessor {i} desalineado")
    fmt = "<" + FMT_COMP[ct] * nc
    return [struct.unpack_from(fmt, binario, ini + k * stride) for k in range(a["count"])]


def geometria_glb(doc, binario):
    """Lista de (triangulos, color sRGB) por primitiva. Aplica traslaciones de nodos simples."""
    out = []
    def color_prim(p):
        if "COLOR_0" in p["attributes"]:
            a = doc["accessors"][p["attributes"]["COLOR_0"]]
            div = {5121: 255.0, 5123: 65535.0}.get(a["componentType"], 1.0)
            cs = leer_accessor(doc, binario, p["attributes"]["COLOR_0"])
            c = [sum(x[k] for x in cs) / len(cs) / div for k in range(3)]
            return tuple(lineal_a_srgb(x) for x in c)
        mat = doc.get("materials", [{}])[p.get("material", 0)] if doc.get("materials") else {}
        f = mat.get("pbrMetallicRoughness", {}).get("baseColorFactor", [0.7, 0.7, 0.7, 1])
        return tuple(lineal_a_srgb(x) for x in f[:3])

    def visitar(ni, off):
        nodo = doc["nodes"][ni]
        t = nodo.get("translation", [0, 0, 0])
        o = va(off, tuple(t))
        if "mesh" in nodo:
            for p in doc["meshes"][nodo["mesh"]]["primitives"]:
                pos = leer_accessor(doc, binario, p["attributes"]["POSITION"])
                if "indices" in p:
                    idx = [x[0] for x in leer_accessor(doc, binario, p["indices"])]
                else:
                    idx = list(range(len(pos)))
                if any(k >= len(pos) for k in idx):
                    raise ErrorGLB("indice fuera de rango")
                tris = [tuple(va(pos[idx[k + j]], o) for j in range(3)) for k in range(0, len(idx) - 2, 3)]
                out.append((tris, color_prim(p), nodo.get("name", "")))
        for c in nodo.get("children", []):
            visitar(c, o)
    escena = doc.get("scenes", [{"nodes": list(range(len(doc.get("nodes", []))))}])[doc.get("scene", 0)]
    for ni in escena["nodes"]:
        visitar(ni, (0.0, 0.0, 0.0))
    return out


def comprobar_glb(ruta, it):
    """Devuelve (errores, avisos, info) de un GLB."""
    errores, avisos = [], []
    info = {"tris": None, "propio": False, "celdas": [], "familia": ""}
    try:
        doc, binario = leer_glb(ruta)
        info["propio"] = doc.get("asset", {}).get("generator") == GENERADOR
        if doc.get("asset", {}).get("version") != "2.0":
            errores.append("asset.version no es 2.0")
        for i, a in enumerate(doc.get("accessors", [])):
            leer_accessor(doc, binario, i)
        for i, me in enumerate(doc.get("meshes", [])):
            for p in me["primitives"]:
                pa = doc["accessors"][p["attributes"]["POSITION"]]
                if "min" not in pa or "max" not in pa:
                    errores.append(f"mesh {i}: POSITION sin min/max")
                    continue
                pos = leer_accessor(doc, binario, p["attributes"]["POSITION"])
                mn = [min(q[k] for q in pos) for k in range(3)]
                mx = [max(q[k] for q in pos) for k in range(3)]
                f32 = lambda v: struct.unpack("<f", struct.pack("<f", v))[0]  # noqa: E731
                if any(f32(pa["min"][k]) != mn[k] or f32(pa["max"][k]) != mx[k] for k in range(3)):
                    errores.append(f"mesh {i}: min/max de POSITION no coinciden con los datos")
        for im in doc.get("images", []):
            if "bufferView" in im:
                v = doc["bufferViews"][im["bufferView"]]
                if binario[v.get("byteOffset", 0):v.get("byteOffset", 0) + 8] != b"\x89PNG\r\n\x1a\n":
                    errores.append("la imagen embebida no es PNG")
        geo = geometria_glb(doc, binario)
        tris = [t for g in geo for t in g[0]]
        info["tris"] = len(tris)
        raiz = doc["nodes"][doc["scenes"][doc.get("scene", 0)]["nodes"][0]] if doc.get("nodes") else {}
        ex = raiz.get("extras", {})
        info["celdas"], info["familia"] = ex.get("celdas", []), ex.get("familia", "")
        problemas = errores if info["propio"] else avisos   # un GLB editado a mano solo genera avisos
        tmax = int(it["tris_max"])
        if len(tris) > tmax:
            problemas.append(f"{len(tris)} triangulos > tris_max {tmax}")
        if not tris:
            errores.append("sin triangulos")
            return errores, avisos, info
        pts = [p for t in tris for p in t]
        mn = [min(p[k] for p in pts) for k in range(3)]
        mx = [max(p[k] for p in pts) for k in range(3)]
        W, H, L = medidas(it)
        for k, (eje, objetivo) in enumerate((("X/largo", L), ("Y/alto", H), ("Z/ancho", W))):
            real = mx[k] - mn[k]
            if abs(real - objetivo) > max(0.05 * objetivo, 1e-4):
                problemas.append(f"{eje}: mide {real:.3f} m, inventario {objetivo} m")
        agarre = any(t.startswith("manos:") for t in it["tags"])
        if agarre:
            if not all(mn[k] - 1e-4 <= 0 <= mx[k] + 1e-4 for k in range(3)):
                problemas.append("origen (agarre) fuera del objeto")
        else:
            if abs(mn[1]) > max(0.01 * H, 1e-4):
                problemas.append(f"origen: min Y = {mn[1]:.4f} (debe apoyar en el suelo, Y = 0)")
            for k, nombre, tam in ((0, "X", L), (2, "Z", W)):
                c = (mn[k] + mx[k]) / 2
                if abs(c) > max(0.02 * tam, 1e-4):
                    problemas.append(f"origen: base descentrada en {nombre} ({c:.3f} m)")
    except (ErrorGLB, ValueError, KeyError, IndexError, struct.error) as e:
        errores.append(f"GLB invalido: {e}")
    return errores, avisos, info


def cmd_verificar():
    items = items_inventario()
    nerr = nav = ok = faltan = 0
    tam = 0
    conocidos = set()
    for it in items:
        ruta = ruta_nomad(it)
        conocidos.add(os.path.normpath(ruta))
        if es_textura(it):
            if os.path.exists(ruta):
                print(f"ERROR {it['id']}: es textura 2D y no deberia tener GLB")
                nerr += 1
            continue
        if not os.path.exists(ruta):
            print(f"FALTA {it['id']}: {rel(ruta)}")
            faltan += 1
            continue
        tam += os.path.getsize(ruta)
        errores, avisos, info = comprobar_glb(ruta, it)
        for e in errores:
            print(f"ERROR {it['id']}: {e}")
        for a in avisos:
            print(f"AVISO {it['id']}: {a}")
        nerr += len(errores)
        nav += len(avisos)
        ok += not errores
    for dirpath, _, files in os.walk(DIR_MODELOS):
        for f in files:
            p = os.path.normpath(os.path.join(dirpath, f))
            if f.endswith(".glb") and p not in conocidos:
                print(f"AVISO {rel(p)}: no corresponde a ningun id del inventario")
                nav += 1
    total = sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fs in os.walk(DIR) for f in fs)
    print(f"Verificados {ok} GLB correctos, {faltan} faltan, {nerr} errores, {nav} avisos. "
          f"Modelos: {tam / 1e6:.2f} MB; carpeta arte/nomad: {total / 1e6:.2f} MB.")
    return 1 if nerr or faltan else 0


# ---------------------------------------------------------------------------
# Indice
# ---------------------------------------------------------------------------
def filas_indice():
    filas = []
    for it in items_inventario():
        W, H, L = medidas(it)
        fila = OrderedDict(id=it["id"], nombre=it["nombre"], categoria=it["categoria"])
        destino = inv.path_of(it).replace(os.sep, "/")
        rig = tag(it, "rig") or "-"
        if es_textura(it):
            fila.update(forma="textura 2D", archivo_nomad="textura 2D (no lleva modelo)", destino_juego=destino,
                        medidas_m=it["medidas_m"], tris="-", tris_max=it["tris_max"], rig=rig, celdas="-",
                        estado_inventario=it["estado"], estado_nomad="textura 2D")
        else:
            ruta = ruta_nomad(it)
            if os.path.exists(ruta):
                errores, _, info = comprobar_glb(ruta, it)
                estado = ("generado" if info["propio"] else "editado") + (" (con errores)" if errores else "")
                tris = str(info["tris"]) if info["tris"] is not None else "?"
                celdas = ", ".join(f"{c}:{CELDAS[c][0]}" for c in info["celdas"] if 0 <= c < len(CELDAS)) or "-"
                forma = info["familia"] or "-"
            else:
                estado, tris, celdas, forma = "falta", "-", "-", elegir(it)[0]
            fila.update(forma=forma, archivo_nomad=rel(ruta).replace(os.sep, "/"), destino_juego=destino,
                        medidas_m=it["medidas_m"], tris=tris, tris_max=it["tris_max"], rig=rig, celdas=celdas,
                        estado_inventario=it["estado"], estado_nomad=estado)
        filas.append(fila)
    return filas


def cmd_indice():
    filas = filas_indice()
    cols = ["id", "nombre", "forma", "archivo_nomad", "destino_juego", "medidas_m", "tris", "tris_max", "rig", "celdas",
            "estado_inventario", "estado_nomad"]
    with open(INDICE_TSV, "w", encoding="utf-8") as f:
        f.write("\t".join(cols) + "\n")
        for r in filas:
            f.write("\t".join(str(r[c]).replace("\t", " ") for c in cols) + "\n")
    modelos = [r for r in filas if r["forma"] != "textura 2D"]
    w = []
    w.append("# Inventario de modelos para Nomad Sculpt")
    w.append("")
    w.append("> Generado por `python3 tools/assets/nomad.py indice`. No editar a mano.")
    w.append("> Hoja de cálculo con los mismos datos: [`inventario_modelos.tsv`](inventario_modelos.tsv). "
             "Cómo trabajar: [README.md](README.md).")
    w.append("")
    w.append(f"**{len(filas)} objetos**: {len(modelos)} modelos de partida en `modelos/` y "
             f"{len(filas) - len(modelos)} texturas 2D (sin modelo). "
             "Medidas `ancho x alto x largo` en metros: largo en X (+X es el frente), alto en Y, ancho en Z. "
             "Las celdas son las del atlas ([`atlas_materiales.tsv`](atlas_materiales.tsv)).")
    w.append("")
    w.append("| Categoría | Objetos | Modelos | Texturas 2D | Generados | Editados | Faltan |")
    w.append("|---|---|---|---|---|---|---|")
    for cat, titulo in inv.CATEGORIES.items():
        rs = [r for r in filas if r["categoria"] == cat]
        if not rs:
            continue
        cuenta = lambda pref: sum(1 for r in rs if r["estado_nomad"].startswith(pref))
        w.append(f"| [{titulo}](#{cat}) | {len(rs)} | {len(rs) - cuenta('textura')} | {cuenta('textura')} | "
                 f"{cuenta('generado')} | {cuenta('editado')} | {cuenta('falta')} |")
    w.append("")
    for cat, titulo in inv.CATEGORIES.items():
        rs = [r for r in filas if r["categoria"] == cat]
        if not rs:
            continue
        w.append(f'<a id="{cat}"></a>')
        w.append("")
        w.append(f"## {titulo} ({len(rs)})")
        w.append("")
        w.append("| Id | Nombre | Archivo Nomad | Destino en el juego | Medidas (m) | Tris / máx | Rig | Celdas | Estado |")
        w.append("|---|---|---|---|---|---|---|---|---|")
        for r in rs:
            if r["forma"] == "textura 2D":
                arch = "textura 2D"
            else:
                p = r["archivo_nomad"][len("arte/nomad/"):]
                arch = f"[`{p[len('modelos/'):]}`]({p})"
            w.append(f"| `{r['id']}` | {r['nombre']} | {arch} | `{r['destino_juego']}` | {r['medidas_m']} | "
                     f"{r['tris']} / {r['tris_max']} | {r['rig']} | {r['celdas']} | {r['estado_inventario']} · {r['estado_nomad']} |")
        w.append("")
    with open(INDICE_MD, "w", encoding="utf-8") as f:
        f.write("\n".join(w) + "\n")
    print(f"Escrito {rel(INDICE_MD)} y {rel(INDICE_TSV)} ({len(filas)} objetos).")
    return 0


# ---------------------------------------------------------------------------
# Hoja de miniaturas (render por pintor con Pillow)
# ---------------------------------------------------------------------------
def render_miniatura(geo, tam=200, ss=2, cam=(1.0, 0.8, 0.75)):
    from PIL import Image, ImageDraw
    cam = vn(cam)                        # por defecto: desde el frente (+X), arriba y a la derecha (+Z)
    der = vn(cruz(vm(cam, -1), (0, 1, 0.0001)))
    arr = cruz(der, vm(cam, -1))
    luz = vn((0.6, 1.0, 0.35))
    tris = []
    for ts, color, _ in geo:
        for t in ts:
            nn = normal_tri(t)
            if vlen(nn) < 1e-14:
                continue
            nn = vn(nn)
            if dot(nn, cam) <= 0:
                continue
            k = 0.5 + 0.5 * max(0.0, dot(nn, luz))
            c = tuple(min(255, int(v * k)) for v in color)
            pts = [(dot(p, der), -dot(p, arr)) for p in t]
            tris.append((sum(dot(p, cam) for p in t) / 3, pts, c))
    S_ = tam * ss
    img = Image.new("RGB", (S_, S_), (46, 40, 36))
    if not tris:
        return img.resize((tam, tam))
    xs = [p[0] for _, ps, _ in tris for p in ps] + [0]
    ys = [p[1] for _, ps, _ in tris for p in ps] + [0]
    ext = max(max(xs) - min(xs), max(ys) - min(ys), 1e-6)
    esc = S_ * 0.84 / ext
    ox = S_ / 2 - (max(xs) + min(xs)) / 2 * esc
    oy = S_ / 2 - (max(ys) + min(ys)) / 2 * esc
    dr = ImageDraw.Draw(img)
    tris.sort(key=lambda x: x[0])
    for _, ps, c in tris:
        dr.polygon([(ox + x * esc, oy + y * esc) for x, y in ps], fill=c)
    # Frente (+X): linea roja desde el origen, como el marcador del juego.
    o = (ox, oy)
    fx = (dot((ext * 0.35, 0, 0), der) * esc + ox, -dot((ext * 0.35, 0, 0), arr) * esc + oy)
    dr.line([o, fx], fill=(220, 40, 30), width=2 * ss)
    dr.ellipse([ox - 3 * ss, oy - 3 * ss, ox + 3 * ss, oy + 3 * ss], outline=(248, 218, 132), width=ss)
    return img.resize((tam, tam), Image.LANCZOS)


def cmd_hoja(prefijo=None, por_pagina=80, columnas=10):
    from PIL import Image, ImageDraw
    items = [it for it in items_inventario() if not es_textura(it) and (not prefijo or it["id"].startswith(prefijo))
             and os.path.exists(ruta_nomad(it))]
    tam, alto_txt = 200, 30
    fuente = _fuente(12)
    paginas = [items[i:i + por_pagina] for i in range(0, len(items), por_pagina)]
    for f in os.listdir(DIR):
        if f.startswith("hoja_modelos") and f.endswith(".png"):
            os.remove(os.path.join(DIR, f))
    salidas = []
    for pi, pag in enumerate(paginas):
        filas = (len(pag) + columnas - 1) // columnas
        hoja = Image.new("RGB", (columnas * tam, filas * (tam + alto_txt)), (33, 22, 15))
        dr = ImageDraw.Draw(hoja)
        for i, it in enumerate(pag):
            x, y = (i % columnas) * tam, (i // columnas) * (tam + alto_txt)
            try:
                doc, binario = leer_glb(ruta_nomad(it))
                mini = render_miniatura(geometria_glb(doc, binario), tam)
            except Exception as e:  # noqa: BLE001 - una miniatura rota no debe parar la hoja
                mini = Image.new("RGB", (tam, tam), (120, 0, 0))
                print(f"AVISO {it['id']}: {e}")
            hoja.paste(mini, (x, y))
            _, sub, nom = it["id"].split(".")
            dr.text((x + 4, y + tam + 2), f"{it['categoria']}.{sub}", fill=(200, 186, 152), font=fuente)
            dr.text((x + 4, y + tam + 15), f"{nom}  {it['medidas_m']}", fill=(242, 228, 192), font=fuente)
            dr.rectangle([x, y, x + tam - 1, y + tam + alto_txt - 1], outline=(16, 15, 18))
        nombre = "hoja_modelos.png" if len(paginas) == 1 else f"hoja_modelos_{pi + 1}.png"
        hoja.quantize(256, method=Image.Quantize.MEDIANCUT).save(os.path.join(DIR, nombre), optimize=True)
        salidas.append(nombre)
    print(f"Escritas {len(salidas)} hojas en {rel(DIR)}: {', '.join(salidas)}")
    return 0


def rel(p): return os.path.relpath(p, ROOT)


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = {a for a in argv[1:] if a.startswith("--")}
    cmd = args[0] if args else ""
    resto = args[1] if len(args) > 1 else None
    forzar = "--forzar" in flags
    colores = "--sin-colores" not in flags
    if cmd == "atlas":
        return cmd_atlas(forzar)
    if cmd == "modelos":
        return cmd_modelos(forzar, resto, colores)
    if cmd == "indice":
        return cmd_indice()
    if cmd == "verificar":
        return cmd_verificar()
    if cmd == "hoja":
        return cmd_hoja(resto)
    if cmd == "todo":
        return cmd_atlas(forzar) or cmd_modelos(forzar, resto, colores) or cmd_indice()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
