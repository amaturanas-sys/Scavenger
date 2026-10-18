#!/usr/bin/env python3
"""La tuberia de modelos: de una imagen (TRELLIS.2), un rig (RigNet) o un GLB cualquiera, al juego.

Uso:
  python3 tools/modelos/modelos.py traer <id> (--url URL | --archivo RUTA) [opciones]
      Baja la fuente a arte/modelos/fuentes/ (fuera de git), la convierte al estilo del juego
      (tools/modelos/lowres.py) en la ruta de su id, la marca 'importado' en assets/inventario.tsv,
      anota de donde salio en arte/modelos/models_manifest.json y corre los chequeos.
      Si el id necesita esqueleto y clips (docs/ANIMACIONES.md: personas, animales, mecanismos), la
      version low-poly queda en arte/modelos/para_riggear/ para riggearla (RigNet) y animarla; el
      modelo riggeado entra despues con --riggeado.
  python3 tools/modelos/modelos.py bajar <id> <fuente.glb> [--salida RUTA] [opciones]
      Solo la conversion, para probar las opciones; no toca el inventario.
  python3 tools/modelos/modelos.py check
      Los GLB importados: triangulos (tris_max), medidas, base en el suelo, texturas (CI, sin dependencias).
  python3 tools/modelos/modelos.py estado [id]
      De donde salio cada modelo, y lo que falta por prioridad (personas, animales, armas...).

Opciones de la conversion:
  --girar G        grados alrededor de +Y para llevar el frente a +X (la imagen de frente: 90)
  --rotar E:G,...  giros previos alrededor de x, y o z (una espada acostada que va de pie: z:90)
  --escala S       el factor a mano (si no, manda el lado mas largo del inventario)
  --origen x,y,z   el punto (m, del modelo ya escalado) que queda en el origen: el agarre de un arma
  --colores N      colores de la paleta (24)
  --sin-luz        sin la luz horneada
  --limpiar F      quita los pedazos sueltos de menos de F del area (0.01; 0: ninguno)
  --riggeado       un modelo con esqueleto y clips (RigNet, GRAIL): no se convierte, se valida y se copia
  --fuente NOMBRE  de donde salio, para el manifiesto: trellis2, rignet, grail, kiln, otro
  --reemplazar     si el id ya tiene modelo, lo reemplaza

Las dependencias de la conversion (no las del chequeo): pip install -r tools/modelos/requirements.txt
"""
import argparse
import hashlib
import json
import os
import shutil
import sys
import urllib.parse
import urllib.request
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
import animaciones  # noqa: E402
import glb  # noqa: E402
import inventario  # noqa: E402

ROOT = inventario.ROOT
ARTE = os.path.join(ROOT, "arte", "modelos")
FUENTES = os.path.join(ARTE, "fuentes")          # las de alta resolucion: fuera de git (.gitignore)
PARA_RIGGEAR = os.path.join(ARTE, "para_riggear")  # low-poly sin esqueleto, para RigNet
MANIFEST = os.path.join(ARTE, "models_manifest.json")
FUENTES_VALIDAS = ("trellis2", "rignet", "grail", "kiln", "otro")
TEXTURE_MAX = 256    # px: el estilo pixelado (assets/models/README.md)
BYTES_AVISO = 2 << 20


# ---------------------------------------------------------------- inventario
def find_item(items, mid):
    for it in items:
        if it["id"] == mid:
            return it
    return None


def dims(it):
    """(ancho, alto, largo) en m."""
    return tuple(float(x) for x in it["medidas_m"].split("x"))


def needs_clips(items, it):
    clips, _ = animaciones.load_clips()
    return [c["clip"] for c in animaciones.index(items, clips).get(it["id"], []) if c["prioridad"] == "obligatoria"]


def set_state(mid, state):
    """Cambia el estado del id en assets/inventario.tsv (el resto de la linea queda igual)."""
    with open(inventario.TSV, encoding="utf-8") as f:
        lines = f.read().split("\n")
    col = inventario.COLUMNS.index("estado")
    for i, line in enumerate(lines):
        cols = line.split("\t")
        if cols and cols[0] == mid and len(cols) == len(inventario.COLUMNS):
            cols[col] = state
            lines[i] = "\t".join(cols)
            break
    else:
        raise SystemExit(f"{mid}: no esta en assets/inventario.tsv")
    with open(inventario.TSV, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


# ---------------------------------------------------------------- manifiesto (el de Cline, con la procedencia)
def load_manifest():
    if os.path.isfile(MANIFEST):
        try:
            with open(MANIFEST, encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, dict):
                data.setdefault("models", {})
                return data
        except json.JSONDecodeError:
            pass
    return {"models": {}}


def save_manifest(data):
    os.makedirs(ARTE, exist_ok=True)
    data["models"] = dict(sorted(data["models"].items()))
    with open(MANIFEST, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")


# ---------------------------------------------------------------- traer
def download(url, dest):
    """Baja url a dest (con el User-Agent de un navegador: algunos espacios de Hugging Face lo piden)."""
    req = urllib.request.Request(url, headers={
        "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36",
        "Accept": "model/gltf-binary, application/octet-stream, */*",
    })
    tmp = dest + ".parcial"
    with urllib.request.urlopen(req, timeout=120) as r, open(tmp, "wb") as f:
        shutil.copyfileobj(r, f)
    os.replace(tmp, dest)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_rotations(text):
    """'z:90,y:-45' -> [('z', 90.0), ('y', -45.0)]: los giros previos de la conversion, en orden."""
    out = []
    for part in (text or "").split(","):
        if part.strip():
            axis, _, deg = part.strip().partition(":")
            if axis.strip().lower() not in ("x", "y", "z"):
                raise SystemExit(f"--rotar: '{part}' (eje x, y o z, y grados: z:90)")
            out.append((axis.strip().lower(), float(deg)))
    return out


def parse_origin(text):
    if not text:
        return None
    parts = [float(x) for x in text.split(",")]
    if len(parts) != 3:
        raise SystemExit("--origen: tres numeros separados por comas (x,y,z en m)")
    return parts


def check_model(it, path):
    """Errores y avisos de un GLB importado (sin dependencias)."""
    errors, warnings = [], []
    rel = os.path.relpath(path, ROOT)
    try:
        g = glb.Glb(path)
    except (glb.GlbError, OSError, ValueError) as e:
        return [f"{rel}: {e}"], []
    tris, tmax = g.triangulos(), int(it["tris_max"])
    if tmax and tris > tmax:
        errors.append(f"{rel}: {tris} triangulos (tris_max {tmax}); bajalo con tools/modelos/modelos.py")
    for name, w, h in g.imagenes():
        if w is None:
            warnings.append(f"{rel}: la imagen '{name}' no se puede medir (externa o de formato raro)")
        elif max(w, h) > TEXTURE_MAX:
            errors.append(f"{rel}: textura '{name}' de {w}x{h} px (como mucho {TEXTURE_MAX})")
    box = g.limites()
    if box is None:
        errors.append(f"{rel}: no tiene mallas")
        return errors, warnings
    lo, hi = box
    ancho, alto, largo = dims(it)
    got = (hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2])
    for axis, want, name in ((0, largo, "largo (X)"), (1, alto, "alto (Y)"), (2, ancho, "ancho (Z)")):
        if want >= 0.05 and not 0.5 <= got[axis] / want <= 2.0:
            warnings.append(f"{rel}: {name} de {got[axis]:.2f} m (inventario {want:g} m): revisa la escala o el giro")
    equip = "uso:equipable" in it["tags"]
    if not equip and abs(lo[1]) > 0.05 + 0.02 * max(got[1], 0.01):
        warnings.append(f"{rel}: la base esta a {lo[1]:+.2f} m del suelo (el origen va en el centro de la base)")
    rig = next((t.split(":", 1)[1] for t in it["tags"] if t.startswith("rig:")), "ninguno")
    if rig != "ninguno" and it["categoria"] in ("personaje", "animal") and g.pieles() == 0:
        warnings.append(f"{rel}: sin esqueleto (rig:{rig}): no se va a animar")
    if g.size > BYTES_AVISO:
        warnings.append(f"{rel}: {g.size / 1048576:.1f} MB (pesa en el APK)")
    return errors, warnings


def check(quiet=False):
    items, errors = inventario.load()
    warnings, count = [], 0
    for it in items:
        if "formato:textura" in it["tags"]:
            continue
        path = os.path.join(ROOT, inventario.path_of(it))
        if not os.path.isfile(path):
            continue
        count += 1
        e, w = check_model(it, path)
        errors += e
        warnings += w
    known = {it["id"] for it in items}
    for mid in load_manifest()["models"]:
        if mid not in known:
            warnings.append(f"arte/modelos/models_manifest.json: '{mid}' no esta en el inventario")
    for w in warnings:
        print(f"aviso: {w}")
    for e in errors:
        print(f"ERROR: {e}")
    if not quiet:
        print(f"{count} modelos importados, {len(errors)} errores, {len(warnings)} avisos.")
    return 1 if errors else 0


def traer(mid, url=None, archivo=None, girar=0.0, rotar=None, escala=None, origen=None, colores=24, luz=True, limpiar=0.01,
          riggeado=False, fuente="otro", reemplazar=False, log=print):
    items, errors = inventario.load()
    if errors:
        raise SystemExit("\n".join(errors))
    it = find_item(items, mid)
    if it is None:
        raise SystemExit(f"{mid}: no esta en assets/inventario.tsv (python3 tools/assets/inventario.py faltan)")
    if "formato:textura" in it["tags"]:
        raise SystemExit(f"{mid}: es una textura (PNG con alfa), no un modelo")
    if fuente not in FUENTES_VALIDAS:
        raise SystemExit(f"--fuente: {', '.join(FUENTES_VALIDAS)}")
    dest = os.path.join(ROOT, inventario.path_of(it))
    if os.path.isfile(dest) and not reemplazar:
        raise SystemExit(f"{mid}: ya tiene modelo ({it['estado']}, {inventario.path_of(it)}); usa --reemplazar")
    # La fuente, fuera de git.
    os.makedirs(FUENTES, exist_ok=True)
    if url:
        ext = os.path.splitext(urllib.parse.urlparse(url).path)[1].lower() or ".glb"
        src = os.path.join(FUENTES, mid + (ext if ext in (".glb", ".gltf", ".obj") else ".glb"))
        log(f"[modelos] bajando {url}")
        download(url, src)
    elif archivo:
        if not os.path.isfile(archivo):
            raise SystemExit(f"{archivo}: no existe")
        src = os.path.join(FUENTES, mid + os.path.splitext(archivo)[1].lower())
        if os.path.abspath(archivo) != os.path.abspath(src):
            shutil.copyfile(archivo, src)
    else:
        raise SystemExit("falta --url o --archivo")
    log(f"[modelos] fuente: {os.path.relpath(src, ROOT)} ({os.path.getsize(src) / 1048576:.2f} MB)")
    clips = needs_clips(items, it)
    source_glb = None
    if src.endswith(".glb"):
        try:
            source_glb = glb.Glb(src)
        except glb.GlbError as e:
            raise SystemExit(str(e))
    if not riggeado and source_glb is not None and source_glb.pieles() and source_glb.animaciones():
        log("[modelos] la fuente trae esqueleto y clips: se toma como --riggeado")
        riggeado = True
    entry = {
        "file": inventario.path_of(it).replace(os.sep, "/"),
        "category": it["categoria"],
        "has_rig": bool(riggeado),
        "source": {"tool": fuente, "url": url or "", "file": os.path.relpath(src, ROOT).replace(os.sep, "/"),
                   "sha256": sha256(src)},
    }
    if riggeado:
        if source_glb is None:
            raise SystemExit("--riggeado: la fuente tiene que ser un GLB")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(src, dest)
        missing = sorted(set(clips) - set(source_glb.animaciones()))
        entry["tris"] = source_glb.triangulos()
        if missing:
            log(f"[modelos] AVISO: faltan clips obligatorios: {', '.join(missing)} (docs/ANIMACIONES.md)")
    else:
        import lowres  # las dependencias de la conversion, solo aca

        data, informe = lowres.convertir(src, dims(it), int(it["tris_max"]) or 300, girar=girar, rotar=rotar, escala=escala,
                                         origen=origen, colores=colores, luz=luz, limpiar=limpiar, nombre=mid)
        entry["params"] = {"girar": girar, "rotar": informe["rotar"], "escala": informe["escala"], "origen": informe["origen"],
                           "colores": colores, "luz": luz, "limpiar": limpiar}
        entry["tris"] = informe["tris"]
        entry["source"]["tris"] = informe["tris_fuente"]
        log(f"[modelos] {informe['tris_fuente']} -> {informe['tris']} triangulos (tris_max {it['tris_max']}), "
            f"{informe['paleta']} colores, caja {informe['caja_m'][0]:.2f} x {informe['caja_m'][1]:.2f} x {informe['caja_m'][2]:.2f} m "
            f"(largo x alto x ancho; inventario {dims(it)[2]:g} x {dims(it)[1]:g} x {dims(it)[0]:g})")
        if clips:  # necesita esqueleto y clips: todavia no entra al juego
            os.makedirs(PARA_RIGGEAR, exist_ok=True)
            out = os.path.join(PARA_RIGGEAR, mid + ".glb")
            with open(out, "wb") as f:
                f.write(data)
            entry["file"] = os.path.relpath(out, ROOT).replace(os.sep, "/")
            manifest = load_manifest()
            manifest["models"][mid] = dict(entry, updated_at=datetime.now(timezone.utc).isoformat(timespec="seconds"))
            save_manifest(manifest)
            log(f"[modelos] {mid} necesita esqueleto y {len(clips)} clips obligatorios (docs/ANIMACIONES.md):")
            log(f"[modelos] la version low-poly quedo en {os.path.relpath(out, ROOT)}; riggeala (RigNet) y animala,")
            log(f"[modelos] y traela con: python3 tools/modelos/modelos.py traer {mid} --archivo <riggeado.glb> --riggeado")
            return 0
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as f:
            f.write(data)
    set_state(mid, "importado")
    manifest = load_manifest()
    manifest["models"][mid] = dict(entry, updated_at=datetime.now(timezone.utc).isoformat(timespec="seconds"))
    save_manifest(manifest)
    # Los documentos y los chequeos, como en el CI.
    items, _ = inventario.load()
    with open(inventario.DOC, "w", encoding="utf-8") as f:
        f.write(inventario.render_doc(items) + "\n")
    e, w = check_model(find_item(items, mid), dest)
    ie, iw = inventario.validate(items)
    clip_rows, _ = animaciones.load_clips()
    ae, aw = animaciones.validate(items, clip_rows)
    for msg in w + [x for x in iw + aw if mid in x]:
        log(f"aviso: {msg}")
    for msg in e + ie + ae:
        log(f"ERROR: {msg}")
    log(f"[modelos] {mid}: importado en {inventario.path_of(it)}")
    log(f"[modelos] miralo en el juego: ./build/estepa --ver {mid} --screenshot ver.png (desde build/, con los assets copiados)")
    return 1 if (e or ie or ae) else 0


def bajar(mid, fuente_path, salida=None, **opts):
    items, _ = inventario.load()
    it = find_item(items, mid)
    if it is None:
        raise SystemExit(f"{mid}: no esta en assets/inventario.tsv")
    import lowres

    data, informe = lowres.convertir(fuente_path, dims(it), int(it["tris_max"]) or 300, nombre=mid, **opts)
    salida = salida or os.path.join(ROOT, "build", "modelos", mid + ".glb")
    os.makedirs(os.path.dirname(os.path.abspath(salida)), exist_ok=True)
    with open(salida, "wb") as f:
        f.write(data)
    print(json.dumps(informe, ensure_ascii=False))
    print(f"[modelos] escrito {salida}")
    return 0


# ---------------------------------------------------------------- estado
PRIORIDAD = [  # docs/PLAN_HERRAMIENTAS_IA.md, D.2: lo que mas se ve, primero
    ("personas", lambda it: it["categoria"] == "personaje"),
    ("animales", lambda it: it["categoria"] == "animal"),
    ("armas y armaduras", lambda it: it["categoria"] in ("arma", "escudo", "armadura")),
    ("estructuras", lambda it: it["categoria"] == "estructura"),
    ("objetos y accesorios", lambda it: True),
]


def estado(mid=None):
    items, _ = inventario.load()
    manifest = load_manifest()["models"]
    if mid:
        it = find_item(items, mid)
        if it is None:
            raise SystemExit(f"{mid}: no esta en el inventario")
        print(f"{mid}: {it['estado']} · {it['medidas_m']} m · tris_max {it['tris_max']}")
        clips = needs_clips(items, it)
        if clips:
            print(f"  necesita esqueleto y {len(clips)} clips obligatorios: {', '.join(clips)}")
        if mid in manifest:
            print("  " + json.dumps(manifest[mid], ensure_ascii=False))
        return 0
    for k, m in manifest.items():
        src = m.get("source", {})
        print(f"{k}: {m.get('file')} · {src.get('tool', '?')} · {m.get('tris', '?')} tris · {m.get('updated_at', '')}")
    pending = [it for it in items if it["estado"] == "pendiente" and "formato:textura" not in it["tags"]]
    print(f"\nPendientes: {len(pending)} modelos (sin contar las texturas). Por prioridad:")
    done = set()
    for name, pred in PRIORIDAD:
        group = [it for it in pending if it["id"] not in done and pred(it)]
        done.update(it["id"] for it in group)
        sample = ", ".join(it["id"].split(".", 1)[1] for it in group[:6])
        print(f"  {name}: {len(group)}" + (f" (p. ej. {sample})" if group else ""))
    return 0


# ---------------------------------------------------------------- linea de comandos
def conversion_args(p):
    p.add_argument("--girar", type=float, default=0.0)
    p.add_argument("--rotar", default="")
    p.add_argument("--escala", type=float, default=None)
    p.add_argument("--origen", default=None)
    p.add_argument("--colores", type=int, default=24)
    p.add_argument("--sin-luz", action="store_true")
    p.add_argument("--limpiar", type=float, default=0.01)


def main(argv):
    ap = argparse.ArgumentParser(prog="modelos.py", description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")
    t = sub.add_parser("traer")
    t.add_argument("id")
    t.add_argument("--url")
    t.add_argument("--archivo")
    t.add_argument("--riggeado", action="store_true")
    t.add_argument("--fuente", default="otro", choices=FUENTES_VALIDAS)
    t.add_argument("--reemplazar", action="store_true")
    conversion_args(t)
    b = sub.add_parser("bajar")
    b.add_argument("id")
    b.add_argument("fuente")
    b.add_argument("--salida")
    conversion_args(b)
    sub.add_parser("check")
    e = sub.add_parser("estado")
    e.add_argument("id", nargs="?")
    if len(argv) < 2:
        print(__doc__)
        return 2
    a = ap.parse_args(argv[1:])
    if a.cmd == "check":
        return check()
    if a.cmd == "estado":
        return estado(a.id)
    opts = dict(girar=a.girar, rotar=parse_rotations(a.rotar), escala=a.escala, origen=parse_origin(a.origen), colores=a.colores,
                luz=not a.sin_luz, limpiar=a.limpiar)
    if a.cmd == "bajar":
        return bajar(a.id, a.fuente, a.salida, **opts)
    if a.cmd == "traer":
        return traer(a.id, url=a.url, archivo=a.archivo, riggeado=a.riggeado, fuente=a.fuente, reemplazar=a.reemplazar, **opts)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
