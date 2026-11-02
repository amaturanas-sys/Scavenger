#!/usr/bin/env python3
"""Las guardas del proyecto: lo que ya salio mal no vuelve a entrar.

Corre en el CI de cada PR (job de Linux, que es obligatorio para fusionar) y antes de cada push, en el
Codespace. Cada guarda nacio de un error real (docs/GUIA_IA.md, seccion 5):

  - archivos sueltos en la raiz o carpetas nuevas sin acordar (ASSETS_MANIFEST.md, build_log.txt,
    campamento.json, __pycache__);
  - archivos vacios o falsos en assets/ (82 GLB y PNG de 0 bytes que decian ser modelos);
  - texto roto por la codificacion ("9 slots xc2x97 6");
  - un .c que no esta en CMakeLists.txt (vat_example.c);
  - el mismo tipo o enumerador declarado en dos headers (ChannelState en world.h y climate.h);
  - comparando con la base (--base: main, o el commit de antes):
      - pruebas borradas o recortadas (mil lineas menos en tests/test_main.c), RUN repetidos o sin funcion;
      - un enum que crece por el medio (KA_TARGET_LEFT antes de KA_MARK): las partidas guardan numeros;
      - VERSION que sube sin su seccion en el README y en docs/ROADMAP.md, o sin cambiar el juego;
      - las reglas de Cline y de Copilot, que dicen lo mismo (el bloque comun).

Uso:
  python3 tools/ci/guardas.py                 # contra el punto donde la rama salio de origin/main
  python3 tools/ci/guardas.py --base HEAD^1   # el CI: contra el commit de antes (main en un PR)

Mira el arbol de trabajo tal como esta (con lo que aun no se commiteo, menos lo ignorado). Sale con 1 si
hay algun ERROR; los AVISOS no frenan, pero Claude los mira al verificar el PR.

Si una prueba se retira a proposito, va en tests/retiradas.txt con el motivo (lo aprueba Claude).
"""

import argparse
import os
import re
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Lo que puede haber en la raiz y en src/ (algo nuevo se agrega aca, a la vista en el PR).
RAIZ_ARCHIVOS = {".clineignore", ".clinerules", ".gitignore", ".gitattributes", "CMakeLists.txt", "README.md", "VERSION"}
RAIZ_CARPETAS = {".claude", ".devcontainer", ".githooks", ".github", "android", "arte", "assets", "docs", "src", "tests", "tools"}
SRC_PERMITIDOS = {"entities", "game", "sim", "ui", "world", "main.c", "platform.c", "platform.h"}
# Basura que nunca va al repositorio, este donde este.
PROHIBIDOS = re.compile(r"(^|/)(__pycache__/|[^/]*\.pyc$|[^/]*\.log$|build_log\.txt$|[^/]*\.(orig|rej|bak)$|campamento\.json$)")
# Los primeros bytes de cada formato binario: un archivo con otra cosa no es lo que dice ser.
FIRMAS = {
    ".glb": b"glTF",
    ".png": b"\x89PNG\r\n\x1a\n",
    ".jpg": b"\xff\xd8\xff",
    ".jpeg": b"\xff\xd8\xff",
    ".ogg": b"OggS",
    ".wav": b"RIFF",
}
TEXTO = {".c", ".h", ".md", ".py", ".tsv", ".txt", ".yml", ".yaml", ".json", ".sh", ".cmake", ".fs", ".vs", ".glsl", ".inc"}
TEXTO_NOMBRES = {".clinerules", ".clineignore", "CMakeLists.txt", "VERSION"}
# Mojibake: UTF-8 leido como Latin-1 (Ã¡, Ã±, Â¿, â€), bytes escritos a mano (xc2x97) y el caracter de reemplazo.
ROTO = re.compile("Ã[ -¿]|Ã[‘-„]|Â[¡¿°º·]|â€|\\bx[cdef][0-9a-f]x[89ab][0-9a-f]|�")
# Herramientas compartidas: cambiarlas para que algo "pase" ya rompio todo (inventario.py).
HERRAMIENTAS = re.compile(r"^(tools/assets/|tools/save/|tools/modelos/|tools/ci/|tools/android/|tools/release/|\.github/workflows/|CMakeLists\.txt$|\.devcontainer/)")
# Lo que cuenta como un cambio del juego (para subir VERSION).
JUEGO = re.compile(r"^(src/|assets/|tools/|tests/|android/|CMakeLists\.txt$)")
# El bloque comun de las reglas de Cline y de Copilot.
REGLAS = (".clinerules", ".github/copilot-instructions.md")
MARCA_INICIO, MARCA_FIN = "<!-- reglas-comunes:inicio -->", "<!-- reglas-comunes:fin -->"

errores, avisos = [], []


def error(tipo, msg):
    errores.append(f"ERROR [{tipo}] {msg}")


def aviso(tipo, msg):
    avisos.append(f"AVISO [{tipo}] {msg}")


def git(*args, check=False):
    r = subprocess.run(["git", *args], cwd=RAIZ, capture_output=True)
    if check and r.returncode != 0:
        sys.exit(f"guardas: fallo 'git {' '.join(args)}': {r.stderr.decode(errors='replace').strip()}")
    return r


def archivos():
    """Lo versionado y lo nuevo que no esta ignorado (lo que subiria un 'git add -A'), que exista."""
    r = git("ls-files", "-z", "--cached", "--others", "--exclude-standard", check=True)
    vistos = []
    for f in r.stdout.decode("utf-8", errors="replace").split("\0"):
        if f and f not in vistos and os.path.isfile(os.path.join(RAIZ, f)):
            vistos.append(f)
    return vistos


def leer(f):
    with open(os.path.join(RAIZ, f), "rb") as h:
        return h.read().decode("utf-8", errors="replace")


def en_base(base, f):
    r = git("show", f"{base}:{f}")
    return r.stdout.decode("utf-8", errors="replace") if r.returncode == 0 else None


def sin_comentarios(src):
    src = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


# --------------------------------------------------------------------------- el arbol
def guarda_lugares(fs):
    for f in fs:
        partes = f.split("/")
        if len(partes) == 1 and f not in RAIZ_ARCHIVOS:
            error("lugar", f"{f}: archivo suelto en la raiz (los documentos van en docs/, en español).")
        elif len(partes) > 1 and partes[0] not in RAIZ_CARPETAS:
            error("lugar", f"{f}: carpeta '{partes[0]}/' nueva en la raiz; se acuerda antes con el usuario y Claude.")
        if partes[0] == "src" and len(partes) > 1 and partes[1] not in SRC_PERMITIDOS:
            error("lugar", f"{f}: 'src/{partes[1]}' es nuevo; la arquitectura (docs/ARQUITECTURA.md) se acuerda antes.")
        if PROHIBIDOS.search(f):
            error("basura", f"{f}: no va al repositorio (registros, caches de Python, copias, salidas de prueba).")


def guarda_binarios(fs):
    for f in fs:
        if not f.startswith(("assets/", "arte/", "android/")) or os.path.basename(f) == ".gitkeep":
            continue
        ruta = os.path.join(RAIZ, f)
        tam = os.path.getsize(ruta)
        if tam == 0:
            error("vacio", f"{f}: archivo vacio. Un modelo o una textura entra con su contenido real "
                           "(tools/modelos/modelos.py traer), nunca un marcador.")
            continue
        firma = FIRMAS.get(os.path.splitext(f)[1].lower())
        if firma:
            with open(ruta, "rb") as h:
                if h.read(len(firma)) != firma:
                    error("formato", f"{f}: no es un {os.path.splitext(f)[1]} de verdad (sus primeros bytes no coinciden).")


def guarda_texto(fs):
    for f in fs:
        if os.path.splitext(f)[1].lower() not in TEXTO and os.path.basename(f) not in TEXTO_NOMBRES:
            continue
        if f == "tools/ci/guardas.py":
            continue  # aca estan los patrones
        for n, linea in enumerate(leer(f).split("\n"), 1):
            m = ROTO.search(linea)
            if m:
                error("texto", f"{f}:{n}: texto roto por la codificacion ('{m.group(0)}'). Escribe el caracter "
                               "(×, á, ñ...) en UTF-8.")


def guarda_cmake(fs):
    cmake = leer("CMakeLists.txt")
    for f in fs:
        if f.startswith("src/") and f.endswith(".c") and f not in cmake:
            error("cmake", f"{f}: no esta en CMakeLists.txt (o se compila, o no va en src/).")


def tipos(texto):
    """Los tipos con cuerpo (typedef enum/struct/union { ... } Nombre;) y los enumeradores de cada enum."""
    texto = sin_comentarios(texto)
    out = []
    for m in re.finditer(r"\btypedef\s+(enum|struct|union)\b[^{;]*\{", texto):
        nivel, i = 1, m.end()
        while i < len(texto) and nivel:
            nivel += {"{": 1, "}": -1}.get(texto[i], 0)
            i += 1
        cuerpo = texto[m.end():i - 1]
        nombre = re.match(r"\s*(\w+)\s*;", texto[i:])
        if not nombre:
            continue
        enums = []
        if m.group(1) == "enum":
            for parte in cuerpo.split(","):
                ident = re.match(r"\s*([A-Za-z_]\w*)", parte)
                if ident:
                    enums.append(ident.group(1))
        out.append((nombre.group(1), m.group(1), enums))
    return out


def guarda_tipos(fs):
    donde_tipo, donde_enum = {}, {}
    for f in fs:
        if not (f.startswith("src/") and f.endswith(".h")):
            continue
        for nombre, _, enums in tipos(leer(f)):
            donde_tipo.setdefault(nombre, []).append(f)
            for e in enums:
                donde_enum.setdefault(e, []).append(f)
    for nombre, lugares in sorted(donde_tipo.items()):
        if len(set(lugares)) > 1:
            error("tipos", f"'{nombre}' esta declarado en {', '.join(sorted(set(lugares)))}: un tipo vive en un solo header.")
    for e, lugares in sorted(donde_enum.items()):
        if len(set(lugares)) > 1:
            error("tipos", f"el enumerador '{e}' esta en {', '.join(sorted(set(lugares)))}.")


def guarda_reglas():
    bloques = []
    for f in REGLAS:
        ruta = os.path.join(RAIZ, f)
        if not os.path.isfile(ruta):
            error("reglas", f"falta {f} (las reglas de Cline y de Copilot).")
            return
        t = leer(f)
        if MARCA_INICIO not in t or MARCA_FIN not in t:
            error("reglas", f"{f}: falta el bloque comun ({MARCA_INICIO} ... {MARCA_FIN}).")
            return
        bloques.append(t.split(MARCA_INICIO, 1)[1].split(MARCA_FIN, 1)[0].strip())
    if bloques[0] != bloques[1]:
        error("reglas", f"el bloque comun de {REGLAS[0]} y de {REGLAS[1]} no es igual: se cambian los dos juntos.")


# --------------------------------------------------------------------------- contra la base
PRUEBA = re.compile(r"^static void (test_\w+)\(void\) \{$", re.M)


def pruebas(texto):
    """Cada funcion de prueba con cuantas comprobaciones tiene, y los RUN en orden."""
    funcs = {}
    lineas = texto.split("\n")
    for i, linea in enumerate(lineas):
        m = PRUEBA.match(linea)
        if not m:
            continue
        cuenta = 0
        for j in range(i + 1, len(lineas)):
            if lineas[j] == "}":
                break
            cuenta += len(re.findall(r"\bCHECK\w*\(", lineas[j]))
        funcs[m.group(1)] = cuenta
    runs = re.findall(r"^\s*RUN\((test_\w+)\);", texto, re.M)
    return funcs, runs


def retiradas():
    ruta = os.path.join(RAIZ, "tests", "retiradas.txt")
    if not os.path.isfile(ruta):
        return set()
    out = set()
    for linea in leer("tests/retiradas.txt").split("\n"):
        m = re.match(r"\s*(test_\w+)\s*:\s*\S", linea)
        if m:
            out.add(m.group(1))
    return out


def guarda_pruebas(base):
    antes = en_base(base, "tests/test_main.c")
    ahora = leer("tests/test_main.c")
    funcs, runs = pruebas(ahora)
    vistas = set()
    for r in runs:
        if r in vistas:
            error("pruebas", f"RUN({r}) esta dos veces.")
        vistas.add(r)
        if r not in funcs:
            error("pruebas", f"RUN({r}) no tiene su funcion.")
    for f in funcs:
        if f not in vistas:
            aviso("pruebas", f"{f} no se corre (le falta su RUN).")
    if antes is None:
        return
    f0, r0 = pruebas(antes)
    ok = retiradas()
    for f, n in f0.items():
        if f in ok:
            continue
        if f not in funcs:
            error("pruebas", f"{f} se borro. Las pruebas no se borran: si se retira a proposito, va en "
                             "tests/retiradas.txt con el motivo.")
        elif funcs[f] < n:
            error("pruebas", f"{f} tenia {n} comprobaciones y ahora {funcs[f]}: las pruebas no se recortan.")
    for r in r0:
        if r not in vistas and r not in ok and r in funcs:
            error("pruebas", f"RUN({r}) se quito: la prueba ya no corre.")


def guarda_enums(base, fs):
    ahora = {}
    for f in fs:
        if f.startswith("src/") and f.endswith(".h"):
            for nombre, clase, enums in tipos(leer(f)):
                if clase == "enum":
                    ahora[nombre] = (f, enums)
    r = git("ls-tree", "-r", "--name-only", base, "--", "src")
    for f in r.stdout.decode("utf-8", errors="replace").split("\n"):
        if not f.endswith(".h"):
            continue
        for nombre, clase, viejos in tipos(en_base(base, f) or ""):
            if clase != "enum" or not viejos:
                continue
            if nombre not in ahora:
                error("enums", f"el enum {nombre} ({f}) desaparecio: las partidas guardan sus numeros.")
                continue
            nuevos = ahora[nombre][1]
            al_final = nuevos[:len(viejos)] == viejos
            antes_del_cierre = nuevos[:len(viejos) - 1] == viejos[:-1] and nuevos[-1:] == viejos[-1:]
            if not (al_final or antes_del_cierre):
                error("enums", f"{nombre} ({ahora[nombre][0]}): los valores cambiaron de lugar. Los enums crecen al "
                               "final (o justo antes de su _COUNT); nunca por el medio ni borrando.")


def version_tupla(v):
    return tuple(int(x) for x in re.findall(r"\d+", v)[:3])


def guarda_version(base, cambiados):
    antes = (en_base(base, "VERSION") or "").strip()
    ahora = leer("VERSION").strip() if os.path.isfile(os.path.join(RAIZ, "VERSION")) else ""
    if not antes or antes == ahora:
        return
    if version_tupla(ahora) <= version_tupla(antes):
        error("version", f"VERSION baja o no cambia de verdad ({antes} -> {ahora}).")
    if f"(v{ahora})" not in leer("README.md"):
        error("version", f"VERSION es {ahora} pero el README no tiene su seccion «… (v{ahora})».")
    if ahora not in leer("docs/ROADMAP.md"):
        error("version", f"VERSION es {ahora} pero docs/ROADMAP.md no tiene su linea.")
    if not any(JUEGO.match(f) for f in cambiados):
        error("version", "VERSION sube sin ningun cambio del juego (src/, assets/, tools/, tests/): una version que "
                         "solo toca documentos no es una version.")


def guarda_cambios(cambiados):
    tocadas = sorted(f for f in cambiados if HERRAMIENTAS.match(f))
    if tocadas:
        aviso("herramientas", "cambia herramientas compartidas o el CI: " + ", ".join(tocadas) +
              ". Claude revisa que no se aflojen para que algo pase.")
    sim = [f for f in cambiados if f.startswith("src/sim/") and f.endswith(".c")]
    if sim and "tests/test_main.c" not in cambiados:
        aviso("pruebas", "cambian reglas de src/sim/ (" + ", ".join(sim) + ") sin tocar tests/test_main.c.")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", help="contra que comparar (por omision: donde la rama salio de origin/main)")
    args = ap.parse_args()

    fs = archivos()
    guarda_lugares(fs)
    guarda_binarios(fs)
    guarda_texto(fs)
    guarda_cmake(fs)
    guarda_tipos(fs)
    guarda_reglas()

    base = args.base
    if not base:
        r = git("merge-base", "origin/main", "HEAD")
        base = r.stdout.decode().strip() if r.returncode == 0 else None
        if not base:
            aviso("base", "sin origin/main para comparar (git fetch origin): solo se miro el arbol.")
    if base:
        if git("rev-parse", "--verify", "--quiet", f"{base}^{{commit}}").returncode != 0:
            sys.exit(f"guardas: no encuentro la base '{base}' (¿falta un git fetch?).")
        r = git("diff", "--name-only", base, check=True)
        nuevos = git("ls-files", "--others", "--exclude-standard", check=True)
        cambiados = set(r.stdout.decode("utf-8", errors="replace").split()) | set(nuevos.stdout.decode("utf-8", errors="replace").split())
        cambiados.discard("")
        guarda_pruebas(base)
        guarda_enums(base, fs)
        guarda_version(base, cambiados)
        guarda_cambios(cambiados)

    for linea in avisos + errores:
        print(linea)
    print(f"guardas: {len(errores)} errores, {len(avisos)} avisos" + (f" (contra {base[:12]})" if base else ""))
    return 1 if errores else 0


if __name__ == "__main__":
    sys.exit(main())
