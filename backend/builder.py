"""Constructor de comidas por roles (estilo "tragamonedas").

En vez de generar la minuta entera automaticamente, el usuario arma cada
comida eligiendo un elemento por **rol** (proteina, carbohidrato, grasa,
vegetal, aderezo). Cada candidato viene **pre-porcionado** para cubrir la
parte que le toca de la meta de esa comida, de modo que cualquier combinacion
de carretes suma valores cercanos a la meta nutricional ("los macros calzan").

Expone:
  * build_slots  -> candidatos por rol para una comida (carretes).
  * random_meal  -> una eleccion aleatoria (un candidato por rol) + totales.
  * summarize    -> suma de una seleccion y su ajuste contra la meta.
"""
from __future__ import annotations

import random as _random

from sqlalchemy.orm import Session

from .learning import get_preferences
from .models import User
from .services import _eligible_foods, _price_in_retailers, user_requirements

# Rol de cada categoria de alimento para los carretes.
ROLE_BY_CATEGORY = {
    "carne": "proteina", "pescado": "proteina", "huevo": "proteina",
    "legumbre": "proteina", "lacteo": "proteina",
    "cereal": "carbohidrato", "panaderia": "carbohidrato",
    "grasa": "grasa", "fruto_seco": "grasa",
    "verdura": "vegetal", "fruta": "vegetal",
    "otro": "aderezo",
}
# Verduras con perfil de carbohidrato (tuberculos / choclo): van al rol carb.
CARB_TUBERS = {"papa", "camote", "choclo_congelado", "zapallo", "betarraga"}

ROLE_LABELS = {
    "proteina": "Proteínas", "carbohidrato": "Carbohidratos", "grasa": "Grasas",
    "vegetal": "Vegetales / Fruta", "aderezo": "Aderezos",
}

# Plantillas de comida: fraccion de los requerimientos diarios y carretes.
MEAL_TEMPLATES = {
    "desayuno": {"fraction": 0.25, "slots": ["carbohidrato", "proteina", "grasa", "vegetal"]},
    "snack":    {"fraction": 0.10, "slots": ["proteina", "vegetal"]},
    "almuerzo": {"fraction": 0.35, "slots": ["proteina", "carbohidrato", "vegetal", "grasa", "aderezo"]},
    "cena":     {"fraction": 0.30, "slots": ["proteina", "carbohidrato", "vegetal", "aderezo"]},
}

# Tope absoluto de gramos por rol (evita porciones absurdas).
ABS_MAX_G = {"proteina": 300, "carbohidrato": 300, "grasa": 50, "vegetal": 250, "aderezo": 40}

_SUM_FIELDS = ("kcal", "protein_g", "carb_g", "fat_g", "fiber_g", "cost_clp", "satiety_contrib")


def role_of(food) -> str:
    if food.id in CARB_TUBERS:
        return "carbohidrato"
    return ROLE_BY_CATEGORY.get(food.category, "aderezo")


# Clasificacion fina por "origen": primer filtro de la ruleta (antes de ordenar
# por kcal/precio). Derivada del nombre/categoria; heuristica, afinable.
_ORIGIN_KEYWORDS = [
    ("pollo", ("pollo", "pechuga", "trutro", "ala de")),
    ("pavo", ("pavo",)),
    ("vacuno", ("vacuno", "posta", "molida", "asiento", "lomo", "churrasco", "asado de tira", " res")),
    ("cerdo", ("cerdo", "chuleta", "costillar", "pulpa de cerdo")),
    ("pescado", ("atun", "atún", "jurel", "salmon", "salmón", "merluza", "reineta", "pescado", "sardina")),
    ("huevo", ("huevo",)),
    ("legumbres", ("lenteja", "poroto", "garbanzo", "arveja")),
    ("lacteo", ("leche", "yogur", "yoghurt", "queso", "quesillo")),
    ("arroz", ("arroz",)),
    ("fideos", ("fideo", "pasta", "spaghetti", "tallarin", "tallarín", "espagueti")),
    ("avena", ("avena",)),
    ("pan", ("pan ", "marraqueta", "hallulla", "tortilla")),
    ("papa", ("papa", "pure de papa")),
    ("frutos secos", ("mani", "maní", "almendra", "nuez", "nueces")),
    ("aceite", ("aceite", "oliva")),
]


def food_origin(food) -> str:
    """Etiqueta de origen del alimento (pollo/vacuno/arroz/...) para filtrar."""
    n = (food.name or "").lower()
    for label, kws in _ORIGIN_KEYWORDS:
        if any(k in n for k in kws):
            return label
    return food.category or "otro"


def meal_type(meal: str) -> str:
    """Normaliza una etiqueta de comida (ej: 'snack2') a su tipo base."""
    m = (meal or "").lower()
    for t in MEAL_TEMPLATES:
        if m.startswith(t):
            return t
    return "almuerzo"


def meal_target(req, meal: str, min_protein: float = 0.0) -> dict:
    f = MEAL_TEMPLATES[meal_type(meal)]["fraction"]
    return {
        "kcal": round(req.kcal * f, 1),
        # Piso de proteina por plato (especificacion del usuario): el objetivo de
        # proteina de la comida no baja de min_protein.
        "protein_g": round(max(req.protein_g * f, min_protein or 0.0), 1),
        "carb_g": round(req.carb_g * f, 1), "fat_g": round(req.fat_g * f, 1),
        "fraction": f,
    }


def _portion_grams(food, role: str, target: dict) -> float:
    """Porcion (g) para que el alimento cubra su parte de la meta de la comida."""
    sp = food.serving_g or 100.0
    if role == "proteina" and food.protein_g > 0:
        g = target["protein_g"] / (food.protein_g / 100.0)
    elif role == "carbohidrato" and food.carb_g > 0:
        g = target["carb_g"] / (food.carb_g / 100.0)
    elif role == "grasa" and food.fat_g > 0:
        # La grasa tambien llega desde proteina/carbohidrato: cubrimos ~60%.
        g = (target["fat_g"] * 0.6) / (food.fat_g / 100.0)
    elif role == "vegetal":
        g = sp * 1.2
    else:  # aderezo / fallback
        g = min(sp, 20.0)

    lo = max(10.0, sp * 0.4)
    hi = min(ABS_MAX_G.get(role, 300), max(sp * max(food.max_servings_day, 1), lo + 5))
    lo = min(lo, hi)  # el tope absoluto del rol siempre manda sobre el piso
    g = max(lo, min(g, hi))
    return float(round(g / 5.0) * 5)  # multiplos de 5 g


# Referencias diarias (aprox.) para puntuar la riqueza de micronutrientes de una
# porcion: fibra y los micros del "paso 3" del armado guiado (verduras/legumbres).
_MICRO_RDA = {
    "fiber_g": 25.0, "vitamin_c_mg": 80.0, "vitamin_e_mg": 12.0,
    "iron_mg": 14.0, "zinc_mg": 11.0, "calcium_mg": 800.0,
}


def micro_score(food, grams: float) -> float:
    """Puntaje 0..100 de aporte de micronutrientes (fibra + vitC/E, Fe, Zn, Ca)
    de una porcion de `grams` g: promedio del %RDA cubierto (cada uno topado al
    100%). Sirve para ordenar las verduras/legumbres por "plato redondo"."""
    scale = (grams or 0.0) / 100.0
    total = 0.0
    for attr, rda in _MICRO_RDA.items():
        val = (getattr(food, attr, 0.0) or 0.0) * scale
        total += min(val / rda, 1.0) if rda else 0.0
    return round(total / len(_MICRO_RDA) * 100.0, 1)


def _candidate(food, role: str, target: dict, price_per_g: float, retailer: str) -> dict:
    g = _portion_grams(food, role, target)
    scale = g / 100.0
    pkg = float(food.package_g or 0)
    return {
        "food_id": food.id, "name": food.name, "brand": food.brand, "category": food.category,
        "role": role, "origin": food_origin(food), "retailer": retailer, "grams": g,
        "servings": round(g / food.serving_g, 2) if food.serving_g else 0.0,
        # Cuantos platos rinde el envase recomendado a esta porcion.
        "package_g": pkg,
        "platos_por_envase": int(pkg // g) if g > 0 and pkg > 0 else 0,
        "cost_clp": round(price_per_g * g, 1),
        "kcal": round(food.kcal * scale, 1), "protein_g": round(food.protein_g * scale, 1),
        "carb_g": round(food.carb_g * scale, 1), "fat_g": round(food.fat_g * scale, 1),
        "fiber_g": round(food.fiber_g * scale, 1),
        # Puntaje de micronutrientes de la porcion (para ordenar el paso 3).
        "micro_score": micro_score(food, g),
        "satiety_contrib": round(food.satiety_index / 100.0 * g, 1),
    }


def _candidates_by_role(db: Session, user: User, meal: str):
    req = user_requirements(user)
    target = meal_target(req, meal, getattr(user, "min_protein_per_meal_g", 0.0))
    preferred = set(user.preferred_retailers or [])
    # Preferencias aprendidas (peso por food_id) para ordenar p.ej. los aderezos.
    prefs = get_preferences(db, user.id) if getattr(user, "id", None) else {}
    slots = MEAL_TEMPLATES[meal_type(meal)]["slots"]
    by_role: dict[str, list] = {r: [] for r in slots}

    for food in _eligible_foods(db, user):
        r = role_of(food)
        if r not in by_role:
            continue
        priced = _price_in_retailers(food, preferred)
        if priced is None:
            continue
        ppg, retailer = priced
        cand = _candidate(food, r, target, ppg, retailer)
        cand["pref"] = round(float(prefs.get(food.id, 0.0)), 3)
        by_role[r].append(cand)

    for r in by_role:
        by_role[r].sort(key=lambda c: c["cost_clp"])  # de lo mas economico hacia arriba
    return target, by_role, slots


def build_slots(db: Session, user: User, meal: str) -> dict:
    target, by_role, slots = _candidates_by_role(db, user, meal)
    return {
        "meal": meal, "meal_type": meal_type(meal), "target": target,
        "slots": [
            {"role": r, "label": ROLE_LABELS.get(r, r), "candidates": by_role[r]}
            for r in slots
        ],
    }


def summarize(selected: list[dict], target: dict) -> dict:
    """Suma una seleccion y calcula su ajuste (fit) contra la meta de la comida."""
    totals = {k: 0.0 for k in _SUM_FIELDS}
    for it in selected:
        for k in _SUM_FIELDS:
            totals[k] += it.get(k, 0.0)
    totals = {k: round(v, 1) for k, v in totals.items()}

    def ratio(a, b):
        return round(a / b, 2) if b else 0.0

    fit = {
        "kcal": ratio(totals["kcal"], target["kcal"]),
        "protein_g": ratio(totals["protein_g"], target["protein_g"]),
        "carb_g": ratio(totals["carb_g"], target["carb_g"]),
        "fat_g": ratio(totals["fat_g"], target["fat_g"]),
    }
    fit_ok = 0.7 <= fit["kcal"] <= 1.3 and fit["protein_g"] >= 0.7
    return {"totals": totals, "fit": fit, "fit_ok": fit_ok}


def random_meal(db: Session, user: User, meal: str, rng=None) -> dict:
    rng = rng or _random
    target, by_role, slots = _candidates_by_role(db, user, meal)
    selection = [rng.choice(by_role[r]) for r in slots if by_role[r]]
    return {
        "meal": meal, "meal_type": meal_type(meal), "target": target,
        "selection": selection, **summarize(selection, target),
    }
