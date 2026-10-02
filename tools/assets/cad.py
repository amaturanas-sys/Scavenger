#!/usr/bin/env python3
"""text-to-cad (cadgen) para los modelos de ESTEPA.

El skill CAD de text-to-cad vive en .claude/skills/cad (Claude Code lo carga
solo en este repositorio). Este script prepara su entorno de Python y revisa
los GLB del inventario sin tener que recordar rutas ni opciones.

Uso:
  python3 tools/assets/cad.py instalar           crea .venv-cad con cadgen (version fijada por el skill)
  python3 tools/assets/cad.py doctor             comprueba la instalacion
  python3 tools/assets/cad.py foto <id|ruta.glb> [camara...]
                                                 fotos del modelo en build/cad/ (por defecto: iso right front top)
  python3 tools/assets/cad.py visor              abre el CAD Viewer sobre assets/models (medir, revisar)

Camaras: front, back, left, right, top, bottom, iso, o "azimut:elevacion".
En los modelos del juego +X es el frente (convencion de Kiln): la camara "right" lo muestra.
"""
import os
import subprocess
import sys
import venv

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import inventario  # noqa: E402

ROOT = inventario.ROOT
SKILL = os.path.join(ROOT, ".claude", "skills", "cad")
VENV = os.path.join(ROOT, ".venv-cad")
OUT = os.path.join(ROOT, "build", "cad")


def venv_python():
    sub = ("Scripts", "python.exe") if os.name == "nt" else ("bin", "python")
    return os.path.join(VENV, *sub)


def cadgen(*args, cwd=ROOT):
    py = venv_python()
    if not os.path.isfile(py):
        print("Falta el entorno: ejecuta 'python3 tools/assets/cad.py instalar'.")
        return 1
    return subprocess.call([py, "-m", "cadgen.cli", *args], cwd=cwd)


def instalar():
    if not os.path.isfile(venv_python()):
        print(f"Creando {os.path.relpath(VENV, ROOT)} ...")
        venv.create(VENV, with_pip=True)
    py = venv_python()
    req = os.path.join(SKILL, "requirements.txt")
    if subprocess.call([py, "-m", "pip", "install", "-r", req]) != 0:
        return 1
    # Las fotos usan Chromium (Playwright). En entornos que ya lo traen, se omite.
    if not os.environ.get("PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD"):
        if subprocess.call([py, "-m", "playwright", "install", "chromium"]) != 0:
            return 1
    return cadgen("doctor", SKILL)


def resolve(target):
    if target.endswith(".glb"):
        return os.path.abspath(target)
    items, _ = inventario.load()
    for it in items:
        if it["id"] == target:
            return os.path.join(ROOT, inventario.path_of(it))
    print(f"{target}: no esta en assets/inventario.tsv")
    return None


def foto(target, cameras):
    path = resolve(target)
    if not path:
        return 1
    if not os.path.isfile(path):
        print(f"{os.path.relpath(path, ROOT)}: el modelo aun no fue importado.")
        return 1
    os.makedirs(OUT, exist_ok=True)
    stem = os.path.splitext(os.path.basename(path))[0]
    rc = 0
    for cam in cameras or ["iso", "right", "front", "top"]:
        out = os.path.join(OUT, f"{stem}_{cam.replace(':', '_')}.png")
        rc |= cadgen("glb", "snapshot", "--camera", cam, path, out)
    return rc


def main(argv):
    cmd = argv[1] if len(argv) > 1 else ""
    if cmd == "instalar":
        return instalar()
    if cmd == "doctor":
        return cadgen("doctor", SKILL)
    if cmd == "foto" and len(argv) > 2:
        return foto(argv[2], argv[3:])
    if cmd == "visor":
        return cadgen("viewer", cwd=os.path.join(ROOT, "assets", "models"))
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
