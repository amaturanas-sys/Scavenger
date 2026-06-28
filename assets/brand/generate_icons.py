from PIL import Image, ImageChops, ImageDraw

SRC = "/root/.claude/uploads/c37acd49-bdcf-57d7-8d95-6e56539c7e17/91e8906d-1000038428.png"

# 1) Cargar y volver transparente el fondo blanco, recortar al contenido.
src = Image.open(SRC).convert("RGBA")
r, g, b, a = src.split()
rgb_min = ImageChops.darker(ImageChops.darker(r, g), b)  # min(r,g,b) por pixel
mask = rgb_min.point(lambda p: 0 if p > 235 else 255)     # near-white -> transparente
src.putalpha(mask)
content = src.crop(src.getbbox())
print("contenido recortado:", content.size)


def fit(size, bg=None, circle=False, margin=0.08):
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    if circle:
        ImageDraw.Draw(canvas).ellipse([0, 0, size - 1, size - 1], fill=(255, 255, 255, 255))
    elif bg == "white":
        canvas = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    avail = int(size * (1 - 2 * margin))
    cw, ch = content.size
    scale = min(avail / cw, avail / ch)
    nw, nh = max(1, int(cw * scale)), max(1, int(ch * scale))
    rc = content.resize((nw, nh), Image.LANCZOS)
    canvas.alpha_composite(rc, ((size - nw) // 2, (size - nh) // 2))
    return canvas


# 2) Frontend
fit(384, bg=None, margin=0.04).save("frontend/logo.png")          # header (badge claro)
fit(64,  bg=None, margin=0.04).save("frontend/favicon.png")       # favicon
fit(512, bg="white", margin=0.10).save("frontend/logo-icon.png")  # apple-touch/PWA

# 3) Android launcher (blanco) + redondo (circulo blanco)
DENS = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
for d, s in DENS.items():
    base = f"android/app/src/main/res/mipmap-{d}"
    fit(s, bg="white", margin=0.10).save(f"{base}/ic_launcher.png")
    fit(s, circle=True, margin=0.12).save(f"{base}/ic_launcher_round.png")

print("OK: iconos regenerados")
