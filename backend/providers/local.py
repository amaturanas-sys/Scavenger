"""Proveedor de dataset local (archivo JSON con alimentos chilenos).

Funciona completamente offline y es la fuente por defecto de SCAVENGER.
"""
from __future__ import annotations

import json
from pathlib import Path

from .. import config
from .base import FoodProvider, FoodRecord


class LocalDatasetProvider(FoodProvider):
    name = "local"

    def __init__(self, path: Path | None = None):
        self.path = Path(path) if path else config.SEED_FOODS_PATH

    def fetch_foods(self) -> list[FoodRecord]:
        with open(self.path, encoding="utf-8") as fh:
            raw = json.load(fh)

        records: list[FoodRecord] = []
        for item in raw.get("alimentos", []):
            per = item.get("per_100g", {})
            records.append(FoodRecord(
                id=item["id"],
                name=item["name"],
                category=item.get("category", "otro"),
                brand=item.get("brand", ""),
                retailer=item.get("retailer", ""),
                package_g=float(item.get("package_g", 1000)),
                price_clp=float(item.get("price_clp", 0)),
                serving_g=float(item.get("serving_g", 100)),
                max_servings_day=float(item.get("max_servings_day", 3)),
                satiety_index=float(item.get("satiety_index", 100)),
                kcal=float(per.get("kcal", 0)),
                protein_g=float(per.get("protein_g", 0)),
                carb_g=float(per.get("carb_g", 0)),
                fat_g=float(per.get("fat_g", 0)),
                fiber_g=float(per.get("fiber_g", 0)),
                sodium_mg=float(per.get("sodium_mg", 0)),
                calcium_mg=float(per.get("calcium_mg", 0)),
                iron_mg=float(per.get("iron_mg", 0)),
                potassium_mg=float(per.get("potassium_mg", 0)),
                vitamin_c_mg=float(per.get("vitamin_c_mg", 0)),
                tags=list(item.get("tags", [])),
            ))
        return records
