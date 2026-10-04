#!/usr/bin/env python3
"""Textos de la interfaz en otros idiomas (assets/i18n/<idioma>.tsv).

El codigo escribe cada texto visible en español dentro de T("...") o N_("...")
(src/sim/lang.h). Los nombres de los objetos salen de assets/inventario.tsv.

Uso:
  python3 tools/assets/i18n.py extraer          añade a en.tsv los textos nuevos (sin traducir) y quita los que ya no se usan
  python3 tools/assets/i18n.py check            falla si falta una traduccion o si un formato (%d, %s...) no coincide
  python3 tools/assets/i18n.py faltan [N]       lista los textos sin traducir
  python3 tools/assets/i18n.py buscar           textos en español que aun no estan en T(...) (ayuda para revisar)

Formato de en.tsv: "español<TAB>english" por linea; \\n y \\t escapados; # comenta.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "src")
INV = os.path.join(ROOT, "assets", "inventario.tsv")
LANGS = ["en"]

LIT = r'"((?:[^"\\\n]|\\.)*)"'
CALL = re.compile(r'\b(?:T|N_)\(\s*((?:' + LIT + r'\s*)+)\)')
PIECE = re.compile(LIT)
FMT = re.compile(r'%[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|h|ll|l|z)?[diouxXeEfgGcsp%]')


def c_unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            out.append({"n": "\n", "t": "\t", "\\": "\\", '"': '"', "'": "'"}.get(n, "\\" + n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def tsv_escape(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def tsv_unescape(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append({"n": "\n", "t": "\t"}.get(s[i + 1], s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def source_files():
    for base, _, files in os.walk(SRC):
        for f in sorted(files):
            if f.endswith((".c", ".h")):
                yield os.path.join(base, f)


def keys_in_code():
    keys = {}
    for path in source_files():
        text = open(path, encoding="utf-8").read()
        for m in CALL.finditer(text):
            s = "".join(c_unescape(p) for p in PIECE.findall(m.group(1)))
            if s:
                keys.setdefault(s, os.path.relpath(path, ROOT))
    return keys


def keys_in_inventory():
    keys = {}
    for line in open(INV, encoding="utf-8"):
        if line.startswith("#") or line.startswith("id\t"):
            continue
        parts = line.rstrip("\n").split("\t")
        if len(parts) > 1 and parts[1]:
            keys.setdefault(parts[1], "assets/inventario.tsv")
    return keys


def all_keys():
    k = keys_in_code()
    for key, where in keys_in_inventory().items():
        k.setdefault(key, where)
    return k


def tsv_path(lang):
    return os.path.join(ROOT, "assets", "i18n", f"{lang}.tsv")


def read_tsv(lang):
    entries = {}
    path = tsv_path(lang)
    if not os.path.exists(path):
        return entries
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        entries[tsv_unescape(parts[0])] = tsv_unescape(parts[1]) if len(parts) > 1 else ""
    return entries


def write_tsv(lang, keys, entries):
    path = tsv_path(lang)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    by_file = {}
    for k, where in keys.items():
        by_file.setdefault(where, []).append(k)
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"# Traducciones al idioma '{lang}'. Columna 1: el texto en español tal como esta en el codigo;\n")
        f.write("# columna 2: la traduccion (vacia = sin traducir, se muestra en español). Conserva %d, %s, %.1f...\n")
        f.write("# Se regenera con 'python3 tools/assets/i18n.py extraer' (las traducciones se conservan).\n")
        for where in sorted(by_file):
            f.write(f"# --- {where}\n")
            for k in sorted(by_file[where]):
                f.write(f"{tsv_escape(k)}\t{tsv_escape(entries.get(k, ''))}\n")


def extraer():
    keys = all_keys()
    for lang in LANGS:
        entries = read_tsv(lang)
        gone = [k for k in entries if k not in keys]
        write_tsv(lang, keys, entries)
        new = sum(1 for k in keys if not entries.get(k))
        print(f"{lang}.tsv: {len(keys)} textos, {new} sin traducir, {len(gone)} quitados (ya no se usan).")


def check(verbose=True):
    ok = True
    keys = all_keys()
    for lang in LANGS:
        entries = read_tsv(lang)
        missing = [k for k in keys if not entries.get(k)]
        stale = [k for k in entries if k not in keys]
        bad = [k for k in keys if entries.get(k) and sorted(FMT.findall(k)) != sorted(FMT.findall(entries[k]))]
        for k in bad:
            print(f"{lang}: formato distinto en «{k}» -> «{entries[k]}»")
        if missing:
            print(f"{lang}: {len(missing)} textos sin traducir (python3 tools/assets/i18n.py faltan).")
        if stale:
            print(f"{lang}: {len(stale)} textos que ya no se usan (python3 tools/assets/i18n.py extraer).")
        ok = ok and not missing and not bad and not stale
        if verbose:
            print(f"{lang}: {len(keys) - len(missing)}/{len(keys)} traducidos.")
    return ok


def faltan(n):
    keys = all_keys()
    for lang in LANGS:
        entries = read_tsv(lang)
        miss = [(w, k) for k, w in keys.items() if not entries.get(k)]
        for w, k in sorted(miss)[:n]:
            print(f"{lang}\t{w}\t{tsv_escape(k)}")


SPANISH = re.compile(r"[áéíóúñ¿¡ÁÉÍÓÚÑ]|\b(?:el|la|los|las|de|del|que|con|para|por|una?|no|se|tu|te|sin)\b", re.I)


def buscar():
    """Literales con pinta de español fuera de T(...)/N_(...)."""
    for path in source_files():
        for no, line in enumerate(open(path, encoding="utf-8"), 1):
            s = line.strip()
            if s.startswith(("//", "#include", "*")) or "TraceLog" in line:
                continue
            masked = CALL.sub("", line)
            masked = re.sub(r"//.*", "", masked)
            for lit in PIECE.findall(masked):
                txt = c_unescape(lit)
                if len(txt) > 2 and " " in txt and SPANISH.search(txt) and not re.match(r"^[a-z_.]+$", txt):
                    print(f"{os.path.relpath(path, ROOT)}:{no}: {txt[:90]}")


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return 1
    if a[0] == "extraer":
        extraer()
    elif a[0] == "check":
        return 0 if check() else 1
    elif a[0] == "faltan":
        faltan(int(a[1]) if len(a) > 1 else 10 ** 9)
    elif a[0] == "buscar":
        buscar()
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
