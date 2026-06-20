"""Distribuye la canasta diaria optimizada en las comidas del dia.

Toma el resultado del optimizador (gramos totales por alimento) y lo
reparte entre desayuno, almuerzo, once y cena segun afinidades por
categoria y la fraccion calorica objetivo de cada comida.
"""
from __future__ import annotations

from .optimizer import OptimizeResult, PlanItem

# Fraccion calorica objetivo por comida (debe sumar 1 entre las activas).
DEFAULT_MEAL_FRACTIONS = {
    "desayuno": 0.25,
    "almuerzo": 0.35,
    "once": 0.15,
    "cena": 0.25,
}

# Afinidad de cada categoria con cada comida (0 = nunca, mayor = mas tipico).
CATEGORY_AFFINITY = {
    "cereal":     {"desayuno": 1.0, "almuerzo": 3.0, "once": 0.5, "cena": 2.0},
    "panaderia":  {"desayuno": 3.0, "almuerzo": 0.5, "once": 3.0, "cena": 1.0},
    "legumbre":   {"desayuno": 0.2, "almuerzo": 3.0, "once": 0.2, "cena": 2.0},
    "carne":      {"desayuno": 0.2, "almuerzo": 3.0, "once": 0.2, "cena": 2.0},
    "pescado":    {"desayuno": 0.2, "almuerzo": 3.0, "once": 0.3, "cena": 2.0},
    "lacteo":     {"desayuno": 3.0, "almuerzo": 0.5, "once": 3.0, "cena": 0.5},
    "huevo":      {"desayuno": 2.5, "almuerzo": 1.5, "once": 1.0, "cena": 1.5},
    "verdura":    {"desayuno": 0.3, "almuerzo": 3.0, "once": 0.5, "cena": 3.0},
    "fruta":      {"desayuno": 2.5, "almuerzo": 1.0, "once": 2.5, "cena": 0.5},
    "grasa":      {"desayuno": 0.5, "almuerzo": 2.0, "once": 0.5, "cena": 2.0},
    "fruto_seco": {"desayuno": 1.5, "almuerzo": 0.5, "once": 2.5, "cena": 0.5},
    "otro":       {"desayuno": 1.0, "almuerzo": 1.0, "once": 1.0, "cena": 1.0},
}


def _split_item(item: PlanItem, meals: list[str], fractions: dict[str, float]) -> dict[str, float]:
    """Reparte los gramos de un item entre comidas segun afinidad x fraccion."""
    aff = CATEGORY_AFFINITY.get(item.category, CATEGORY_AFFINITY["otro"])
    weights = {m: aff.get(m, 0.0) * fractions.get(m, 0.0) for m in meals}
    total_w = sum(weights.values())
    if total_w <= 0:
        # Sin afinidad: reparte por fraccion calorica.
        weights = {m: fractions.get(m, 0.0) for m in meals}
        total_w = sum(weights.values()) or 1.0
    return {m: item.grams * w / total_w for m, w in weights.items()}


def _scaled_item(item: PlanItem, grams: float) -> dict:
    """Genera la version de un item escalada a `grams`."""
    if item.grams <= 0:
        ratio = 0.0
    else:
        ratio = grams / item.grams
    return {
        "food_id": item.food_id,
        "name": item.name,
        "brand": item.brand,
        "category": item.category,
        "grams": round(grams, 1),
        "servings": round(item.servings * ratio, 2),
        "cost_clp": round(item.cost_clp * ratio, 1),
        "kcal": round(item.kcal * ratio, 1),
        "protein_g": round(item.protein_g * ratio, 1),
        "carb_g": round(item.carb_g * ratio, 1),
        "fat_g": round(item.fat_g * ratio, 1),
        "fiber_g": round(item.fiber_g * ratio, 1),
        "satiety_contrib": round(item.satiety_contrib * ratio, 1),
    }


def distribute_into_meals(
    result: OptimizeResult,
    meals: list[str] | None = None,
    fractions: dict[str, float] | None = None,
) -> dict:
    """Construye la estructura de comidas a partir del resultado optimizado."""
    meals = meals or ["desayuno", "almuerzo", "once", "cena"]
    fractions = fractions or {m: DEFAULT_MEAL_FRACTIONS.get(m, 1.0 / len(meals)) for m in meals}
    # Normaliza fracciones por si la lista de comidas no cubre las 4.
    total_f = sum(fractions.get(m, 0.0) for m in meals) or 1.0
    fractions = {m: fractions.get(m, 0.0) / total_f for m in meals}

    meal_items: dict[str, list[dict]] = {m: [] for m in meals}
    for item in result.items:
        grams_per_meal = _split_item(item, meals, fractions)
        for m, g in grams_per_meal.items():
            if g >= 0.5:
                meal_items[m].append(_scaled_item(item, g))

    meals_out = []
    for m in meals:
        items = meal_items[m]
        subtotal = {
            "kcal": round(sum(i["kcal"] for i in items), 1),
            "protein_g": round(sum(i["protein_g"] for i in items), 1),
            "carb_g": round(sum(i["carb_g"] for i in items), 1),
            "fat_g": round(sum(i["fat_g"] for i in items), 1),
            "cost_clp": round(sum(i["cost_clp"] for i in items), 1),
            "satiety": round(sum(i["satiety_contrib"] for i in items), 1),
        }
        meals_out.append({"meal": m, "items": items, "subtotal": subtotal})

    return {
        "meals": meals_out,
        "totals": result.totals,
        "warnings": result.warnings,
        "feasible": result.feasible,
    }
