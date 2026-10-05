#!/usr/bin/env python3
"""El logo del juego (arte/logo/logo_fuente.png, sobre fondo blanco) a todos sus usos:
  - assets/ui/logo.png: el menu principal (fondo transparente);
  - assets/ui/icono.png: el icono de la ventana (Windows y Linux);
  - android/res/mipmap-*/ic_launcher.png: el icono de la app en Android;
  - tools/windows/estepa.ico: el icono del .exe.
Uso: python3 tools/assets/logo.py
"""
import os
from collections import deque

from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
SRC = os.path.join(ROOT, "arte", "logo", "logo_fuente.png")


def cut_background(im):
    """Quita el fondo blanco desde los bordes (relleno por inundacion), sin tocar el blanco de dentro."""
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    seen = bytearray(w * h)
    q = deque()
    for x in range(w):
        q.append((x, 0)), q.append((x, h - 1))
    for y in range(h):
        q.append((0, y)), q.append((w - 1, y))
    while q:
        x, y = q.popleft()
        if x < 0 or y < 0 or x >= w or y >= h or seen[y * w + x]:
            continue
        r, g, b, a = px[x, y]
        if min(r, g, b) < 200:  # el borde de la placa: aqui para
            continue
        seen[y * w + x] = 1
        # Casi blanco: transparente; el halo gris del borde, semitransparente.
        px[x, y] = (r, g, b, 0 if min(r, g, b) > 238 else int(255 * (255 - min(r, g, b)) / 55))
        q.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    return im.crop(im.getbbox())


def square(im, pad=0.0):
    side = int(max(im.size) * (1 + pad))
    out = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    out.paste(im, ((side - im.width) // 2, (side - im.height) // 2), im)
    return out


def main():
    logo = square(cut_background(Image.open(SRC)))
    logo.resize((320, 320), Image.LANCZOS).save(os.path.join(ROOT, "assets", "ui", "logo.png"), optimize=True)
    logo.resize((128, 128), Image.LANCZOS).save(os.path.join(ROOT, "assets", "ui", "icono.png"), optimize=True)
    icon = square(logo, 0.04)  # un margen chico: los lanzadores recortan las esquinas
    for name, size in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = os.path.join(ROOT, "android", "res", "mipmap-" + name)
        os.makedirs(d, exist_ok=True)
        icon.resize((size, size), Image.LANCZOS).save(os.path.join(d, "ic_launcher.png"), optimize=True)
    icon.save(os.path.join(ROOT, "tools", "windows", "estepa.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("logo listo")


if __name__ == "__main__":
    main()
