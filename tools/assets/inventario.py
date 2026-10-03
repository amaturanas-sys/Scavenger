#!/usr/bin/env python3
"""Inventario de assets de ESTEPA (assets/inventario.tsv).

Uso:
  python3 tools/assets/inventario.py check            valida el TSV y los archivos importados
  python3 tools/assets/inventario.py doc              regenera docs/INVENTARIO.md
  python3 tools/assets/inventario.py doc --verificar  falla si docs/INVENTARIO.md esta desactualizado
  python3 tools/assets/inventario.py resumen          avance por categoria y estado
  python3 tools/assets/inventario.py faltan [cat]     objetos sin modelo (opcionalmente de una categoria)

Cada id "categoria.subcategoria.nombre" corresponde a un archivo fijo:
  assets/models/categoria/subcategoria/nombre.glb     (modelos)
  assets/textures/categoria/subcategoria/nombre.png   (etiqueta formato:textura)
Sin dependencias: solo la biblioteca estandar de Python 3.
"""
import os
import re
import sys
from collections import Counter, OrderedDict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TSV = os.path.join(ROOT, "assets", "inventario.tsv")
DOC = os.path.join(ROOT, "docs", "INVENTARIO.md")
COLUMNS = ["id", "nombre", "etiquetas", "medidas_m", "tris_max", "estado", "notas"]

CATEGORIES = OrderedDict([
    ("mapa", "Mapa: terreno, vegetación, agua e hitos"),
    ("estructura", "Estructuras"),
    ("vehiculo", "Vehículos"),
    ("animal", "Animales"),
    ("arma", "Armas"),
    ("proyectil", "Proyectiles"),
    ("escudo", "Escudos"),
    ("totem", "Tótems"),
    ("armadura", "Armaduras (por piezas)"),
    ("vestimenta", "Vestimenta (personalización)"),
    ("accesorio", "Accesorios: amuletos, tatuajes, arreos y mensajes"),
    ("asedio", "Maquinaria de asedio"),
    ("utileria", "Utilería y consumibles"),
    ("personaje", "Personajes y NPCs"),
])
STATES = ["pendiente", "kiln", "importado", "refinado"]
# Etiquetas: clave -> valores permitidos (None = valor libre).
TAGS = {
    "bioma": {"estepa", "desierto", "bosque", "fiordo", "glaciar", "ciudad", "cualquiera"},
    "faccion": {"nomada", "imperio", "culto", "neutral", "salvaje", "cualquiera"},
    "acto": {"1", "2", "3", "4"},
    "uso": None,
    "talla": {"pequena", "comun", "grande", "gigante"},
    "material": None,
    "rig": {"humanoide", "cuadrupedo", "ave", "ninguno"},
    "especial": {"legado", "narrativo", "rastro_padre", "gran_guerrero", "hito"},
    "formato": {"modelo", "textura"},
    "estilo": None,
    "requiere": None,
    "motivo": None,
    "manos": {"una", "dos", "escudo"},
}
ID_RE = re.compile(r"^[a-z0-9_]+\.[a-z0-9_]+\.[a-z0-9_]+$")
DIMS_RE = re.compile(r"^\d+(\.\d+)?x\d+(\.\d+)?x\d+(\.\d+)?$")


def load():
    items, errors = [], []
    with open(TSV, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            cols = line.split("\t")
            if cols[0] == "id":
                if cols != COLUMNS:
                    errors.append(f"linea {n}: la cabecera debe ser: {' | '.join(COLUMNS)}")
                continue
            if len(cols) != len(COLUMNS):
                errors.append(f"linea {n}: {len(cols)} columnas (se esperan {len(COLUMNS)})")
                continue
            it = dict(zip(COLUMNS, cols))
            it["linea"] = n
            it["tags"] = [t for t in it["etiquetas"].split(",") if t]
            it["categoria"] = it["id"].split(".")[0]
            items.append(it)
    return items, errors


# Variantes estacionales de un modelo (sufijo del archivo; ver src/world/props.c).
SEASONS = ("primavera", "verano", "otono", "invierno")


def path_of(it):
    rel = it["id"].replace(".", "/")
    if "formato:textura" in it["tags"]:
        return os.path.join("assets", "textures", rel + ".png")
    return os.path.join("assets", "models", rel + ".glb")


def validate(items):
    errors, warnings = [], []
    seen = Counter(it["id"] for it in items)
    for it in items:
        where = f"linea {it['linea']} ({it['id']})"
        if not ID_RE.match(it["id"]):
            errors.append(f"{where}: id invalido (minusculas, digitos y _; formato categoria.subcategoria.nombre)")
        if seen[it["id"]] > 1:
            errors.append(f"{where}: id repetido")
        if it["categoria"] not in CATEGORIES:
            errors.append(f"{where}: categoria desconocida '{it['categoria']}' (agregala en CATEGORIES)")
        if not it["nombre"].strip():
            errors.append(f"{where}: falta el nombre")
        if not DIMS_RE.match(it["medidas_m"]):
            errors.append(f"{where}: medidas '{it['medidas_m']}' (formato ancho x alto x largo, p. ej. 1.5x2x3)")
        if not it["tris_max"].isdigit():
            errors.append(f"{where}: tris_max debe ser un entero")
        if it["estado"] not in STATES:
            errors.append(f"{where}: estado '{it['estado']}' (validos: {', '.join(STATES)})")
        for tag in it["tags"]:
            key, _, value = tag.partition(":")
            if key not in TAGS or not value:
                errors.append(f"{where}: etiqueta '{tag}' (claves validas: {', '.join(TAGS)})")
            elif TAGS[key] is not None and value not in TAGS[key]:
                errors.append(f"{where}: valor '{value}' para '{key}' (validos: {', '.join(sorted(TAGS[key]))})")

        exists = os.path.isfile(os.path.join(ROOT, path_of(it)))
        if it["estado"] != "pendiente" and not exists:
            errors.append(f"{where}: estado '{it['estado']}' pero falta {path_of(it)}")
        if it["estado"] == "pendiente" and exists:
            warnings.append(f"{where}: {path_of(it)} existe; cambia el estado a 'importado'")

    # Archivos que no corresponden a ningun id del inventario.
    known = {os.path.normpath(path_of(it)) for it in items}
    for base, ext in (("assets/models", ".glb"), ("assets/textures", ".png")):
        for dirpath, _, files in os.walk(os.path.join(ROOT, base)):
            for name in files:
                if name.endswith(ext) or name.endswith(".gltf"):
                    rel = os.path.normpath(os.path.relpath(os.path.join(dirpath, name), ROOT))
                    stem, dot, extension = rel.rpartition(".")
                    if "@" in os.path.basename(stem):
                        # Variante estacional: nombre@estacion.glb junto al modelo base.
                        base, _, season = stem.rpartition("@")
                        if season not in SEASONS:
                            errors.append(f"{rel}: estacion '{season}' (validas: {', '.join(SEASONS)})")
                        elif base + dot + extension not in known:
                            errors.append(f"{rel}: variante de un id que no esta en el inventario")
                        continue
                    if rel not in known:
                        errors.append(f"{rel}: no corresponde a ningun id del inventario")
    return errors, warnings


def render_doc(items):
    out = []
    w = out.append
    w("# Inventario de assets")
    w("")
    w("> Generado por `tools/assets/inventario.py doc` a partir de [`assets/inventario.tsv`](../assets/inventario.tsv).")
    w("> No editar a mano: edita el TSV y regenera.")
    w("")
    total = len(items)
    done = sum(1 for it in items if it["estado"] != "pendiente")
    w(f"**{total} objetos** · {done} con modelo · {total - done} pendientes.")
    w("")
    w("Cada objeto se reemplaza dejando su archivo en la ruta que marca su id; el juego lo carga solo y, mientras falte, "
      "dibuja un marcador con sus medidas. Convenciones de importación: [assets/models/README.md](../assets/models/README.md).")
    w("")
    w("## Avance por categoría")
    w("")
    w("| Categoría | Total | " + " | ".join(s.capitalize() for s in STATES) + " |")
    w("|---|---|" + "---|" * len(STATES))
    for cat, title in CATEGORIES.items():
        rows = [it for it in items if it["categoria"] == cat]
        counts = Counter(it["estado"] for it in rows)
        w(f"| [{title}](#{cat}) | {len(rows)} | " + " | ".join(str(counts.get(s, 0)) for s in STATES) + " |")
    w("")
    w("## Etiquetas")
    w("")
    w("Formato `clave:valor`, separadas por comas. Una clave puede repetirse (p. ej., dos biomas).")
    w("")
    w("| Clave | Valores |")
    w("|---|---|")
    for key, values in TAGS.items():
        w(f"| `{key}` | " + (", ".join(f"`{v}`" for v in sorted(values)) if values else "libre") + " |")
    w("")
    for cat, title in CATEGORIES.items():
        rows = [it for it in items if it["categoria"] == cat]
        if not rows:
            continue
        w(f'<a id="{cat}"></a>')
        w("")
        w(f"## {title}")
        w("")
        w("| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |")
        w("|---|---|---|---|---|---|---|")
        for it in rows:
            tags = " ".join(f"`{t}`" for t in it["tags"])
            notes = it["notas"].replace("|", "/")
            w(f"| `{it['id']}` | {it['nombre']} | {it['medidas_m']} | {it['tris_max']} | {it['estado']} | {tags} | {notes} |")
        w("")
    return "\n".join(out)


def main(argv):
    cmd = argv[1] if len(argv) > 1 else "check"
    items, errors = load()
    if cmd == "check":
        errs, warns = validate(items)
        errors += errs
        for msg in warns:
            print("AVISO:", msg)
        for msg in errors:
            print("ERROR:", msg)
        print(f"{len(items)} objetos, {len(errors)} errores, {len(warns)} avisos.")
        return 1 if errors else 0
    if cmd == "doc":
        text = render_doc(items) + "\n"
        if "--verificar" in argv:
            current = open(DOC, encoding="utf-8").read() if os.path.exists(DOC) else ""
            if current != text:
                print("docs/INVENTARIO.md esta desactualizado: ejecuta 'python3 tools/assets/inventario.py doc'.")
                return 1
            print("docs/INVENTARIO.md al dia.")
            return 0
        with open(DOC, "w", encoding="utf-8") as f:
            f.write(text)
        print(f"Escrito {os.path.relpath(DOC, ROOT)} ({len(items)} objetos).")
        return 0
    if cmd == "resumen":
        for cat in CATEGORIES:
            rows = [it for it in items if it["categoria"] == cat]
            counts = Counter(it["estado"] for it in rows)
            print(f"{cat:12} {len(rows):4}  " + "  ".join(f"{s}:{counts.get(s, 0)}" for s in STATES))
        return 0
    if cmd == "faltan":
        cat = argv[2] if len(argv) > 2 else None
        for it in items:
            if it["estado"] == "pendiente" and (cat is None or it["categoria"] == cat):
                print(f"{it['id']:48} {path_of(it)}")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
