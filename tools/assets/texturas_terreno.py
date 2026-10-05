#!/usr/bin/env python3
"""Texturas del terreno de ESTEPA: un atlas pequeño (low-res, pixeles nitidos) editable.

Uso:
  python3 tools/assets/texturas_terreno.py generar [--forzar]   escribe assets/terrain/texturas.png (no pisa tus cambios sin --forzar)
  python3 tools/assets/texturas_terreno.py guia                 assets/terrain/texturas_guia.png (x4, con nombres)

El atlas tiene celdas de 32x32 en 4 columnas (128x128). Cada celda es un detalle en tonos casi
grises: el juego la multiplica por el color del suelo (que cambia con la region y la estacion),
asi que conviene pintar luces y sombras, no colores fuertes. Una celda cubre 8 m de suelo.
El orden de las celdas lo fija el juego (src/world/terrain.c, TerrainTex): no lo cambies.

Inspiradas en fotos de referencia: las matas de pasto de la estepa (montículos), el pasto alto
y seco, el suelo del bosque de alerces, la tundra de grava del altiplano, el musgo de la costa,
las ondas de la arena, los estratos del muro del desierto, la grava de los rios trenzados, la
nieve y las grietas del glaciar, la roca, el barro de las orillas, el muro de hielo, la arena
volcanica negra de los fiordos, los cantos rodados de los rios y los seracs del glaciar.
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "assets", "terrain", "texturas.png")
GUIDE = os.path.join(ROOT, "assets", "terrain", "texturas_guia.png")
CELL = 32
COLS = 4

NAMES = [
    "estepa_matas", "estepa_pasto_alto", "bosque_suelo", "altiplano_tundra",
    "costa_musgo", "desierto_arena", "estratos", "grava_trenzada",
    "nieve_glaciar", "roca", "barro_orilla", "muro_hielo",
    "arena_volcanica", "cantos_rodados", "hielo_seracs", "blanco",
]


def clamp(v):
    return max(0, min(255, int(v)))


def tile(fn, seed):
    rnd = random.Random(seed)
    img = Image.new("RGB", (CELL, CELL))
    px = img.load()
    for y in range(CELL):
        for x in range(CELL):
            r, g, b = fn(x, y, rnd)
            px[x, y] = (clamp(r), clamp(g), clamp(b))
    return img


def wrap_noise(seed, freq):
    """Ruido de valores que se repite en la celda (sin costuras al repetir)."""
    rnd = random.Random(seed)
    n = freq
    grid = [[rnd.random() for _ in range(n)] for _ in range(n)]

    def f(x, y):
        fx, fy = x / CELL * n, y / CELL * n
        x0, y0 = int(fx) % n, int(fy) % n
        x1, y1 = (x0 + 1) % n, (y0 + 1) % n
        tx, ty = fx - int(fx), fy - int(fy)
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        a = grid[y0][x0] * (1 - tx) + grid[y0][x1] * tx
        b = grid[y1][x0] * (1 - tx) + grid[y1][x1] * tx
        return a * (1 - ty) + b * ty

    return f


def gray(v, tint=(1.0, 1.0, 1.0)):
    return v * tint[0], v * tint[1], v * tint[2]


def steppe_tussock():
    # Montículos de pasto: cupulas redondas con luz arriba a la izquierda y sombra abajo.
    rnd = random.Random(1)
    mounds = [(rnd.uniform(0, CELL), rnd.uniform(0, CELL), rnd.uniform(3.0, 5.0)) for _ in range(9)]
    n = wrap_noise(2, 8)

    def f(x, y, _):
        v = 150 + 40 * n(x, y)
        for mx, my, r in mounds:
            for ox in (-CELL, 0, CELL):
                for oy in (-CELL, 0, CELL):
                    dx, dy = x - mx - ox, y - my - oy
                    d = math.hypot(dx, dy)
                    if d < r:
                        v = 180 + 35 * (-(dx + dy) / (2 * r)) + 15 * (1 - d / r)
                    elif d < r + 1.6 and dx + dy > 0:
                        v = min(v, 125)
        return gray(v, (1.02, 1.0, 0.92))

    return f


def tall_grass():
    n = wrap_noise(3, 16)

    def f(x, y, rnd):
        streak = math.sin((x * 1.7 + y * 0.35) * 0.9 + 6.0 * n(x, y))
        return gray(165 + 45 * streak + rnd.uniform(-12, 12), (1.03, 1.0, 0.9))

    return f


def forest_floor():
    n = wrap_noise(4, 8)

    def f(x, y, rnd):
        v = 140 + 50 * n(x, y) + rnd.uniform(-30, 30)
        if rnd.random() < 0.12:
            v += 50  # agujas al sol
        return gray(v, (0.96, 1.0, 0.92))

    return f


def tundra():
    def f(x, y, rnd):
        v = 150 + rnd.uniform(-45, 45)
        if (x * 7 + y * 13) % 11 == 0:
            v = 215  # liquen claro
        return gray(v, (1.0, 1.0, 1.0))

    return f


def moss():
    n = wrap_noise(5, 8)

    def f(x, y, rnd):
        v = 150 + 60 * n(x, y) + rnd.uniform(-15, 15)
        if rnd.random() < 0.05:
            v = 215  # piedra entre el musgo
        return gray(v, (0.95, 1.02, 0.97))

    return f


def sand():
    def f(x, y, rnd):
        ripple = math.sin((y + 0.4 * x) * 2 * math.pi / 8.0)
        return gray(185 + 35 * ripple + rnd.uniform(-8, 8), (1.03, 1.0, 0.95))

    return f


def strata():
    n = wrap_noise(6, 4)

    def f(x, y, rnd):
        band = math.sin((y + 3 * n(x, y)) * 2 * math.pi / 6.4)
        v = 170 + 45 * band + rnd.uniform(-10, 10)
        return gray(v, (1.05, 0.95, 0.88))

    return f


def braided_gravel():
    n = wrap_noise(7, 4)

    def f(x, y, rnd):
        # Canalitos claros a lo largo (como los lechos trenzados vistos desde arriba).
        c = abs(math.sin((x + 9 * n(x, y)) * 2 * math.pi / 10.7))
        v = 135 + rnd.uniform(-25, 25) + (60 if c < 0.18 else 0)
        return gray(v, (0.96, 0.99, 1.04))

    return f


def snow_glacier():
    n = wrap_noise(8, 4)

    def f(x, y, rnd):
        flow = math.sin((y + 6 * n(x, y)) * 2 * math.pi / 16)
        v = 225 + 18 * flow + rnd.uniform(-6, 6)
        if abs(math.sin((x * 0.9 + y * 0.2) * 2 * math.pi / 32)) < 0.04:
            v = 160  # grieta
        return gray(v, (0.98, 1.0, 1.04))

    return f


def rock():
    n = wrap_noise(9, 8)

    def f(x, y, rnd):
        v = 150 + 55 * n(x, y) + rnd.uniform(-15, 15)
        if abs(n(x, y) - 0.5) < 0.03:
            v = 90  # fisura
        return gray(v)

    return f


def mud():
    # Barro seco cuarteado: grietas de Voronoi (repetidas en la celda).
    rnd = random.Random(11)
    pts = [(rnd.uniform(0, CELL), rnd.uniform(0, CELL)) for _ in range(10)]

    def f(x, y, r):
        ds = sorted(min(math.hypot(x - px - ox, y - py - oy) for ox in (-CELL, 0, CELL) for oy in (-CELL, 0, CELL)) for px, py in pts)
        crack = ds[1] - ds[0] < 1.1
        return gray((115 if crack else 172) + r.uniform(-10, 10), (1.02, 1.0, 0.95))

    return f


def ice_wall():
    n = wrap_noise(10, 8)

    def f(x, y, rnd):
        v = 200 + 40 * math.sin((x + 4 * n(x, y)) * 2 * math.pi / 5.3) + rnd.uniform(-8, 8)
        return gray(v, (0.92, 1.0, 1.1))

    return f


def volcanic_sand():
    # Arena negra de la costa de fiordos: grano fino, ondas suaves del oleaje, guijarros claros sueltos.
    n = wrap_noise(12, 8)

    def f(x, y, rnd):
        wave = math.sin((y + 3.0 * n(x, y)) * 2 * math.pi / 10.7)
        v = 150 + 22 * wave + rnd.uniform(-22, 22)
        if rnd.random() < 0.03:
            v = 215  # guijarro claro
        return gray(v, (0.98, 0.98, 1.0))

    return f


def cobbles():
    # Cantos rodados de la orilla de los rios: piedras redondas con luz arriba y junta oscura.
    rnd = random.Random(13)
    stones = [(rnd.uniform(0, CELL), rnd.uniform(0, CELL), rnd.uniform(2.8, 4.8), rnd.uniform(0.85, 1.1)) for _ in range(48)]

    def f(x, y, r):
        v = 95 + r.uniform(-8, 8)  # la junta entre piedras
        best = 1e9
        for sx, sy, rad, tone in stones:
            for ox in (-CELL, 0, CELL):
                for oy in (-CELL, 0, CELL):
                    dx, dy = x - sx - ox, y - sy - oy
                    d = math.hypot(dx, dy) / rad
                    if d < 1.0 and d < best:
                        best = d
                        v = (175 + 45 * (-(dx + dy) / (2 * rad)) - 25 * d * d) * tone
        return gray(v + r.uniform(-6, 6), (1.0, 0.99, 0.96))

    return f


def ice_serac():
    # El frente del glaciar: columnas de hielo con grietas verticales azules.
    n = wrap_noise(14, 4)

    def f(x, y, rnd):
        col = math.sin((x + 3.0 * n(x, y)) * 2 * math.pi / 6.4)
        v = 214 + 30 * col + rnd.uniform(-6, 6)
        if col < -0.75:
            v = 140  # grieta
        return gray(v, (0.86, 0.97, 1.12))

    return f


def flat(v):
    return lambda x, y, rnd: (v, v, v)


MAKERS = [steppe_tussock(), tall_grass(), forest_floor(), tundra(), moss(), sand(), strata(), braided_gravel(),
          snow_glacier(), rock(), mud(), ice_wall(), volcanic_sand(), cobbles(), ice_serac(), flat(255)]


def build():
    atlas = Image.new("RGB", (CELL * COLS, CELL * COLS))
    for i, fn in enumerate(MAKERS):
        atlas.paste(tile(fn, 100 + i), ((i % COLS) * CELL, (i // COLS) * CELL))
    return atlas


def generar(force):
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    if os.path.exists(OUT) and not force:
        print(f"{OUT} ya existe (puede tener tus cambios): usa --forzar para rehacerlo.")
        return
    build().save(OUT)
    print(f"Escrito {OUT}")


def guia():
    atlas = Image.open(OUT).convert("RGB")
    big = atlas.resize((atlas.width * 4, atlas.height * 4), Image.NEAREST)
    out = Image.new("RGB", (big.width, big.height + 12 * COLS), (20, 16, 12))
    out.paste(big, (0, 0))
    d = ImageDraw.Draw(out)
    for i, name in enumerate(NAMES):
        x, y = (i % COLS) * CELL * 4, (i // COLS) * CELL * 4
        d.rectangle((x, y, x + 110, y + 11), fill=(0, 0, 0))
        d.text((x + 2, y), f"{i} {name}", fill=(255, 230, 150))
    out.save(GUIDE)
    print(f"Escrito {GUIDE}")


def main():
    args = sys.argv[1:]
    if not args or args[0] not in ("generar", "guia"):
        print(__doc__)
        return 1
    if args[0] == "generar":
        generar("--forzar" in args)
    else:
        guia()
    return 0


if __name__ == "__main__":
    sys.exit(main())
