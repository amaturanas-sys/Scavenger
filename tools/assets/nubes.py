#!/usr/bin/env python3
"""Atlas de nubes de ESTEPA: recortes de cielo de fotografias de referencia, pixelados.

Uso:
  python3 tools/assets/nubes.py generar --fotos CARPETA [--forzar]   escribe assets/sky/nubes.png
  python3 tools/assets/nubes.py guia                                  assets/sky/nubes_guia.png (x4, con nombres)

El atlas es de 128x128 (potencia de 2: OpenGL ES 2), con 8 celdas de 64x32 en 2 columnas:
  fila 0: cumulos (buen tiempo)   fila 1: estratos (nubes altas y finas)
  fila 2: cielo cubierto          fila 3: nubarrones de tormenta
Cada celda sale de un recorte de cielo de una foto. Se reduce a 64x32 (pixeles nitidos), se
pasa a una paleta corta de grises azulados (el juego la tiñe con la luz del dia) y la forma se
recorta con transparencia binaria y tramado ordenado, como las texturas del suelo.

Las fotos no se guardan en el repo, solo el atlas. FUENTES dice de que foto (el comienzo del
nombre del archivo) y que recorte (x, y, ancho, alto en fraccion de la foto) sale cada celda.
"""
import glob
import math
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "assets", "sky", "nubes.png")
GUIDE = os.path.join(ROOT, "assets", "sky", "nubes_guia.png")
TW, TH = 64, 32

# (nombre, foto, recorte en fracciones, modo): "cielo_azul" separa la nube del azul por lo
# blanca; "cubierto" toma la forma de una elipse rota por la luz de la propia foto.
FUENTES = [
    ("cumulo_a", "94780071", (0.02, 0.00, 0.40, 0.20), "cielo_azul"),
    ("cumulo_b", "94780071", (0.50, 0.02, 0.40, 0.20), "cielo_azul"),
    ("estrato_a", "5f45a47d", (0.00, 0.08, 0.55, 0.22), "estrato"),
    ("estrato_b", "5f45a47d", (0.45, 0.06, 0.55, 0.22), "estrato"),
    ("cubierto_a", "5106c242", (0.00, 0.00, 0.50, 0.22), "cubierto"),
    ("cubierto_b", "5106c242", (0.50, 0.00, 0.50, 0.22), "cubierto"),
    ("tormenta_a", "c4835e2a", (0.00, 0.00, 0.45, 0.22), "cubierto"),
    ("tormenta_b", "c4835e2a", (0.55, 0.00, 0.45, 0.22), "cubierto"),
]

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
LEVELS = 6  # grises de la paleta


def find_photo(folder, prefix):
    hits = glob.glob(os.path.join(folder, prefix + "*"))
    if not hits:
        raise SystemExit(f"No encuentro la foto {prefix}* en {folder}")
    return hits[0]


def tile(folder, photo, box, mode):
    im = Image.open(find_photo(folder, photo)).convert("RGB")
    w, h = im.size
    x0, y0, bw, bh = box
    crop = im.crop((int(x0 * w), int(y0 * h), int((x0 + bw) * w), int((y0 + bh) * h)))
    small = crop.resize((TW, TH), Image.LANCZOS)
    px = small.load()
    lum = [[0.0] * TW for _ in range(TH)]
    for y in range(TH):
        for x in range(TW):
            r, g, b = px[x, y]
            lum[y][x] = (0.3 * r + 0.59 * g + 0.11 * b) / 255.0
    mean = sum(sum(row) for row in lum) / (TW * TH)
    alpha = [[0.0] * TW for _ in range(TH)]
    for y in range(TH):
        for x in range(TW):
            r, g, b = px[x, y]
            L = lum[y][x]
            # La forma: una elipse que no toca el borde de la celda, recortada por la foto.
            ex, ey = (x + 0.5 - TW / 2) / (TW / 2), (y + 0.5 - TH / 2) / (TH / 2)
            if mode == "estrato":
                ey *= 1.9  # nubes altas: largas y finas
            e = math.sqrt(ex * ex + ey * ey)
            border = max(0.0, min(1.0, (1.0 - e) * 4.0))
            if mode == "cielo_azul":  # lo blanco frente al azul
                blue = max(0.0, (b - (r + g) / 2) / 255.0)
                a = max(0.0, min(1.0, (L - 0.45) * 2.6 - blue * 3.0)) * border
            elif mode == "estrato":  # vetas mas claras que el cielo de alrededor
                a = max(0.0, min(1.0, 0.55 + (L - mean) * 6.0)) * border
            else:  # cubierto: la elipse rota por la luz de la foto
                a = max(0.0, min(1.0, 0.75 + (L - mean) * 2.5)) * border
            alpha[y][x] = a
    inside = [lum[y][x] for y in range(TH) for x in range(TW) if alpha[y][x] > 0.3] or [mean]
    lo, hi = min(inside), max(inside)
    dark = photo == "c4835e2a"  # nubarrones: la paleta baja
    v0, v1 = (0.28, 0.78) if dark else (0.5, 1.0)
    out = Image.new("RGBA", (TW, TH), (0, 0, 0, 0))
    po = out.load()
    for y in range(TH):
        for x in range(TW):
            if alpha[y][x] * 16.0 <= BAYER[y % 4][x % 4] + 0.5:
                continue
            t = (lum[y][x] - lo) / (hi - lo) if hi > lo else 0.5
            v = v0 + (v1 - v0) * max(0.0, min(1.0, t))
            q = round(v * (LEVELS - 1)) / (LEVELS - 1)  # paleta corta de grises azulados
            c = int(q * 255)
            po[x, y] = (c, c, min(255, c + 10), 255)
    return out


def build(folder):
    atlas = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    for i, (_, photo, box, mode) in enumerate(FUENTES):
        atlas.paste(tile(folder, photo, box, mode), ((i % 2) * TW, (i // 2) * TH))
    return atlas


def generar(folder, force):
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    if os.path.exists(OUT) and not force:
        print(f"{OUT} ya existe (puede tener tus cambios): usa --forzar para rehacerlo.")
        return
    build(folder).save(OUT)
    print(f"Escrito {OUT}")


def guia():
    atlas = Image.open(OUT).convert("RGBA")
    big = atlas.resize((atlas.width * 4, atlas.height * 4), Image.NEAREST)
    out = Image.new("RGB", big.size, (60, 96, 140))
    out.paste(big, (0, 0), big)
    d = ImageDraw.Draw(out)
    for i, (name, *_rest) in enumerate(FUENTES):
        d.text(((i % 2) * TW * 4 + 4, (i // 2) * TH * 4 + 2), f"{i} {name}", fill=(255, 230, 150))
    out.save(GUIDE)
    print(f"Escrito {GUIDE}")


def main():
    args = sys.argv[1:]
    if not args or args[0] not in ("generar", "guia"):
        print(__doc__)
        return 1
    if args[0] == "generar":
        if "--fotos" not in args:
            print("Falta --fotos CARPETA")
            return 1
        generar(args[args.index("--fotos") + 1], "--forzar" in args)
    else:
        guia()
    return 0


if __name__ == "__main__":
    sys.exit(main())
