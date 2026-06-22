"""Calculo de requerimientos nutricionales del usuario.

Usa la ecuacion de Mifflin-St Jeor para el metabolismo basal (TMB),
factores de actividad para el gasto total (GET/TDEE) y ajustes segun
objetivo. Define rangos de macronutrientes y minimos de micronutrientes.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass

# Factores de actividad fisica (PAL) aplicados sobre la TMB.
ACTIVITY_FACTORS = {
    "sedentario": 1.2,
    "ligero": 1.375,
    "moderado": 1.55,
    "activo": 1.725,
    "muy_activo": 1.9,
}

# Ajuste calorico segun objetivo (deficit/superavit).
GOAL_ADJUSTMENT = {
    "bajar": -0.20,   # deficit 20%
    "mantener": 0.0,
    "subir": 0.15,    # superavit 15%
}

# Gramos de proteina por kg de peso segun objetivo.
PROTEIN_G_PER_KG = {
    "bajar": 2.0,
    "mantener": 1.6,
    "subir": 1.8,
}

# Reparto de grasa como fraccion de las calorias totales.
FAT_KCAL_FRACTION = 0.28

# Factores caloricos (kcal por gramo de macronutriente).
KCAL_PER_G = {"protein": 4.0, "carb": 4.0, "fat": 9.0}


@dataclass
class Requirements:
    """Requerimientos diarios calculados para un usuario."""

    bmr: float            # Tasa metabolica basal (kcal)
    tdee: float           # Gasto energetico total (kcal)
    kcal: float           # Objetivo calorico ajustado por meta
    protein_g: float      # Proteina objetivo (g)
    carb_g: float         # Carbohidratos objetivo (g)
    fat_g: float          # Grasa objetivo (g)
    fiber_g: float        # Fibra minima (g)
    micros: dict          # Minimos de micronutrientes
    limits: dict          # Maximos (ej: sodio)

    def to_dict(self) -> dict:
        return asdict(self)


def mifflin_st_jeor(sex: str, weight_kg: float, height_cm: float, age: int) -> float:
    """Tasa metabolica basal segun Mifflin-St Jeor."""
    base = 10 * weight_kg + 6.25 * height_cm - 5 * age
    return base + 5 if sex.upper().startswith("M") else base - 161


def compute_requirements(
    sex: str,
    age: int,
    weight_kg: float,
    height_cm: float,
    activity_level: str,
    goal: str,
) -> Requirements:
    """Calcula los requerimientos diarios completos del usuario."""
    bmr = mifflin_st_jeor(sex, weight_kg, height_cm, age)
    pal = ACTIVITY_FACTORS.get(activity_level, 1.55)
    tdee = bmr * pal
    # Piso defensivo: ante inputs degenerados (que la API ya valida por rango)
    # evita energia/macros negativos que volverian infactible al optimizador.
    kcal = max(tdee * (1 + GOAL_ADJUSTMENT.get(goal, 0.0)), 0.0)

    protein_g = max(PROTEIN_G_PER_KG.get(goal, 1.6) * weight_kg, 0.0)
    fat_g = max((kcal * FAT_KCAL_FRACTION) / KCAL_PER_G["fat"], 0.0)

    # El resto de las calorias se asigna a carbohidratos.
    remaining_kcal = kcal - protein_g * KCAL_PER_G["protein"] - fat_g * KCAL_PER_G["fat"]
    carb_g = max(remaining_kcal, 0.0) / KCAL_PER_G["carb"]

    # Fibra: ~14 g por cada 1000 kcal (recomendacion general).
    fiber_g = max(14.0 * kcal / 1000.0, 0.0)

    # Minimos de micronutrientes (referencia adulto, ajustable por sexo).
    micros = {
        "calcium_mg": 1000.0,
        "iron_mg": 18.0 if sex.upper().startswith("F") else 8.0,
        "potassium_mg": 3500.0,
        "vitamin_c_mg": 75.0 if sex.upper().startswith("F") else 90.0,
    }
    # Maximos (limites superiores tolerables).
    limits = {"sodium_mg": 2300.0}

    return Requirements(
        bmr=round(bmr, 1),
        tdee=round(tdee, 1),
        kcal=round(kcal, 1),
        protein_g=round(protein_g, 1),
        carb_g=round(carb_g, 1),
        fat_g=round(fat_g, 1),
        fiber_g=round(fiber_g, 1),
        micros=micros,
        limits=limits,
    )
