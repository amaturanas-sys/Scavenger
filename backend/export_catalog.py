"""Exporta el catalogo de la base de datos a data/chilean_foods.json.

Tras correr `refresh_prices` (precios reales) y/o `enrich_nutrition`
(nutricion real), los datos quedan en la base de datos. Este modulo los
vuelca de vuelta al archivo de catalogo que lee el proveedor local, para
**versionar y compartir** los datos reales en el repositorio.

Uso:
    python3 -m backend.export_catalog                 # a data/chilean_foods.json
    python3 -m backend.export_catalog --output x.json
"""
from __future__ import annotations

import argparse
import json
from datetime import date
from pathlib import Path

from sqlalchemy.orm import Session

from . import config
from .database import SessionLocal, init_db
from .models import Food, FoodPrice

_PER_100G_FIELDS = (
    "kcal", "protein_g", "carb_g", "fat_g", "fiber_g",
    "sodium_mg", "calcium_mg", "iron_mg", "potassium_mg", "vitamin_c_mg",
)


def export_catalog(db: Session, path: Path) -> int:
    """Escribe el catalogo actual de la BD al archivo JSON. Devuelve N alimentos."""
    foods = db.query(Food).order_by(Food.name).all()

    # Cadenas presentes (a partir de los precios).
    retailers: dict[str, str] = {}
    for rid, name in db.query(FoodPrice.retailer_id, FoodPrice.retailer).distinct().all():
        if rid:
            retailers[rid] = name

    alimentos = []
    for f in foods:
        prices = sorted(f.prices, key=lambda p: p.price_per_g)
        alimentos.append({
            "id": f.id, "name": f.name, "brand": f.brand, "category": f.category,
            "package_g": f.package_g, "serving_g": f.serving_g,
            "max_servings_day": f.max_servings_day, "satiety_index": f.satiety_index,
            "tags": f.tags or [],
            "per_100g": {k: getattr(f, k) for k in _PER_100G_FIELDS},
            "prices": [
                {"retailer": p.retailer, "retailer_id": p.retailer_id,
                 "price_clp": p.price_clp, "package_g": p.package_g}
                for p in prices
            ],
        })

    catalog = {
        "_meta": {
            "descripcion": "Catalogo exportado desde la base de datos de SCAVENGER.",
            "moneda": "CLP",
            "generado_por": "backend.export_catalog",
            "actualizado": date.today().isoformat(),
        },
        "retailers": [{"id": rid, "name": name} for rid, name in sorted(retailers.items(), key=lambda x: x[1])],
        "alimentos": alimentos,
    }

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding="utf-8")
    return len(alimentos)


def main() -> None:
    parser = argparse.ArgumentParser(description="Exporta el catalogo de la BD a JSON.")
    parser.add_argument("--output", default=str(config.SEED_FOODS_PATH),
                        help="Ruta de salida (def: data/chilean_foods.json).")
    args = parser.parse_args()

    init_db()
    db = SessionLocal()
    try:
        n = export_catalog(db, Path(args.output))
        print(f"Catalogo exportado: {n} alimentos -> {args.output}")
    finally:
        db.close()


if __name__ == "__main__":
    main()
