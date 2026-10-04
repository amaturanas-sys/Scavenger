#!/usr/bin/env python3
"""Mezcla un diccionario {español: traduccion} (archivo .py con TR = {...}) en assets/i18n/<idioma>.tsv.
Uso: python3 tools/assets/i18n_merge.py archivo.py [idioma=en] [--pisar]"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import i18n  # noqa: E402

path, lang = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else "en")
ns = {}
exec(open(path, encoding="utf-8").read(), ns)
tr = ns["TR"]
keys = i18n.all_keys()
entries = i18n.read_tsv(lang)
put = unknown = 0
for k, v in tr.items():
    if k not in keys:
        unknown += 1
        continue
    if v and (not entries.get(k) or "--pisar" in sys.argv):
        entries[k] = v
        put += 1
i18n.write_tsv(lang, keys, entries)
print(f"{put} traducciones nuevas; {unknown} claves que el codigo ya no usa.")
