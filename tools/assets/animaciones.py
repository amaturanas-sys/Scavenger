#!/usr/bin/env python3
"""Indice de animaciones de ESTEPA (assets/animaciones.tsv).

Une cada modelo del inventario (assets/inventario.tsv) con los clips de
animacion que necesita, segun su esqueleto (rig):

  humanoide   personajes (personaje.*). La ropa y la armadura con rig:humanoide
              se pegan al esqueleto del cuerpo y no llevan clips propios.
  cuadrupedo  animales con rig:cuadrupedo (animal.*). Las bardas se pegan al esqueleto.
  ave         animales con rig:ave.
  mecanismo   objetos animados (vehiculos, maquinas de asedio, puertas...): solo
              los que nombra la columna "aplica" de cada clip.

Uso:
  python3 tools/assets/animaciones.py check            valida el indice y los GLB importados
  python3 tools/assets/animaciones.py doc              regenera docs/ANIMACIONES.md
  python3 tools/assets/animaciones.py doc --verificar  falla si docs/ANIMACIONES.md esta desactualizado
  python3 tools/assets/animaciones.py modelo <id>      clips que necesita un modelo

Los clips del GLB deben llamarse exactamente como la columna "clip".
Sin dependencias: solo la biblioteca estandar de Python 3.
"""
import json
import os
import struct
import sys
from collections import OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import inventario  # noqa: E402

ROOT = inventario.ROOT
TSV = os.path.join(ROOT, "assets", "animaciones.tsv")
DOC = os.path.join(ROOT, "docs", "ANIMACIONES.md")
COLUMNS = ["rig", "clip", "descripcion", "bucle", "aplica", "prioridad", "acciones"]
RIGS = OrderedDict([
    ("humanoide", "Humanoide (personajes y NPCs)"),
    ("cuadrupedo", "Cuadrúpedo (monturas, ganado y fauna)"),
    ("ave", "Ave (cetrería y fauna)"),
    ("mecanismo", "Mecanismo (vehículos, asedio, puertas)"),
])


def load_clips():
    clips, errors = [], []
    with open(TSV, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            cols = line.split("\t")
            if cols[0] == "rig":
                if cols != COLUMNS:
                    errors.append(f"linea {n}: la cabecera debe ser: {' | '.join(COLUMNS)}")
                continue
            if len(cols) != len(COLUMNS):
                errors.append(f"linea {n}: {len(cols)} columnas (se esperan {len(COLUMNS)})")
                continue
            c = dict(zip(COLUMNS, cols))
            c["linea"] = n
            c["filtros"] = [x.strip() for x in c["aplica"].split(",") if x.strip()]
            clips.append(c)
    return clips, errors


def base_models(items, rig):
    """Modelos que llevan esqueleto propio de este rig (los que reciben clips '*')."""
    if rig == "humanoide":
        return [it for it in items if it["categoria"] == "personaje"]
    if rig in ("cuadrupedo", "ave"):
        return [it for it in items if it["categoria"] == "animal" and f"rig:{rig}" in it["tags"]]
    return []  # mecanismo: solo por filtro explicito


def matches(it, flt):
    if flt == "*":
        return True
    if flt.startswith("id:"):
        return it["id"] == flt[3:]
    if ":" in flt:
        return flt in it["tags"]
    return it["id"].startswith(flt)


def models_for_clip(items, clip):
    pool = base_models(items, clip["rig"]) if clip["rig"] != "mecanismo" else items
    if clip["filtros"] == ["*"]:
        return list(pool)
    return [it for it in pool if any(matches(it, f) for f in clip["filtros"])]


def index(items, clips):
    """id del modelo -> lista de clips que necesita (en orden del indice)."""
    out = OrderedDict()
    for clip in clips:
        for it in models_for_clip(items, clip):
            out.setdefault(it["id"], []).append(clip)
    return out


def glb_animations(path):
    """Nombres de las animaciones de un GLB (lee solo el bloque JSON)."""
    with open(path, "rb") as f:
        head = f.read(12)
        if len(head) < 12 or head[:4] != b"glTF":
            return None
        length, kind = struct.unpack("<I4s", f.read(8))
        if kind != b"JSON":
            return None
        doc = json.loads(f.read(length).decode("utf-8"))
    return [a.get("name", "") for a in doc.get("animations", [])]


def validate(items, clips):
    errors, warnings = [], []
    seen = set()
    for c in clips:
        where = f"linea {c['linea']} ({c['rig']}.{c['clip']})"
        if c["rig"] not in RIGS:
            errors.append(f"{where}: rig desconocido (validos: {', '.join(RIGS)})")
        key = (c["rig"], c["clip"])
        if key in seen:
            errors.append(f"{where}: clip repetido")
        seen.add(key)
        if not c["clip"] or not all(ch.islower() or ch.isdigit() or ch == "_" for ch in c["clip"]):
            errors.append(f"{where}: nombre de clip invalido (minusculas, digitos y _)")
        if c["bucle"] not in ("si", "no"):
            errors.append(f"{where}: bucle debe ser si o no")
        if c["prioridad"] not in ("obligatoria", "opcional"):
            errors.append(f"{where}: prioridad debe ser obligatoria u opcional")
        if not c["filtros"]:
            errors.append(f"{where}: aplica vacio (usa * para todos)")
        for flt in c["filtros"]:
            if flt != "*" and not any(matches(it, flt) for it in items):
                errors.append(f"{where}: el filtro '{flt}' no coincide con ningun modelo del inventario")
        if c["rig"] == "mecanismo" and c["filtros"] == ["*"]:
            errors.append(f"{where}: los clips de mecanismo deben nombrar a que modelos se aplican")
    # GLB importados: deben traer los clips obligatorios con el nombre exacto.
    idx = index(items, clips)
    by_id = {it["id"]: it for it in items}
    for mid, mclips in idx.items():
        it = by_id[mid]
        path = os.path.join(ROOT, inventario.path_of(it))
        if not os.path.isfile(path):
            continue
        names = glb_animations(path)
        if names is None:
            errors.append(f"{inventario.path_of(it)}: no es un GLB valido")
            continue
        needed = {c["clip"] for c in mclips if c["prioridad"] == "obligatoria"}
        known = {c["clip"] for c in mclips}
        missing = sorted(needed - set(names))
        if missing:
            msg = f"{mid}: faltan clips obligatorios: {', '.join(missing)}"
            (errors if it["estado"] in ("importado", "refinado") else warnings).append(msg)
        extra = sorted(set(names) - known)
        if extra:
            warnings.append(f"{mid}: clips que no estan en el indice: {', '.join(extra)}")
    return errors, warnings


def render_doc(items, clips):
    idx = index(items, clips)
    by_id = {it["id"]: it for it in items}
    out = []
    w = out.append
    w("# Índice de animaciones")
    w("")
    w("> Generado por `tools/assets/animaciones.py doc` a partir de [`assets/animaciones.tsv`](../assets/animaciones.tsv)")
    w("> y del [inventario](INVENTARIO.md). No editar a mano.")
    w("")
    w(f"**{len(clips)} clips** en {len(RIGS)} tipos de esqueleto · **{len(idx)} modelos animados**.")
    w("")
    w("## Cómo se nombran y se usan")
    w("")
    w("- Cada animación dentro del GLB se llama **exactamente** como su clip (p. ej. `caminar`, `galopar`).")
    w("- La ropa, la armadura y las bardas (`rig:humanoide` o `rig:cuadrupedo` en el inventario) se pegan al")
    w("  esqueleto del cuerpo: no llevan clips propios, se mueven con los del cuerpo.")
    w("- **Obligatoria:** el juego la usa ya o en la fase siguiente. **Opcional:** mejora, pero tiene sustituto.")
    w("- El juego elige el clip según el estado (`src/sim/anim_index.c`); si el modelo no lo trae, usa `idle`.")
    w("- Validación: `python3 tools/assets/animaciones.py check` (también en CI). Un modelo `importado` o")
    w("  `refinado` sin sus clips obligatorios es un error.")
    w("")
    for rig, title in RIGS.items():
        rc = [c for c in clips if c["rig"] == rig]
        if not rc:
            continue
        w(f"## {title}")
        w("")
        w("| Clip | Bucle | Prioridad | Descripción | Lo usa | Se aplica a |")
        w("|---|---|---|---|---|---|")
        for c in rc:
            app = "todos" if c["filtros"] == ["*"] else ", ".join(f"`{x}`" for x in c["filtros"])
            w(f"| `{c['clip']}` | {c['bucle']} | {c['prioridad']} | {c['descripcion']} | {c['acciones']} | {app} |")
        w("")
    w("## Modelos y sus clips")
    w("")
    w("| Modelo | Rig | Obligatorios | Opcionales | Clips |")
    w("|---|---|---|---|---|")
    for mid, mclips in idx.items():
        rigs = sorted({c["rig"] for c in mclips})
        ob = sum(1 for c in mclips if c["prioridad"] == "obligatoria")
        op = len(mclips) - ob
        names = ", ".join(c["clip"] for c in mclips)
        w(f"| `{mid}` ({by_id[mid]['nombre']}) | {', '.join(rigs)} | {ob} | {op} | {names} |")
    w("")
    return "\n".join(out)


def main(argv):
    cmd = argv[1] if len(argv) > 1 else "check"
    items, inv_errors = inventario.load()
    clips, errors = load_clips()
    errors += inv_errors
    if cmd == "check":
        errs, warns = validate(items, clips)
        errors += errs
        for msg in warns:
            print("AVISO:", msg)
        for msg in errors:
            print("ERROR:", msg)
        print(f"{len(clips)} clips, {len(index(items, clips))} modelos animados, {len(errors)} errores, {len(warns)} avisos.")
        return 1 if errors else 0
    if cmd == "doc":
        text = render_doc(items, clips) + "\n"
        if "--verificar" in argv:
            current = open(DOC, encoding="utf-8").read() if os.path.exists(DOC) else ""
            if current != text:
                print("docs/ANIMACIONES.md esta desactualizado: ejecuta 'python3 tools/assets/animaciones.py doc'.")
                return 1
            print("docs/ANIMACIONES.md al dia.")
            return 0
        with open(DOC, "w", encoding="utf-8") as f:
            f.write(text)
        print(f"Escrito {os.path.relpath(DOC, ROOT)}.")
        return 0
    if cmd == "modelo" and len(argv) > 2:
        mclips = index(items, clips).get(argv[2])
        if not mclips:
            print(f"{argv[2]}: no lleva clips propios (o no existe).")
            return 1
        for c in mclips:
            print(f"{c['clip']:22} {c['prioridad']:12} {'bucle' if c['bucle'] == 'si' else 'una vez':8} {c['descripcion']}")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
