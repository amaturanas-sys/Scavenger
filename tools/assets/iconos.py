#!/usr/bin/env python3
"""Iconos de los menus de ESTEPA: siluetas en un atlas PNG editable.

Uso:
  python3 tools/assets/iconos.py generar [--forzar]   dibuja assets/ui/iconos.png (no pisa tus cambios sin --forzar)
  python3 tools/assets/iconos.py guia                 assets/ui/iconos_guia.png (x4, con nombre e indice) e iconos.tsv
  python3 tools/assets/iconos.py check                comprueba que iconos.tsv y src/ui/icons.c nombran los mismos iconos

El atlas tiene celdas de 32x32 en 16 columnas. Cada icono es una silueta blanca con
contorno oscuro: el juego la tiñe (oro, plata, turquesa) segun el estado del boton.
Para cambiar un icono, pinta su celda en iconos.png (blanco = la silueta, el contorno
oscuro es opcional, transparente = fondo). El orden de las celdas lo da iconos.tsv:
el juego lo lee al arrancar, asi que puedes mover iconos de celda si actualizas el TSV.
"""
import math
import os
import re
import sys

try:  # Pillow solo hace falta para dibujar; 'check' funciona sin el
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    Image = ImageDraw = ImageFont = None

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "assets", "ui", "iconos.png")
GUIDE = os.path.join(ROOT, "assets", "ui", "iconos_guia.png")
TSV = os.path.join(ROOT, "assets", "ui", "iconos.tsv")
ICONS_C = os.path.join(ROOT, "src", "ui", "icons.c")

CELL = 32
COLS = 16
S = 128  # lienzo de trabajo de cada icono (se reduce a CELL)
W = (255, 255, 255, 255)
K = (0, 0, 0, 0)  # recorte: borra


# ------------------------------------------------------------------ primitivas
def png_size(path):
    import struct
    with open(path, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24])


class Pen:
    def __init__(self):
        self.img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    def poly(self, pts, c=W):
        self.d.polygon([(x, y) for x, y in pts], fill=c)

    def rect(self, x0, y0, x1, y1, c=W, r=0):
        if r:
            self.d.rounded_rectangle((x0, y0, x1, y1), radius=r, fill=c)
        else:
            self.d.rectangle((x0, y0, x1, y1), fill=c)

    def ell(self, cx, cy, rx, ry=None, c=W):
        ry = rx if ry is None else ry
        self.d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=c)

    def line(self, pts, w=8, c=W):
        self.d.line([(x, y) for x, y in pts], fill=c, width=w, joint="curve")
        for x, y in (pts[0], pts[-1]):
            self.ell(x, y, w / 2 - 0.5, c=c)

    def thick(self, x0, y0, x1, y1, w, c=W):
        """Barra gruesa entre dos puntos (rectangulo girado)."""
        dx, dy = x1 - x0, y1 - y0
        l = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / l * w / 2, dx / l * w / 2
        self.poly([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)], c)

    def arc(self, cx, cy, r, a0, a1, w=8, c=W):
        self.d.arc((cx - r, cy - r, cx + r, cy + r), a0, a1, fill=c, width=w)

    def ring(self, cx, cy, r, w=8, c=W):
        self.arc(cx, cy, r, 0, 360, w, c)


def rot(pts, cx, cy, deg):
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    return [(cx + (x - cx) * ca - (y - cy) * sa, cy + (x - cx) * sa + (y - cy) * ca) for x, y in pts]


# ------------------------------------------------------------------ piezas reutilizables
def blade_sabre(p, deg=-45, cx=64, cy=64):
    # hoja curva, guarda y empuñadura, a lo largo de un eje girado
    hoja = [(60, 14), (70, 22), (72, 40), (70, 70), (66, 84), (58, 84), (60, 66), (62, 40)]
    p.poly(rot(hoja, cx, cy, deg))
    p.poly(rot([(46, 84), (82, 84), (82, 92), (46, 92)], cx, cy, deg))
    p.poly(rot([(58, 92), (68, 92), (68, 114), (58, 114)], cx, cy, deg))
    p.poly(rot([(54, 112), (72, 112), (72, 120), (54, 120)], cx, cy, deg))


def yurt(p, x=0, y=0, k=1.0):
    def t(pts):
        return [(x + 64 + (a - 64) * k, y + 64 + (b - 64) * k) for a, b in pts]
    p.poly(t([(14, 70), (64, 28), (114, 70)]))
    p.poly(t([(20, 70), (108, 70), (108, 110), (20, 110)]))
    p.poly(t([(54, 82), (74, 82), (74, 110), (54, 110)]), K)
    p.poly(t([(58, 22), (70, 22), (70, 34), (58, 34)]))


def flame(p, cx, cy, k=1.0):
    pts = [(0, -46), (14, -22), (22, -30), (26, -6), (24, 14), (14, 26), (-14, 26), (-24, 14), (-26, -4), (-18, -20), (-10, -12)]
    p.poly([(cx + a * k, cy + b * k) for a, b in pts])
    inner = [(0, -14), (8, 2), (8, 16), (-8, 16), (-8, 2)]
    p.poly([(cx + a * k, cy + b * k) for a, b in inner], K)


def horse_head(p):
    p.poly([(40, 112), (46, 70), (56, 40), (66, 20), (74, 14), (78, 26), (92, 40), (112, 62), (114, 74), (104, 80),
            (88, 72), (76, 68), (74, 90), (80, 112)])
    p.poly([(66, 20), (60, 6), (72, 14)])  # oreja
    p.ell(84, 44, 4, c=K)  # ojo
    p.poly([(56, 40), (44, 46), (36, 70), (34, 96), (40, 112), (46, 70)])  # crin


# ------------------------------------------------------------------ iconos
ICONS = []


def icon(name, desc):
    def deco(fn):
        ICONS.append((name, desc, fn))
        return fn
    return deco


# -- menu principal y pausa
@icon("nueva", "Nueva partida: un caballo al galope")
def _(p):
    horse_head(p)


@icon("cargar", "Cargar partida: rollo abierto")
def _(p):
    p.rect(26, 22, 102, 106, r=6)
    p.ell(26, 30, 12, 10)
    p.ell(102, 98, 12, 10)
    for y in (42, 58, 74, 90):
        p.rect(38, y, 90, y + 6, K)


@icon("guardar", "Guardar partida: arcon")
def _(p):
    p.rect(16, 48, 112, 110, r=4)
    p.poly([(16, 50), (24, 26), (104, 26), (112, 50)])
    p.rect(16, 50, 112, 56, K)
    p.rect(56, 44, 72, 66)
    p.rect(60, 54, 68, 62, K)


@icon("instructivo", "Instructivo: libro abierto")
def _(p):
    p.poly([(10, 30), (60, 38), (60, 108), (10, 98)])
    p.poly([(118, 30), (68, 38), (68, 108), (118, 98)])
    for y in (50, 64, 78):
        p.line([(20, y), (50, y + 5)], 4, K)
        p.line([(78, y + 5), (108, y)], 4, K)


@icon("salir", "Salir: puerta con flecha")
def _(p):
    p.rect(20, 14, 72, 114)
    p.rect(30, 24, 62, 104, K)
    p.ell(54, 66, 4)
    p.thick(60, 64, 102, 64, 14)
    p.poly([(96, 44), (122, 64), (96, 84)])


@icon("continuar", "Continuar: punta de flecha")
def _(p):
    p.poly([(34, 18), (108, 64), (34, 110), (50, 64)])


@icon("titulo", "Volver al titulo: estela de piedra")
def _(p):
    p.poly([(36, 116), (34, 40), (44, 18), (64, 10), (84, 18), (94, 40), (92, 116)])
    p.ell(64, 42, 10, c=K)
    p.rect(50, 62, 78, 66, K)
    p.rect(46, 74, 82, 78, K)
    p.rect(50, 86, 78, 90, K)


# -- pestañas del menu Tab
@icon("acciones", "Acciones: mano abierta")
def _(p):
    p.rect(36, 56, 92, 112, r=14)
    for i, (x, top) in enumerate(((40, 26), (54, 14), (68, 12), (82, 20))):
        p.rect(x, top, x + 10, 70, r=5)
    p.poly([(36, 74), (18, 52), (26, 46), (44, 64)])


@icon("obras", "Obras en grupo: empalizada")
def _(p):
    for x in (16, 40, 64, 88):
        p.poly([(x, 112), (x, 36), (x + 10, 20), (x + 20, 36), (x + 20, 112)])
    p.rect(10, 56, 118, 64)
    p.rect(10, 86, 118, 94)


@icon("fabricar", "Fabricar: martillo y yunque")
def _(p):
    p.poly([(14, 74), (114, 74), (100, 86), (84, 86), (84, 98), (96, 112), (32, 112), (44, 98), (44, 86), (28, 86)])
    p.thick(40, 64, 86, 18, 10)
    p.poly(rot([(70, 8), (104, 8), (104, 30), (70, 30)], 87, 19, -45))


@icon("reparar", "Reparar: aguja e hilo")
def _(p):
    p.thick(24, 108, 98, 24, 12)
    p.poly([(94, 18), (114, 8), (106, 30)])
    p.ell(86, 38, 4, c=K)
    p.arc(60, 74, 34, 110, 330, 8)


# -- acciones
@icon("tomar", "Tomar: mano que agarra")
def _(p):
    p.rect(30, 50, 96, 104, r=16)
    for x in (32, 48, 64, 80):
        p.ell(x + 8, 48, 9)
    p.poly([(30, 70), (14, 48), (24, 40), (40, 60)])
    p.ell(64, 26, 14)


@icon("lanzar", "Lanzar: piedra en arco")
def _(p):
    p.arc(64, 96, 50, 200, 330, 7)
    p.ell(104, 70, 14)
    p.poly([(10, 108), (30, 84), (38, 112)])


@icon("empunar", "Cambiar empuñadura: espada y flechas")
def _(p):
    blade_sabre(p, -30)
    p.arc(64, 64, 52, 200, 250, 6)
    p.poly([(14, 44), (22, 22), (34, 42)])


@icon("enfundar", "Enfundar: vaina")
def _(p):
    p.poly(rot([(54, 30), (74, 30), (78, 100), (64, 118), (50, 100)], 64, 64, -35))
    p.poly(rot([(46, 22), (82, 22), (82, 30), (46, 30)], 64, 64, -35))
    p.poly(rot([(58, 4), (70, 4), (70, 22), (58, 22)], 64, 64, -35))


@icon("fogata", "Fogata")
def _(p):
    flame(p, 64, 60, 1.05)
    p.thick(20, 112, 108, 92, 10)
    p.thick(20, 92, 108, 112, 10)


@icon("tienda", "Tienda de campaña")
def _(p):
    p.poly([(8, 110), (64, 18), (120, 110)])
    p.poly([(52, 110), (64, 66), (76, 110)], K)
    p.thick(64, 18, 64, 8, 4)


@icon("trinchera", "Cavar trinchera: pala")
def _(p):
    p.thick(30, 98, 92, 22, 9)
    p.poly(rot([(18, 84), (42, 84), (46, 112), (30, 124), (14, 112)], 30, 98, -40))
    p.thick(84, 14, 104, 34, 9)


@icon("trepa", "Trepa: gancho con cuerda")
def _(p):
    p.thick(64, 20, 64, 70, 9)
    p.arc(42, 70, 22, 0, 180, 9)
    p.arc(86, 70, 22, 0, 180, 9)
    p.poly([(16, 72), (24, 56), (28, 74)])
    p.poly([(112, 72), (104, 56), (100, 74)])
    p.ring(64, 14, 8, 5)
    p.arc(44, 20, 24, 270, 90, 4)
    p.line([(44, 44), (24, 70), (30, 110), (12, 124)], 4)


@icon("lazo", "Lazo")
def _(p):
    p.ring(70, 48, 36, 8)
    p.line([(42, 74), (30, 96), (40, 110), (20, 124)], 7)


@icon("antorcha", "Antorcha")
def _(p):
    flame(p, 64, 38, 0.75)
    p.poly([(56, 60), (72, 60), (68, 120), (60, 120)])
    p.rect(52, 58, 76, 66)


@icon("ensillar", "Ensillar: silla de montar")
def _(p):
    p.poly([(14, 66), (26, 48), (40, 58), (64, 60), (88, 58), (102, 40), (114, 52), (110, 76), (88, 82), (40, 82), (18, 78)])
    p.thick(56, 82, 50, 108, 5)
    p.rect(40, 104, 60, 112, r=3)


# -- obras
@icon("refugio", "Refugio")
def _(p):
    p.poly([(10, 112), (70, 28), (118, 112)])
    p.poly([(66, 112), (78, 64), (100, 112)], K)
    p.thick(70, 28, 60, 14, 5)


@icon("muro_piedra", "Muro de piedra")
def _(p):
    for row, y in enumerate((30, 58, 86)):
        off = 0 if row % 2 == 0 else 20
        for x in range(-20 + off, 120, 40):
            p.rect(max(10, x + 2), y, min(118, x + 36), y + 24, r=3)


@icon("hoguera", "Hoguera grande")
def _(p):
    flame(p, 44, 70, 0.7)
    flame(p, 84, 70, 0.7)
    flame(p, 64, 56, 1.0)
    p.poly([(14, 116), (64, 96), (114, 116)])


@icon("totem", "Totem")
def _(p):
    p.rect(52, 40, 76, 120)
    p.ell(64, 32, 22, 20)
    p.ell(56, 30, 4, c=K)
    p.ell(72, 30, 4, c=K)
    p.poly([(30, 50), (52, 56), (52, 66)])
    p.poly([(98, 50), (76, 56), (76, 66)])
    p.rect(44, 76, 84, 82)


@icon("horno", "Horno")
def _(p):
    p.d.pieslice((14, 30, 114, 130), 180, 360, fill=W)
    p.rect(14, 78, 114, 112)
    p.d.pieslice((44, 64, 84, 104), 180, 360, fill=K)
    p.rect(44, 84, 84, 112, K)
    p.rect(80, 14, 94, 46)


@icon("fundicion", "Horno de fundición")
def _(p):
    p.poly([(30, 114), (40, 40), (88, 40), (98, 114)])
    p.rect(52, 12, 76, 40)
    flame(p, 64, 92, 0.55)


@icon("torre", "Torre de vigía")
def _(p):
    p.poly([(36, 116), (48, 44), (80, 44), (92, 116)])
    p.poly([(28, 44), (64, 12), (100, 44)])
    p.rect(56, 56, 72, 74, K)
    p.thick(40, 116, 88, 70, 4, K)


@icon("corral", "Corral")
def _(p):
    for x in (14, 50, 86):
        p.rect(x, 40, x + 12, 116)
    p.rect(8, 54, 120, 64)
    p.rect(8, 84, 120, 94)


# -- contenedores
@icon("bolsillo", "Bolsillos: saquito")
def _(p):
    p.poly([(40, 30), (88, 30), (78, 44), (104, 70), (106, 104), (90, 116), (38, 116), (22, 104), (24, 70), (50, 44)])
    p.rect(42, 40, 86, 46, K)


@icon("mochila", "Mochila")
def _(p):
    p.rect(26, 34, 102, 116, r=18)
    p.rect(36, 70, 92, 104, K, r=6)
    p.rect(40, 74, 88, 100, r=4)
    p.arc(64, 34, 22, 180, 360, 8)


@icon("alforjas", "Alforjas de la montura")
def _(p):
    p.poly([(16, 40), (112, 40), (112, 50), (16, 50)])
    p.rect(12, 50, 52, 108, r=10)
    p.rect(76, 50, 116, 108, r=10)
    p.rect(20, 62, 44, 70, K)
    p.rect(84, 62, 108, 70, K)


@icon("carreta", "Carreta de la tribu")
def _(p):
    p.poly([(14, 40), (100, 40), (96, 78), (18, 78)])
    p.thick(100, 64, 124, 56, 6)
    p.ring(36, 92, 20, 8)
    p.ring(82, 92, 20, 8)
    p.ell(36, 92, 5)
    p.ell(82, 92, 5)


@icon("campamento", "Acopio y armería del campamento")
def _(p):
    yurt(p, 0, 6, 0.9)
    p.thick(100, 50, 100, 8, 4)
    p.poly([(100, 8), (124, 16), (100, 24)])


@icon("botin", "Bolsa de botín")
def _(p):
    p.poly([(44, 26), (84, 26), (74, 40), (108, 70), (110, 104), (92, 118), (36, 118), (18, 104), (20, 70), (54, 40)])
    p.rect(46, 38, 82, 44, K)
    p.ell(64, 84, 14, c=K)
    p.ell(64, 84, 8)


# -- huecos de armadura
@icon("casco", "Casco")
def _(p):
    p.d.pieslice((20, 24, 108, 112), 180, 360, fill=W)
    p.rect(20, 66, 108, 84)
    p.poly([(20, 84), (40, 84), (36, 112), (22, 106)])
    p.poly([(108, 84), (88, 84), (92, 112), (106, 106)])
    p.thick(64, 24, 64, 8, 6)
    p.rect(58, 66, 70, 96)


@icon("cuello", "Gorjal")
def _(p):
    p.d.chord((18, 30, 110, 110), 0, 180, fill=W)
    p.d.chord((40, 30, 88, 80), 0, 180, fill=K)
    for x in (36, 56, 76):
        p.rect(x, 80, x + 14, 84, K)


@icon("torso", "Coraza")
def _(p):
    p.poly([(30, 18), (50, 26), (64, 34), (78, 26), (98, 18), (112, 40), (100, 54), (98, 114), (30, 114), (28, 54), (16, 40)])
    p.ell(64, 30, 12, 8, K)
    for y in (64, 80, 96):
        p.rect(36, y, 92, y + 4, K)


@icon("hombreras", "Hombreras")
def _(p):
    for cx in (34, 94):
        p.d.pieslice((cx - 30, 30, cx + 30, 100), 180, 360, fill=W)
        p.rect(cx - 30, 64, cx + 30, 76)
        p.rect(cx - 24, 80, cx + 24, 90)
        p.rect(cx - 18, 94, cx + 18, 102)


@icon("brazales", "Brazales")
def _(p):
    for x in (18, 70):
        p.poly([(x, 26), (x + 40, 26), (x + 34, 108), (x + 6, 108)])
        p.rect(x + 2, 50, x + 38, 54, K)
        p.rect(x + 4, 76, x + 36, 80, K)


@icon("guantes", "Guanteletes")
def _(p):
    p.rect(30, 62, 94, 118, r=10)
    for x in (32, 48, 64, 80):
        p.rect(x, 22, x + 13, 70, r=6)
    p.poly([(30, 82), (10, 60), (20, 52), (40, 70)])
    p.rect(30, 98, 94, 102, K)


@icon("faldar", "Faldar")
def _(p):
    p.rect(30, 22, 98, 36)
    p.poly([(30, 36), (98, 36), (114, 110), (14, 110)])
    for x in (44, 64, 84):
        p.thick(x, 40, x + (x - 64) * 0.35, 108, 4, K)


@icon("grebas", "Grebas")
def _(p):
    for x in (22, 70):
        p.poly([(x, 16), (x + 36, 16), (x + 34, 60), (x + 28, 112), (x + 8, 112), (x + 2, 60)])
        p.ell(x + 18, 40, 8, 6, K)


@icon("botas", "Botas")
def _(p):
    p.poly([(34, 14), (66, 14), (68, 82), (110, 94), (114, 114), (30, 114)])
    p.rect(30, 100, 114, 104, K)


@icon("amuleto", "Amuleto")
def _(p):
    p.arc(64, 40, 34, 180, 360, 6)
    p.line([(30, 40), (52, 74)], 6)
    p.line([(98, 40), (76, 74)], 6)
    p.ell(64, 88, 26)
    p.ell(64, 88, 12, c=K)
    p.poly([(64, 78), (72, 88), (64, 98), (56, 88)])


@icon("tatuaje", "Tatuaje: espiral")
def _(p):
    pts = []
    for i in range(120):
        t = i / 119.0
        a = t * 3.2 * math.pi
        r = 6 + 46 * t
        pts.append((64 + math.cos(a) * r, 64 + math.sin(a) * r))
    p.line(pts, 9)


# -- armas
@icon("sable", "Sable")
def _(p):
    blade_sabre(p)


@icon("daga", "Daga o cuchillo")
def _(p):
    pts = [(58, 18), (64, 10), (70, 18), (70, 70), (58, 70)]
    p.poly(rot(pts, 64, 64, 40))
    p.poly(rot([(46, 70), (82, 70), (82, 78), (46, 78)], 64, 64, 40))
    p.poly(rot([(58, 78), (70, 78), (70, 106), (58, 106)], 64, 64, 40))


@icon("hacha", "Hacha")
def _(p):
    p.thick(36, 116, 84, 18, 10)
    p.poly([(70, 24), (112, 8), (120, 40), (108, 62), (80, 54)])


@icon("maza", "Maza")
def _(p):
    p.thick(30, 116, 74, 50, 10)
    p.ell(84, 38, 24)
    for a in range(0, 360, 60):
        x, y = 84 + math.cos(math.radians(a)) * 30, 38 + math.sin(math.radians(a)) * 30
        p.ell(x, y, 7)


@icon("lanza", "Lanza")
def _(p):
    p.thick(18, 118, 92, 30, 7)
    p.poly(rot([(88, 6), (100, 30), (92, 48), (84, 48), (76, 30)], 90, 30, 40))


@icon("guja", "Guja o alabarda")
def _(p):
    p.thick(20, 120, 80, 40, 8)
    p.poly([(74, 46), (88, 6), (112, 20), (100, 50), (84, 60)])


@icon("espada", "Espada larga")
def _(p):
    pts = [(60, 6), (64, 0), (68, 6), (68, 82), (60, 82)]
    p.poly(rot(pts, 64, 64, 45))
    p.poly(rot([(42, 82), (86, 82), (86, 90), (42, 90)], 64, 64, 45))
    p.poly(rot([(60, 90), (68, 90), (68, 116), (60, 116)], 64, 64, 45))
    p.ell(*rot([(64, 120)], 64, 64, 45)[0], 6)


@icon("arco", "Arco")
def _(p):
    p.arc(30, 64, 62, 300, 60, 9)
    p.thick(61, 10, 61, 118, 3)
    p.arc(30, 64, 62, 296, 300, 12)


@icon("ballesta", "Ballesta")
def _(p):
    p.thick(64, 118, 64, 30, 12)
    p.arc(64, 70, 50, 200, 340, 9)
    p.thick(18, 54, 110, 54, 3)
    p.rect(58, 92, 70, 104, K)


@icon("honda", "Honda")
def _(p):
    p.line([(20, 20), (58, 88)], 5)
    p.line([(108, 20), (70, 88)], 5)
    p.ell(64, 96, 16)
    p.ell(64, 96, 7, c=K)


@icon("mosquete", "Mosquete o cañón")
def _(p):
    p.thick(14, 40, 104, 82, 9)
    p.poly([(92, 70), (122, 84), (116, 104), (88, 90)])
    p.rect(70, 74, 80, 92)


@icon("escudo", "Escudo redondo")
def _(p):
    p.ell(64, 64, 52)
    p.ring(64, 64, 40, 5, K)
    p.ell(64, 64, 14, c=K)
    p.ell(64, 64, 9)


@icon("paves", "Escudo grande")
def _(p):
    p.poly([(26, 14), (102, 14), (104, 76), (64, 118), (24, 76)])
    p.thick(64, 26, 64, 100, 5, K)
    p.thick(36, 56, 92, 56, 5, K)


# -- municion
@icon("flecha", "Flechas")
def _(p):
    p.thick(18, 110, 104, 24, 5)
    p.poly([(96, 14), (116, 12), (114, 32), (104, 24)])
    p.poly([(18, 110), (10, 92), (24, 96)])
    p.poly([(18, 110), (36, 118), (32, 104)])


@icon("virote", "Virotes")
def _(p):
    p.thick(26, 102, 96, 32, 8)
    p.poly([(88, 22), (110, 18), (106, 40)])
    p.poly([(26, 102), (16, 86), (34, 92)])


@icon("piedras_honda", "Piedras de honda")
def _(p):
    for cx, cy, r in ((44, 82, 20), (84, 86, 18), (64, 52, 18), (98, 54, 12), (30, 50, 11)):
        p.ell(cx, cy, r, r * 0.85)


@icon("bala", "Balas")
def _(p):
    for cx, cy in ((44, 76), (84, 76), (64, 44)):
        p.ell(cx, cy, 18)
        p.ell(cx - 6, cy - 6, 5, c=K)


# -- materiales
@icon("lena", "Leña")
def _(p):
    for y in (36, 64, 92):
        p.rect(14, y - 11, 104, y + 11, r=11)
        p.ell(104, y, 13)
        p.ring(104, y, 6, 3, K)


@icon("piedra", "Piedra")
def _(p):
    p.poly([(18, 102), (26, 56), (54, 30), (88, 34), (112, 64), (108, 102)])
    p.line([(54, 50), (66, 70), (60, 88)], 4, K)


@icon("pieles", "Pieles")
def _(p):
    p.poly([(28, 20), (50, 30), (78, 30), (100, 20), (96, 46), (110, 60), (104, 82), (112, 110), (82, 98), (46, 98),
            (16, 110), (24, 82), (18, 60), (32, 46)])


@icon("plumas", "Plumas")
def _(p):
    p.poly([(30, 112), (40, 70), (62, 30), (92, 10), (96, 40), (82, 72), (52, 98)])
    p.line([(28, 118), (88, 18)], 4, K)


@icon("hueso", "Hueso")
def _(p):
    p.thick(36, 92, 92, 36, 14)
    for cx, cy in ((28, 92), (36, 100), (92, 28), (100, 36)):
        p.ell(cx, cy, 13)


@icon("tendones", "Tendones")
def _(p):
    for r in (40, 28, 16):
        p.ring(64, 64, r, 6)
    p.line([(104, 64), (120, 96)], 6)


@icon("pedernal", "Pedernal")
def _(p):
    p.poly([(40, 112), (24, 64), (52, 18), (96, 30), (110, 74), (82, 110)])
    p.poly([(52, 34), (72, 30), (62, 58)], K)
    p.poly([(84, 62), (98, 76), (80, 92)], K)


@icon("cuerda", "Cuerda")
def _(p):
    p.ell(64, 60, 44, 36)
    p.ell(64, 60, 30, 22, K)
    p.ell(64, 60, 20, 14)
    p.ell(64, 60, 10, 6, K)
    p.line([(100, 80), (118, 112)], 8)


@icon("mineral", "Mineral")
def _(p):
    p.poly([(16, 100), (30, 52), (62, 34), (100, 46), (114, 92), (82, 112), (38, 114)])
    for cx, cy in ((50, 70), (80, 64), (66, 92)):
        p.poly([(cx, cy - 9), (cx + 8, cy), (cx, cy + 9), (cx - 8, cy)], K)


@icon("lingote", "Metal: lingote")
def _(p):
    p.poly([(10, 96), (30, 56), (98, 56), (118, 96)])
    p.poly([(30, 56), (40, 40), (88, 40), (98, 56)])
    p.rect(10, 96, 118, 104, K)


@icon("carbon", "Carbón")
def _(p):
    for cx, cy, r in ((46, 80, 26), (86, 84, 22), (66, 50, 22)):
        p.poly([(cx - r, cy), (cx - r * 0.4, cy - r), (cx + r * 0.6, cy - r * 0.8), (cx + r, cy + r * 0.2),
                (cx + r * 0.3, cy + r), (cx - r * 0.7, cy + r * 0.8)])


# -- consumibles
@icon("carne", "Carne fresca")
def _(p):
    p.poly(rot([(14, 40), (40, 16), (76, 18), (98, 44), (92, 70), (64, 82), (30, 78), (12, 60)], 56, 48, 30))
    p.thick(78, 78, 102, 102, 14)
    p.ell(110, 98, 11)
    p.ell(98, 112, 11)
    p.ell(48, 46, 10, 7, K)


@icon("carne_seca", "Carne seca")
def _(p):
    for i, y in enumerate((34, 62, 90)):
        p.poly([(16, y), (104, y - 10), (112, y + 6), (24, y + 18)])
        p.rect(30 + i * 20, y - 2, 36 + i * 20, y + 8, K)


@icon("hierbas", "Hierbas curativas")
def _(p):
    p.thick(64, 120, 64, 50, 5)
    for (cx, cy, a) in ((44, 76, -35), (84, 66, 35), (46, 44, -30), (82, 36, 30), (64, 22, 0)):
        p.poly(rot([(cx - 16, cy), (cx, cy - 10), (cx + 16, cy), (cx, cy + 10)], cx, cy, a))


@icon("miel", "Miel: olla")
def _(p):
    p.ell(64, 80, 40, 34)
    p.rect(40, 34, 88, 52)
    p.rect(34, 30, 94, 38)
    p.poly([(52, 52), (62, 52), (60, 70), (56, 74), (52, 70)], K)


@icon("leche", "Leche: jarra")
def _(p):
    p.poly([(40, 20), (84, 20), (80, 40), (96, 70), (92, 114), (32, 114), (28, 70), (44, 40)])
    p.arc(96, 74, 18, 270, 90, 7)


@icon("unguento", "Ungüento")
def _(p):
    p.rect(30, 50, 98, 112, r=12)
    p.rect(36, 28, 92, 50, r=6)
    p.rect(44, 70, 84, 92, K, r=4)
    p.poly([(64, 74), (76, 84), (64, 90), (52, 84)])


@icon("agua", "Odre de agua")
def _(p):
    p.poly([(50, 14), (78, 14), (76, 30), (104, 54), (110, 96), (90, 116), (38, 116), (18, 96), (24, 54), (52, 30)])
    p.poly([(56, 60), (68, 80), (60, 96), (48, 82)], K)


@icon("herramienta", "Herramienta")
def _(p):
    p.thick(30, 108, 86, 36, 10)
    p.poly(rot([(66, 14), (110, 14), (110, 36), (66, 36)], 88, 25, -38))


@icon("mensaje", "Mensaje")
def _(p):
    p.rect(18, 34, 110, 98, r=4)
    p.poly([(18, 36), (64, 72), (110, 36)], K)
    p.poly([(26, 36), (64, 64), (102, 36)])


@icon("bulto", "Objeto o bulto")
def _(p):
    p.poly([(64, 12), (114, 36), (114, 92), (64, 118), (14, 92), (14, 36)])
    p.line([(14, 36), (64, 60), (114, 36)], 5, K)
    p.line([(64, 60), (64, 118)], 5, K)


@icon("estructura", "Estructura")
def _(p):
    yurt(p)


@icon("animal", "Animal")
def _(p):
    p.ell(62, 62, 38, 22)
    p.thick(92, 58, 108, 30, 14)
    p.ell(112, 28, 12, 9)
    for x in (34, 46, 76, 88):
        p.thick(x, 70, x, 112, 9)
    p.line([(24, 56), (12, 84)], 6)


@icon("persona", "Persona")
def _(p):
    p.ell(64, 26, 16)
    p.poly([(40, 46), (88, 46), (98, 86), (86, 88), (82, 66), (80, 118), (66, 118), (64, 86), (62, 118), (48, 118),
            (46, 66), (42, 88), (30, 86)])


# -- estados y datos
@icon("corte", "Contra el corte")
def _(p):
    p.poly([(16, 108), (100, 16), (112, 28), (28, 116)])
    p.poly([(16, 108), (8, 120), (28, 116)])


@icon("golpe", "Contra el golpe")
def _(p):
    p.ell(64, 64, 26)
    for a in range(0, 360, 45):
        x0, y0 = 64 + math.cos(math.radians(a)) * 36, 64 + math.sin(math.radians(a)) * 36
        x1, y1 = 64 + math.cos(math.radians(a)) * 56, 64 + math.sin(math.radians(a)) * 56
        p.thick(x0, y0, x1, y1, 8)


@icon("punta", "Contra la punta")
def _(p):
    p.poly([(64, 8), (88, 56), (72, 56), (72, 120), (56, 120), (56, 56), (40, 56)])


@icon("cobertura", "Cobertura")
def _(p):
    p.poly([(64, 8), (110, 26), (104, 80), (64, 120), (24, 80), (18, 26)])
    p.poly([(64, 24), (94, 36), (90, 74), (64, 102)], K)


@icon("vida", "Vida")
def _(p):
    p.ell(44, 46, 26)
    p.ell(84, 46, 26)
    p.poly([(20, 56), (108, 56), (64, 112)])


@icon("sangre", "Sangre")
def _(p):
    p.poly([(64, 10), (92, 60), (36, 60)])
    p.ell(64, 78, 32)
    p.ell(54, 76, 8, 12, K)


@icon("veneno", "Veneno")
def _(p):
    p.ell(64, 52, 36, 34)
    p.rect(44, 76, 84, 106, r=6)
    p.ell(50, 52, 10, c=K)
    p.ell(78, 52, 10, c=K)
    for x in (52, 64, 76):
        p.rect(x - 2, 90, x + 2, 106, K)


@icon("peso", "Peso")
def _(p):
    p.poly([(26, 112), (38, 44), (90, 44), (102, 112)])
    p.ring(64, 34, 14, 7)


@icon("tiempo", "Tiempo")
def _(p):
    p.rect(26, 10, 102, 20)
    p.rect(26, 108, 102, 118)
    p.poly([(34, 20), (94, 20), (68, 64), (94, 108), (34, 108), (60, 64)])
    p.poly([(44, 28), (84, 28), (64, 54)], K)


@icon("ok", "Se puede")
def _(p):
    p.line([(20, 66), (50, 96), (108, 32)], 16)


@icon("falta", "Falta algo")
def _(p):
    p.line([(26, 26), (102, 102)], 16)
    p.line([(102, 26), (26, 102)], 16)


@icon("tribu", "La tribu")
def _(p):
    for cx, k in ((36, 0.8), (92, 0.8), (64, 1.0)):
        p.ell(cx, 40 - 4 * k, 13 * k)
        p.d.pieslice((cx - 26 * k, 58 - 4 * k, cx + 26 * k, 110 + 30 * k), 180, 360, fill=W)


@icon("velocidad", "Velocidad")
def _(p):
    p.poly([(10, 30), (60, 64), (10, 98), (24, 64)])
    p.poly([(60, 30), (110, 64), (60, 98), (74, 64)])


# -- idioma, tatuajes, joyas, campamento, viajes (version 2: se añaden al atlas sin tocar las celdas ya pintadas)
@icon("idioma", "Idioma: globo")
def _(p):
    p.ring(64, 64, 50, 8)
    p.arc(64, 64, 50, 0, 360, 8)
    p.d.ellipse((44, 14, 84, 114), outline=W, width=7)
    p.thick(14, 64, 114, 64, 7)
    p.thick(22, 40, 106, 40, 5)
    p.thick(22, 88, 106, 88, 5)


@icon("druida", "Druida: báculo y capucha")
def _(p):
    p.poly([(40, 120), (46, 52), (54, 30), (64, 22), (76, 30), (84, 52), (90, 120)])
    p.ell(64, 44, 10, 12, K)
    p.thick(100, 120, 100, 22, 7)
    p.ring(100, 18, 10, 5)


@icon("orfebre", "Orfebre: anillo y martillo")
def _(p):
    p.ring(48, 76, 30, 10)
    p.poly([(36, 40), (48, 28), (60, 40), (48, 52)])
    p.thick(70, 110, 108, 56, 9)
    p.poly(rot([(92, 30), (124, 30), (124, 50), (92, 50)], 108, 40, -55))


@icon("anillo", "Anillo")
def _(p):
    p.ring(64, 76, 34, 12)
    p.poly([(48, 36), (64, 18), (80, 36), (64, 52)])
    p.poly([(56, 36), (64, 28), (72, 36), (64, 44)], K)


@icon("brazalete", "Brazalete")
def _(p):
    p.d.ellipse((14, 34, 114, 94), outline=W, width=16)
    for x in (34, 64, 94):
        p.ell(x, 40 if x == 64 else 46, 7, c=K)


@icon("collar", "Collar")
def _(p):
    p.arc(64, 30, 46, 20, 160, 7)
    for a in range(30, 160, 22):
        x, y = 64 + math.cos(math.radians(a)) * 46, 30 + math.sin(math.radians(a)) * 46
        p.ell(x, y, 7)
    p.poly([(52, 80), (76, 80), (64, 112)])


@icon("arete", "Aretes")
def _(p):
    for cx in (40, 88):
        p.ring(cx, 30, 12, 6)
        p.thick(cx, 42, cx, 62, 5)
        p.poly([(cx, 60), (cx + 16, 86), (cx, 112), (cx - 16, 86)])


@icon("hebilla", "Hebilla de cinturón")
def _(p):
    p.rect(8, 50, 120, 78)
    p.rect(30, 30, 98, 98, r=10)
    p.rect(42, 42, 86, 86, K, r=6)
    p.thick(64, 42, 64, 86, 7)


@icon("gema", "Piedra preciosa")
def _(p):
    p.poly([(30, 40), (98, 40), (118, 62), (64, 118), (10, 62)])
    p.line([(10, 62), (118, 62)], 4, K)
    p.line([(30, 40), (46, 62), (64, 118)], 4, K)
    p.line([(98, 40), (82, 62), (64, 118)], 4, K)


@icon("guardian", "Guardián del campamento")
def _(p):
    p.ell(56, 24, 14)
    p.poly([(36, 42), (76, 42), (84, 82), (74, 84), (72, 120), (40, 120), (38, 84), (28, 82)])
    p.thick(100, 124, 100, 14, 7)
    p.poly([(92, 22), (100, 4), (108, 22)])


@icon("reclutar", "Reclutar")
def _(p):
    p.ell(52, 30, 16)
    p.poly([(28, 52), (76, 52), (86, 118), (18, 118)])
    p.rect(94, 40, 104, 80)
    p.rect(80, 55, 118, 65)


@icon("entrenar", "Capacitar en un oficio")
def _(p):
    p.ell(48, 34, 16)
    p.poly([(24, 56), (72, 56), (82, 120), (14, 120)])
    p.poly([(98, 14), (106, 34), (126, 36), (110, 50), (116, 70), (98, 58), (80, 70), (86, 50), (70, 36), (90, 34)])


@icon("disolver", "Disolver el campamento")
def _(p):
    yurt(p, 0, 18, 0.7)
    flame(p, 64, 46, 0.6)


@icon("mochila_grande", "Mochila grande")
def _(p):
    p.rect(20, 22, 108, 120, r=16)
    p.rect(30, 56, 98, 70, K)
    p.rect(30, 84, 98, 110, K, r=6)
    p.rect(34, 88, 94, 106, r=4)
    p.arc(64, 22, 20, 180, 360, 8)


@icon("soltar", "Dejar la mochila en el suelo")
def _(p):
    p.rect(30, 8, 98, 70, r=12)
    p.thick(64, 74, 64, 104, 10)
    p.poly([(44, 96), (84, 96), (64, 120)])
    p.rect(10, 116, 118, 122)


@icon("recoger", "Recoger la mochila")
def _(p):
    p.rect(30, 58, 98, 120, r=12)
    p.thick(64, 54, 64, 22, 10)
    p.poly([(44, 30), (84, 30), (64, 6)])


@icon("mensajero", "Mensajero: cuerno")
def _(p):
    p.poly([(14, 76), (90, 40), (110, 20), (118, 60), (100, 100), (90, 84), (20, 92)])
    p.ell(16, 84, 10)


@icon("despachar", "Despachar seguidores")
def _(p):
    p.ell(30, 40, 12)
    p.poly([(14, 56), (46, 56), (52, 112), (8, 112)])
    p.thick(60, 70, 104, 70, 8)
    p.poly([(98, 54), (124, 70), (98, 86)])
    p.rect(64, 106, 124, 110)


@icon("lugar", "Lugar conocido")
def _(p):
    p.ell(64, 48, 36)
    p.poly([(32, 62), (96, 62), (64, 122)])
    p.ell(64, 48, 14, c=K)


@icon("alimentar", "Alimentar a los animales")
def _(p):
    p.d.chord((14, 40, 114, 120), 0, 180, fill=W)
    p.rect(14, 76, 114, 82)
    for x, y in ((40, 64), (58, 56), (76, 62), (92, 54), (50, 44), (70, 40)):
        p.ell(x, y, 8, 6)


@icon("pastar", "Pastar")
def _(p):
    for x, h in ((18, 70), (36, 90), (54, 60), (72, 96), (90, 72), (108, 86)):
        p.poly([(x - 8, 118), (x + 2, 118 - h), (x + 8, 118)])
    p.rect(8, 112, 120, 120)


@icon("soldado", "Soldado")
def _(p):
    p.d.pieslice((40, 6, 76, 42), 180, 360, fill=W)
    p.ell(58, 30, 13)
    p.poly([(36, 46), (80, 46), (86, 120), (30, 120)])
    p.poly([(70, 60), (112, 60), (110, 94), (91, 112), (72, 94)])


@icon("pastor", "Pastor: cayado")
def _(p):
    p.thick(48, 122, 48, 34, 8)
    p.arc(68, 34, 20, 180, 360, 8)
    p.thick(88, 34, 88, 50, 8)
    p.ell(96, 98, 22, 16)
    p.ell(114, 88, 8, 7)


@icon("cazador", "Cazador")
def _(p):
    p.arc(10, 64, 58, 300, 60, 7)
    p.thick(39, 14, 39, 114, 3)
    p.thick(30, 64, 118, 64, 5)
    p.poly([(110, 54), (126, 64), (110, 74)])


@icon("explorador", "Explorador: ojo")
def _(p):
    p.d.chord((8, 30, 120, 130), 200, 340, fill=W)
    p.d.chord((8, -2, 120, 98), 20, 160, fill=W)
    p.ell(64, 64, 22, c=K)
    p.ell(64, 64, 12)


@icon("nivel", "Nivel: estrella")
def _(p):
    pts = []
    for i in range(10):
        a = math.radians(-90 + i * 36)
        r = 56 if i % 2 == 0 else 24
        pts.append((64 + math.cos(a) * r, 66 + math.sin(a) * r))
    p.poly(pts)


@icon("bloqueado", "Bloqueado: candado")
def _(p):
    p.rect(26, 58, 102, 118, r=8)
    p.arc(64, 58, 26, 180, 360, 10)
    p.thick(38, 58, 38, 64, 10)
    p.thick(90, 58, 90, 64, 10)
    p.ell(64, 84, 8, c=K)
    p.rect(60, 86, 68, 104, K)


@icon("dialogo", "Hablar")
def _(p):
    p.rect(10, 18, 118, 86, r=18)
    p.poly([(30, 80), (56, 80), (24, 114)])
    for x in (38, 64, 90):
        p.ell(x, 52, 7, c=K)


@icon("mapa", "Mapa")
def _(p):
    p.poly([(8, 24), (44, 12), (84, 24), (120, 12), (120, 104), (84, 116), (44, 104), (8, 116)])
    p.thick(44, 14, 44, 102, 4, K)
    p.thick(84, 26, 84, 114, 4, K)


@icon("frio", "Frio: copo de nieve")
def _(p):
    for deg in (0, 60, 120):
        a = math.radians(deg)
        dx, dy = math.cos(a) * 54, math.sin(a) * 54
        p.thick(64 - dx, 64 - dy, 64 + dx, 64 + dy, 9)
        for sgn in (-1, 1):
            ex, ey = 64 + sgn * dx * 0.62, 64 + sgn * dy * 0.62
            for b in (-35, 35):
                bb = math.radians(deg + b + (0 if sgn > 0 else 180))
                p.thick(ex, ey, ex + math.cos(bb) * 20, ey + math.sin(bb) * 20, 6)


@icon("sol", "Calor: sol")
def _(p):
    p.ell(64, 64, 28)
    for i in range(8):
        a = math.radians(i * 45)
        p.thick(64 + math.cos(a) * 38, 64 + math.sin(a) * 38, 64 + math.cos(a) * 58, 64 + math.sin(a) * 58, 9)


@icon("ropa", "Ropa: tunica")
def _(p):
    p.poly([(44, 14), (84, 14), (122, 40), (106, 62), (92, 52), (96, 118), (32, 118), (36, 52), (22, 62), (6, 40)])
    p.poly([(52, 14), (64, 34), (76, 14)], c=K)


@icon("sombrero", "Sombrero o gorro")
def _(p):
    p.d.ellipse((6, 76, 122, 100), fill=W)
    p.rect(36, 34, 92, 86, r=14)
    p.rect(36, 66, 92, 74, K)


@icon("capa", "Capa o abrigo")
def _(p):
    p.poly([(46, 12), (82, 12), (90, 30), (118, 118), (10, 118), (38, 30)])
    p.ell(64, 22, 9, c=K)
    p.thick(64, 34, 64, 112, 4, K)


@icon("escarmiento", "Escarmiento: colmillos")
def _(p):
    p.d.chord((10, 10, 118, 80), 0, 180, fill=W)
    for x in (34, 94):
        p.poly([(x - 14, 46), (x + 14, 46), (x, 118)])
    for x in (52, 64, 76):
        p.poly([(x - 6, 46), (x + 6, 46), (x, 70)])


@icon("hervir", "Hervir: caldero con vapor")
def _(p):
    p.d.chord((14, 52, 114, 124), 0, 180, fill=W)
    p.rect(10, 52, 118, 64, r=4)
    for x in (40, 64, 88):
        p.arc(x - 6, 30, 10, 270, 90, 5)
        p.arc(x + 6, 14, 10, 90, 270, 5)


@icon("jarra", "Cerveza: jarra")
def _(p):
    p.rect(28, 30, 88, 118, r=8)
    p.arc(88, 74, 22, 270, 90, 10)
    p.ell(40, 30, 14)
    p.ell(62, 24, 16)
    p.ell(82, 30, 12)


@icon("vino", "Vino: anfora")
def _(p):
    p.d.ellipse((30, 40, 98, 112), fill=W)
    p.rect(52, 12, 76, 46)
    p.rect(46, 8, 82, 18, r=4)
    p.arc(46, 40, 14, 90, 270, 7)
    p.arc(82, 40, 14, 270, 90, 7)
    p.poly([(56, 108), (72, 108), (64, 124)])


@icon("espiritus", "Espiritus malditos: fantasma")
def _(p):
    p.d.chord((24, 10, 104, 90), 180, 360, fill=W)
    p.rect(24, 50, 104, 100)
    for i, x in enumerate((24, 44, 64, 84)):
        p.poly([(x, 100), (x + 20, 100), (x + 10, 120 if i % 2 == 0 else 110)])
    p.ell(48, 52, 9, c=K)
    p.ell(80, 52, 9, c=K)
    p.ell(64, 78, 7, c=K)


@icon("beber", "Beber: gota")
def _(p):
    p.poly([(64, 8), (98, 70), (30, 70)])
    p.ell(64, 80, 36)
    p.ell(52, 82, 8, c=K)


# ------------------------------------------------------------------ atlas
def render(fn):
    pen = Pen()
    fn(pen)
    big = pen.img
    small = big.resize((CELL, CELL), Image.LANCZOS)
    # Silueta nitida: blanco donde hay tinta, transparente fuera.
    a = small.getchannel("A").point(lambda v: 255 if v >= 110 else 0)
    sil = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    sil.paste((255, 255, 255, 255), mask=a)
    # Contorno oscuro de 1 px (se lee sobre cualquier fondo).
    out = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    px, sp = out.load(), sil.load()
    for y in range(CELL):
        for x in range(CELL):
            if sp[x, y][3]:
                continue
            near = any(0 <= x + dx < CELL and 0 <= y + dy < CELL and sp[x + dx, y + dy][3]
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if near:
                px[x, y] = (24, 16, 10, 220)
    out.alpha_composite(sil)
    return out


def atlas_size():
    rows = (len(ICONS) + COLS - 1) // COLS
    return COLS * CELL, rows * CELL


def read_tsv_map():
    """nombre -> indice segun iconos.tsv (vacio si no existe)."""
    m = {}
    if os.path.exists(TSV):
        for line in open(TSV, encoding="utf-8"):
            if line.startswith("#") or line.startswith("indice"):
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 2 and parts[0].isdigit():
                m[parts[1]] = int(parts[0])
    return m


def generar(force):
    """Sin --forzar: añade al atlas solo los iconos nuevos, en celdas libres; lo pintado no se toca."""
    if os.path.exists(OUT) and not force:
        cells = read_tsv_map()
        missing = [(n, d, fn) for n, d, fn in ICONS if n not in cells]
        if not missing:
            print(f"{os.path.relpath(OUT, ROOT)} ya tiene todos los iconos (tus cambios se respetan; --forzar lo redibuja).")
            return
        img = Image.open(OUT).convert("RGBA")
        nxt = max(cells.values(), default=-1) + 1
        rows = (nxt + len(missing) + COLS - 1) // COLS
        if rows * CELL > img.height:  # crece hacia abajo
            big = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
            big.alpha_composite(img, (0, 0))
            img = big
        for n, d, fn in missing:
            img.alpha_composite(render(fn), ((nxt % COLS) * CELL, (nxt // COLS) * CELL))
            cells[n] = nxt
            nxt += 1
        img.save(OUT, optimize=True)
        write_tsv(cells)
        print(f"Añadidos {len(missing)} iconos a {os.path.relpath(OUT, ROOT)}: {', '.join(n for n, _, _ in missing)}.")
        return
    w, h = atlas_size()
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for i, (name, desc, fn) in enumerate(ICONS):
        img.alpha_composite(render(fn), ((i % COLS) * CELL, (i // COLS) * CELL))
    img.save(OUT, optimize=True)
    write_tsv({n: i for i, (n, _, _) in enumerate(ICONS)})
    print(f"Escrito {os.path.relpath(OUT, ROOT)} ({len(ICONS)} iconos, {w}x{h}).")


def write_tsv(cells):
    desc = {n: d for n, d, _ in ICONS}
    with open(TSV, "w", encoding="utf-8") as f:
        f.write("# Iconos de los menus: celda del atlas assets/ui/iconos.png (32x32, 16 columnas).\n")
        f.write("# El juego lo lee al arrancar: si mueves un icono de celda, cambia aqui su indice.\n")
        f.write("indice\tnombre\tcolumna\tfila\tdescripcion\n")
        for n, i in sorted(cells.items(), key=lambda kv: kv[1]):
            f.write(f"{i}\t{n}\t{i % COLS}\t{i // COLS}\t{desc.get(n, '')}\n")


def guia():
    cells = read_tsv_map() or {n: i for i, (n, _, _) in enumerate(ICONS)}
    write_tsv(cells)  # conserva tus celdas; solo refresca columnas y descripciones
    names = {i: n for n, i in cells.items()}
    print(f"Escrito {os.path.relpath(TSV, ROOT)}.")
    src = Image.open(OUT).convert("RGBA") if os.path.exists(OUT) else None
    if src is None:
        print("Falta iconos.png: ejecuta 'generar' primero.")
        return
    Z = 4
    cw, ch = CELL * Z, CELL * Z + 14
    rows = (src.height // CELL)
    g = Image.new("RGBA", (COLS * cw, rows * ch), (40, 28, 20, 255))
    d = ImageDraw.Draw(g)
    font = ImageFont.load_default()
    for r in range(rows):
        for c in range(COLS):
            i = r * COLS + c
            x, y = c * cw, r * ch
            d.rectangle((x, y, x + cw - 1, y + ch - 1), outline=(120, 82, 28, 255))
            cell = src.crop((c * CELL, r * CELL, (c + 1) * CELL, (r + 1) * CELL)).resize((cw, cw), Image.NEAREST)
            g.alpha_composite(cell, (x, y))
            if i in names:
                d.text((x + 3, y + cw + 1), f"{i} {names[i]}", fill=(242, 228, 192, 255), font=font)
    g.save(GUIDE, optimize=True)
    print(f"Escrito {os.path.relpath(GUIDE, ROOT)}.")


def check():
    names = [n for n, _, _ in ICONS]
    ok = True
    tsv = []
    if os.path.exists(TSV):
        for line in open(TSV, encoding="utf-8"):
            if line.startswith("#") or line.startswith("indice"):
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 2:
                tsv.append(parts[1])
    else:
        print("Falta assets/ui/iconos.tsv: ejecuta 'guia'.")
        ok = False
    src = open(ICONS_C, encoding="utf-8").read() if os.path.exists(ICONS_C) else ""
    m = re.search(r"ICON_NAMES\[ICON_COUNT\]\s*=\s*\{(.*?)\};", src, re.S)
    cnames = re.findall(r'"([a-z_0-9]+)"', m.group(1)) if m else []
    if not cnames:
        print("No encuentro ICON_NAMES en src/ui/icons.c.")
        ok = False
    for n in cnames:
        if n not in tsv:
            print(f"El juego pide el icono '{n}', que no esta en iconos.tsv.")
            ok = False
    for n in tsv:
        if n not in names:
            print(f"iconos.tsv nombra '{n}', que el generador no conoce.")
    if os.path.exists(OUT):
        iw, ih = png_size(OUT)
        cells = (iw // CELL) * (ih // CELL)
        if iw % CELL or ih % CELL:
            print("iconos.png no es multiplo de 32 px.")
            ok = False
        if cells < len(tsv):
            print("iconos.png tiene menos celdas que iconos.tsv.")
            ok = False
    else:
        print("Falta assets/ui/iconos.png.")
        ok = False
    print(f"{len(cnames)} iconos en el juego, {len(tsv)} en el atlas: {'bien' if ok else 'HAY ERRORES'}.")
    return ok


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    if args[0] in ("generar", "guia") and Image is None:
        print("Hace falta Pillow: pip install pillow")
        return 1
    if args[0] == "generar":
        generar("--forzar" in args)
        return 0
    if args[0] == "guia":
        guia()
        return 0
    if args[0] == "check":
        return 0 if check() else 1
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main())
