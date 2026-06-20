"""Completa la nutricion de alimentos cruzando con FatSecret.

Util para alimentos nuevos que llegan sin valores nutricionales (por
ejemplo, productos traidos por el scraping de precios). Por cada alimento
busca su nutricion por 100 g en FatSecret y rellena los campos faltantes.
"""
from __future__ import annotations

from dataclasses import dataclass, field

from sqlalchemy.orm import Session

from .models import Food
from .providers.fatsecret import FatSecretProvider

NUTRIENT_FIELDS = (
    "kcal", "protein_g", "carb_g", "fat_g", "fiber_g",
    "sodium_mg", "calcium_mg", "iron_mg", "potassium_mg", "vitamin_c_mg",
)


@dataclass
class EnrichResult:
    enriched: int = 0
    skipped: int = 0
    not_found: int = 0
    misses: list[str] = field(default_factory=list)

    def summary(self) -> str:
        return (f"[fatsecret] {self.enriched} enriquecidos, {self.skipped} omitidos "
                f"(ya tenian datos), {self.not_found} sin coincidencia.")


def _is_missing(food: Food) -> bool:
    """Un alimento sin nutricion util (todo en cero)."""
    return food.kcal <= 0 and food.protein_g <= 0 and food.carb_g <= 0 and food.fat_g <= 0


def enrich_foods(
    db: Session,
    provider: FatSecretProvider | None = None,
    only_missing: bool = True,
    limit: int | None = None,
    log=print,
) -> EnrichResult:
    """Rellena la nutricion de los alimentos usando FatSecret."""
    provider = provider or FatSecretProvider()
    if not provider.configured:
        raise SystemExit(
            "Faltan credenciales de FatSecret. Define SCAVENGER_FATSECRET_KEY y "
            "SCAVENGER_FATSECRET_SECRET para habilitar el cruce nutricional."
        )

    foods = db.query(Food).order_by(Food.name).all()
    if limit:
        foods = foods[:limit]

    result = EnrichResult()
    for food in foods:
        if only_missing and not _is_missing(food):
            result.skipped += 1
            continue
        try:
            per = provider.nutrition_for(food.name, food.brand)
        except PermissionError as exc:
            raise SystemExit(str(exc)) from exc

        if not per:
            result.not_found += 1
            result.misses.append(food.id)
            continue

        for f in NUTRIENT_FIELDS:
            setattr(food, f, per[f])
        result.enriched += 1

    db.commit()
    log(result.summary())
    return result
