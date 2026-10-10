#!/usr/bin/env python3
"""Esquema de la partida guardada (docs/PARTIDAS.md).

Lee los structs de los encabezados de src/ y escribe src/game/save_schema.inc: la descripcion, campo por campo, de
todo lo que se guarda (lo que alcanzan las raices de ROOTS). Las posiciones y los tamaños los pone
el compilador (offsetof, sizeof), asi que el esquema siempre coincide con el build. Cada bloque de
la partida lleva su esquema: una version nueva del juego lee las partidas viejas campo por campo,
por nombre, aunque los structs hayan cambiado.

Uso:
  python3 tools/save/esquema.py          regenera src/game/save_schema.inc (tras cambiar un struct que se guarda)
  python3 tools/save/esquema.py check    falla si no esta al dia (CI)
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "src", "game", "save_schema.inc")

# Lo que se guarda: una raiz por bloque de la partida (src/game/save_game.c).
ROOTS = ["SavedMisc", "Player", "Kingdom", "Troop", "GameActions", "Combat", "Hazards", "Disasters",
         "SavedProp", "MemoryPage", "MapMarker", "DeathSave", "QbSlot"]

SIGNED = {"int", "short", "long long", "signed", "signed char", "signed short", "signed int",
          "int8_t", "int16_t", "int32_t", "int64_t"}
UNSIGNED = {"unsigned", "unsigned int", "unsigned short", "unsigned char", "unsigned long long",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t"}
FLOATS = {"float", "double"}
BOOLS = {"bool", "_Bool"}
# Tipos de raylib que no cambian: se guardan tal cual.
OPAQUE = {"Vector2", "Vector3", "Vector4", "Quaternion", "Color", "Rectangle", "Matrix"}
# Cambian de tamaño entre Windows y Linux/Android: no sirven en una partida.
FORBIDDEN = {"long", "unsigned long", "long int", "size_t", "ssize_t", "intptr_t", "uintptr_t", "wchar_t", "long double"}


def strip_comments(text):
    """Quita los comentarios (respetando las comillas)."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            out.append(" ")
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def matching_brace(text, i):
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return j
    raise SystemExit("llave sin cerrar")


def find_types(files):
    """Los typedef struct y typedef enum de los archivos: {nombre: (archivo, cuerpo)}, {enums}."""
    structs, enums = {}, set()
    for path in files:
        with open(path, encoding="utf-8") as f:
            text = strip_comments(f.read())
        for m in re.finditer(r"\btypedef\s+(struct|enum)\s*\w*\s*\{", text):
            end = matching_brace(text, m.end() - 1)
            name = re.match(r"\s*(\w+)\s*;", text[end + 1:])
            if not name:
                continue
            name = name.group(1)
            if m.group(1) == "enum":
                enums.add(name)
                continue
            if name in structs:
                raise SystemExit(f"{name}: dos structs con el mismo nombre ({structs[name][0]} y {path})")
            structs[name] = (os.path.relpath(path, ROOT), text[m.end():end])
    return structs, enums


DECL = re.compile(r"^((?:const\s+|volatile\s+)*)((?:unsigned|signed)\s+(?:long\s+long|short|char|int|long)|"
                  r"long\s+long|long\s+double|long\s+int|unsigned\s+long|unsigned|signed|\w+)\s+(.*)$", re.S)


def split_top(text, sep):
    """Parte en sep fuera de corchetes y parentesis."""
    out, depth, cur = [], 0, []
    for c in text:
        if c in "[(":
            depth += 1
        elif c in "])":
            depth -= 1
        if c == sep and depth == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    out.append("".join(cur))
    return out


def parse_fields(owner, body):
    """Los campos de un struct: [(nombre, tipo, dimensiones, es_puntero)]."""
    if "{" in body:
        raise SystemExit(f"{owner}: un struct anidado dentro de otro no se puede guardar (sacalo a su propio typedef)")
    fields = []
    for decl in split_top(body, ";"):
        decl = " ".join(decl.split())
        if not decl:
            continue
        m = DECL.match(decl)
        if not m or "(" in decl:
            raise SystemExit(f"{owner}: no entiendo el campo '{decl}'")
        ctype = " ".join(m.group(2).split())
        for d in split_top(m.group(3), ","):
            d = d.strip()
            pointer = d.startswith("*")
            d = d.lstrip("* ").replace("const ", "")
            name = re.match(r"(\w+)", d)
            if not name:
                raise SystemExit(f"{owner}: no entiendo el campo '{decl}'")
            dims = re.findall(r"\[([^\]]*)\]", d)
            fields.append((name.group(1), ctype, dims, pointer))
    return fields


def kind_of(owner, name, ctype, dims, structs, enums):
    if ctype in FORBIDDEN:
        raise SystemExit(f"{owner}.{name}: '{ctype}' cambia de tamaño entre plataformas; usa int32_t, int64_t o float")
    if ctype == "char":
        return "SF_TEXT" if dims else "SF_INT"
    if ctype in SIGNED or ctype in enums:
        return "SF_INT"
    if ctype in UNSIGNED:
        return "SF_UINT"
    if ctype in FLOATS:
        return "SF_FLOAT"
    if ctype in BOOLS:
        return "SF_BOOL"
    if ctype in OPAQUE:
        return "SF_BYTES"
    if ctype in structs:
        return "SF_STRUCT"
    raise SystemExit(f"{owner}.{name}: tipo desconocido '{ctype}' (agregalo a esquema.py si es un tipo de raylib)")


def generate():
    files = []
    for base, _, names in os.walk(os.path.join(ROOT, "src")):
        files += [os.path.join(base, n) for n in names if n.endswith(".h")]
    structs, enums = find_types(sorted(files))

    order, seen = [], set()

    def visit(name, stack):
        if name in seen:
            return
        if name in stack:
            raise SystemExit(f"{name}: se contiene a si mismo")
        if name not in structs:
            raise SystemExit(f"{name}: no encuentro su typedef struct en src/")
        for fname, ctype, dims, pointer in parse_fields(name, structs[name][1]):
            if not pointer and kind_of(name, fname, ctype, dims, structs, enums) == "SF_STRUCT":
                visit(ctype, stack + [name])
        seen.add(name)
        order.append(name)

    for r in ROOTS:
        visit(r, [])

    out = ["// Esquema de la partida guardada: generado por tools/save/esquema.py, no editar a mano.",
           "// Describe, campo por campo, todo lo que se guarda (docs/PARTIDAS.md); las posiciones y los",
           "// tamaños los pone el compilador. Si cambias un struct que se guarda, vuelve a correr el script",
           "// (CI lo verifica). Los punteros no se guardan: al cargar, el juego los vuelve a enlazar.",
           ""]
    for name in order:
        out.append(f"// {name} ({structs[name][0]})")
        out.append(f"static const SfField SF_F_{name}[] = {{")
        for fname, ctype, dims, pointer in parse_fields(name, structs[name][1]):
            if pointer:
                out.append(f"    // {fname}: puntero, no se guarda")
                continue
            kind = kind_of(name, fname, ctype, dims, structs, enums)
            sub = f"&SF_T_{ctype}" if kind == "SF_STRUCT" else "NULL"
            macro = "SF_ONE" if not dims else "SF_ARR" if len(dims) == 1 else "SF_ARR2"
            if len(dims) > 2:
                raise SystemExit(f"{name}.{fname}: arreglos de mas de dos dimensiones")
            out.append(f'    {macro}({name}, {fname}, "{ctype}", {kind}, {sub}),')
        out.append("};")
        out.append(f"static const SfType SF_T_{name} = SF_TYPE({name}, SF_F_{name});")
        out.append("")
    return "\n".join(out)


def main():
    text = generate()
    if len(sys.argv) > 1 and sys.argv[1] == "check":
        try:
            with open(OUT, encoding="utf-8") as f:
                current = f.read()
        except FileNotFoundError:
            current = ""
        if current != text:
            print("src/game/save_schema.inc no esta al dia: corre python3 tools/save/esquema.py", file=sys.stderr)
            return 1
        print("esquema de la partida al dia")
        return 0
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"escrito {os.path.relpath(OUT, ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
