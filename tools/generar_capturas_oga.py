#!/usr/bin/env python3
"""Genera capturas de modelos OpenGameArt importados para referencia visual.

Selecciona 1-2 modelos representativos por categoría, renderiza cada uno con
xvfb-run + estepa --ver, y crea hojas de contacto (contact sheets) por categoría.

Uso:
    python3 tools/generar_capturas_oga.py [--modelos-por-cat N]
"""

import os, sys, subprocess, tempfile, json
from collections import defaultdict

sys.path.insert(0, 'tools/assets')
import inventario

ROOT = inventario.ROOT
OUT_DIR = os.path.join(ROOT, "docs", "referencia", "oga_capturas")
ESTEPA = os.path.join(ROOT, "build", "estepa")
FRAMES = 40

# Seleccionar 2 modelos representativos por categoria (o todos si hay menos)
def seleccionar_modelos(items, modelos_por_cat=2):
    """Devuelve {categoria: [ids]} con hasta modelos_por_cat por categoria."""
    por_cat = defaultdict(list)
    for it in items:
        if it['estado'] != 'importado':
            continue
        if 'OpenGameArt' not in it.get('notas', ''):
            continue
        cat = '.'.join(it['id'].split('.')[:2])
        por_cat[cat].append(it['id'])
    return {cat: ids[:modelos_por_cat] for cat, ids in sorted(por_cat.items())}

def capturar(mid, out_png):
    """Renderiza un modelo con estepa --ver y guarda la captura."""
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    cmd = [
        'xvfb-run', '-a', ESTEPA,
        '--ver', mid,
        '--frames', str(FRAMES),
        '--screenshot', out_png,
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=os.path.join(ROOT, 'build'))
    return os.path.isfile(out_png) and os.path.getsize(out_png) > 0

def main():
    modelos_por_cat = 2
    if '--modelos-por-cat' in sys.argv:
        i = sys.argv.index('--modelos-por-cat')
        modelos_por_cat = int(sys.argv[i + 1])

    items, errors = inventario.load()
    if errors:
        for e in errors:
            print(f"ERROR: {e}")
        return 1

    os.makedirs(OUT_DIR, exist_ok=True)
    seleccion = seleccionar_modelos(items, modelos_por_cat)

    total = sum(len(ids) for ids in seleccion.values())
    print(f"Categorias: {len(seleccion)}, modelos a capturar: {total}\n")

    exitosos = []
    fallidos = []

    i = 0
    for cat, ids in seleccion.items():
        cat_dir = os.path.join(OUT_DIR, cat.replace('.', '/'))
        os.makedirs(cat_dir, exist_ok=True)
        for mid in ids:
            i += 1
            nombre_archivo = mid.replace('.', '_') + '.png'
            out_png = os.path.join(cat_dir, nombre_archivo)
            print(f"[{i}/{total}] {mid} -> {out_png}")
            if capturar(mid, out_png):
                exitosos.append((mid, out_png))
            else:
                fallidos.append(mid)
                print(f"  FALLO: no se genero {out_png}")

    print(f"\n{'='*60}")
    print(f"Exitosos: {len(exitosos)}, Fallidos: {len(fallidos)}")
    if fallidos:
        for m in fallidos:
            print(f"  FALLO: {m}")

    # Crear hojas de contacto por categoria
    print("\nCreando hojas de contacto por categoria...")
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print("  PIL no disponible, omitiendo hojas de contacto.")
        return 0

    for cat, ids in seleccion.items():
        imagenes = []
        for mid in ids:
            nombre_archivo = mid.replace('.', '_') + '.png'
            ruta_png = os.path.join(OUT_DIR, cat.replace('.', '/'), nombre_archivo)
            if os.path.isfile(ruta_png):
                imagenes.append((mid, ruta_png))

        if not imagenes:
            continue

        # Calcular tamaño de la hoja de contacto
        cols = min(2, len(imagenes))
        filas = (len(imagenes) + cols - 1) // cols
        celda_w, celda_h = 400, 300
        sheet_w = cols * celda_w
        sheet_h = filas * celda_h + 60  # espacio para titulos

        sheet = Image.new('RGB', (sheet_w, sheet_h), (240, 240, 240))
        draw = ImageDraw.Draw(sheet)

        try:
            font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
        except Exception:
            font = ImageFont.load_default()

        for idx, (mid, ruta_png) in enumerate(imagenes):
            img = Image.open(ruta_png)
            img.thumbnail((celda_w - 20, celda_h - 40), Image.LANCZOS)

            cx = (idx % cols) * celda_w + 10
            cy = (idx // cols) * celda_h + 40

            sheet.paste(img, (cx, cy))
            draw.text((cx, cy - 25), mid, fill=(40, 40, 40), font=font)

        # Titulo de la categoria
        draw.text((20, 10), f"Categoria: {cat}", fill=(30, 30, 30), font=font)

        sheet_path = os.path.join(OUT_DIR, f"{cat.replace('.', '_')}_contacto.png")
        sheet.save(sheet_path)
        print(f"  Hoja de contacto: {sheet_path}")

    print(f"\nTodo listo en {OUT_DIR}")
    return 0

if __name__ == '__main__':
    sys.exit(main())