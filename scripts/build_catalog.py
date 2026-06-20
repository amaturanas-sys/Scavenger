#!/usr/bin/env python3
"""Genera el catalogo multi-cadena de SCAVENGER.

Lee `data/foods_base.json` (nutricion + precio de referencia autorado) y
produce `data/chilean_foods.json` con un precio por cada cadena de
supermercado, usando un modelo de posicionamiento:

    precio_cadena = base_price_clp * factor_cadena
                    * ajuste_categoria[cadena][grupo]
                    * jitter_determinista(alimento, cadena)

Los mayoristas quedan mas baratos, las cadenas premium mas caras, y hay
variacion por categoria (ej: ferias/Santa Isabel mejores en frutas y
verduras; mayoristas mejores en abarrotes y carnes). Es 100% reproducible
y editable. Se reemplaza por precios reales al conectar los scrapers.

Uso:
    python3 scripts/build_catalog.py
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
SRC = BASE_DIR / "data" / "foods_base.json"
OUT = BASE_DIR / "data" / "chilean_foods.json"

# Grupos de categoria para ajustar precios.
GROUP = {
    "verdura": "produce", "fruta": "produce",
    "carne": "meat", "pescado": "meat",
    "lacteo": "dairy", "huevo": "dairy",
    "cereal": "packaged", "panaderia": "packaged", "legumbre": "packaged",
    "grasa": "packaged", "fruto_seco": "packaged", "otro": "packaged",
}

# Ajuste por cadena y grupo (multiplicador relativo, ~0.85-1.06).
CATEGORY_ADJUST = {
    "jumbo":        {"produce": 1.06, "meat": 1.00, "dairy": 1.00, "packaged": 1.00},
    "lider":        {"produce": 0.97, "meat": 0.95, "dairy": 0.98, "packaged": 0.96},
    "santa_isabel": {"produce": 0.94, "meat": 1.00, "dairy": 0.98, "packaged": 1.00},
    "tottus":       {"produce": 0.98, "meat": 0.94, "dairy": 0.97, "packaged": 0.97},
    "unimarc":      {"produce": 1.00, "meat": 1.00, "dairy": 1.00, "packaged": 1.00},
    "mayorista10":  {"produce": 1.00, "meat": 0.92, "dairy": 0.93, "packaged": 0.85},
}


def _jitter(food_id: str, retailer_id: str) -> float:
    """Variacion determinista en [-4%, +4%] segun alimento y cadena."""
    h = hashlib.md5(f"{food_id}|{retailer_id}".encode()).hexdigest()
    # Usa 4 hex -> 0..65535 -> [-0.04, 0.04]
    frac = int(h[:4], 16) / 0xFFFF
    return 1.0 + (frac - 0.5) * 0.08


def _carries(food_id: str, retailer_id: str) -> bool:
    """Decide (determinista) si una cadena vende el producto.

    Simula que no todo local tiene todo el surtido, sin dejar nunca un
    alimento con menos de 4 cadenas. Jumbo siempre lo tiene.
    """
    if retailer_id == "jumbo":
        return True
    h = hashlib.md5(f"carries|{food_id}|{retailer_id}".encode()).hexdigest()
    return int(h[:2], 16) % 9 != 0  # ~11% de probabilidad de no tenerlo


def build() -> dict:
    data = json.loads(SRC.read_text(encoding="utf-8"))
    retailers = data["retailers"]
    out_foods = []

    for food in data["alimentos"]:
        group = GROUP.get(food["category"], "packaged")
        base = food["base_price_clp"]
        prices = []
        for r in retailers:
            rid = r["id"]
            if not _carries(food["id"], rid):
                continue
            adj = CATEGORY_ADJUST[rid][group]
            price = base * r["factor"] * adj * _jitter(food["id"], rid)
            price = int(round(price / 10.0) * 10)  # redondea a $10
            prices.append({
                "retailer": r["name"],
                "retailer_id": rid,
                "price_clp": price,
                "package_g": food["package_g"],
            })

        # Garantiza al menos 4 cadenas (rellena con las mas baratas faltantes).
        if len(prices) < 4:
            present = {p["retailer_id"] for p in prices}
            for r in sorted(retailers, key=lambda x: x["factor"]):
                if r["id"] in present:
                    continue
                adj = CATEGORY_ADJUST[r["id"]][group]
                price = int(round(base * r["factor"] * adj / 10.0) * 10)
                prices.append({"retailer": r["name"], "retailer_id": r["id"],
                               "price_clp": price, "package_g": food["package_g"]})
                if len(prices) >= 4:
                    break

        prices.sort(key=lambda p: p["price_clp"])
        out_foods.append({
            "id": food["id"], "name": food["name"], "brand": food.get("brand", ""),
            "category": food["category"], "package_g": food["package_g"],
            "serving_g": food["serving_g"], "max_servings_day": food["max_servings_day"],
            "satiety_index": food["satiety_index"], "tags": food.get("tags", []),
            "per_100g": food["per_100g"], "prices": prices,
        })

    return {
        "_meta": {
            **data.get("_meta", {}),
            "fuente_precio": "Estimacion modelada por cadena (build_catalog.py). "
                             "Reemplazable por scraping real (Jumbo/Lider/etc.).",
            "cadenas": [r["name"] for r in retailers],
            "generado_por": "scripts/build_catalog.py",
        },
        "retailers": retailers,
        "alimentos": out_foods,
    }


def main() -> None:
    catalog = build()
    OUT.write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding="utf-8")
    n_foods = len(catalog["alimentos"])
    n_prices = sum(len(f["prices"]) for f in catalog["alimentos"])
    print(f"Catalogo generado: {n_foods} alimentos, {n_prices} precios "
          f"({len(catalog['retailers'])} cadenas) -> {OUT}")


if __name__ == "__main__":
    main()
