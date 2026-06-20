"""CLI para completar la nutricion de alimentos via FatSecret.

Uso:
    python3 -m backend.enrich_nutrition            # solo alimentos sin datos
    python3 -m backend.enrich_nutrition --all      # recalcula todos
    python3 -m backend.enrich_nutrition --limit 10

Requiere credenciales (SCAVENGER_FATSECRET_KEY / SCAVENGER_FATSECRET_SECRET)
y los hosts de FatSecret en el allowlist de egress del entorno.
"""
from __future__ import annotations

import argparse

from .database import SessionLocal, init_db
from .nutrition_enrich import enrich_foods
from .seed import seed_foods


def main() -> None:
    parser = argparse.ArgumentParser(description="Completa nutricion via FatSecret.")
    parser.add_argument("--all", action="store_true",
                        help="Recalcula todos los alimentos (no solo los que faltan).")
    parser.add_argument("--limit", type=int, default=None, help="Maximo de alimentos a procesar.")
    args = parser.parse_args()

    init_db()
    db = SessionLocal()
    try:
        seed_foods(db, "local", refresh=False)
        enrich_foods(db, only_missing=not args.all, limit=args.limit)
    finally:
        db.close()


if __name__ == "__main__":
    main()
