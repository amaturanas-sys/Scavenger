#!/usr/bin/env python3
"""Pipeline Asset Downloader for SCAVENGERS THRIVE (Estepa) — the command Cline knows.

Downloads a GLB from Hugging Face (TRELLIS.2, RigNet) or GRAIL and brings it into the game through
the model pipeline (tools/modelos/modelos.py traer): the low-poly conversion, the path of its id in
assets/inventario.tsv, the 'importado' state and the provenance in arte/modelos/models_manifest.json.

Usage:
    python3 tools/fetch_pipeline_asset.py --url <URL> --name <ID or NAME> --category <CAT> [--animated] [--girar 90]

Options:
    --url        Direct download link to the GLB file
    --name       The inventory id (arma.corta.akinakes) or its last part (akinakes)
    --category   character, creature, prop, yurt, environment, other (narrows --name when it is not an id)
    --animated   The GLB is rigged and animated (RigNet + clips): validated and copied, not converted
    --id         The inventory id, when --name is ambiguous
    --girar      Degrees around +Y to turn the front to +X (TRELLIS front views: usually 90)

The game draws a model from the path of its id; no C code changes are needed for new models or clips.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "modelos"))
sys.path.insert(0, os.path.join(HERE, "assets"))
import inventario  # noqa: E402
import modelos  # noqa: E402

# Cline's categories -> the inventory's (assets/inventario.tsv).
CATEGORIES = {
    "character": {"personaje"},
    "creature": {"animal"},
    "prop": {"arma", "escudo", "proyectil", "armadura", "vestimenta", "accesorio", "utileria", "totem", "asedio", "vehiculo"},
    "yurt": {"estructura"},
    "environment": {"mapa", "estructura"},
    "other": None,
}


def resolve(items, name, category, explicit=None):
    """The inventory id for --name (an id, or its last part narrowed by the category)."""
    if explicit:
        return explicit
    if name.count(".") == 2:
        return name
    allowed = CATEGORIES.get(category)
    found = [it["id"] for it in items
             if it["id"].rsplit(".", 1)[-1] == name and (allowed is None or it["categoria"] in allowed)]
    if len(found) == 1:
        return found[0]
    if not found:
        raise SystemExit(f"[Pipeline] ERROR: no inventory id ends in '{name}' ({category}); "
                         "see python3 tools/assets/inventario.py faltan")
    raise SystemExit(f"[Pipeline] ERROR: '{name}' is ambiguous: {', '.join(found)}; pass --id")


def main():
    ap = argparse.ArgumentParser(description="Download a pipeline asset into the game (tools/modelos)")
    ap.add_argument("--url", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--category", required=True, choices=sorted(CATEGORIES))
    ap.add_argument("--animated", action="store_true")
    ap.add_argument("--id")
    ap.add_argument("--girar", type=float, default=0.0)
    ap.add_argument("--reemplazar", action="store_true")
    a = ap.parse_args()
    items, errors = inventario.load()
    if errors:
        raise SystemExit("\n".join(errors))
    mid = resolve(items, a.name, a.category, a.id)
    source = "rignet" if a.animated else "trellis2"
    print(f"[Pipeline] {a.name} -> {mid} ({source})")
    return modelos.traer(mid, url=a.url, girar=a.girar, riggeado=a.animated, fuente=source, reemplazar=a.reemplazar,
                         log=lambda msg: print(msg.replace("[modelos]", "[Pipeline]")))


if __name__ == "__main__":
    sys.exit(main())
