#!/usr/bin/env python3
"""Importa modelos .glb del OpenGameArt (gltf.zip) al inventario del juego."""
import zipfile, os, sys, tempfile, re
sys.path.insert(0, 'tools/modelos')
sys.path.insert(0, 'tools/assets')
import lowres
import inventario

REF_DIR = "arte/fuentes/opengameart"  # el paquete FREE Remastered Meadows (CC0): arte/fuentes/README.md
ZIP_PATH = os.path.join(REF_DIR, "gltf.zip")

CATEGORY_MAP = [
    (r'^EA01_Env_Tree_', 'mapa.vegetacion.arbol'),
    (r'^EA01_Env_Bush_', 'mapa.vegetacion.arbusto'),
    (r'^EA01_Env_Grass_', 'mapa.vegetacion.hierba'),
    (r'^EA01_Env_Trunk_', 'mapa.vegetacion.tronco'),
    (r'^EA01_Env_Stump_', 'mapa.vegetacion.tocon'),
    (r'^EA01_Env_Rock', 'mapa.roca'),
    (r'^EA01_Env_Clif', 'mapa.roca.acantilado'),
    (r'^EA_Swamp_Rock', 'mapa.roca'),
    (r'^EA01_Env_Bridge', 'estructura.puente'),
    (r'^EA01_Env_Fence', 'estructura.cerca'),
    (r'^EA01_GT_Road', 'mapa.camino'),
    (r'^EA01_GT_DownRoad', 'mapa.camino'),
    (r'^EA01_Props_', 'utileria'),
    (r'^EA01_Prop_', 'utileria'),
    (r'^EA01_Env_Tent', 'estructura.tienda'),
    (r'^EA01_Cloud', 'mapa.cielo.nube'),
    (r'^EA_FrostP_', 'estructura.portal'),
    (r'^EA_Props_', 'utileria'),
    (r'^EM_', 'estructura.cerca'),
    (r'^EA01_Env_Mushroom', 'mapa.vegetacion.seta'),
    (r'^EA01_Env_Plant', 'mapa.vegetacion.planta'),
    (r'^EA01_Env_Hill', 'mapa.terreno.colina'),
    (r'^EA01_Env_Stone', 'mapa.roca.piedra'),
    (r'^EA01_Env_WatchTower', 'estructura.atalaya'),
]

def categorizar(nombre_archivo):
    base = os.path.splitext(nombre_archivo)[0]
    base = re.sub(r'\.\d+$', '', base)
    for patron, categoria in CATEGORY_MAP:
        if re.match(patron, nombre_archivo):
            sufijo = re.sub(r'^[A-Z0-9_]+_', '', base)
            sufijo = re.sub(r'[^a-z0-9_]', '_', sufijo.lower())
            sufijo = re.sub(r'_+', '_', sufijo).strip('_')
            return categoria, sufijo or 'generico'
    sufijo = re.sub(r'[^a-z0-9_]', '_', base.lower())
    sufijo = re.sub(r'_+', '_', sufijo).strip('_')
    return 'utileria.objeto_generico', sufijo or 'generico'

def medir_dimensiones(glb_path):
    import trimesh
    import numpy as np
    try:
        scene = trimesh.load(glb_path)
        if hasattr(scene, 'geometry'):
            all_v = np.vstack([scene.geometry[n].vertices for n in scene.geometry])
        else:
            all_v = scene.vertices
        lo = all_v.min(axis=0); hi = all_v.max(axis=0)
        return (hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2])
    except Exception:
        return (1.0, 1.0, 1.0)

def convertir_modelo(glb_path, mid, dims, tris_max=2000, girar=0.0):
    try:
        data, informe = lowres.convertir(
            glb_path, dims, tris_max,
            girar=girar, rotar="", escala=None, origen=None,
            colores=24, luz=True, limpiar=0.01, nombre=mid, suave=False)
        return data, informe, None
    except Exception as e:
        return None, None, str(e)

def slugify(nombre_archivo):
    base = os.path.splitext(nombre_archivo)[0]
    base = re.sub(r'\.\d+$', '', base)
    base = re.sub(r'^(EA01|EA|EM)_', '', base)
    base = re.sub(r'[^a-z0-9_]', '_', base.lower())
    base = re.sub(r'_+', '_', base).strip('_')
    return base or 'generico'

def nombre_legible(mid):
    partes = mid.split('.')
    if len(partes) >= 3:
        return partes[2].replace('_', ' ').title()
    return "Objeto generico"

def main():
    limite = None
    if len(sys.argv) > 1 and sys.argv[1] == '--limite':
        limite = int(sys.argv[2])

    items, errors = inventario.load()
    if errors:
        for e in errors: print(f"ERROR: {e}")
        return 1
    ids_existentes = {it['id'] for it in items}

    tmp_dir = tempfile.mkdtemp(prefix="oga_import_")
    print(f"Extrayendo a {tmp_dir}...")

    with zipfile.ZipFile(ZIP_PATH, 'r') as z:
        glb_files = sorted([f for f in z.namelist() if f.endswith('.glb')])
        if limite:
            glb_files = glb_files[:limite]

        print(f"Procesando {len(glb_files)} modelos...\n")

        nuevos = []
        errores = []

        for i, gf in enumerate(glb_files, 1):
            nombre = os.path.basename(gf)
            size_kb = z.getinfo(gf).file_size / 1024
            extract_path = os.path.join(tmp_dir, nombre)
            os.makedirs(os.path.dirname(extract_path), exist_ok=True)
            with z.open(gf) as src, open(extract_path, 'wb') as dst:
                dst.write(src.read())

            cat_base, sufijo = categorizar(nombre)
            mid = f"{cat_base}.{sufijo}"
            c = 1
            while mid in ids_existentes or mid in [n[0] for n in nuevos]:
                mid = f"{cat_base}.{sufijo}_{c}"; c += 1

            dims = medir_dimensiones(extract_path)

            if size_kb < 50: tris_max = 300
            elif size_kb < 200: tris_max = 800
            elif size_kb < 500: tris_max = 1500
            else: tris_max = 3000

            print(f"[{i}/{len(glb_files)}] {nombre} ({size_kb:.1f} KB) -> {mid}")
            print(f"  dims: {dims[0]:.2f}x{dims[1]:.2f}x{dims[2]:.2f} tris_max: {tris_max}")

            data, informe, err = convertir_modelo(extract_path, mid, dims, tris_max=tris_max)
            if err or data is None:
                print(f"  ERROR: {err or 'sin datos'}")
                errores.append((mid, err or 'sin datos'))
                continue

            rel = mid.replace(".", "/")
            dest_dir = os.path.join("assets", "models", os.path.dirname(rel))
            os.makedirs(dest_dir, exist_ok=True)
            dest = os.path.join(dest_dir, os.path.basename(rel) + ".glb")
            with open(dest, "wb") as f:
                f.write(data)

            print(f"  guardado {dest} ({len(data)/1024:.1f} KB) tris: {informe.get('tris','?')}")
            nuevos.append((mid, dims, informe.get('tris', 0)))
            ids_existentes.add(mid)

        print(f"\n{'='*60}")
        print(f"Exitosos: {len(nuevos)}, Errores: {len(errores)}")
        if errores:
            for m, e in errores: print(f"  {m}: {e}")

        # Agregar al TSV
        print("\nAgregando entradas a assets/inventario.tsv...")
        lineas = []
        for mid, dims, tris in nuevos:
            a, h, l = dims
            medidas = f"{a:.2f}x{h:.2f}x{l:.2f}"
            subcategoria = mid.split('.')[1] if '.' in mid else 'generico'
            etiquetas = ['rig:ninguno', 'formato:modelo']
            if subcategoria in ('arbol','arbusto','hierba','tronco','tocon','seta','planta'):
                etiquetas.insert(0, 'bioma:estepa')
            elif subcategoria in ('roca','acantilado','piedra'):
                etiquetas.insert(0, 'bioma:estepa')
            elif subcategoria in ('camino',):
                etiquetas.insert(0, 'bioma:estepa')
            elif subcategoria in ('puente','cerca','tienda','portal'):
                etiquetas.append('uso:decorado')
            linea = f"{mid}\t{nombre_legible(mid)}\t{','.join(etiquetas)}\t{medidas}\t{tris}\timportado\tOpenGameArt FREE Remastered Meadows"
            lineas.append(linea)

        with open(inventario.TSV, 'a', encoding='utf-8') as f:
            for linea in lineas:
                f.write(linea + "\n")
        print(f"Agregadas {len(lineas)} entradas al TSV.")
        print(f"Temp: {tmp_dir}")
        return 0 if not errores else 1

if __name__ == '__main__':
    sys.exit(main())