"""Lista de compras consolidada por cadena de supermercado.

Toma una minuta (diaria o semanal) y agrega cuanto hay que comprar de cada
alimento, agrupado por la cadena donde conviene comprarlo. Para cada item
calcula los **envases** a comprar (los productos se venden por envase, no a
granel exacto) redondeando hacia arriba, y entrega subtotales por cadena.
"""
from __future__ import annotations

import math
from collections.abc import Iterator

from sqlalchemy.orm import Session

from .models import Food


def _iter_items(payload: dict) -> Iterator[dict]:
    """Recorre los items de una minuta diaria o semanal."""
    for meal in payload.get("meals", []):
        yield from meal.get("items", [])
    for day in payload.get("days", []):
        for meal in day.get("plan", {}).get("meals", []):
            yield from meal.get("items", [])


def build_shopping_list(db: Session, payload: dict) -> dict:
    """Construye la lista de compras agrupada por cadena.

    Devuelve, por cadena, los productos con la cantidad necesaria, los envases
    a comprar y el costo de esos envases, mas subtotales y el total general.
    """
    # Agrega gramos y costo consumido por (alimento, cadena).
    agg: dict[tuple, dict] = {}
    for it in _iter_items(payload):
        fid = it.get("food_id")
        if not fid:
            continue
        key = (fid, it.get("retailer", "") or "Sin asignar")
        a = agg.setdefault(key, {
            "grams": 0.0, "cost": 0.0,
            "name": it.get("name", ""), "brand": it.get("brand", ""),
            "category": it.get("category", ""),
        })
        a["grams"] += it.get("grams", 0.0)
        a["cost"] += it.get("cost_clp", 0.0)

    foods = {f.id: f for f in db.query(Food).all()}
    retailers: dict[str, dict] = {}

    for (food_id, retailer), a in agg.items():
        food = foods.get(food_id)
        package_g = retailer_id = package_price = None
        if food:
            fp = next((p for p in food.prices if p.retailer == retailer), None)
            if fp:
                package_g, package_price, retailer_id = fp.package_g, fp.price_clp, fp.retailer_id
            else:  # cae al precio/envase mas economico del alimento
                package_g, package_price = food.package_g, food.price_clp

        packages = math.ceil(a["grams"] / package_g) if package_g else None
        packages_cost = round(packages * package_price, 1) if (packages and package_price) else None

        item = {
            "food_id": food_id, "name": a["name"], "brand": a["brand"], "category": a["category"],
            "needed_g": round(a["grams"], 1), "consumed_cost_clp": round(a["cost"], 1),
            "package_g": package_g, "packages": packages,
            "package_price_clp": package_price, "packages_cost_clp": packages_cost,
        }
        r = retailers.setdefault(retailer, {"retailer": retailer, "retailer_id": retailer_id, "items": []})
        r["items"].append(item)

    out_retailers = []
    total_consumed = total_packages = 0.0
    for retailer in sorted(retailers):
        r = retailers[retailer]
        r["items"].sort(key=lambda x: x["consumed_cost_clp"], reverse=True)
        sub_consumed = round(sum(i["consumed_cost_clp"] for i in r["items"]), 1)
        sub_packages = round(sum((i["packages_cost_clp"] or 0) for i in r["items"]), 1)
        r["subtotal_consumed_clp"] = sub_consumed
        r["subtotal_packages_clp"] = sub_packages
        r["item_count"] = len(r["items"])
        total_consumed += sub_consumed
        total_packages += sub_packages
        out_retailers.append(r)

    return {
        "retailers": out_retailers,
        "total_consumed_clp": round(total_consumed, 1),
        "total_packages_clp": round(total_packages, 1),
        "retailer_count": len(out_retailers),
    }
